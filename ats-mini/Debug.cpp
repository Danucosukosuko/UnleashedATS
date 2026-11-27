#include "Common.h"
#include "Menu.h"
#include "Storage.h"
#include "Debug.h"

#define DEBUG_MAX_BANDS 48

bool debugMode = false;
bool debugUsbLog = false;

// The limits the band table came with, to put them back
static uint32_t defaultMin[DEBUG_MAX_BANDS];
static uint32_t defaultMax[DEBUG_MAX_BANDS];
static bool defaultsKept = false;

static void keepDefaults()
{
  if(defaultsKept) return;
  for(int i=0 ; i<getTotalBands() && i<DEBUG_MAX_BANDS ; i++)
  {
    defaultMin[i] = bands[i].minimumFreq;
    defaultMax[i] = bands[i].maximumFreq;
  }
  defaultsKept = true;
}

void debugSetMode(bool on)
{
  debugMode = on;
  if(!on) debugUsbLog = false;
  prefs.begin("debug", false, STORAGE_PARTITION);
  prefs.putBool("Enabled", on);
  prefs.end();
}

void debugApplyBandLimits()
{
  keepDefaults();

  prefs.begin("debug", true, STORAGE_PARTITION);
  debugMode = prefs.getBool("Enabled", false);
  for(int i=0 ; i<getTotalBands() && i<DEBUG_MAX_BANDS ; i++)
  {
    char key[16];
    sprintf(key, "Min-%d", i);
    uint32_t minimum = prefs.getUInt(key, 0);
    sprintf(key, "Max-%d", i);
    uint32_t maximum = prefs.getUInt(key, 0);
    if(minimum && maximum && minimum < maximum)
    {
      bands[i].minimumFreq = minimum;
      bands[i].maximumFreq = maximum;
      if(bands[i].currentFreq < minimum || bands[i].currentFreq > maximum)
        bands[i].currentFreq = minimum;
    }
  }
  prefs.end();
}

void debugSetBandLimits(uint8_t band, uint32_t minimum, uint32_t maximum)
{
  if(band >= getTotalBands() || band >= DEBUG_MAX_BANDS || minimum >= maximum) return;
  keepDefaults();

  bands[band].minimumFreq = minimum;
  bands[band].maximumFreq = maximum;

  char key[16];
  prefs.begin("debug", false, STORAGE_PARTITION);
  sprintf(key, "Min-%d", band);
  prefs.putUInt(key, minimum);
  sprintf(key, "Max-%d", band);
  prefs.putUInt(key, maximum);
  prefs.end();
}

void debugResetBandLimits(uint8_t band)
{
  if(band >= getTotalBands() || band >= DEBUG_MAX_BANDS) return;
  keepDefaults();

  bands[band].minimumFreq = defaultMin[band];
  bands[band].maximumFreq = defaultMax[band];

  char key[16];
  prefs.begin("debug", false, STORAGE_PARTITION);
  sprintf(key, "Min-%d", band);
  prefs.remove(key);
  sprintf(key, "Max-%d", band);
  prefs.remove(key);
  prefs.end();
}

bool debugBandLimitsChanged(uint8_t band)
{
  keepDefaults();
  return(band < DEBUG_MAX_BANDS &&
         (bands[band].minimumFreq != defaultMin[band] || bands[band].maximumFreq != defaultMax[band]));
}
