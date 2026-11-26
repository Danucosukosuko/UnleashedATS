#ifndef STATIONS_H
#define STATIONS_H

#include <stdint.h>

// FM station lists, downloaded one country at a time from the
// UnleashedATS repository (see stations/README.md there)

#define STATION_PS_LEN     8
#define STATION_NAME_LEN  20
#define STATION_CITY_LEN  16
#define STATION_REGION_LEN 24
#define STATION_NO_REGION 0xFF

typedef struct __attribute__((packed))
{
  uint16_t freq;                    // Frequency in 10kHz units
  uint8_t region;                   // Province or state, STATION_NO_REGION if unknown
  char ps[STATION_PS_LEN];          // Expected RDS name, not terminated
  char name[STATION_NAME_LEN];      // Station name, not terminated
  char city[STATION_CITY_LEN];      // Municipality, not terminated
} StationRecord;

typedef struct
{
  char code[3];                     // ISO 3166 country code
  char name[24];                    // Country name, in English
  uint16_t count;                   // Number of stations
  uint8_t region;                   // Region index in the downloaded index
} StationCountry;

// Index of countries, downloaded on request
bool stationsLoadIndex();
int stationsRegionCount();
const char *stationsRegionName(int region);
int stationsIndexCount(int region);
const StationCountry *stationsIndexCountry(int region, int idx);
bool stationsDownload(const StationCountry *country);

// Countries stored on the receiver
int stationsLocalCount();
bool stationsLocalCountry(int idx, StationCountry *country);
bool stationsIsLocal(const char *code);
bool stationsLocalDelete(int idx);

// Stations of one stored country, loaded into PSRAM
bool stationsOpen(int idx);
// Provinces or states of the open country, sorted by name, and the
// station view they select (-1 selects every station)
int stationsRegionsOpen();
const char *stationsRegionOpenName(int idx);
void stationsSelectRegion(int idx);
int stationsOpenCount();
bool stationsOpenRecord(int idx, StationRecord *record);
int stationsOpenFind(uint32_t freqHz);

// The province the listener is in: the last one picked in the Stations
// menu. Identification only names the stations of that province.
const char *stationsHomeRegion();
void stationsSetHomeRegion(const char *name);

// Names of the stations listed on a frequency, cycling through them on
// periodic calls. Returns NULL when there is nothing listed.
const char *stationsIdentify(uint32_t freqHz, bool periodic);

// Copy a fixed size field into a terminated string
void stationsField(char *dst, const char *src, int size);

#endif // STATIONS_H
