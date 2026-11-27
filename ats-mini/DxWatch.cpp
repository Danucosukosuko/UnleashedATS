#include "Common.h"
#include "Utils.h"
#include "Menu.h"
#include "Storage.h"
#include "DxWatch.h"

#include <LittleFS.h>

#define DXW_FIRST_HZ    87500000  // Broadcast FM band
#define DXW_LAST_HZ    108000000
#define DXW_STEP_HZ       100000
#define DXW_CHANNELS   ((DXW_LAST_HZ - DXW_FIRST_HZ) / DXW_STEP_HZ + 1)
#define DXW_LEARN_SWEEPS       2  // Sweeps spent learning the local stations
#define DXW_MIN_RSSI          10  // Weakest signal worth decoding (dBuV)
#define DXW_MIN_SNR            4  // Noisiest signal worth decoding (dB)
#define DXW_PI_TIME         450U  // Time given to the decoder to report a PI (msecs)
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
  if(!known) return;

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

//
// Wait for a program identification with a clean block A
//
static uint16_t readPi()
{
  uint32_t start = millis();

  rx.getRdsStatus(0, 1, 0);
  while(millis() - start < DXW_PI_TIME)
  {
    rx.getRdsStatus();
    if(rx.getNumRdsFifoUsed() && rx.getRdsBlockAReliable())
    {
      uint16_t pi = rx.getRdsPI();
      if(pi) return(pi);
    }
    delay(15);
  }
  return(0);
}

static void readPs(char *ps)
{
  uint32_t start = millis();
  strcpy(ps, "");

  while(millis() - start < DXW_PS_TIME)
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

static void logCatch()
{
  fs::File file = LittleFS.open(DXWATCH_LOG, LittleFS.exists(DXWATCH_LOG)? "a" : "w");
  if(!file) return;

  if(!file.size()) file.print("time,frequency_mhz,pi,ps,rssi_dbuv,snr_db\n");
  const char *now = clockGet();
  file.printf("%s,%.1f,%04X,%s,%u,%u\n", now && *now? now : "", lastCatch.freq / 1000000.0,
              lastCatch.pi, lastCatch.ps, lastCatch.rssi, lastCatch.snr);
  file.close();
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

  bool redraw = !(channel % 8);
  uint16_t pi = (sigRssi>=DXW_MIN_RSSI && sigSnr>=DXW_MIN_SNR)? readPi() : 0;

  if(pi && phase==DXW_LEARNING)
  {
    if(!listed(known, knownCount, pi) && knownCount<DXW_MAX_KNOWN) known[knownCount++] = pi;
  }
  else if(pi && !listed(known, knownCount, pi) && !listed(caught, caughtCount, pi))
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
