#include "Common.h"
#include "Draw.h"
#include "Utils.h"
#include "Stations.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <FS.h>
#include <stdarg.h>
#include <esp_heap_caps.h>

#define STATIONS_DIR        "/stations"
#define STATIONS_TEMP       "/stations/download.tmp"
#define STATIONS_MAGIC      "UST1"
#define STATIONS_MAX_LOCAL  32   // Countries kept on the receiver
#define STATIONS_MAX_INDEX 300   // Countries in the downloaded index
#define STATIONS_MAX_REGIONS 10  // Regions in the downloaded index
#define STATIONS_MAX_MATCH  12   // Stations listed on one frequency
#define STATIONS_TIMEOUT 15000   // Connection timeout (msecs)
#define STATIONS_STALL    8000   // A download this long without data is resumed (msecs)
#define STATIONS_RETRIES     6   // Times a broken download is resumed
#define STATIONS_CHUNK    4096   // Bytes read from the network at a time
#define STATIONS_CYCLE       2   // Identification calls per listed name

typedef struct __attribute__((packed))
{
  char magic[4];
  char code[3];
  char name[24];
  uint16_t count;
} StationFileHeader;

// Use the CA bundle shipped with the ESP32 core, as the OTA update does
extern const uint8_t caBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t caBundleEnd[] asm("_binary_x509_crt_bundle_end");

// Keep the JSON documents in PSRAM, internal RAM is scarce
struct SpiRamAllocator : ArduinoJson::Allocator
{
  void *allocate(size_t size) override { return(heap_caps_malloc(size, MALLOC_CAP_SPIRAM)); }
  void deallocate(void *ptr) override { heap_caps_free(ptr); }
  void *reallocate(void *ptr, size_t size) override { return(heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM)); }
};
static SpiRamAllocator spiRamAllocator;

// Downloaded index
static char regionNames[STATIONS_MAX_REGIONS][20];
static uint16_t regionFirst[STATIONS_MAX_REGIONS];
static uint16_t regionTotal[STATIONS_MAX_REGIONS];
static int regionCount = 0;
static StationCountry *indexList = nullptr;
static int indexCount = 0;

// Countries stored on the receiver
static StationCountry localList[STATIONS_MAX_LOCAL];
static int localCount = -1;

// Country loaded for browsing
static StationRecord *openList = nullptr;
static int openCount = 0;

void stationsField(char *dst, const char *src, int size)
{
  int i;
  for(i=0 ; i<size && src[i] ; i++) dst[i] = src[i];
  dst[i] = '\0';
  while(i>0 && dst[i-1]==' ') dst[--i] = '\0';
}

static void copyAscii(char *dst, const char *src, int size, bool terminate)
{
  int i = 0;
  for( ; src && *src && i<size ; src++)
    dst[i++] = (*src>=' ' && *src<='~')? *src : ' ';
  if(terminate && i>=size) i = size - 1;
  while(i<size) dst[i++] = '\0';
}

static String filePath(const char *code)
{
  char code2[3] = { code[0], code[1], '\0' };
  return(String(STATIONS_DIR "/") + code2 + ".bin");
}

//
// Network access
//

//
// Diagnostics on the USB serial port, when it is in ad hoc mode
//
static void stationsLog(const char *format, ...)
{
  if(usbModeIdx == USB_OFF) return;

  char line[200];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);

  Serial.printf("#STN %lu rssi=%d heap=%u/%u %s\r\n", millis(), WiFi.RSSI(),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), line);
}

enum ConnectResult { CONNECT_OK, CONNECT_RETRY, CONNECT_FATAL };

// Start a request, from the given offset when resuming
static ConnectResult stationsConnect(HTTPClient &http, NetworkClientSecure &client, const String &url,
                                     size_t offset, int *code)
{
  if(WiFi.status() != WL_CONNECTED)
  {
    statusShow("Station lists", "Connect Wi-Fi first.");
    return(CONNECT_FATAL);
  }

  // Certificates cannot be checked without the date
  if(!clockGetDate(nullptr, nullptr, nullptr, nullptr))
  {
    statusShow("Station lists", "Set the clock first.");
    return(CONNECT_FATAL);
  }

  client.setCACertBundle(caBundleStart, caBundleEnd - caBundleStart);
  client.setHandshakeTimeout(STATIONS_TIMEOUT / 1000);
  http.setConnectTimeout(STATIONS_TIMEOUT);
  http.setTimeout(STATIONS_STALL);
  http.setUserAgent(getUserAgent());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);

  if(!http.begin(client, url)) return(CONNECT_RETRY);
  if(offset)
  {
    char range[32];
    snprintf(range, sizeof(range), "bytes=%u-", (unsigned)offset);
    http.addHeader("Range", range);
  }

  uint32_t started = millis();
  *code = http.GET();
  stationsLog("GET offset=%u code=%d size=%d in %lu ms", (unsigned)offset, *code, http.getSize(),
              (unsigned long)(millis() - started));
  if(*code == HTTP_CODE_OK || (offset && *code == HTTP_CODE_PARTIAL_CONTENT))
  {
    http.getStreamPtr()->setTimeout(STATIONS_STALL);
    return(CONNECT_OK);
  }

  http.end();
  // A missing file will not appear by trying again
  return(*code == HTTP_CODE_NOT_FOUND? CONNECT_FATAL : CONNECT_RETRY);
}

//
// Download a whole file into PSRAM, resuming where it stopped when the
// connection stalls or breaks. Returns a terminated buffer to free().
//
static char *stationsFetch(const String &url, const char *label, size_t *size)
{
  static const char *title = "Station lists";
  NetworkClientSecure client;
  HTTPClient http;
  char status[64];
  char *buf = nullptr;
  size_t total = 0, got = 0;
  uint32_t lastDraw = 0;

  for(int attempt=0 ; attempt<=STATIONS_RETRIES ; attempt++)
  {
    if(attempt)
    {
      snprintf(status, sizeof(status), "%s: resuming (%d)...", label, attempt);
      statusShow(title, status, 0);
      drawScreen();
      delay(1000);
    }

    int code = 0;
    ConnectResult result = stationsConnect(http, client, url, got, &code);
    if(result == CONNECT_FATAL)
    {
      if(code == HTTP_CODE_NOT_FOUND) statusShow(title, "List not found.");
      free(buf);
      return(nullptr);
    }
    if(result == CONNECT_RETRY) continue;

    // A server that ignores the range sends everything again
    if(code == HTTP_CODE_OK) got = 0;

    if(!buf)
    {
      int length = http.getSize();
      if(length <= 0)
      {
        http.end();
        statusShow(title, "Unknown list size.");
        return(nullptr);
      }
      total = length;
      buf = static_cast<char *>(ps_malloc(total + 1));
      if(!buf)
      {
        http.end();
        statusShow(title, "Not enough memory.");
        return(nullptr);
      }
    }

    NetworkClient *stream = http.getStreamPtr();
    uint32_t chunkTime = millis();
    size_t logged = got;
    while(got < total)
    {
      if(consumeAbortPending())
      {
        http.end();
        free(buf);
        statusShow(title, "CANCELED!");
        return(nullptr);
      }

      // The TLS client does not wait for data: an empty read only means
      // that the next record has not arrived yet
      int ready = stream->available();
      if(ready <= 0)
      {
        if(millis() - chunkTime > STATIONS_STALL || (!client.connected() && !stream->available()))
        {
          stationsLog("stall at %u of %u after %lu ms, connected=%d", (unsigned)got,
                      (unsigned)total, (unsigned long)(millis() - chunkTime), client.connected());
          break;
        }
        delay(5);
        continue;
      }

      int count = stream->read((uint8_t *)buf + got, min((size_t)ready, min((size_t)STATIONS_CHUNK, total - got)));
      if(count <= 0)
      {
        delay(5);
        continue;
      }
      got += count;
      chunkTime = millis();
      if(got - logged >= 16384)
      {
        stationsLog("got %u of %u", (unsigned)got, (unsigned)total);
        logged = got;
      }

      if(millis() - lastDraw > 500)
      {
        snprintf(status, sizeof(status), "%s: %u of %u KB", label, (unsigned)(got / 1024), (unsigned)(total / 1024));
        statusShow(title, status, 0);
        drawScreen();
        lastDraw = millis();
      }
    }
    http.end();

    if(got >= total)
    {
      stationsLog("complete, %u bytes", (unsigned)total);
      buf[total] = '\0';
      *size = total;
      return(buf);
    }
  }

  free(buf);
  statusShow(title, "Download interrupted.");
  return(nullptr);
}

// Lets ArduinoJson and the Stream helpers walk a buffer in memory
class MemoryStream : public Stream
{
  const char *data;
  size_t size, pos = 0;

  public:
    MemoryStream(const char *data, size_t size) : data(data), size(size) { setTimeout(0); }
    int available() override { return(size - pos); }
    int read() override { return(pos < size? (uint8_t)data[pos++] : -1); }
    int peek() override { return(pos < size? (uint8_t)data[pos] : -1); }
    size_t write(uint8_t) override { return(0); }
};

bool stationsLoadIndex()
{
  consumeAbortPending();
  statusShow("Station lists", "Loading countries...", 0);
  drawScreen();

  size_t size;
  char *json = stationsFetch(STATIONS_URL "/index.json", "Countries", &size);
  if(!json) return(false);

  JsonDocument doc(&spiRamAllocator);
  DeserializationError error = deserializeJson(doc, json, size);
  free(json);

  if(error)
  {
    statusShow("Station lists", "Damaged list of countries.");
    return(false);
  }

  if(!indexList)
    indexList = static_cast<StationCountry *>(ps_calloc(STATIONS_MAX_INDEX, sizeof(StationCountry)));
  if(!indexList)
  {
    statusShow("Station lists", "Not enough memory.");
    return(false);
  }

  regionCount = indexCount = 0;
  JsonArray regions = doc["regions"];
  for(JsonObject region : regions)
  {
    if(regionCount>=STATIONS_MAX_REGIONS) break;

    copyAscii(regionNames[regionCount], region["name"] | "", sizeof(regionNames[0]), true);
    regionFirst[regionCount] = indexCount;
    regionTotal[regionCount] = 0;

    JsonArray countries = region["countries"];
    for(JsonObject country : countries)
    {
      const char *code = country["code"] | "";
      if(indexCount>=STATIONS_MAX_INDEX || strlen(code)!=2) continue;

      StationCountry *entry = &indexList[indexCount++];
      copyAscii(entry->code, code, sizeof(entry->code), true);
      copyAscii(entry->name, country["name"] | code, sizeof(entry->name), true);
      entry->count  = country["count"] | 0;
      entry->region = regionCount;
      regionTotal[regionCount]++;
    }

    regionCount++;
  }

  statusShow(nullptr);
  return(regionCount>0);
}

int stationsRegionCount() { return(regionCount); }

const char *stationsRegionName(int region)
{
  return(region>=0 && region<regionCount? regionNames[region] : "");
}

int stationsIndexCount(int region)
{
  return(region>=0 && region<regionCount? regionTotal[region] : 0);
}

const StationCountry *stationsIndexCountry(int region, int idx)
{
  if(region<0 || region>=regionCount || idx<0 || idx>=regionTotal[region]) return(nullptr);
  return(&indexList[regionFirst[region] + idx]);
}

//
// Stored countries
//

static void scanLocal()
{
  localCount = 0;

  fs::File dir = LittleFS.open(STATIONS_DIR);
  if(!dir || !dir.isDirectory()) return;

  for(fs::File file = dir.openNextFile() ; file && localCount<STATIONS_MAX_LOCAL ; file = dir.openNextFile())
  {
    StationFileHeader header;
    String name = file.name();

    if(!name.endsWith(".bin")) continue;
    if(file.read((uint8_t *)&header, sizeof(header))!=sizeof(header)) continue;
    if(memcmp(header.magic, STATIONS_MAGIC, 4)) continue;

    StationCountry *entry = &localList[localCount++];
    memcpy(entry->code, header.code, 2);
    entry->code[2] = '\0';
    stationsField(entry->name, header.name, sizeof(header.name));
    entry->count = header.count;
    entry->region = 0;
  }

  // Sort by name
  for(int i=1 ; i<localCount ; i++)
    for(int j=i ; j>0 && strcmp(localList[j-1].name, localList[j].name)>0 ; j--)
    {
      StationCountry t = localList[j];
      localList[j] = localList[j-1];
      localList[j-1] = t;
    }
}

int stationsLocalCount()
{
  if(localCount<0) scanLocal();
  return(localCount);
}

bool stationsLocalCountry(int idx, StationCountry *country)
{
  if(idx<0 || idx>=stationsLocalCount()) return(false);
  *country = localList[idx];
  return(true);
}

bool stationsIsLocal(const char *code)
{
  for(int i=0 ; i<stationsLocalCount() ; i++)
    if(!strncmp(localList[i].code, code, 2)) return(true);
  return(false);
}

bool stationsLocalDelete(int idx)
{
  if(idx<0 || idx>=stationsLocalCount()) return(false);

  bool result = LittleFS.remove(filePath(localList[idx].code));
  localCount = -1;
  openCount = 0;
  return(result);
}

bool stationsDownload(const StationCountry *country)
{
  static const char *title = "Station lists";
  char status[64];

  if(!country) return(false);

  consumeAbortPending();
  snprintf(status, sizeof(status), "Downloading %s...", country->name);
  statusShow(title, status, 0);
  drawScreen();

  // Make sure the list fits, counting the space of the list it replaces
  String path = filePath(country->code);
  size_t needed = sizeof(StationFileHeader) + (size_t)country->count * sizeof(StationRecord);
  size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
  for(int i=0 ; i<stationsLocalCount() ; i++)
  {
    fs::File old = LittleFS.open(filePath(localList[i].code), "r");
    if(old) freeBytes += old.size();
  }
  if(needed + needed / 8 + 8192 > freeBytes)
  {
    statusShow(title, "Not enough storage space.");
    return(false);
  }

  char code2[3] = { country->code[0], country->code[1], '\0' };
  size_t size;
  char *json = stationsFetch(String(STATIONS_URL "/") + code2 + ".json", country->name, &size);
  if(!json) return(false);

  MemoryStream memory(json, size);
  Stream *stream = &memory;
  if(!stream->find("\"stations\"") || !stream->find("["))
  {
    free(json);
    statusShow(title, "Damaged station list.");
    return(false);
  }

  LittleFS.mkdir(STATIONS_DIR);
  fs::File file = LittleFS.open(STATIONS_TEMP, "w");
  if(!file)
  {
    free(json);
    statusShow(title, "Failed opening local storage!");
    return(false);
  }

  StationFileHeader header = {};
  memcpy(header.magic, STATIONS_MAGIC, 4);
  memcpy(header.code, country->code, 2);
  copyAscii(header.name, country->name, sizeof(header.name), false);
  file.write((uint8_t *)&header, sizeof(header));

  JsonDocument filter;
  filter["f"] = true;
  filter["p"] = true;
  filter["n"] = true;
  filter["c"] = true;

  JsonDocument doc(&spiRamAllocator);
  uint16_t count = 0;
  bool ok = true;

  do
  {
    if(consumeAbortPending())
    {
      ok = false;
      statusShow(title, "CANCELED!");
      break;
    }

    DeserializationError error = deserializeJson(doc, *stream, DeserializationOption::Filter(filter));
    if(error)
    {
      ok = false;
      snprintf(status, sizeof(status), "Damaged list at station %u: %s", count + 1, error.c_str());
      statusShow(title, status);
      break;
    }

    float mhz = doc["f"] | 0.0f;
    uint32_t freq = lroundf(mhz * 100.0f);
    if(freq<6400 || freq>10800 || count==UINT16_MAX) continue;

    StationRecord record;
    record.freq = freq;
    copyAscii(record.ps,   doc["p"] | "", STATION_PS_LEN, false);
    copyAscii(record.name, doc["n"] | "", STATION_NAME_LEN, false);
    copyAscii(record.city, doc["c"] | "", STATION_CITY_LEN, false);
    if(file.write((uint8_t *)&record, sizeof(record))!=sizeof(record))
    {
      ok = false;
      statusShow(title, "Storage is full.");
      break;
    }

    if(!(++count & 63))
    {
      snprintf(status, sizeof(status), "%s: %u stations stored", country->name, count);
      statusShow(title, status, 0);
      drawScreen();
    }
  }
  while(stream->findUntil(",", "]"));

  free(json);

  if(ok)
  {
    // The header goes in last, with the final count
    header.count = count;
    file.seek(0);
    file.write((uint8_t *)&header, sizeof(header));
  }
  file.close();

  if(!ok || !count)
  {
    LittleFS.remove(STATIONS_TEMP);
    if(ok) statusShow(title, "The list is empty.");
    return(false);
  }

  // Only one country is kept, the new one replaces whatever was there
  for(int i=0 ; i<stationsLocalCount() ; i++)
    LittleFS.remove(filePath(localList[i].code));

  // Move the new list to its permanent place
  LittleFS.remove(path);
  LittleFS.rename(STATIONS_TEMP, path);
  localCount = -1;
  openCount = 0;

  snprintf(status, sizeof(status), "%s: %u stations stored.", country->name, count);
  statusShow(title, status);
  identifyFrequency(currentFrequency);
  return(true);
}

//
// Browsing one country
//

bool stationsOpen(int idx)
{
  if(idx<0 || idx>=stationsLocalCount()) return(false);

  fs::File file = LittleFS.open(filePath(localList[idx].code), "r");
  if(!file) return(false);

  StationFileHeader header;
  if(file.read((uint8_t *)&header, sizeof(header))!=sizeof(header) || memcmp(header.magic, STATIONS_MAGIC, 4))
    return(false);

  free(openList);
  openCount = 0;
  openList = static_cast<StationRecord *>(ps_malloc((size_t)header.count * sizeof(StationRecord) + 1));
  if(!openList) return(false);

  int count = file.read((uint8_t *)openList, (size_t)header.count * sizeof(StationRecord)) / sizeof(StationRecord);
  openCount = count;
  return(count>0);
}

int stationsOpenCount() { return(openCount); }

bool stationsOpenRecord(int idx, StationRecord *record)
{
  if(idx<0 || idx>=openCount) return(false);
  *record = openList[idx];
  return(true);
}

int stationsOpenFind(uint32_t freqHz)
{
  uint16_t freq = freqHz / 10000;
  int i;
  for(i=0 ; i<openCount && openList[i].freq<freq ; i++);
  return(i<openCount? i : 0);
}

//
// Identification by frequency, the way EiBi does it for shortwave
//

static int findInFile(fs::File &file, uint16_t count, uint16_t freq, StationRecord *matches, int found)
{
  int lo = 0, hi = count - 1, first = -1;
  StationRecord record;

  // Binary search for the first record on the frequency
  while(lo<=hi)
  {
    int mid = (lo + hi) / 2;
    file.seek(sizeof(StationFileHeader) + (size_t)mid * sizeof(record));
    if(file.read((uint8_t *)&record, sizeof(record))!=sizeof(record)) return(found);

    if(record.freq<freq) lo = mid + 1;
    else
    {
      if(record.freq==freq) first = mid;
      hi = mid - 1;
    }
  }

  if(first<0) return(found);

  file.seek(sizeof(StationFileHeader) + (size_t)first * sizeof(record));
  while(found<STATIONS_MAX_MATCH && file.read((uint8_t *)&record, sizeof(record))==sizeof(record) && record.freq==freq)
    matches[found++] = record;

  return(found);
}

const char *stationsIdentify(uint32_t freqHz, bool periodic)
{
  static StationRecord matches[STATIONS_MAX_MATCH];
  static uint32_t lastFreq = 0;
  static int found = 0, shown = 0, calls = 0;
  static char text[64];

  if(!stationsLocalCount()) return(nullptr);

  if(!periodic || freqHz!=lastFreq)
  {
    lastFreq = freqHz;
    found = shown = calls = 0;

    for(int i=0 ; i<localCount && found<STATIONS_MAX_MATCH ; i++)
    {
      fs::File file = LittleFS.open(filePath(localList[i].code), "r");
      StationFileHeader header;
      if(file && file.read((uint8_t *)&header, sizeof(header))==sizeof(header))
        found = findInFile(file, header.count, freqHz / 10000, matches, found);
    }
  }
  else if(found>1 && ++calls>=STATIONS_CYCLE)
  {
    // Several stations share the frequency, show them in turn
    calls = 0;
    shown = (shown + 1) % found;
  }

  if(!found) return(nullptr);

  char name[STATION_NAME_LEN + 1], city[STATION_CITY_LEN + 1];
  stationsField(name, matches[shown].name, STATION_NAME_LEN);
  stationsField(city, matches[shown].city, STATION_CITY_LEN);

  if(*city)
    snprintf(text, sizeof(text), "%s, %s", name, city);
  else
    snprintf(text, sizeof(text), "%s", name);

  return(text);
}
