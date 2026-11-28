#include "Common.h"
#include "Utils.h"
#include "Menu.h"
#include "Storage.h"
#include "Stations.h"
#include "DxWatch.h"

#include <LittleFS.h>
#include <time.h>

#define DXW_FIRST_HZ    87500000  // Broadcast FM band
#define DXW_LAST_HZ    108000000
#define DXW_STEP_HZ       100000
#define DXW_CHANNELS   ((DXW_LAST_HZ - DXW_FIRST_HZ) / DXW_STEP_HZ + 1)
#define DXW_LEARN_SWEEPS       3  // Sweeps spent learning the local stations
#define DXW_TAKEN_SNR          2  // Any signal this clean while learning marks a channel as local (dB)
#define DXW_MIN_RSSI          10  // Weakest signal worth decoding (dBuV)
#define DXW_MIN_SNR            4  // Noisiest signal worth decoding while learning (dB)
#define DXW_CATCH_SNR          5  // Noisiest signal worth reporting (dB)
#define DXW_LEARN_PI_TIME  1500U  // Time given to the decoder to report a PI while learning (msecs)
#define DXW_PI_TIME        1200U  // Time given to the decoder to confirm a new PI (msecs)
#define DXW_PI_REPEATS         2  // Matching groups needed before a new PI is believed
#define DXW_PS_TIME        3000U  // Time given to the decoder to spell out a name (msecs)
#define DXW_RESUME_TIME   60000U  // A catch nobody attends is left after this (msecs)
#define DXW_MAX_KNOWN        160  // Program identifications remembered as local
#define DXW_MAX_CAUGHT        64  // Catches remembered, so each is reported once
#define DXW_DIM_LEVEL         10  // Backlight while watching

static DxWatchPhase phase = DXW_OFF;
static uint16_t channel = 0;
static uint16_t sweeps = 0;
static uint16_t catches = 0;
static uint32_t catchTime = 0;
static uint32_t savedFreq = 0;
static int savedBand = -1;
static DxCatch lastCatch;

static uint16_t *known = nullptr;      // Local program identifications
static uint8_t *taken = nullptr;       // Channels with a local station on them
static uint16_t knownCount = 0;
static uint16_t caught[DXW_MAX_CAUGHT];
static uint16_t caughtCount = 0;

DxWatchPhase dxWatchPhase()      { return(phase); }
uint16_t dxWatchSweeps()         { return(sweeps); }
uint16_t dxWatchCatches()        { return(catches); }
const DxCatch *dxWatchLastCatch(){ return(catches? &lastCatch : nullptr); }
uint32_t dxWatchFrequency()      { return(DXW_FIRST_HZ + (uint32_t)channel * DXW_STEP_HZ); }

static bool listed(const uint16_t *list, uint16_t count, uint16_t pi)
{
  for(int i=0 ; i<count ; i++)
    if(list[i]==pi) return(true);
  return(false);
}

static void quiet(bool on)
{
  // Fade rather than mute, the hardware mute thumps the speaker
  if(on)
  {
    softVolume(true);
    rx.setVolume(0);
    ledcWrite(PIN_LCD_BL, min((int)currentBrt, DXW_DIM_LEVEL));
  }
  else
  {
    ledcWrite(PIN_LCD_BL, currentBrt);
    rx.setVolume(0);
    softVolume(false);
  }
}

void dxWatchStart()
{
  if(!known) known = static_cast<uint16_t *>(ps_calloc(DXW_MAX_KNOWN, sizeof(uint16_t)));
  if(!taken) taken = static_cast<uint8_t *>(ps_calloc(DXW_CHANNELS, sizeof(uint8_t)));
  if(!known || !taken) return;
  memset(taken, 0, DXW_CHANNELS);

  savedBand = bandIdx;
  savedFreq = currentFrequency;
  if(currentMode!=FM) selectBroadcastBand("FM");
  if(currentMode!=FM) return;

  // The program identification is all DX Watch looks at
  rx.setRdsConfig(1, 2, 2, 2, 2);

  knownCount = caughtCount = catches = sweeps = channel = 0;
  phase = DXW_LEARNING;
  quiet(true);
}

void dxWatchStop(bool stay)
{
  if(phase==DXW_OFF) return;

  bool caughtOne = phase==DXW_CATCH;
  phase = DXW_OFF;
  rx.setRdsConfig(getRDSMode()!=RDS_OFF, 2, 2, 2, 2);

  if(!stay && savedFreq)
  {
    if(!caughtOne) rx.setVolume(0);
    if(savedBand==bandIdx) updateFrequency(savedFreq, false);
    else if(savedBand>=0 && savedBand<getTotalBands())
    {
      bands[savedBand].currentFreq = savedFreq;
      selectBand(savedBand);
    }
  }

  clearStationInfo();
  identifyFrequency(currentFrequency);
  if(!caughtOne) quiet(false);
  else ledcWrite(PIN_LCD_BL, currentBrt);
  prefsRequestSave(SAVE_CUR_BAND);
}

static void learn(uint16_t pi)
{
  if(pi && !listed(known, knownCount, pi) && knownCount<DXW_MAX_KNOWN) known[knownCount++] = pi;
}

// The waits below give way to a click on the encoder, so that leaving
// DX Watch never feels stuck
static bool clicked()
{
  return(digitalRead(ENCODER_PUSH_BUTTON)==LOW);
}

//
// Wait for the same program identification in a row of groups with a
// clean block A, so that a corrupt group is never taken for a station
//
static uint16_t readPi(uint32_t time, int repeats)
{
  uint32_t start = millis();
  uint16_t last = 0;
  int seen = 0;

  rx.getRdsStatus(0, 1, 0);
  while(millis() - start < time && !clicked())
  {
    rx.getRdsStatus();
    if(rx.getNumRdsFifoUsed() && rx.getRdsBlockAReliable())
    {
      uint16_t pi = rx.getRdsPI();
      if(pi && pi==last) seen++;
      else if(pi) { last = pi; seen = 1; }
      if(pi && seen>=repeats) return(pi);
    }
    delay(15);
  }
  return(0);
}

static void readPs(char *ps)
{
  uint32_t start = millis();
  strcpy(ps, "");

  while(millis() - start < DXW_PS_TIME && !clicked())
  {
    rx.getRdsStatus();
    const char *name = rx.getNumRdsFifoUsed()? rx.getRdsStationName() : nullptr;
    if(name)
    {
      int i;
      for(i=0 ; i<8 && name[i] ; i++) ps[i] = (name[i]>=' ' && name[i]<='~')? name[i] : ' ';
      ps[i] = '\0';
      while(i>0 && ps[i-1]==' ') ps[--i] = '\0';
      return;
    }
    delay(20);
  }
}

#define DXLOG_HEADER "time_utc,frequency_mhz,pi,ps,rssi_dbuv,snr_db"

// Logs from before 1.03 had the local time of day only: keep their catches
// under the header that has the UTC date and time
static void migrateLog()
{
  fs::File file = LittleFS.open(DXWATCH_LOG, "r");
  if(!file) return;
  String first = file.readStringUntil('\n');
  if(!first.startsWith("time,"))
  {
    file.close();
    return;
  }
  String rest = file.readString();
  file.close();

  file = LittleFS.open(DXWATCH_LOG, "w");
  if(!file) return;
  file.print(DXLOG_HEADER "\n");
  // Old rows: "HH:MM,..." become ",..." with no UTC time
  int start = 0;
  while(start < (int)rest.length())
  {
    int end = rest.indexOf('\n', start);
    if(end < 0) end = rest.length();
    String row = rest.substring(start, end);
    int comma = row.indexOf(',');
    if(comma >= 0) file.print(row.substring(comma) + "\n");
    start = end + 1;
  }
  file.close();
}

static void logCatch()
{
  migrateLog();
  fs::File file = LittleFS.open(DXWATCH_LOG, LittleFS.exists(DXWATCH_LOG)? "a" : "w");
  if(!file) return;

  if(!file.size()) file.print(DXLOG_HEADER "\n");

  // UTC date and time, when the clock is known, for logbooks such as FMLIST
  char utc[24] = "";
  time_t now = time(nullptr);
  if(now > 1700000000)
  {
    struct tm t;
    gmtime_r(&now, &t);
    strftime(utc, sizeof(utc), "%Y-%m-%dT%H:%M:%SZ", &t);
  }

  // A comma in the name would break the columns
  char ps[sizeof(lastCatch.ps)];
  strlcpy(ps, lastCatch.ps, sizeof(ps));
  for(char *p = ps ; *p ; p++) if(*p == ',' || *p == '"') *p = ' ';

  file.printf("%s,%.1f,%04X,%s,%u,%u\n", utc, lastCatch.freq / 1000000.0,
              lastCatch.pi, ps, lastCatch.rssi, lastCatch.snr);
  file.close();
}

void dxWatchWriteLog(Print &out)
{
  migrateLog();
  fs::File file = LittleFS.open(DXWATCH_LOG, "r");
  if(!file) return;
  uint8_t buffer[256];
  while(int n = file.read(buffer, sizeof(buffer))) out.write(buffer, n);
  file.close();
}

int dxWatchLoadLog(DxLogEntry *entries, int max)
{
  migrateLog();
  fs::File file = LittleFS.open(DXWATCH_LOG, "r");
  if(!file) return(0);

  int count = 0;
  file.readStringUntil('\n');
  while(file.available())
  {
    String row = file.readStringUntil('\n');
    row.trim();
    if(!row.length()) continue;

    // time_utc,frequency_mhz,pi,ps,rssi_dbuv,snr_db
    String field[6];
    int start = 0;
    for(int i=0 ; i<6 ; i++)
    {
      int comma = row.indexOf(',', start);
      field[i] = comma < 0? row.substring(start) : row.substring(start, comma);
      start = comma < 0? row.length() : comma + 1;
    }

    // Newest first, dropping the oldest ones when there are too many
    if(count == max) count--;
    memmove(&entries[1], &entries[0], count * sizeof(DxLogEntry));
    DxLogEntry *e = &entries[0];
    strlcpy(e->time, field[0].c_str(), sizeof(e->time));
    e->freq = (uint32_t)(field[1].toFloat() * 10 + 0.5) * 100000;
    e->pi = strtoul(field[2].c_str(), nullptr, 16);
    strlcpy(e->ps, field[3].c_str(), sizeof(e->ps));
    e->rssi = field[4].toInt();
    e->snr = field[5].toInt();
    count++;
  }
  file.close();
  return(count);
}

bool dxWatchClearLog()
{
  return(!LittleFS.exists(DXWATCH_LOG) || LittleFS.remove(DXWATCH_LOG));
}

bool dxWatchTick()
{
  if(phase==DXW_OFF) return(false);

  // A catch is left playing for a while, then watching goes on
  if(phase==DXW_CATCH)
  {
    if(millis() - catchTime < DXW_RESUME_TIME) return(false);
    phase = DXW_WATCHING;
    quiet(true);
    return(true);
  }

  uint32_t freq = dxWatchFrequency();
  if(!rx.setFrequencyConfirmed(freqFromHz(freq, FM))) return(false);
  currentFrequency = freq;
  getCurrentBand()->currentFreq = freq;

  rx.getCurrentReceivedSignalQuality();
  uint8_t sigRssi = rx.getCurrentRSSI();
  uint8_t sigSnr  = rx.getCurrentSNR();
  rssi = sigRssi;
  snr  = sigSnr;

  // The display follows every channel
  bool redraw = true;

  if(phase==DXW_LEARNING)
  {
    // Anything heard here belongs to the place, decoded or not
    if(sigSnr>=DXW_TAKEN_SNR) taken[channel] = 1;
    uint16_t pi = (sigRssi>=DXW_MIN_RSSI && sigSnr>=DXW_MIN_SNR)? readPi(DXW_LEARN_PI_TIME, 1) : 0;
    if(pi)
    {
      taken[channel] = 1;
      learn(pi);
    }
  }
  else if(!taken[channel] && sigRssi>=DXW_MIN_RSSI && sigSnr>=DXW_CATCH_SNR)
  {
    // A channel that was empty while learning has a signal now
    uint16_t pi = readPi(DXW_PI_TIME, DXW_PI_REPEATS);

    if(pi && listed(known, knownCount, pi))
    {
      // A local station spilling over, or coming and going: not news
      taken[channel] = 1;
    }
    else if(pi && !listed(caught, caughtCount, pi) && stationsIdentify(freq, false))
    {
      // Listed for the home province: a nearby station, not a catch
      taken[channel] = 1;
      learn(pi);
    }
    else if(pi && !listed(caught, caughtCount, pi))
    {
      // Something that is not usually there: let it play
      if(caughtCount<DXW_MAX_CAUGHT) caught[caughtCount++] = pi;
      lastCatch.freq = freq;
      lastCatch.pi = pi;
      lastCatch.rssi = sigRssi;
      lastCatch.snr = sigSnr;
      catches++;
      phase = DXW_CATCH;
      catchTime = millis();
      clearStationInfo();
      quiet(false);
      readPs(lastCatch.ps);
      logCatch();
      return(true);
    }
  }

  // Next channel, and next sweep at the end of the band
  if(++channel>=DXW_CHANNELS)
  {
    channel = 0;
    sweeps++;
    if(phase==DXW_LEARNING && sweeps>=DXW_LEARN_SWEEPS) phase = DXW_WATCHING;
    redraw = true;
  }

  return(redraw);
}
