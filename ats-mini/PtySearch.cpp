#include "Common.h"
#include "Utils.h"
#include "Menu.h"
#include "Storage.h"
#include "PtySearch.h"

#define PTYS_STEP_HZ     100000  // FM channels
#define PTYS_MIN_RSSI        12  // Weakest station worth reading (dBuV)
#define PTYS_MIN_SNR          5  // Noisiest station worth reading (dB)
#define PTYS_RDS_TIME     1200U  // Time given to the decoder to report the programme type (msecs)

extern const char *rdsProgramTypes[32];
extern const char *rbdsProgramTypes[32];

static PtySearchPhase phase = PTYS_OFF;
static uint8_t codes[32];       // Programme type codes offered
static int choices = 0;
static int choice = 0;
static uint8_t lastCode = 10;   // Remembered between searches
static uint32_t startFreq = 0, freq = 0;

static const char *const *table()
{
  return((getRDSMode() & RDS_RBDS)? rbdsProgramTypes : rdsProgramTypes);
}

PtySearchPhase ptySearchPhase() { return(phase); }
int ptySearchChoices()          { return(choices); }
int ptySearchChoice()           { return(choice); }
uint32_t ptySearchFrequency()   { return(freq); }
const char *ptySearchName(int i){ return(i>=0 && i<choices? table()[codes[i]] : ""); }

void ptySearchOpen()
{
  // Every named type, leaving out the test and alarm codes
  choices = 0;
  for(int code=1 ; code<30 ; code++)
    if(table()[code]) codes[choices++] = code;

  choice = 0;
  for(int i=0 ; i<choices ; i++)
    if(codes[i] == lastCode) choice = i;

  phase = PTYS_CHOOSE;
  currentCmd = CMD_PTYSEARCH;
}

static void finish(bool found)
{
  phase = PTYS_OFF;
  currentCmd = CMD_NONE;
  rx.setRdsConfig(getRDSMode()!=RDS_OFF, 2, 2, 2, 2);

  if(!found) updateFrequency(startFreq, false);
  clearStationInfo();
  identifyFrequency(currentFrequency);
  prefsRequestSave(SAVE_CUR_BAND);

  // Fade back in, the hardware mute thumps the speaker
  rx.setVolume(0);
  softVolume(false);

  char text[48];
  snprintf(text, sizeof(text), found? "Found %s" : "No %s station found", table()[lastCode]);
  statusShow("PTY Search", text);
}

void ptySearchRotate(int16_t enc)
{
  if(phase == PTYS_CHOOSE && choices)
    choice = ((choice + enc) % choices + choices) % choices;
  else if(phase == PTYS_SEARCHING)
    // Tuning by hand stops the search where it is
    finish(true);
}

void ptySearchClick()
{
  if(phase == PTYS_SEARCHING) { finish(false); return; }
  if(phase != PTYS_CHOOSE || !choices) return;

  lastCode = codes[choice];
  if(currentMode != FM) selectBroadcastBand("FM");
  if(currentMode != FM) { phase = PTYS_OFF; currentCmd = CMD_NONE; return; }

  // The programme type is read even with RDS turned off
  rx.setRdsConfig(1, 2, 2, 2, 2);
  softVolume(true);
  rx.setVolume(0);
  startFreq = freq = currentFrequency;
  phase = PTYS_SEARCHING;
}

// The same programme type and station in two good groups in a row
static int readPty()
{
  uint32_t start = millis();
  uint16_t lastPi = 0;
  int lastPty = -1, seen = 0;

  rx.getRdsStatus(0, 1, 0);
  while(millis() - start < PTYS_RDS_TIME && digitalRead(ENCODER_PUSH_BUTTON) != LOW)
  {
    rx.getRdsStatus();
    if(rx.getNumRdsFifoUsed() && rx.getRdsBlocksReliable())
    {
      uint16_t pi = rx.getRdsPI();
      int pty = (rx.getRdsBlock(1) >> 5) & 0x1F;
      if(pi && pi == lastPi && pty == lastPty) seen++;
      else { lastPi = pi; lastPty = pty; seen = 1; }
      if(pi && seen >= 2) return(pty);
    }
    delay(15);
  }
  return(-1);
}

bool ptySearchTick()
{
  if(phase != PTYS_SEARCHING) return(false);

  const Band *band = getCurrentBand();
  freq += PTYS_STEP_HZ;
  if(freq > band->maximumFreq) freq = band->minimumFreq;

  // All the way round without finding it
  if(freq == startFreq || (freq > startFreq && freq - startFreq < PTYS_STEP_HZ))
  {
    finish(false);
    return(true);
  }

  if(!rx.setFrequencyConfirmed(freqFromHz(freq, FM))) return(false);
  currentFrequency = freq;
  getCurrentBand()->currentFreq = freq;

  rx.getCurrentReceivedSignalQuality();
  rssi = rx.getCurrentRSSI();
  snr = rx.getCurrentSNR();
  if(rssi >= PTYS_MIN_RSSI && snr >= PTYS_MIN_SNR && readPty() == lastCode)
    finish(true);

  return(true);
}
