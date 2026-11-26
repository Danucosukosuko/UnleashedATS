#include "Common.h"
#include "Utils.h"
#include "Menu.h"
#include "Memories.h"
#include "Draw.h"
#include "Storage.h"

#define ATS_PEAK_DROP        6  // Level drop that tells two stations apart (dB)
#define ATS_NAME_TIME    6000U  // Time given to the decoder to spell out a name (msecs)
#define ATS_NAME_POLL       20  // Station name polling interval (msecs)
#define ATS_NAME_LENGTH      8  // Length of an RDS station name

//
// Channel spacing used for the sweep, in Hz
//
static uint32_t autoStoreSpacing(const Band *band)
{
  switch(band->bandType)
  {
    case FM_BAND_TYPE:
      // A 100kHz grid catches both the 100kHz and the 200kHz band plans
      return(100000);
    case MW_BAND_TYPE:
    case LW_BAND_TYPE:
      // Follow the current step to stay on the regional 9 or 10kHz grid
      return(getCurrentStep()->step==9000? 9000 : 10000);
    default:
      // International 5kHz channel spacing
      return(5000);
  }
}

//
// Wait for the decoder to spell out the name of the station the
// receiver is on, and copy it into the given buffer
//
static bool autoStoreName(char *name, uint8_t size)
{
  uint32_t startTime = millis();

  *name = 0;

  // Start from a clean slate, the previous station may still be in there
  rx.getRdsStatus(0, 1, 0);
  rx.RdsInit();

  while((millis() - startTime) < ATS_NAME_TIME)
  {
    rx.getRdsStatus();

    // The decoder hands over a name only once all of it has arrived in order
    const char *ps = rx.getNumRdsFifoUsed()? rx.getRdsStationName() : NULL;
    if(ps)
    {
      uint8_t len;
      for(len=0 ; len<ATS_NAME_LENGTH && len<size-1 && ps[len] ; len++)
        name[len] = (ps[len]>=' ' && ps[len]<='~')? ps[len] : ' ';
      name[len] = 0;

      // Drop the padding the broadcasters add
      while(len && name[len-1]==' ') name[--len] = 0;
      char *start = name + strspn(name, " ");
      if(start!=name) memmove(name, start, strlen(start) + 1);
      return(*name != 0);
    }

    // Do not make the user sit through the wait to stop the sweep. The
    // flags are left alone here, the sweep is the one that acts on them.
    if(seekStop || digitalRead(ENCODER_PUSH_BUTTON)==LOW) break;

    delay(ATS_NAME_POLL);
  }

  return(false);
}

//
// Save a station into the first free memory slot. Returns the number
// of slots taken, or ATS_NO_SLOTS when the memory is full.
//
static int autoStoreSave(uint32_t freq, const char *name)
{
  int slot = -1;

  for(int i=0 ; i<getTotalMemories() ; i++)
  {
    Memory memory = getMemory(i);

    // Do not store the same station twice
    if(memory.freq==freq && memory.mode==currentMode) return(0);
    if(!memory.freq && slot<0) slot = i;
  }

  if(slot<0) return(ATS_NO_SLOTS);

  Memory memory = {};
  memory.freq = freq;
  memory.band = bandIdx;
  memory.mode = currentMode;
  strncpy(memory.name, name, sizeof(memory.name) - 1);
  setMemory(slot, memory);

  return(1);
}

//
// Pick up the name of the station at the peak of a run and store it
//
static int autoStoreCommit(uint32_t peakFreq, bool withNames)
{
  char name[ATS_NAME_LENGTH + 1] = "";

  // Drop back on the peak to pick up the station name
  if(withNames)
  {
    rx.setFrequencyConfirmed(freqFromHz(peakFreq, currentMode));
    autoStoreName(name, sizeof(name));
  }

  return(autoStoreSave(peakFreq, name));
}

//
// Sweep the current band from end to end and store every station found
// into the memory, naming it after the RDS name when there is one.
// Returns the number of stations stored, or a negative ATS_* value.
//
int autoStoreRun(uint8_t flags)
{
  const Band *band = getCurrentBand();
  uint32_t spacing = autoStoreSpacing(band);
  bool withNames = (currentMode==FM) && (getRDSMode()!=RDS_OFF);
  uint8_t minRssi, minSnr;
  int stored = 0;
  int result = 0;
  int shown = -1;

  // SSB carries no carrier to measure, and the BFO would be lost anyway
  if(isSSB()) return(ATS_BAD_MODE);

  getSignalThresholds(&minRssi, &minSnr);

  // Make room for the stations about to be found
  for(int i=0 ; i<getTotalMemories() ; i++)
  {
    Memory memory = getMemory(i);
    if(memory.freq && ((flags & ATS_CLEAR_ALL) || ((flags & ATS_REPLACE) && memory.band==bandIdx)))
    {
      memory.freq = 0;
      setMemory(i, memory);
    }
  }

  // Mute the audio
  muteOn(MUTE_TEMP, true, !audioClamp);
  // Flag is set by rotary encoder and cleared on seek/scan entry
  seekStop = false;
  // Save current frequency
  uint32_t curFreq = currentFrequency;

  // Strongest point of the channel run being followed
  uint32_t peakFreq = 0;
  uint8_t peakRssi = 0;

  // Start on the channel grid
  uint32_t first = (band->minimumFreq + spacing - 1) / spacing * spacing;

  for(uint32_t freq=first ; freq<=band->maximumFreq ; freq+=spacing)
  {
    bool commit = false;

    rx.setFrequencyConfirmed(freqFromHz(freq, currentMode));

    // Measure RSSI/SNR values
    rx.getCurrentReceivedSignalQuality();
    uint8_t curRssi = rx.getCurrentRSSI();
    uint8_t curSnr  = rx.getCurrentSNR();
    bool above = curRssi>=minRssi && curSnr>=minSnr;

    // A run of channels above the thresholds holds a single station at
    // its strongest point. A deep enough valley starts a new station.
    if(above)
    {
      if(!peakFreq || curRssi>peakRssi)
      {
        peakFreq = freq;
        peakRssi = curRssi;
      }
      else if(peakRssi-curRssi>=ATS_PEAK_DROP)
      {
        commit = true;
      }
    }
    else if(peakFreq)
    {
      commit = true;
    }

    if(commit)
    {
      int taken = autoStoreCommit(peakFreq, withNames);

      // Nowhere left to store the remaining stations
      if(taken==ATS_NO_SLOTS)
      {
        result = ATS_NO_SLOTS;
        break;
      }

      stored += taken;

      // Keep following the band if the run has not ended yet
      peakFreq = above? freq : 0;
      peakRssi = above? curRssi : 0;
    }

    // Show the progress through the band
    int percent = 100LL * (freq - band->minimumFreq) / (band->maximumFreq - band->minimumFreq);
    if(percent!=shown)
    {
      char text[16];
      sprintf(text, "ATS %d%%", percent);
      drawMessage(text);
      shown = percent;
    }

    // Let the user cut a long sweep short
    if(consumeAbortPending())
    {
      peakFreq = 0;
      break;
    }
  }

  // The last run can reach the end of the band
  if(peakFreq && !result)
  {
    int taken = autoStoreCommit(peakFreq, withNames);
    if(taken==ATS_NO_SLOTS) result = ATS_NO_SLOTS;
    else stored += taken;
  }

  // Restore current frequency
  updateFrequency(curFreq, false);
  // Unmute the audio
  muteOn(MUTE_TEMP, false, !audioClamp);

  // Keep whatever the memory looks like now
  if(stored || (flags & (ATS_REPLACE | ATS_CLEAR_ALL)))
    prefsRequestSave(SAVE_MEMORIES);

  return(result? result : stored);
}
