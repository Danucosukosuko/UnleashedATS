#include "Common.h"
#include "Draw.h"
#include "Menu.h"
#include "Ota.h"
#include "Utils.h"
#include "Notes.h"
#include "UpdateScreen.h"

#include <atomic>
#include <memory>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_app_desc.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <ArduinoJson.h>

#define OTA_HTTP_TIMEOUT_SECONDS 15
#define OTA_JSON_MAX   (96 * 1024)  // Largest release list read from GitHub
#define OTA_NOTES_MAX  (8 * 1024)   // Longest release notes kept

enum OtaVariant : uint8_t { OTA_OSPI = 1, OTA_QSPI = 2, OTA_LILYGO = 3 };
static const char *const otaVariantNames[] = {"", "ospi", "qspi", "lilygo-t-embed"};

struct OtaMetadata
{
  uint32_t magic;
  uint16_t version;
  uint8_t format;
  uint8_t generation;
  uint8_t variant;
  uint8_t reserved[3];
};

static constexpr uint32_t OTA_METADATA_MAGIC = 0x4D535441; // "ATSM" in the image
static constexpr uint8_t OTA_METADATA_FORMAT = 1;
static const OtaMetadata otaMetadata __attribute__((section(".rodata_custom_desc"), used, aligned(4))) =
{
  OTA_METADATA_MAGIC, VER_APP, OTA_METADATA_FORMAT, VER_OTA,
#if defined(LILYGO_SI473X)
  OTA_LILYGO,
#elif defined(CONFIG_SPIRAM_MODE_OCT)
  OTA_OSPI,
#else
  OTA_QSPI,
#endif
  // The CFW version, read back when this image becomes the previous firmware
  {(uint8_t)(VER_CFW & 0xFF), (uint8_t)(VER_CFW >> 8), VER_BETA}
};
static constexpr size_t OTA_METADATA_OFFSET = sizeof(esp_image_header_t) +
  sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
static_assert(sizeof(OtaMetadata) == 12, "OTA metadata must have a stable layout");
static_assert(OTA_METADATA_OFFSET == 288, "Update the release metadata checker for this image format");
static_assert(VER_OTA > 0 && VER_OTA <= UINT8_MAX, "VER_OTA must fit in the metadata");

static_assert(VER_APP > 0 && VER_APP <= UINT16_MAX, "VER_APP must fit in the metadata");

// Accumulate only the image prefix, even when the transport splits its headers.
static constexpr size_t OTA_PREFIX_SIZE = OTA_METADATA_OFFSET + sizeof(OtaMetadata);

// Use the CA bundle shipped with the ESP32 core, including after redirects.
extern const uint8_t caBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t caBundleEnd[] asm("_binary_x509_crt_bundle_end");

struct OtaState
{
  std::atomic<bool> busy{false};
  std::atomic<OtaPhase> phase{OTA_IDLE};
  std::atomic<size_t> received{0};
  std::atomic<size_t> imageSize{0};
  std::atomic<uint16_t> latestVersion{0};
  std::atomic<const char *> error{nullptr};
  std::atomic<bool> resultPending{false};
  std::atomic<bool> cancelRequested{false};

  uint8_t prefix[OTA_PREFIX_SIZE];
};

static OtaState ota;

// Tag of the latest release, as "v<CFW version>-<ATS-Mini version>",
// with "-beta<n>" for betas
static char otaLatestTag[24] = "";
static char *otaNotes = nullptr;
static bool otaReinstall = false;
static bool otaReview = false;
// What the update screen calls the operation going on
static const char *otaTitle = "Updating firmware";
static std::atomic<bool> otaUploading{false};
static std::atomic<bool> otaVerifying{false};
static std::atomic<bool> otaReviewReady{false};
uint8_t otaChannel = OTA_STABLE;

const char *otaReleaseTag()   { return(otaLatestTag); }
const char *otaReleaseNotes() { return(otaNotes? otaNotes : ""); }

struct OtaSpiRamAllocator : ArduinoJson::Allocator
{
  void *allocate(size_t size) override { return(heap_caps_malloc(size, MALLOC_CAP_SPIRAM)); }
  void deallocate(void *ptr) override { heap_caps_free(ptr); }
  void *reallocate(void *ptr, size_t size) override { return(heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM)); }
};
static OtaSpiRamAllocator otaAllocator;

static bool otaFail(const char *error)
{
  Update.abort();
  ota.error = error;
  ota.phase = OTA_FAILED;
  ota.resultPending = true;
  return false;
}

static void otaPrepare(size_t imageSize)
{
  otaVerifying = false;
  ota.error = nullptr;
  ota.resultPending = false;
  ota.received = 0;
  ota.imageSize = imageSize;
  ota.phase = OTA_WRITING;
}

bool otaBegin(size_t imageSize)
{
  if(ota.busy.exchange(true)) return false;
  otaTitle = "Updating firmware";
  otaUploading = true;
  ota.cancelRequested = false;
  otaPrepare(imageSize);
  return true;
}

static bool otaCheckMetadata()
{
  OtaMetadata metadata;
  memcpy(&metadata, ota.prefix + OTA_METADATA_OFFSET, sizeof(metadata));

  if(metadata.magic != OTA_METADATA_MAGIC || metadata.format != OTA_METADATA_FORMAT)
    return otaFail("Missing OTA metadata; use USB.");

  // A volatile read retains the embedded descriptor through linker GC/LTO.
  const volatile OtaMetadata &installed = otaMetadata;
  if(metadata.generation != installed.generation)
    return otaFail("Incompatible firmware; use USB.");
  if(metadata.variant != installed.variant)
    return otaFail("Wrong firmware variant; select the matching image.");
  return true;
}

bool otaWrite(uint8_t *data, size_t size)
{
  if(ota.phase.load() != OTA_WRITING) return false;
  if(ota.cancelRequested.load()) return otaFail("Update canceled.");
  size_t offset = ota.received.load();
  if(size > ota.imageSize.load() - offset)
    return otaFail("Firmware exceeds the declared size.");
  size_t chunkSize = size;
  if(offset < OTA_PREFIX_SIZE)
  {
    size_t count = OTA_PREFIX_SIZE - offset;
    if(count > size) count = size;
    memcpy(ota.prefix + offset, data, count);
    data += count;
    size -= count;
    if(offset + count < OTA_PREFIX_SIZE)
    {
      ota.received += chunkSize;
      return true;
    }
    if(!otaCheckMetadata()) return false;
    // Do not touch flash until compatibility has been checked.
    if(!Update.begin(ota.imageSize.load(), U_FLASH))
      return otaFail("Unable to start update.");
    if(Update.write(ota.prefix, OTA_PREFIX_SIZE) != OTA_PREFIX_SIZE)
      return otaFail(Update.errorString());
  }
  if(size && Update.write(data, size) != size)
    return otaFail(Update.errorString());
  ota.received += chunkSize;
  return true;
}

bool otaFinish()
{
  if(ota.cancelRequested.load()) return otaFail("Update canceled.");
  if(ota.phase.load() != OTA_WRITING || ota.received.load() < OTA_PREFIX_SIZE)
    return otaFail("Firmware is incomplete.");
  // Checking the whole image takes a moment
  otaVerifying = true;
  if(!Update.end()) return otaFail(Update.errorString());
  ota.phase = OTA_COMPLETE;
  return true;
}

void otaEndUpload()
{
  if(ota.phase.load() == OTA_COMPLETE || ota.phase.load() == OTA_REBOOT_PENDING)
    ota.phase = OTA_REBOOT_PENDING;
  else
  {
    if(ota.phase.load() == OTA_WRITING)
      otaFail(ota.cancelRequested.load()? "Update canceled." : "Upload interrupted.");
    ota.busy = false;
  }
}

OtaStatus otaStatus()
{
  OtaPhase phase = ota.phase.load();
  switch(phase)
  {
    case OTA_CHECK_QUEUED: return {phase, "Checking for updates..."};
    case OTA_AVAILABLE:
    {
      char text[48];
      snprintf(text, sizeof(text), "New release %s available.", otaLatestTag);
      return {phase, text};
    }
    case OTA_QUEUED:     return {phase, "Update requested..."};
    case OTA_CONNECTING: return {phase, "Connecting to GitHub..."};
    case OTA_COMPLETE:
    case OTA_REBOOT_PENDING: return {phase, "DONE! Rebooting..."};
    case OTA_CURRENT:    return {phase, "Already up to date."};
    case OTA_FAILED:
    {
      const char *error = ota.error.load();
      return {phase, String("Failed: ") + (error? error : "Update failed.")};
    }
    case OTA_WRITING:
    {
      size_t bytes = ota.received.load(), total = ota.imageSize.load();
      size_t percent = bytes * 100 / total;
      char text[48];
      snprintf(text, sizeof(text), "... %u bytes, %u%% ...",
               unsigned(bytes), unsigned(percent < 100? percent : 99));
      return {phase, text};
    }
    default: return {phase, ""};
  }
}

// The animated update screen follows the operation
static void otaDrawProgress()
{
  char line1[64], line2[64];

  switch(ota.phase.load())
  {
    case OTA_CHECK_QUEUED:
    case OTA_QUEUED:
    case OTA_CONNECTING:
      updateScreenShow(UPDATE_ICON_SEARCH, otaTitle, "Connecting to GitHub",
                       otaChannel==OTA_BETA? "Looking at releases and betas" : "Looking at releases",
                       -1, "Click to cancel");
      break;

    case OTA_WRITING:
    {
      if(otaVerifying)
      {
        updateScreenShow(UPDATE_ICON_VERIFY, otaTitle, "Checking the new firmware", "Almost done",
                         99, "Keep the receiver on");
        break;
      }
      size_t bytes = ota.received.load(), total = ota.imageSize.load();
      int percent = total? bytes * 100 / total : 0;
      if(otaUploading || !otaLatestTag[0]) strcpy(line1, "Receiving the firmware");
      else snprintf(line1, sizeof(line1), "Downloading %s", otaLatestTag);
      snprintf(line2, sizeof(line2), "%.2f of %.2f MB", bytes / 1048576.0, total / 1048576.0);
      updateScreenShow(UPDATE_ICON_DOWNLOAD, otaTitle, line1, line2, percent > 99? 99 : percent,
                       "Click to cancel, keep the receiver on");
      break;
    }

    case OTA_COMPLETE:
    case OTA_REBOOT_PENDING:
      updateScreenShow(UPDATE_ICON_OK, "Firmware installed", otaUploading? "" : otaLatestTag,
                       "Restarting...", 100, "");
      break;

    default:
      break;
  }
}

// The outcome of a check or of a failed update, on the receiver
static void otaShowResult()
{
  char line[64];
  const bool checking = otaTitle[0] == 'C';

  switch(ota.phase.load())
  {
    case OTA_CURRENT:
      snprintf(line, sizeof(line), "%s is the latest", otaLatestTag);
      updateScreenResult(UPDATE_ICON_OK, "Up to date", line,
                         otaChannel==OTA_BETA? "Releases and betas checked" : "Releases checked", 4000);
      break;
    case OTA_AVAILABLE:
      snprintf(line, sizeof(line), "%s is available", otaLatestTag);
      updateScreenResult(UPDATE_ICON_NEW, "New release", line, "Update FW > Update installs it", 6000);
      break;
    case OTA_FAILED:
    {
      const char *error = ota.error.load();
      updateScreenResult(UPDATE_ICON_FAIL, checking? "Check failed" : "Update failed",
                         error? error : "Update failed.", "", 6000);
      break;
    }
    default:
      updateScreenHide();
      break;
  }
}

bool otaRequestReinstall()
{
  otaReinstall = true;
  if(otaRequestLatest(true)) return(true);
  otaReinstall = false;
  return(false);
}

bool otaRequestReview()
{
  if(!otaRequestLatest(false)) return(false);
  otaReview = true;
  return(true);
}

// A release ranks above its betas
static uint32_t otaRank(unsigned version, unsigned beta)
{
  return(version * 100 + (beta? beta : 99));
}

// Tags look like v1.02-2.46 or, for betas, v1.02-2.46-beta1
static bool otaParseTag(const char *tag, unsigned *version, unsigned *beta)
{
  unsigned major, minor, baseMajor, baseMinor;
  int used = 0;
  if(strlen(tag) >= sizeof(otaLatestTag) ||
     sscanf(tag, "v%u.%u-%u.%u%n", &major, &minor, &baseMajor, &baseMinor, &used) != 4 || !used ||
     major > UINT16_MAX / 100 || minor > 99 || major * 100 + minor > UINT16_MAX)
    return(false);
  *version = major * 100 + minor;
  *beta = 0;

  const char *rest = tag + used;
  if(!*rest) return(true);
  int more = 0;
  return(sscanf(rest, "-beta%u%n", beta, &more) == 1 && more && !rest[more] && *beta >= 1 && *beta <= 98);
}

// Read a whole response into PSRAM. Secure clients return nothing between
// TLS records, so wait for data instead of taking an empty read for the end.
static char *otaReadBody(HTTPClient &http, size_t *length)
{
  NetworkClient *stream = http.getStreamPtr();
  int declared = http.getSize();
  if(declared > OTA_JSON_MAX) return(nullptr);
  size_t limit = declared > 0? size_t(declared) : OTA_JSON_MAX;
  char *buffer = static_cast<char *>(ps_malloc(limit + 1));
  if(!buffer) return(nullptr);

  size_t got = 0;
  uint32_t lastData = millis();
  while(got < limit)
  {
    int available = stream->available();
    if(available > 0)
    {
      int n = stream->read(reinterpret_cast<uint8_t *>(buffer) + got, min(size_t(available), limit - got));
      if(n > 0) { got += n; lastData = millis(); }
    }
    else if(!stream->connected()) break;
    else if(millis() - lastData > OTA_HTTP_TIMEOUT_SECONDS * 1000) break;
    else delay(2);
  }

  if(declared > 0 && got < size_t(declared)) { free(buffer); return(nullptr); }
  buffer[got] = '\0';
  *length = got;
  return(buffer);
}

// Find the newest release of the selected channel, with its notes
static bool otaFindRelease(NetworkClientSecure &client, HTTPClient &http, String &tag, unsigned *version, unsigned *beta)
{
  const bool betas = otaChannel == OTA_BETA;
  String url = String(FIRMWARE_API) + (betas? "/releases?per_page=10" : "/releases/latest");

  if(!http.begin(client, url)) return otaFail("Unable to connect to GitHub.");
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if(consumeAbortPending()) { http.end(); return otaFail("Update canceled."); }
  if(code != HTTP_CODE_OK)
  {
    http.end();
    return otaFail(code == 403 || code == 429? "GitHub is busy, try again later." : "Unable to find the latest release.");
  }

  size_t length = 0;
  std::unique_ptr<char, decltype(&free)> body(otaReadBody(http, &length), &free);
  http.end();
  if(!body) return otaFail("Unable to read the release list.");

  JsonDocument filter;
  if(betas)
  {
    filter[0]["tag_name"] = true;
    filter[0]["draft"] = true;
    filter[0]["body"] = true;
  }
  else
  {
    filter["tag_name"] = true;
    filter["body"] = true;
  }

  JsonDocument doc(&otaAllocator);
  if(deserializeJson(doc, body.get(), length, DeserializationOption::Filter(filter)))
    return otaFail("Unexpected release list.");

  uint32_t bestRank = 0;
  const char *bestNotes = "";
  auto consider = [&](JsonObjectConst release) {
    unsigned v, b;
    const char *name = release["tag_name"] | "";
    if((release["draft"] | false) || !otaParseTag(name, &v, &b)) return;
    if(otaRank(v, b) <= bestRank) return;
    bestRank = otaRank(v, b);
    tag = name;
    *version = v;
    *beta = b;
    bestNotes = release["body"] | "";
  };
  if(betas) for(JsonObjectConst release : doc.as<JsonArrayConst>()) consider(release);
  else consider(doc.as<JsonObjectConst>());

  if(!bestRank) return otaFail("Unexpected release version.");

  if(!otaNotes) otaNotes = static_cast<char *>(ps_malloc(OTA_NOTES_MAX + 1));
  if(otaNotes) strlcpy(otaNotes, bestNotes, OTA_NOTES_MAX + 1);
  return(true);
}

bool otaRequestLatest(bool install)
{
  if(ota.busy.exchange(true)) return false;
  ota.cancelRequested = false;
  ota.error = nullptr;
  ota.resultPending = false;
  otaUploading = false;
  otaTitle = otaReinstall? "Reinstalling firmware" : install? "Updating firmware" : "Checking for updates";
  ota.phase = install? OTA_QUEUED : OTA_CHECK_QUEUED;
  return true;
}

static bool otaDownloadLatest(bool install)
{
  ota.phase = OTA_CONNECTING;
  otaDrawProgress();
  if(WiFi.status() != WL_CONNECTED)
    return otaFail("Connect Wi-Fi first.");
  if(!clockGetDate(nullptr, nullptr, nullptr, nullptr))
    return otaFail("Set the clock before updating.");

  NetworkClientSecure client;
  client.setCACertBundle(caBundleStart, caBundleEnd - caBundleStart);
  client.setHandshakeTimeout(OTA_HTTP_TIMEOUT_SECONDS);
  HTTPClient http;
  http.setConnectTimeout(OTA_HTTP_TIMEOUT_SECONDS * 1000);
  http.setTimeout(OTA_HTTP_TIMEOUT_SECONDS * 1000);
  http.useHTTP10(true);
  http.setUserAgent(getUserAgent());

  // UnleashedATS tags carry the CFW version and the ATS-Mini version it is
  // based on, for example v1.00-2.46. Only the CFW version decides.
  String tag;
  unsigned version, beta;
  if(!otaFindRelease(client, http, tag, &version, &beta)) return false;
  ota.latestVersion = version;
  strcpy(otaLatestTag, tag.c_str());
  const bool newer = otaRank(version, beta) > otaRank(VER_CFW, VER_BETA) || otaReinstall;
  otaReinstall = false;
  if(!newer || !install)
  {
    ota.phase = newer? OTA_AVAILABLE : OTA_CURRENT;
    // On the receiver a newer release opens its notes instead of a message
    if(newer && otaReview) otaReviewReady = true;
    else ota.resultPending = true;
    return true;
  }
  int code;
  String url = String(FIRMWARE_URL) + "/releases/download/" + tag +
               "/" CFW_NAME "-" + tag + "-" + otaVariantNames[otaMetadata.variant] + "-ota.bin";

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if(!http.begin(client, url)) return otaFail("Unable to start firmware download.");
  code = http.GET();
  if(consumeAbortPending()) return otaFail("Update canceled.");
  if(code != HTTP_CODE_OK)
    return otaFail(code == 404? "Release has no matching OTA file." : "Firmware download failed.");
  int total = http.getSize();
  if(total < int(OTA_PREFIX_SIZE)) return otaFail("Invalid firmware download size.");
  otaPrepare(total);

  NetworkClient *stream = http.getStreamPtr();
  constexpr size_t bufferSize = 1024;
  std::unique_ptr<uint8_t, decltype(&free)> buffer(
    static_cast<uint8_t *>(ps_malloc(bufferSize)), &free);
  if(!buffer) return otaFail("Not enough PSRAM for update.");

  uint32_t lastData = millis(), lastDraw = 0;
  while(ota.received.load() < size_t(total))
  {
    if(consumeAbortPending()) return otaFail("Update canceled.");
    int available = stream->available();
    if(available > 0)
    {
      size_t count = size_t(available);
      if(count > bufferSize) count = bufferSize;
      size_t remaining = total - ota.received.load();
      if(count > remaining) count = remaining;
      int received = stream->read(buffer.get(), count);
      if(received > 0)
      {
        if(!otaWrite(buffer.get(), received)) return false;
        lastData = millis();
      }
    }
    else if(!http.connected())
      return otaFail("Firmware download interrupted.");
    if(millis() - lastData > OTA_HTTP_TIMEOUT_SECONDS * 1000)
      return otaFail("Firmware download timed out.");
    if(millis() - lastDraw >= 250)
    {
      otaDrawProgress();
      lastDraw = millis();
    }
    delay(1);
  }
  http.end();

  if(consumeAbortPending()) return otaFail("Update canceled.");
  if(!otaFinish()) return false;
  ota.phase = OTA_REBOOT_PENDING;
  return true;
}

void otaTick()
{
  // Display drawing and GitHub downloads run on the main task.
  // Browser uploads and page requests run on the async network task.
  OtaPhase phase = ota.phase.load();
  bool worked = false;
  if(phase == OTA_CHECK_QUEUED || phase == OTA_QUEUED || phase == OTA_WRITING ||
     phase == OTA_COMPLETE || phase == OTA_REBOOT_PENDING)
  {
    worked = true;
    currentCmd = CMD_NONE;
    otaDrawProgress();
    if(phase == OTA_CHECK_QUEUED || phase == OTA_QUEUED)
    {
      // Clear stale cancellation input before starting the operation.
      consumeAbortPending();
      otaDownloadLatest(phase == OTA_QUEUED);
      otaReview = false;
      if(ota.phase.load() != OTA_REBOOT_PENDING) ota.busy = false;
      if(otaReviewReady.exchange(false))
      {
        // A newer release: its notes, then Install or Cancel
        updateScreenHide();
        notesOpen();
        drawScreen();
      }
    }
    uint32_t lastDraw = 0;
    bool firstUploadTick = true;
    while((phase = ota.phase.load()) == OTA_WRITING || phase == OTA_COMPLETE || phase == OTA_REBOOT_PENDING)
    {
      // Upload callbacks own Update and the prefix buffer; only signal from here.
      if(phase == OTA_WRITING)
      {
        if(firstUploadTick)
        {
          // Encoder movement before the upload must not cancel it.
          consumeAbortPending();
          firstUploadTick = false;
        }
        else if(consumeAbortPending()) ota.cancelRequested = true;
      }
      if(millis() - lastDraw >= 250 || phase == OTA_REBOOT_PENDING)
      {
        otaDrawProgress();
        lastDraw = millis();
      }
      if(phase == OTA_REBOOT_PENDING)
      {
        delay(1000);
        ESP.restart();
      }
      delay(20);
    }
  }
  // Show each result once on the receiver, until a click or a few seconds.
  // Keep the result available to the web page after returning to the radio screen.
  if(ota.resultPending.exchange(false))
  {
    currentCmd = CMD_NONE;
    otaShowResult();
    drawScreen();
  }
  else if(worked)
  {
    // Whatever ended the operation, the radio screen comes back
    updateScreenHide();
    if(currentCmd != CMD_OTANOTES) drawScreen();
  }
}

//
// The firmware in the other slot, and the protection against updates that
// do not start. The bootloader keeps a new image on probation until it is
// confirmed; a reset before that brings back the previous one.
//

// Arduino would confirm every image as soon as it starts
extern "C" bool verifyRollbackLater() { return(true); }

void otaConfirmRunning()
{
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if(running && esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY)
    esp_ota_mark_app_valid_cancel_rollback();
}

bool otaPreviousFirmware(char *label, size_t size)
{
  const esp_partition_t *other = esp_ota_get_next_update_partition(nullptr);
  esp_app_desc_t desc;
  if(!other || esp_ota_get_partition_description(other, &desc) != ESP_OK) return(false);

  // Never offer an image that already failed to start
  esp_ota_img_states_t state;
  if(esp_ota_get_state_partition(other, &state) == ESP_OK &&
     (state == ESP_OTA_IMG_INVALID || state == ESP_OTA_IMG_ABORTED))
    return(false);

  OtaMetadata metadata;
  if(esp_partition_read(other, OTA_METADATA_OFFSET, &metadata, sizeof(metadata)) != ESP_OK) return(false);
  const bool known = metadata.magic == OTA_METADATA_MAGIC;
  if(known && metadata.variant != otaMetadata.variant) return(false);

  if(label && size)
  {
    unsigned cfw = metadata.reserved[0] | (metadata.reserved[1] << 8);
    if(known && cfw && metadata.reserved[2])
      snprintf(label, size, "v%u.%02u-%u.%02u-beta%u", cfw / 100, cfw % 100,
               metadata.version / 100, metadata.version % 100, metadata.reserved[2]);
    else if(known && cfw)
      snprintf(label, size, "v%u.%02u-%u.%02u", cfw / 100, cfw % 100, metadata.version / 100, metadata.version % 100);
    else if(known)
      snprintf(label, size, "%u.%02u of %s", metadata.version / 100, metadata.version % 100, desc.date);
    else
      snprintf(label, size, "%.12s of %s", desc.project_name, desc.date);
  }
  return(true);
}

const char *otaBootPrevious()
{
  if(!otaPreviousFirmware(nullptr, 0)) return("No previous firmware.");
  // This checks the whole image before selecting it
  if(esp_ota_set_boot_partition(esp_ota_get_next_update_partition(nullptr)) != ESP_OK)
    return("The previous firmware is damaged.");
  delay(300);
  esp_restart();
  return(nullptr);
}
