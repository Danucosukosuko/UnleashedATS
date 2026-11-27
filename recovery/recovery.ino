//
// UnleashedATS recovery app
//
// Lives in its own partition, which firmware updates never touch, and is
// started by the UnleashedATS bootloader when the encoder button is held for
// two seconds at power on. It works whatever state the firmware is in:
// erases the stored data part by part, starts either firmware slot, installs
// a firmware over Wi-Fi, and has a web page of its own for all of it.
//

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_app_format.h>
#include <esp_app_desc.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <soc/rtc_cntl_reg.h>
#include <time.h>

#include "Display.h"
#include "Rotary.h"

#define RECOVERY_VERSION  "1.02"
#define FIRMWARE_URL      "https://github.com/Danucosukosuko/UnleashedATS"
#define STORAGE_PARTITION "settings"
#define AP_NAME           "ATS-Mini Recovery"
#define HTTP_TIMEOUT      15000

// Pins, as in ats-mini/Common.h
#if defined(LILYGO_SI473X)
#define PIN_POWER_ON        46
#define AUDIO_MUTE          17
#define PIN_AMP_EN          -1
#define PIN_LCD_BL          15
#define ENCODER_PUSH_BUTTON  0
#define VARIANT_NAME        "lilygo-t-embed"
#define VARIANT_ID           3
#define HALF_STEP          true
static const int8_t heldHigh[] = { 10, 9, PIN_LCD_BL };
#else
#define PIN_POWER_ON        15
#define AUDIO_MUTE           3
#define PIN_AMP_EN          10
#define PIN_LCD_BL          38
#define ENCODER_PUSH_BUTTON 21
#define HALF_STEP         false
#if defined(CONFIG_SPIRAM_MODE_OCT)
#define VARIANT_NAME        "ospi"
#define VARIANT_ID           1
#else
#define VARIANT_NAME        "qspi"
#define VARIANT_ID           2
#endif
static const int8_t heldHigh[] = { 6, 5, 8, 9, PIN_LCD_BL, PIN_POWER_ON };
#endif
#define ENCODER_PIN_A        2
#define ENCODER_PIN_B        1

#define ITEM_COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Colors: the recovery app has no themes
#define C_BG      0x0000
#define C_TEXT    0xFFFF
#define C_MUTED   0xBDF7
#define C_DIM     0x7BEF
#define C_ACCENT  0xFD20  // Amber, so that it never looks like the firmware
#define C_OK      0x07E0
#define C_WARN    0xF800
#define C_HL      0x7A00
#define C_BOX     0x2104

static const lgfx::IFont *FONT_SMALL = &lgfx::fonts::Font2;

LGFX tft;
LGFX_Sprite spr(&tft);
Rotary encoder(ENCODER_PIN_B, ENCODER_PIN_A, HALF_STEP);
WebServer server(80);

// The image metadata UnleashedATS and ATS-Mini put at offset 288
struct OtaMetadata
{
  uint32_t magic;
  uint16_t version;
  uint8_t format;
  uint8_t generation;
  uint8_t variant;
  uint8_t reserved[3];
};
static constexpr uint32_t OTA_METADATA_MAGIC = 0x4D535441;
static constexpr size_t OTA_METADATA_OFFSET = 288;
static constexpr size_t OTA_PREFIX_SIZE = OTA_METADATA_OFFSET + sizeof(OtaMetadata);

extern const uint8_t caBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t caBundleEnd[] asm("_binary_x509_crt_bundle_end");

//
// Things that can be done, from the encoder or from the web page
//
enum Action
{
  ACT_NONE, ACT_START, ACT_SLOT1, ACT_SLOT2, ACT_BROWSER, ACT_LATEST, ACT_SETTINGS, ACT_MEMORIES,
  ACT_NETWORK, ACT_FILES, ACT_FACTORY, ACT_USB, ACT_COUNT
};

static const struct { const char *name; const char *web; const char *help; bool confirm; } actions[ACT_COUNT] =
{
  { "",                     "",          "",                                                 false },
  { "Start the firmware",   "start",     "Starts the firmware selected for booting",         false },
  { "Slot 1",               "slot1",     "Makes this firmware the one that boots",           true  },
  { "Slot 2",               "slot2",     "Makes this firmware the one that boots",           true  },
  { "Install from browser", "browser",   "Upload a firmware file over Wi-Fi",                false },
  { "Install latest",       "latest",    "Downloads the latest release over Wi-Fi",          true  },
  { "Reset settings",       "settings",  "Keeps memories, station lists, logs and Wi-Fi",    true  },
  { "Erase memories",       "memories",  "Clears the memory slots",                          true  },
  { "Forget Wi-Fi",         "network",   "Clears the networks and the web login",            true  },
  { "Erase files",          "files",     "Station lists, DX log, splash image and patches",  true  },
  { "Factory reset",        "factory",   "Erases everything, as on a new receiver",          true  },
  { "USB flashing",         "usb",       "Waits for esptool, no BOOT button needed",         false },
};

static volatile int encoderSteps = 0;
static volatile Action webAction = ACT_NONE;

// State shown on the screen and on the web page
static char message[96] = "";
static bool messageWarn = false;
static uint32_t messageTime = 0;
static int progress = -1;          // Percent while installing
static char progressTitle[40] = "";
static char progressLine[64] = "";
static bool busy = false;
static uint32_t restartAt = 0;

// Firmware slots
static const esp_partition_t *slots[2];
static char slotLabel[2][40];
static bool slotValid[2];
static int bootSlot = 0;

// Saved networks and web login, from the firmware settings
static String netSsid[3], netPass[3];
static int netCount = 0, netTry = 0;
static uint32_t netTime = 0;
static String loginUser, loginPass;

//
// Encoder
//
IRAM_ATTR void encoderIsr()
{
  unsigned char status = encoder.process();
  if(status == DIR_CW) encoderSteps++;
  else if(status == DIR_CCW) encoderSteps--;
}

static bool buttonDown() { return(digitalRead(ENCODER_PUSH_BUTTON) == LOW); }

static void setMessage(const char *text, bool warn = false)
{
  strlcpy(message, text? text : "", sizeof(message));
  messageWarn = warn;
  messageTime = millis();
}

//
// Firmware slots
//
static void describeSlot(int i)
{
  esp_app_desc_t desc;
  OtaMetadata meta;
  esp_ota_img_states_t state;
  const esp_partition_t *p = slots[i];

  slotValid[i] = false;
  if(!p || esp_ota_get_partition_description(p, &desc) != ESP_OK)
  {
    strcpy(slotLabel[i], "empty");
    return;
  }
  if(esp_ota_get_state_partition(p, &state) == ESP_OK &&
     (state == ESP_OTA_IMG_INVALID || state == ESP_OTA_IMG_ABORTED))
  {
    strcpy(slotLabel[i], "failed to start");
    return;
  }
  if(esp_partition_read(p, OTA_METADATA_OFFSET, &meta, sizeof(meta)) != ESP_OK) meta.magic = 0;

  unsigned cfw = meta.reserved[0] | (meta.reserved[1] << 8);
  if(meta.magic == OTA_METADATA_MAGIC && meta.variant != VARIANT_ID)
    snprintf(slotLabel[i], sizeof(slotLabel[i]), "other variant");
  else if(meta.magic == OTA_METADATA_MAGIC && cfw && meta.reserved[2])
    snprintf(slotLabel[i], sizeof(slotLabel[i]), "v%u.%02u-%u.%02u-beta%u", cfw / 100, cfw % 100,
             meta.version / 100, meta.version % 100, meta.reserved[2]);
  else if(meta.magic == OTA_METADATA_MAGIC && cfw)
    snprintf(slotLabel[i], sizeof(slotLabel[i]), "v%u.%02u-%u.%02u", cfw / 100, cfw % 100,
             meta.version / 100, meta.version % 100);
  else if(meta.magic == OTA_METADATA_MAGIC)
    snprintf(slotLabel[i], sizeof(slotLabel[i]), "ATS-Mini %u.%02u", meta.version / 100, meta.version % 100);
  else
    snprintf(slotLabel[i], sizeof(slotLabel[i]), "%.16s", desc.project_name);
  slotValid[i] = !(meta.magic == OTA_METADATA_MAGIC && meta.variant != VARIANT_ID);
}

static void readSlots()
{
  slots[0] = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  slots[1] = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  bootSlot = boot && boot == slots[1]? 1 : 0;
  describeSlot(0);
  describeSlot(1);
}

static void startSlot(int i)
{
  if(!slots[i] || !slotValid[i]) { setMessage("Nothing to start in that slot", true); return; }
  setMessage("Checking the firmware...");
  if(esp_ota_set_boot_partition(slots[i]) != ESP_OK) { setMessage("That firmware is damaged", true); readSlots(); return; }
  setMessage("Starting it");
  restartAt = millis() + 800;
}

//
// Stored data
//
static void clearNamespace(const char *name)
{
  Preferences prefs;
  prefs.begin(name, false, STORAGE_PARTITION);
  prefs.clear();
  prefs.end();
}

static void eraseFiles()
{
  const esp_partition_t *fs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "littlefs");
  // The firmware formats it again on its next start
  if(fs) esp_partition_erase_range(fs, 0, fs->size);
  clearNamespace("stations");
}

static void resetSettings()
{
  // The battery calibration belongs to the hardware, keep it
  Preferences prefs;
  prefs.begin("debug", true, STORAGE_PARTITION);
  int16_t batteryCal = prefs.getShort("BattCal", 0);
  prefs.end();
  clearNamespace("settings");
  clearNamespace("bands");
  clearNamespace("debug");
  prefs.begin("debug", false, STORAGE_PARTITION);
  prefs.putShort("BattCal", batteryCal);
  prefs.end();
}

static void factoryReset()
{
  nvs_flash_erase();
  nvs_flash_init();
  nvs_flash_erase_partition(STORAGE_PARTITION);
  nvs_flash_init_partition(STORAGE_PARTITION);
  eraseFiles();
}

static void loadNetworks()
{
  Preferences prefs;
  netCount = 0;
  if(!prefs.begin("network", true, STORAGE_PARTITION)) return;
  for(int i=0 ; i<3 ; i++)
  {
    char key[16];
    snprintf(key, sizeof(key), "wifissid%d", i + 1);
    String ssid = prefs.getString(key, "");
    snprintf(key, sizeof(key), "wifipass%d", i + 1);
    String pass = prefs.getString(key, "");
    if(ssid.length()) { netSsid[netCount] = ssid; netPass[netCount++] = pass; }
  }
  loginUser = prefs.getString("loginusername", "");
  loginPass = prefs.getString("loginpassword", "");
  prefs.end();
}

//
// Screens
//
static void drawProgressScreen()
{
  spr.fillSprite(C_BG);
  spr.fillRect(0, 0, 320, 20, C_ACCENT);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_BG);
  spr.drawString("RECOVERY", 6, 2, FONT_SMALL);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C_TEXT);
  spr.drawString(progressTitle, 160, 50, FONT_SMALL);
  spr.setTextColor(C_MUTED);
  spr.drawString(progressLine, 160, 72, FONT_SMALL);

  if(progress >= 0)
  {
    spr.drawRoundRect(30, 92, 260, 16, 4, C_MUTED);
    int fill = 256 * progress / 100;
    if(fill > 4) spr.fillRoundRect(32, 94, fill, 12, 3, C_OK);
    char text[8];
    snprintf(text, sizeof(text), "%d%%", progress);
    spr.setTextColor(C_TEXT);
    spr.drawString(text, 160, 122, FONT_SMALL);
  }

  spr.fillRect(0, 148, 320, 22, C_BOX);
  spr.setTextColor(C_MUTED);
  spr.drawString("Keep the receiver on", 160, 159, FONT_SMALL);
  spr.pushSprite(0, 0);
}

static void setProgress(const char *title, const char *line, int percent)
{
  static uint32_t lastDraw = 0;
  strlcpy(progressTitle, title, sizeof(progressTitle));
  strlcpy(progressLine, line, sizeof(progressLine));
  progress = percent;
  if(millis() - lastDraw > 150 || percent >= 100 || percent < 0)
  {
    drawProgressScreen();
    lastDraw = millis();
  }
}

static const char *itemLabel(int item, char *buffer, size_t size)
{
  if(item == ACT_SLOT1 || item == ACT_SLOT2)
  {
    int i = item - ACT_SLOT1;
    snprintf(buffer, size, "Slot %d: %s%s", i + 1, slotLabel[i], i == bootSlot? " (boots)" : "");
    return(buffer);
  }
  return(actions[item].name);
}

static void drawMenu(int selected, bool armed)
{
  char line[64];
  spr.fillSprite(C_BG);

  spr.fillRect(0, 0, 320, 20, C_ACCENT);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_BG);
  spr.drawString("RECOVERY", 6, 2, FONT_SMALL);
  spr.setTextDatum(TR_DATUM);
  spr.drawString("UnleashedATS " RECOVERY_VERSION " " VARIANT_NAME, 314, 2, FONT_SMALL);

  // Where the web page is
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_MUTED);
  if(WiFi.status() == WL_CONNECTED && (millis() / 3000) % 2)
    snprintf(line, sizeof(line), "Web: http://%s", WiFi.localIP().toString().c_str());
  else
    snprintf(line, sizeof(line), "Web: Wi-Fi " AP_NAME ", 10.1.1.1");
  spr.drawString(line, 6, 23, FONT_SMALL);

  const int rows = 6, count = ACT_COUNT - 1;
  int first = selected - 1 - rows / 2;
  first = first < 0? 0 : first > count - rows? count - rows : first;
  for(int r=0 ; r<rows ; r++)
  {
    int item = first + r + 1, y = 42 + r * 17;
    if(item == selected)
    {
      spr.fillRect(0, y - 1, 320, 17, C_HL);
      spr.setTextColor(C_TEXT);
    }
    else spr.setTextColor(C_TEXT);
    spr.drawString(itemLabel(item, line, sizeof(line)), 10, y, FONT_SMALL);
  }

  // What the selected item does, or what just happened
  bool showMessage = message[0] && millis() - messageTime < 4000;
  bool noFirmware = !slotValid[0] && !slotValid[1];
  const char *help = showMessage? message : armed? "Click again to confirm, rotate to cancel" :
                     noFirmware? "No firmware can start: install one" : actions[selected].help;
  spr.fillRect(0, 148, 320, 22, C_BOX);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor((showMessage && messageWarn) || (!showMessage && !armed && noFirmware)? C_WARN : armed? C_ACCENT : C_TEXT);
  spr.drawString(help, 160, 159, FONT_SMALL);

  spr.pushSprite(0, 0);
}

static void drawFlashingScreen()
{
  spr.fillSprite(C_BG);
  spr.fillRect(0, 0, 320, 20, C_ACCENT);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(C_BG);
  spr.drawString("RECOVERY", 6, 2, FONT_SMALL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C_TEXT);
  spr.drawString("USB flashing mode", 160, 50, FONT_SMALL);
  spr.setTextColor(C_MUTED);
  spr.drawString("Waiting for esptool or a web flasher over USB", 160, 74, FONT_SMALL);
  spr.drawString("ESP32-S3 download mode", 160, 94, FONT_SMALL);
  spr.fillRect(0, 124, 320, 46, C_BOX);
  spr.setTextColor(C_TEXT);
  spr.drawString("Switch off and on to leave without flashing", 160, 136, FONT_SMALL);
  spr.setTextColor(C_MUTED);
  spr.drawString("Still here after flashing? Power off fully", 160, 156, FONT_SMALL);
  spr.pushSprite(0, 0);
}

// Waiting for a firmware from the browser, with the way to get there
static void browserScreen()
{
  char line[64];
  uint32_t lastDraw = 0;
  bool pressed = buttonDown();

  for(;;)
  {
    server.handleClient();
    if(restartAt && (int32_t)(millis() - restartAt) >= 0) esp_restart();

    // A click leaves, unless a firmware is on its way
    bool down = buttonDown();
    if(pressed && !down && !busy) break;
    pressed = down;

    if(!busy && millis() - lastDraw > 500)
    {
      spr.fillSprite(C_BG);
      spr.fillRect(0, 0, 320, 20, C_ACCENT);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(C_BG);
      spr.drawString("RECOVERY", 6, 2, FONT_SMALL);
      spr.setTextDatum(MC_DATUM);
      spr.setTextColor(C_TEXT);
      spr.drawString("Install from the browser", 160, 34, FONT_SMALL);
      spr.setTextColor(C_MUTED);
      spr.drawString("1. Join the Wi-Fi network (no password)", 160, 58, FONT_SMALL);
      spr.setTextColor(C_ACCENT);
      spr.drawString(AP_NAME, 160, 76, FONT_SMALL);
      spr.setTextColor(C_MUTED);
      spr.drawString("2. Open http://10.1.1.1 and upload the", 160, 98, FONT_SMALL);
      spr.drawString("-ota.bin or ats-mini.ino.bin file", 160, 114, FONT_SMALL);
      if(WiFi.status() == WL_CONNECTED)
      {
        snprintf(line, sizeof(line), "Also on your network: http://%s", WiFi.localIP().toString().c_str());
        spr.setTextColor(C_DIM);
        spr.drawString(line, 160, 134, FONT_SMALL);
      }
      spr.fillRect(0, 148, 320, 22, C_BOX);
      bool showMessage = message[0] && millis() - messageTime < 6000;
      spr.setTextColor(showMessage && messageWarn? C_WARN : C_TEXT);
      spr.drawString(showMessage? message : "Click to go back", 160, 159, FONT_SMALL);
      spr.pushSprite(0, 0);
      lastDraw = millis();
    }
    delay(5);
  }
}

static void rebootToDownload()
{
  drawFlashingScreen();
  // Latch the display lines so that the panel keeps the screen above while
  // the ROM waits; the firmware releases them when it starts
  ledcDetach(PIN_LCD_BL);
  for(size_t i=0 ; i<ITEM_COUNT(heldHigh) ; i++)
  {
    pinMode(heldHigh[i], OUTPUT);
    digitalWrite(heldHigh[i], HIGH);
    gpio_hold_en((gpio_num_t)heldHigh[i]);
  }
  delay(20);
  REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
  esp_restart();
}

//
// Installing a firmware, from GitHub or from the browser. It always goes to
// the slot that does not boot, so the one there now stays as the way back.
//
static struct
{
  const esp_partition_t *target;
  esp_ota_handle_t handle;
  size_t received;
  size_t total;
  uint8_t prefix[OTA_PREFIX_SIZE];
  bool started;
  bool failed;
  char error[64];
} inst;

static void installFail(const char *error)
{
  if(inst.started) esp_ota_abort(inst.handle);
  inst.started = false;
  inst.failed = true;
  strlcpy(inst.error, error, sizeof(inst.error));
  progress = -1;
  busy = false;
  setMessage(error, true);
}

static void installBegin(size_t total)
{
  readSlots();
  memset(&inst, 0, sizeof(inst));
  inst.target = slots[bootSlot ^ 1];
  inst.total = total;
  busy = true;
  if(!inst.target) installFail("No firmware slot to install into");
  else if(total && total > inst.target->size) installFail("The firmware does not fit");
}

static bool installWrite(const uint8_t *data, size_t size)
{
  if(inst.failed) return(false);

  size_t offset = inst.received;
  inst.received += size;
  if(offset < OTA_PREFIX_SIZE)
  {
    // Look at the start of the image before erasing anything
    size_t count = min(size, OTA_PREFIX_SIZE - offset);
    memcpy(inst.prefix + offset, data, count);
    data += count;
    size -= count;
    if(offset + count < OTA_PREFIX_SIZE) return(true);

    const esp_image_header_t *header = reinterpret_cast<const esp_image_header_t *>(inst.prefix);
    OtaMetadata meta;
    memcpy(&meta, inst.prefix + OTA_METADATA_OFFSET, sizeof(meta));
    if(header->magic != ESP_IMAGE_HEADER_MAGIC || header->chip_id != ESP_CHIP_ID_ESP32S3)
    { installFail("Not an ESP32-S3 firmware"); return(false); }
    if(meta.magic == OTA_METADATA_MAGIC && meta.variant != VARIANT_ID)
    { installFail("Firmware for another receiver variant"); return(false); }

    setProgress("Installing firmware", "Erasing the slot...", 0);
    if(esp_ota_begin(inst.target, inst.total? inst.total : OTA_SIZE_UNKNOWN, &inst.handle) != ESP_OK)
    { installFail("Unable to erase the slot"); return(false); }
    inst.started = true;
    if(esp_ota_write(inst.handle, inst.prefix, OTA_PREFIX_SIZE) != ESP_OK)
    { installFail("Unable to write the firmware"); return(false); }
  }
  if(size && esp_ota_write(inst.handle, data, size) != ESP_OK)
  { installFail("Unable to write the firmware"); return(false); }

  char line[64];
  snprintf(line, sizeof(line), "%.2f MB written", inst.received / 1048576.0);
  setProgress("Installing firmware", line, inst.total? min(99, (int)(inst.received * 100 / inst.total)) : 50);
  return(true);
}

static bool installEnd()
{
  if(inst.failed) return(false);
  if(!inst.started) { installFail("The firmware is incomplete"); return(false); }
  setProgress("Installing firmware", "Checking it...", 99);
  inst.started = false;
  if(esp_ota_end(inst.handle) != ESP_OK) { installFail("The firmware is damaged"); return(false); }
  if(esp_ota_set_boot_partition(inst.target) != ESP_OK) { installFail("Unable to select the firmware"); return(false); }
  setProgress("Firmware installed", "Starting it...", 100);
  busy = false;
  restartAt = millis() + 1500;
  return(true);
}

static void installLatest()
{
  if(WiFi.status() != WL_CONNECTED)
  {
    setMessage(netCount? "Not connected yet, try again in a moment" : "No saved network: upload a file from the web page", true);
    return;
  }

  busy = true;
  setProgress("Install latest", "Setting the clock...", -1);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  for(int i=0 ; i<150 && time(nullptr) < 1700000000 ; i++) delay(100);
  if(time(nullptr) < 1700000000) { busy = false; setMessage("No time from the network", true); return; }

  NetworkClientSecure client;
  client.setCACertBundle(caBundleStart, caBundleEnd - caBundleStart);
  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT);
  http.setTimeout(HTTP_TIMEOUT);
  http.useHTTP10(true);
  http.setUserAgent("UnleashedATS-Recovery/" RECOVERY_VERSION);

  // The latest-release redirect names the tag
  setProgress("Install latest", "Asking GitHub...", -1);
  if(!http.begin(client, FIRMWARE_URL "/releases/latest")) { busy = false; setMessage("Unable to reach GitHub", true); return; }
  int code = http.sendRequest("HEAD");
  String location = http.getLocation();
  http.end();
  const String prefix = FIRMWARE_URL "/releases/tag/";
  if((code != 301 && code != 302) || !location.startsWith(prefix)) { busy = false; setMessage("No release found", true); return; }
  String tag = location.substring(prefix.length());

  String url = String(FIRMWARE_URL) + "/releases/download/" + tag + "/UnleashedATS-" + tag + "-" VARIANT_NAME "-ota.bin";
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if(!http.begin(client, url)) { busy = false; setMessage("Unable to start the download", true); return; }
  code = http.GET();
  int total = http.getSize();
  if(code != HTTP_CODE_OK || total < (int)OTA_PREFIX_SIZE)
  {
    http.end();
    busy = false;
    setMessage(code == 404? "The release has no file for this receiver" : "Download failed", true);
    return;
  }

  char line[64];
  snprintf(line, sizeof(line), "Downloading %s", tag.c_str());
  setProgress("Install latest", line, 0);
  installBegin(total);

  NetworkClient *stream = http.getStreamPtr();
  static uint8_t buffer[2048];
  uint32_t lastData = millis();
  while(!inst.failed && inst.received < (size_t)total)
  {
    // Secure clients give nothing between TLS records: wait, do not stop
    int available = stream->available();
    if(available > 0)
    {
      int n = stream->read(buffer, min((size_t)available, min(sizeof(buffer), (size_t)total - inst.received)));
      if(n > 0) { installWrite(buffer, n); lastData = millis(); }
    }
    else if(!http.connected()) { installFail("Download interrupted"); break; }
    else if(millis() - lastData > HTTP_TIMEOUT) { installFail("Download timed out"); break; }
    else delay(2);
  }
  http.end();
  if(!inst.failed) installEnd();
}

//
// Running an action
//
static void runAction(int action)
{
  switch(action)
  {
    case ACT_START:    startSlot(bootSlot); break;
    case ACT_SLOT1:    startSlot(0); break;
    case ACT_SLOT2:    startSlot(1); break;
    case ACT_BROWSER:  browserScreen(); break;
    case ACT_LATEST:   installLatest(); break;
    case ACT_SETTINGS: resetSettings(); setMessage("Settings reset"); break;
    case ACT_MEMORIES: clearNamespace("memories"); setMessage("Memories erased"); break;
    case ACT_NETWORK:  clearNamespace("network"); loadNetworks(); WiFi.disconnect(); setMessage("Wi-Fi networks forgotten"); break;
    case ACT_FILES:    eraseFiles(); setMessage("Files erased"); break;
    case ACT_FACTORY:  factoryReset(); loadNetworks(); WiFi.disconnect(); setMessage("Everything erased"); break;
    case ACT_USB:      rebootToDownload(); break;
  }
  progress = -1;
}

//
// Web page
//
static const char pageRecovery[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>UnleashedATS Recovery</title>
<style>
body{font-family:system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:1em;max-width:44em;margin:auto}
h1{color:#f90;font-size:1.4em} h2{font-size:1.1em;border-bottom:1px solid #444;padding-bottom:.3em;margin-top:1.6em}
button{background:#333;color:#eee;border:1px solid #666;border-radius:4px;padding:.45em 1em;margin:.2em;cursor:pointer}
button.warn{border-color:#c33} button:hover{background:#444} button:disabled{opacity:.4}
table{border-collapse:collapse;width:100%} td{padding:.4em;border-bottom:1px solid #333}
#msg{min-height:1.4em;color:#fc6} progress{width:100%;height:1.2em} .muted{color:#999;font-size:.9em}
</style></head><body>
<h1>UnleashedATS Recovery</h1>
<p class="muted" id="about"></p>
<p id="msg"></p>
<progress id="bar" max="100" value="0" hidden></progress>

<h2>Firmware</h2>
<table id="slots"></table>
<p><button onclick="act('start')">Start the firmware</button></p>

<h2>Install</h2>
<p>Upload a firmware file (the <code>-ota.bin</code> or <code>ats-mini.ino.bin</code> of your variant). It goes to the slot that does not boot, so the firmware there now stays as the way back.</p>
<p><input type="file" id="file" accept=".bin"> <button onclick="upload()">Upload and install</button></p>
<p><button onclick="act('latest','Download and install the latest release?')">Install the latest release</button>
<span class="muted">needs the receiver on a network with internet</span></p>

<h2>Erase</h2>
<p><button class="warn" onclick="act('settings','Reset the settings? Memories, lists and Wi-Fi stay.')">Reset settings</button>
<button class="warn" onclick="act('memories','Erase the memories?')">Erase memories</button>
<button class="warn" onclick="act('network','Forget the Wi-Fi networks and the web login?')">Forget Wi-Fi</button>
<button class="warn" onclick="act('files','Erase the station lists, DX log, splash image and patches?')">Erase files</button>
<button class="warn" onclick="act('factory','Erase EVERYTHING, as on a new receiver?')">Factory reset</button></p>

<h2>Receiver</h2>
<p><button onclick="act('usb','Restart into USB flashing mode?')">USB flashing mode</button></p>

<script>
const $=id=>document.getElementById(id);
function show(s){
  $('about').textContent='Recovery '+s.version+', '+s.variant+' variant. Hold the encoder for two seconds at power on to get here.';
  if(s.message)$('msg').textContent=s.message;
  $('bar').hidden=s.progress<0; if(s.progress>=0)$('bar').value=s.progress;
  const t=$('slots'); t.textContent='';
  s.slots.forEach((x,i)=>{const r=t.insertRow();
    r.insertCell().textContent='Slot '+(i+1);
    r.insertCell().textContent=x.label+(x.boots?'  (boots)':'');
    const c=r.insertCell();
    if(x.valid){const b=document.createElement('button');b.textContent='Start';b.onclick=()=>act('slot'+(i+1),'Start the firmware in slot '+(i+1)+'?');c.appendChild(b);}
  });
}
async function poll(){try{const r=await fetch('/status',{cache:'no-store'});show(await r.json());}catch(e){}}
async function act(what,question){
  if(question&&!confirm(question))return;
  const r=await fetch('/action',{method:'POST',body:new URLSearchParams({do:what})});
  $('msg').textContent=await r.text();
}
function upload(){
  const f=$('file').files[0]; if(!f){$('msg').textContent='Choose a file first.';return;}
  const x=new XMLHttpRequest(), d=new FormData(); d.append('firmware',f);
  $('bar').hidden=false;
  x.upload.onprogress=e=>{if(e.lengthComputable){$('bar').value=e.loaded*100/e.total;$('msg').textContent='Uploading... '+Math.round(e.loaded*100/e.total)+'%';}};
  x.onload=()=>{$('msg').textContent=x.responseText;};
  x.onerror=()=>{$('msg').textContent='Upload interrupted.';};
  x.open('POST','/upload'); x.send(d);
}
poll(); setInterval(poll,1000);
</script></body></html>)HTML";

static bool authorized()
{
  // The same login as the firmware's web interface, when one is set
  if(!loginUser.length() || !loginPass.length()) return(true);
  if(server.authenticate(loginUser.c_str(), loginPass.c_str())) return(true);
  server.requestAuthentication();
  return(false);
}

static void jsonString(String &out, const char *text)
{
  out += '"';
  for(const char *p = text ; *p ; p++)
  {
    if(*p == '"' || *p == '\\') out += '\\';
    if((uint8_t)*p >= ' ') out += *p;
  }
  out += '"';
}

static void webStatus()
{
  if(!authorized()) return;
  String out = "{\"version\":\"" RECOVERY_VERSION "\",\"variant\":\"" VARIANT_NAME "\",\"message\":";
  jsonString(out, message);
  out += ",\"progress\":" + String(progress) + ",\"busy\":" + (busy? "true" : "false") + ",\"slots\":[";
  for(int i=0 ; i<2 ; i++)
  {
    if(i) out += ',';
    out += "{\"label\":";
    jsonString(out, slotLabel[i]);
    out += String(",\"valid\":") + (slotValid[i]? "true" : "false") + ",\"boots\":" + (i == bootSlot? "true" : "false") + "}";
  }
  out += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}

static void webAct()
{
  if(!authorized()) return;
  String what = server.arg("do");
  for(int i=1 ; i<ACT_COUNT ; i++)
    if(i != ACT_BROWSER && what == actions[i].web)
    {
      if(busy) return server.send(409, "text/plain", "Busy, wait for the current operation.");
      webAction = (Action)i;
      return server.send(200, "text/plain", String(actions[i].name) + "...");
    }
  server.send(400, "text/plain", "Unknown action.");
}

static void webUploadDone()
{
  if(!authorized()) return;
  if(inst.failed) return server.send(400, "text/plain", String("Failed: ") + inst.error);
  server.send(200, "text/plain", "Installed. The receiver starts it now.");
}

static void webUpload()
{
  HTTPUpload &upload = server.upload();
  switch(upload.status)
  {
    case UPLOAD_FILE_START:
      if(loginUser.length() && loginPass.length() && !server.authenticate(loginUser.c_str(), loginPass.c_str()))
      {
        memset(&inst, 0, sizeof(inst));
        inst.failed = true;
        strcpy(inst.error, "Login required");
        return;
      }
      setProgress("Installing firmware", "Receiving from the browser...", 0);
      installBegin(0);
      break;
    case UPLOAD_FILE_WRITE:
      installWrite(upload.buf, upload.currentSize);
      break;
    case UPLOAD_FILE_END:
      installEnd();
      break;
    case UPLOAD_FILE_ABORTED:
      installFail("Upload interrupted");
      break;
  }
}

static void startNetwork()
{
  loadNetworks();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NAME);
  WiFi.softAPConfig(IPAddress(10, 1, 1, 1), IPAddress(10, 1, 1, 1), IPAddress(255, 255, 255, 0));
  if(netCount) WiFi.begin(netSsid[0].c_str(), netPass[0].c_str());
  netTime = millis();

  server.on("/", HTTP_GET, []() { if(authorized()) server.send(200, "text/html", pageRecovery); });
  server.on("/status", HTTP_GET, webStatus);
  server.on("/action", HTTP_POST, webAct);
  server.on("/upload", HTTP_POST, webUploadDone, webUpload);
  server.onNotFound([]() { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
  server.begin();
}

static void networkTick()
{
  // Go through the saved networks until one of them answers
  if(netCount > 1 && WiFi.status() != WL_CONNECTED && millis() - netTime > 15000)
  {
    netTry = (netTry + 1) % netCount;
    WiFi.begin(netSsid[netTry].c_str(), netPass[netTry].c_str());
    netTime = millis();
  }
}

//
// Startup and main loop
//
void setup()
{
  // After the USB flashing mode, take the display lines back, and the
  // button the firmware may have held down to start this app
  for(size_t i=0 ; i<ITEM_COUNT(heldHigh) ; i++) gpio_hold_dis((gpio_num_t)heldHigh[i]);
  gpio_hold_dis((gpio_num_t)ENCODER_PUSH_BUTTON);

  pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);

  // The radio stays silent
  if(PIN_AMP_EN >= 0) { pinMode(PIN_AMP_EN, OUTPUT); digitalWrite(PIN_AMP_EN, LOW); }
  pinMode(AUDIO_MUTE, OUTPUT);
  digitalWrite(AUDIO_MUTE, HIGH);
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  delay(100);

  ledcAttach(PIN_LCD_BL, 16000, 8);
  ledcWrite(PIN_LCD_BL, 0);
  tft.init();
  tft.setRotation(3);
  spr.setPsram(psramFound());
  spr.setColorDepth(16);
  spr.createSprite(320, 170);

  readSlots();
  setProgress("Recovery", "Release the button", -1);
  ledcWrite(PIN_LCD_BL, 160);
  while(buttonDown()) delay(20);

  startNetwork();

  // The bootloader also comes here when no firmware can start at all
  if(!slotValid[0] && !slotValid[1]) setMessage("No firmware can start: install one", true);
  else
  {
    esp_reset_reason_t why = esp_reset_reason();
    if(why == ESP_RST_PANIC || why == ESP_RST_INT_WDT || why == ESP_RST_TASK_WDT || why == ESP_RST_WDT)
      setMessage("The firmware kept failing to start", true);
  }

  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), encoderIsr, CHANGE);
}

void loop()
{
  static int selected = ACT_START;
  static bool armed = false;
  static bool pressed = false;
  static uint32_t lastDraw = 0;

  server.handleClient();
  networkTick();

  if(restartAt && (int32_t)(millis() - restartAt) >= 0) esp_restart();

  // Requests from the web page run here, one at a time
  if(webAction != ACT_NONE)
  {
    Action action = webAction;
    webAction = ACT_NONE;
    runAction(action);
    readSlots();
  }

  int steps = encoderSteps;
  if(steps)
  {
    encoderSteps -= steps;
    selected = ((selected - 1 + steps) % (ACT_COUNT - 1) + (ACT_COUNT - 1)) % (ACT_COUNT - 1) + 1;
    armed = false;
    message[0] = '\0';
    lastDraw = 0;
  }

  // A click, on release
  bool down = buttonDown();
  if(pressed && !down && !busy)
  {
    if(actions[selected].confirm && !armed) armed = true;
    else
    {
      armed = false;
      runAction(selected);
      readSlots();
    }
    lastDraw = 0;
  }
  pressed = down;

  if(!busy && millis() - lastDraw > 250)
  {
    drawMenu(selected, armed);
    lastDraw = millis();
  }
  delay(5);
}
