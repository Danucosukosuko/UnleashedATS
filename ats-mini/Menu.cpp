#include "math.h"
#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Draw.h"
#include "EIBI.h"
#include "Ota.h"
#include "BleMode.h"
#include "Menu.h"
#include "Memories.h"
#include "Patches.h"
#include "Storage.h"
#include "AltFreq.h"
#include "Stations.h"
#include "DxWatch.h"
#include "Debug.h"

#include <time.h>

//
// Bands Menu
//
// TO CONFIGURE YOUR OWN BAND PLAN:
// Add new bands by inserting new lines in the table below. Remove
// bands by deleting lines. Change bands by editing lines below.
//
// NOTE:
// You have to RESET PREFERENCES after adding or removing lines in this
// table. Turn your receiver on with the encoder push button pressed
// at first time to RESET the preferences.
//

int bandIdx = 0;

// All band frequencies are in Hz.
// Band limits are expanded to align with the nearest tuning scale mark
// Do not forget to update the bands table in the manual.md
Band bands[] =
{
  {"VHF",  FM_BAND_TYPE, FM, FM,  64000000, 108000000, 103900000, 2, 0, 0, 0},
  // All band. LW, MW and SW (from 150kHz to 30MHz)
  {"ALL",  SW_BAND_TYPE, AM, AM,    150000,  30000000,  15000000, 1, 4, 0, 0},
  {"11M",  SW_BAND_TYPE, AM, AM,  25600000,  26100000,  25850000, 1, 4, 0, 0},
  {"13M",  SW_BAND_TYPE, AM, AM,  21500000,  21900000,  21650000, 1, 4, 0, 0},
  {"15M",  SW_BAND_TYPE, AM, AM,  18900000,  19100000,  18950000, 1, 4, 0, 0},
  {"16M",  SW_BAND_TYPE, AM, AM,  17400000,  18100000,  17650000, 1, 4, 0, 0},
  {"19M",  SW_BAND_TYPE, AM, AM,  15100000,  15900000,  15450000, 1, 4, 0, 0},
  {"22M",  SW_BAND_TYPE, AM, AM,  13500000,  13900000,  13650000, 1, 4, 0, 0},
  {"25M",  SW_BAND_TYPE, AM, AM,  11000000,  13000000,  11850000, 1, 4, 0, 0},
  {"31M",  SW_BAND_TYPE, AM, AM,   9000000,  11000000,   9650000, 1, 4, 0, 0},
  {"41M",  SW_BAND_TYPE, AM, AM,   7000000,   9000000,   7300000, 1, 4, 0, 0},
  {"49M",  SW_BAND_TYPE, AM, AM,   5000000,   7000000,   6000000, 1, 4, 0, 0},
  {"60M",  SW_BAND_TYPE, AM, AM,   4000000,   5100000,   4950000, 1, 4, 0, 0},
  {"75M",  SW_BAND_TYPE, AM, AM,   3500000,   4000000,   3950000, 1, 4, 0, 0},
  {"90M",  SW_BAND_TYPE, AM, AM,   3000000,   3500000,   3300000, 1, 4, 0, 0},
//  {"25M",  SW_BAND_TYPE, AM, AM,  11600000,  12100000,  11850000, 1, 4, 0},
//  {"31M",  SW_BAND_TYPE, AM, AM,   9400000,   9900000,   9650000, 1, 4, 0},
//  {"41M",  SW_BAND_TYPE, AM, AM,   7200000,   7500000,   7300000, 1, 4, 0},
//  {"49M",  SW_BAND_TYPE, AM, AM,   5900000,   6200000,   6000000, 1, 4, 0},
//  {"60M",  SW_BAND_TYPE, AM, AM,   4700000,   5100000,   4950000, 1, 4, 0},
//  {"75M",  SW_BAND_TYPE, AM, AM,   3900000,   4000000,   3950000, 1, 4, 0},
//  {"90M",  SW_BAND_TYPE, AM, AM,   3200000,   3400000,   3300000, 1, 4, 0},
  {"MW3",  MW_BAND_TYPE, AM, AM,   1700000,   3500000,   2500000, 1, 4, 0, 0},
  {"MW2",  MW_BAND_TYPE, AM, AM,    495000,   1701000,    783000, 2, 4, 0, 0},
  {"MW1",  MW_BAND_TYPE, AM, AM,    150000,   1800000,    810000, 3, 4, 0, 0},
  {"160M", MW_BAND_TYPE, LSB, LSB,   1800000,   2000000,   1900000, 5, 4, 0, 0},
  {"80M",  SW_BAND_TYPE, LSB, LSB,   3500000,   4000000,   3800000, 5, 4, 0, 0},
  {"40M",  SW_BAND_TYPE, LSB, LSB,   7000000,   7300000,   7150000, 5, 4, 0, 0},
  {"30M",  SW_BAND_TYPE, LSB, LSB,  10000000,  10200000,  10125000, 5, 4, 0, 0},
  {"20M",  SW_BAND_TYPE, USB, USB,  14000000,  14400000,  14100000, 5, 4, 0, 0},
  {"17M",  SW_BAND_TYPE, USB, USB,  18000000,  18200000,  18115000, 5, 4, 0, 0},
  {"15M",  SW_BAND_TYPE, USB, USB,  21000000,  21500000,  21225000, 5, 4, 0, 0},
  {"12M",  SW_BAND_TYPE, USB, USB,  24800000,  25000000,  24940000, 5, 4, 0, 0},
  {"10M",  SW_BAND_TYPE, USB, USB,  28000000,  29700000,  28500000, 5, 4, 0, 0},
  // https://www.hfunderground.com/wiki/CB
  // Also see MIN_CB_FREQUENCY and MAX_CB_FREQUENCY
  {"CB",   SW_BAND_TYPE, AM, AM,  25000000,  28000000,  27135000, 0, 4, 0, 0},
};

int getTotalBands() { return(ITEM_COUNT(bands)); }
Band *getCurrentBand() { return(&bands[bandIdx]); }

static int findBand(uint32_t freq, uint8_t mode, int preferredBand)
{
  if(preferredBand >= 0 && preferredBand < getTotalBands() &&
     isFreqInBand(&bands[preferredBand], freq)) return preferredBand;

  int best = -1;
  uint32_t bestWidth = UINT32_MAX;
  // ALL is wider than every overlapping named band, so it is the fallback.
  for(int i = 0; i < getTotalBands(); i++)
  {
    const Band &band = bands[i];
    if(!isFreqInBand(&band, freq)) continue;
    const uint32_t width = band.maximumFreq - band.minimumFreq;
    // Equal widths prefer the default mode; otherwise keep table order.
    if(best < 0 || width < bestWidth ||
       (width == bestWidth && band.defaultMode == mode && bands[best].defaultMode != mode))
    {
      best = i;
      bestWidth = width;
    }
  }
  return best;
}

//
// Main Menu
//

#define MENU_MODE         0
#define MENU_BAND         1
#define MENU_VOLUME       2
#define MENU_VFO          3
#define MENU_STEP         4
#define MENU_SEEK         5
#define MENU_SCAN         6
#define MENU_MEMORY       7
#define MENU_MEMSCAN      8
#define MENU_AUTOSTORE    9
#define MENU_STATIONS    10
#define MENU_AFLIST      11
#define MENU_EONLIST     12
#define MENU_DXWATCH     13
#define MENU_SQUELCH     14
#define MENU_BW          15
#define MENU_AGC_ATT     16
#define MENU_AVC         17
#define MENU_SOFTMUTE    18
#define MENU_SETTINGS    19

int8_t menuIdx = MENU_VOLUME;

static const char *menu[] =
{
  "Mode",
  "Band",
  "Volume",
  "VFO A/B",
  "Step",
  "Seek",
  "Scan",
  "Memory",
  "Mem Scan",
  "Auto Store",
  "Stations",
  "RDS AF",
  "RDS EON",
  "DX Watch",
  "Squelch",
  "Bandwidth",
  "AGC/ATTN",
  "AVC",
  "SoftMute",
  "Settings",
};

//
// Settings Menu
//

#define MENU_BRIGHTNESS   0
#define MENU_DATETIME     1
#define MENU_UTCOFFSET    2
#define MENU_SLEEP        3
#define MENU_SLEEPMODE    4
#define MENU_OFFTIMER     5
#define MENU_THEME        6
#define MENU_UI           7
#define MENU_ZOOM         8
#define MENU_SCROLL       9
#define MENU_ENC_ACCEL    10
#define MENU_RDS          11
#define MENU_FM_STEREO    12
#define MENU_FM_REGION    13
#define MENU_DXMODE       14
#define MENU_ANTCAP       15
#define MENU_CALIBRATION  16
#define MENU_DSP_PATCHES  17
#define MENU_LOADEIBI     18
#define MENU_LOADSTN      19
#define MENU_UPDATEFW     20
#define MENU_USBMODE      21
#define MENU_TCPMODE      22
#define MENU_BLEMODE      23
#define MENU_WIFIMODE     24
#define MENU_ABOUT        25
#define MENU_DEBUG        26  // Only listed in debug mode


static uint8_t updateFwIdx = 0;
static const char *const updateFwActions[] = {"Check", "Update"};

int8_t settingsIdx = MENU_BRIGHTNESS;

static const char *settings[] =
{
  "Brightness",
  "Date/Time",
  "UTC Offset",
  "Sleep Timer",
  "Sleep Mode",
  "Off Timer",
  "Theme",
  "UI Layout",
  "Zoom Menu",
  "Scroll Dir.",
  "Enc. Accel.",
  "RDS",
  "FM Stereo",
  "FM Region",
  "DX Mode",
  "Capacitor",
  "Calibration",
  "DSP Patches",
  "Load EiBi",
  "Load Stations",
  "Update FW",
  "USB Port",
  "TCP Port",
  "Bluetooth",
  "Wi-Fi",
  "About",
  "Debug",
};

// The last setting only shows up in debug mode
static int visibleSettings() { return(ITEM_COUNT(settings) - (debugMode? 0 : 1)); }

//
// FM Region Menu
//
const FMRegion fmRegions[] = {
  // 50uS de-emphasis
  { 0x1, "EU/JP/AU" },
  // 75uS de-emphasis
  { 0x2, "US" },
};

int getTotalFMRegions() { return(ITEM_COUNT(fmRegions)); }

//
// Mode Menu
//

const char *bandModeDesc[] = { "FM", "LSB", "USB", "AM" };

int getTotalModes() { return(ITEM_COUNT(bandModeDesc)); }

//
// FM Stereo Menu
//

uint8_t fmStereoIdx = FM_STEREO_AUTO;
const char *const fmStereoDesc[] = { "Auto", "Mono" };

int getTotalFMStereoModes() { return(ITEM_COUNT(fmStereoDesc)); }

bool audioClamp = true;

// Weak signal listening: no soft mute, earlier mono blend, sensitive seek
bool dxMode = false;

//
// Auto Store Menu
//

static uint8_t autoStoreIdx = 0;
static const char *const autoStoreActions[] = { "Back", "Add New", "Replace" };

//
// Station list menus. Item 0 of every list is "Back".
//

static uint8_t stnLevel = 0;      // 0: countries, 1: provinces, 2: stations
static int stnIdx = 0;            // Highlighted item
static int stnCountryIdx = 0;     // Country being browsed
static int stnRegionIdx = -1;     // Province being browsed, -1 for all
static uint8_t loadLevel = 0;     // 0: regions, 1: countries
static int loadIdx = 0;           // Highlighted item
static int loadRegion = 0;        // Region being browsed

//
// RDS AF and EON menus. Item 0 is "Back". Clicking a frequency tunes to
// it and lets the encoder move between the frequencies of the list.
//

static int afMenuIdx = 0;
static bool afMenuTuning = false;
static uint8_t eonLevel = 0;      // 0: networks, 1: frequencies
static int eonIdx = 0;
static bool eonTuning = false;
static EonNetwork eonSelected;    // Network being browsed

//
// DX Watch: a warning first, then the sweep
//
static uint8_t dxwIdx = 1;

//
// Debug menu (debug mode only)
//
static uint8_t aboutPresses = 0;  // Short presses on the About screen
static int debugIdx = 1;
static int debugEdit = 0;         // 1: editing the minimum, 2: the maximum
static uint32_t debugValue = 0;
static const char *const debugItems[] = { "Back", "Min", "Max", "Reset Band", "USB Log", "Debug Off" };

static void freqText(char *text, size_t size, uint32_t freq, bool fm)
{
  if(fm) snprintf(text, size, "%.2f", freq / 1000000.0);
  else snprintf(text, size, "%lu k", (unsigned long)(freq / 1000));
}

static void doDebug(int16_t enc, int16_t enca)
{
  if(!debugEdit)
  {
    debugIdx = wrap_range(debugIdx, enc, 0, LAST_ITEM(debugItems));
    return;
  }

  // Past what the receiver is known to support, on purpose
  bool fm = bands[bandIdx].bandMode==FM;
  int64_t step  = fm? 100000 : 1000;
  int64_t value = (int64_t)debugValue + step * enca;
  int64_t lo = fm? 30000000 : 100000, hi = fm? 150000000 : 35000000;
  debugValue = value<lo? lo : value>hi? hi : value;
}

static void clickDebug(bool shortPress)
{
  Band *band = &bands[bandIdx];

  if(debugEdit)
  {
    uint32_t minimum = debugEdit==1? debugValue : band->minimumFreq;
    uint32_t maximum = debugEdit==2? debugValue : band->maximumFreq;
    debugEdit = 0;
    if(minimum >= maximum)
    {
      drawMessage("Min >= Max");
      delay(1000);
      return;
    }
    debugSetBandLimits(bandIdx, minimum, maximum);
    if(band->currentFreq < minimum || band->currentFreq > maximum) band->currentFreq = minimum;
    selectBand(bandIdx);
    prefsRequestSave(SAVE_CUR_BAND);
    return;
  }

  switch(debugIdx)
  {
    case 0: currentCmd = CMD_NONE; break;
    case 1:
    case 2:
      debugEdit = debugIdx;
      debugValue = debugIdx==1? band->minimumFreq : band->maximumFreq;
      break;
    case 3:
      debugResetBandLimits(bandIdx);
      if(band->currentFreq < band->minimumFreq || band->currentFreq > band->maximumFreq)
        band->currentFreq = band->minimumFreq;
      selectBand(bandIdx);
      drawMessage("Band reset");
      delay(800);
      break;
    case 4: debugUsbLog = !debugUsbLog; break;
    case 5:
      debugSetMode(false);
      settingsIdx = 0;
      currentCmd = CMD_NONE;
      drawMessage("Debug Off");
      delay(800);
      break;
  }
}
static const char *const dxwActions[] = { "Back", "Start" };

uint8_t dspPatchesIdx = DSP_PATCHES_DEFAULT;

//
// Memory Menu
//

uint8_t memoryIdx = 0;
Memory newMemory;

//
// RDS Menu
//

uint8_t rdsModeIdx = 0;
const RDSMode rdsMode[] =
{
  { RDS_PS, "PS"},
  { RDS_PS | RDS_CT, "PS+CT" },
  { RDS_PS | RDS_PI, "PS+PI" },
  { RDS_PS | RDS_PI | RDS_CT, "PS+PI+CT" },
  { RDS_PS | RDS_PI | RDS_RT | RDS_PT, "ALL-CT (EU)" },
  { RDS_PS | RDS_PI | RDS_RT | RDS_PT | RDS_RBDS, "ALL-CT (US)" },
  { RDS_PS | RDS_PI | RDS_RT | RDS_PT | RDS_CT, "ALL (EU)" },
  { RDS_PS | RDS_PI | RDS_RT | RDS_PT | RDS_CT | RDS_RBDS, "ALL (US)" },
  { RDS_OFF, "OFF"},
  { RDS_PS | RDS_PI | RDS_RT | RDS_PT | RDS_CT | RDS_AF, "ALL+AF (EU)" },
  { RDS_PS | RDS_PI | RDS_RT | RDS_PT | RDS_CT | RDS_RBDS | RDS_AF, "ALL+AF (US)" },
};

uint8_t getRDSMode() { return(rdsMode[rdsModeIdx].mode); }
int getTotalRDSModes() { return(ITEM_COUNT(rdsMode)); }

//
// Sleep Mode Menu
//

uint8_t sleepModeIdx = SLEEP_LOCKED;
const char *const sleepModeDesc[] =
{ "Locked", "Unlocked", "CPU Sleep" };

int getTotalSleepModes() { return(ITEM_COUNT(sleepModeDesc)); }

//
// UTC Offset Menu
// https://en.wikipedia.org/wiki/List_of_UTC_offsets
// https://www.timeanddate.com/time/time-zones-interesting.html
//
uint8_t utcOffsetIdx = 15; // UTC+0
const UTCOffset utcOffsets[] =
{
  { -12 * 4, "UTC-12" },
  { -11 * 4, "UTC-11" },
  { -10 * 4, "UTC-10" },
  { -9 * 4 - 2, "UTC-9:30" },
  { -9 * 4, "UTC-9" },
  { -8 * 4, "UTC-8" },
  { -7 * 4, "UTC-7" },
  { -6 * 4, "UTC-6" },
  { -5 * 4, "UTC-5" },
  { -4 * 4, "UTC-4" },
  { -3 * 4 - 2, "UTC-3:30" },
  { -3 * 4, "UTC-3" },
  { -2 * 4 - 2, "UTC-2:30" },
  { -2 * 4, "UTC-2" },
  { -1 * 4, "UTC-1" },
  {  0 * 4, "UTC+0" },
  {  1 * 4, "UTC+1" },
  {  2 * 4, "UTC+2" },
  {  3 * 4, "UTC+3" },
  {  3 * 4 + 2, "UTC+3:30" },
  {  4 * 4, "UTC+4" },
  {  4 * 4 + 2, "UTC+4:30" },
  {  5 * 4, "UTC+5" },
  {  5 * 4 + 2, "UTC+5:30" },
  {  5 * 4 + 3, "UTC+5:45" },
  {  6 * 4, "UTC+6" },
  {  6 * 4 + 2, "UTC+6:30" },
  {  7 * 4, "UTC+7" },
  {  8 * 4, "UTC+8" },
  {  8 * 4 + 3, "UTC+8:45" },
  {  9 * 4, "UTC+9" },
  {  9 * 4 + 2, "UTC+9:30" },
  { 10 * 4, "UTC+10" },
  { 10 * 4 + 2, "UTC+10:30" },
  { 11 * 4, "UTC+11" },
  { 12 * 4, "UTC+12" },
  { 12 * 4 + 3, "UTC+12:45" },
  { 13 * 4, "UTC+13" },
  { 13 * 4 + 3, "UTC+13:45" },
  { 14 * 4, "UTC+14" },
};

int8_t getCurrentUTCOffset() { return(utcOffsets[utcOffsetIdx].offset); }
int getTotalUTCOffsets() { return(ITEM_COUNT(utcOffsets)); }

//
// Date/Time Menu
//
enum DateTimeField
{
  DATETIME_YEAR,
  DATETIME_MONTH,
  DATETIME_DAY,
  DATETIME_HOUR,
  DATETIME_MINUTE,
  DATETIME_SECOND,
  DATETIME_FIELD_COUNT,
};

static const uint16_t dateTimeMaxYear = 2105;
static uint8_t dateTimeField;
static uint16_t dateTimeYear;
static uint8_t dateTimeMonth;
static uint8_t dateTimeDay;
static uint8_t dateTimeHour;
static uint8_t dateTimeMinute;
static uint8_t dateTimeSecond;

//
// UI Layout Menu
//
uint8_t uiLayoutIdx = 0;
const char *const uiLayoutDesc[] =
{ "Default", "S-Meter" };

int getTotalUILayouts() { return(ITEM_COUNT(uiLayoutDesc)); }

//
// USB Port Mode Menu
//

uint8_t usbModeIdx = USB_OFF;
const char *const usbModeDesc[] =
{ "Off", "Ad hoc", "CAT TS-480" };

int getTotalUSBModes() { return(ITEM_COUNT(usbModeDesc)); }

//
// TCP Port Mode Menu
//

uint8_t tcpModeIdx = TCP_OFF;
const char *const tcpModeDesc[] =
{ "Off", "Ad hoc", "CAT TS-480" };

int getTotalTCPModes() { return(ITEM_COUNT(tcpModeDesc)); }

//
// Bluetooth Mode Menu
//

uint8_t bleModeIdx = BLE_OFF;
static uint8_t bleModeMenuIdx = BLE_OFF;
const char *const bleModeDesc[] =
{ "Off", "Ad hoc", "CAT TS-480", "HID", "Unpair All" };

int getTotalBluetoothModes() { return(ITEM_COUNT(bleModeDesc)); }

//
// WiFi Mode Menu
//

uint8_t wifiModeIdx = NET_OFF;
const char *const wifiModeDesc[] =
{ "Off", "AP Only", "AP+Connect", "Connect", "Sync Only" };

int getTotalWiFiModes() { return(ITEM_COUNT(wifiModeDesc)); }

//
// Step Menu
//

// FM (Hz)
static const Step fmSteps[] =
{
  {   10000, "10k",   1 },
  {   50000, "50k",   5 },
  {  100000, "100k", 10 },
  {  200000, "200k", 20 },
  { 1000000, "1M",   10 },
};

// SSB (Hz)
static const Step ssbSteps[] =
{
  {    10, "10",  1  },
  {    25, "25",  1  },
  {    50, "50",  1  },
  {   100, "100", 1  },
  {   500, "500", 1  },
  {  1000, "1k",  1  },
  {  5000, "5k",  5  },
  {  9000, "9k",  9  },
  { 10000, "10k", 10 },
};

// AM (Hz)
static const Step amSteps[] =
{
  {    1000, "1k",    1 },
  {    5000, "5k",    5 },
  {    9000, "9k",    9 },
  {   10000, "10k",  10 },
  {   50000, "50k",  10 },
  {  100000, "100k", 10 },
  { 1000000, "1M",   10 },
};

static const Step *steps[4] = { fmSteps, ssbSteps, ssbSteps, amSteps };
static const uint8_t defaultStepIdx[4] = { 2, 5, 5, 1 };

static int getLastStep(int mode)
{
  switch(mode)
  {
    case FM:  return(LAST_ITEM(fmSteps));
    case LSB: return(LAST_ITEM(ssbSteps));
    case USB: return(LAST_ITEM(ssbSteps));
    case AM:  return(LAST_ITEM(amSteps));
  }

  return(0);
}

const Step *getCurrentStep()
{
  uint8_t idx = bands[bandIdx].currentStepIdx > getLastStep(currentMode) ? defaultStepIdx[currentMode] : bands[bandIdx].currentStepIdx;
  return(&steps[currentMode][idx]);
}

static uint8_t freqInputPos = 0;

static uint8_t getDefaultFreqInputPos(int step)
{
  return (uint8_t)(log10(step) * 2);
}

void resetFreqInputPos()
{
  freqInputPos = getDefaultFreqInputPos(getCurrentStep()->step);
}

uint8_t getFreqInputPos()
{
  // FM highlighting starts at 10 kHz; AM/SSB highlighting starts at 1 Hz.
  return freqInputPos - (currentMode == FM ? 8 : 0);
}

int getFreqInputStep()
{
  return (freqInputPos % 2 ? 5 : 1) * pow(10, freqInputPos / 2);
}

static uint8_t getMinFreqInputPos()
{
  return getDefaultFreqInputPos(steps[currentMode][0].step);
}

static uint8_t getMaxFreqInputPos()
{
  return (uint8_t)log10(getCurrentBand()->maximumFreq) * 2 - (currentMode == FM ? 2 : 0);
}

//
// Bandwidth Menu
//

static const Bandwidth fmBandwidths[] =
{
  { 0, "Auto" }, // Automatic - default
  { 1, "110k" }, // Force wide (110 kHz) channel filter.
  { 2, "84k"  },
  { 3, "60k"  },
  { 4, "40k"  }
};

static const Bandwidth ssbBandwidths[] =
{
  { 4, "0.5k" },
  { 5, "1.0k" },
  { 0, "1.2k" },
  { 1, "2.2k" },
  { 2, "3.0k" },
  { 3, "4.0k" }
};

static const Bandwidth amBandwidths[] =
{
  { 4, "1.0k" },
  { 5, "1.8k" },
  { 3, "2.0k" },
  { 6, "2.5k" },
  { 2, "3.0k" },
  { 1, "4.0k" },
  { 0, "6.0k" }
};

static const Bandwidth *bandwidths[4] =
{
  fmBandwidths, ssbBandwidths, ssbBandwidths, amBandwidths
};

static const uint8_t defaultBwIdx[4] = { 0, 4, 4, 4 };

static int getLastBandwidth(int mode)
{
  switch(mode)
  {
    case FM:  return(LAST_ITEM(fmBandwidths));
    case LSB: return(LAST_ITEM(ssbBandwidths));
    case USB: return(LAST_ITEM(ssbBandwidths));
    case AM:  return(LAST_ITEM(amBandwidths));
  }

  return(0);
}

static int8_t getCurrentBandwidthIdx()
{
  int8_t idx = bands[bandIdx].bandwidthIdx;
  return(idx<0 || idx>getLastBandwidth(currentMode)? defaultBwIdx[currentMode] : idx);
}

const Bandwidth *getCurrentBandwidth()
{
  return(&bandwidths[currentMode][getCurrentBandwidthIdx()]);
}

static void setBandwidth()
{
  uint8_t idx = getCurrentBandwidth()->idx;

  switch(currentMode)
  {
    case FM:
      rx.setFmBandwidth(idx);
      break;
    case AM:
      rx.setBandwidth(idx, 1);
      break;
    case LSB:
    case USB:
      // Set Audio
      rx.setSSBAudioBandwidth(idx);
      // If audio bandwidth selected is about 2 kHz or below, it is
      // recommended to set Sideband Cutoff Filter to 0.
      rx.setSSBSidebandCutoffFilter(idx==0 || idx==4 || idx==5? 0 : 1);
      break;
  }
}

// Seek mode. Pass true to toggle, false to return the current one
uint8_t seekMode(bool toggle)
{
  static uint8_t mode = SEEK_DEFAULT;

  if(toggle) mode = (mode + 1) % SEEK_MODES;

  // The schedule needs shortwave, a loaded schedule and a clock
  if(mode==SEEK_SCHEDULE && (currentMode==FM || !eibiAvailable() || !clockAvailable()))
  {
    if(!toggle) return(SEEK_DEFAULT);
    mode = SEEK_AUTO;
  }

  // There is no band plan to sweep in SSB
  if(mode==SEEK_AUTO && isSSB())
  {
    if(!toggle) return(SEEK_DEFAULT);
    mode = SEEK_DEFAULT;
  }

  return(mode);
}

//
// Utility functions to change menu values
//

static inline int min(int x, int y) { return(x<y? x:y); }

static inline int wrap_range(int v, int enc, int vMin, int vMax)
{
  v += enc;
  v  = v>vMax? vMin + (v - vMax - 1) % (vMax - vMin + 1) : v<vMin? vMax - (vMin - v - 1) % (vMax - vMin + 1) : v;
  return(v);
}

static inline int clamp_range(int v, int enc, int vMin, int vMax)
{
  v += enc;
  v  = v>vMax? vMax : v<vMin? vMin : v;
  return(v);
}

static uint8_t dateTimeDaysInMonth(uint16_t year, uint8_t month)
{
  struct tm fields = {};
  fields.tm_year = year - 1900;
  fields.tm_mon = month;
  fields.tm_mday = 0;
  mktime(&fields);
  return(fields.tm_mday);
}

static void dateTimeInit()
{
  dateTimeField = DATETIME_YEAR;
  dateTimeYear = dateTimeMonth = dateTimeDay = 0;
  dateTimeHour = dateTimeMinute = dateTimeSecond = 0;

  if(!clockAvailable()) return;

  time_t now = time(NULL);
  struct tm fields;
  gmtime_r(&now, &fields);
  dateTimeHour = fields.tm_hour;
  dateTimeMinute = fields.tm_min;
  dateTimeSecond = fields.tm_sec;

  // A time-only RDS update does not provide a usable date.
  if(!clockGetDate(NULL, NULL, NULL, NULL)) return;

  dateTimeYear = min(fields.tm_year + 1900, dateTimeMaxYear);
  dateTimeMonth = fields.tm_mon + 1;
  dateTimeDay = fields.tm_mday;
}

static void doDateTime(int16_t enc)
{
  bool dateMissing = dateTimeYear < CLOCK_MIN_YEAR || !dateTimeMonth || !dateTimeDay;
  if(dateMissing)
  {
    dateTimeYear = CLOCK_MIN_YEAR;
    dateTimeMonth = 1;
    dateTimeDay = 1;
    // The first step toward a higher value selects the minimum date value.
    if(dateTimeField <= DATETIME_DAY && enc > 0) enc--;
  }

  switch(dateTimeField)
  {
    case DATETIME_YEAR:   dateTimeYear   = clamp_range(dateTimeYear,   enc, CLOCK_MIN_YEAR, dateTimeMaxYear);break;
    case DATETIME_MONTH:  dateTimeMonth  = wrap_range(dateTimeMonth,  enc, 1, 12);break;
    case DATETIME_DAY:    dateTimeDay    = wrap_range(dateTimeDay,    enc, 1, dateTimeDaysInMonth(dateTimeYear, dateTimeMonth));break;
    case DATETIME_HOUR:   dateTimeHour   = wrap_range(dateTimeHour,   enc, 0, 23);break;
    case DATETIME_MINUTE: dateTimeMinute = wrap_range(dateTimeMinute, enc, 0, 59);break;
    case DATETIME_SECOND: dateTimeSecond = wrap_range(dateTimeSecond, enc, 0, 59);break;
  }

  // Keep the day valid when the month or year changes.
  if(dateTimeField <= DATETIME_MONTH)
    dateTimeDay = min(dateTimeDay, dateTimeDaysInMonth(dateTimeYear, dateTimeMonth));
}

static void clickDateTime(bool shortPress)
{
  if(!shortPress)
  {
    dateTimeField = (dateTimeField + 1) % DATETIME_FIELD_COUNT;
    return;
  }

  uint32_t epoch;
  if(clockUTCDateTimeToEpoch(dateTimeYear, dateTimeMonth, dateTimeDay,
                            dateTimeHour, dateTimeMinute, dateTimeSecond, &epoch))
    clockSetEpoch(epoch);

  currentCmd = CMD_NONE;
}

//
// Encoder input handlers
//

void doSelectDigit(int16_t enc)
{
  freqInputPos = clamp_range(freqInputPos, -enc, getMinFreqInputPos(), getMaxFreqInputPos());
}

void doVolume(int16_t enc)
{
  volume = clamp_range(volume, enc, 0, 63);
  rx.setVolume(volume);
  if(enc) prefsRequestSave(SAVE_SETTINGS);
}

static void clickVolume(bool shortPress)
{
  if(shortPress) muteOn(MUTE_MAIN, !muteOn(MUTE_MAIN)); else currentCmd = CMD_NONE;
}

static void clickSquelch(bool shortPress)
{
  if(shortPress)
  {
    if(currentSquelch[currentMode] & 0x7f)
      currentSquelch[currentMode] &= 0x80;
    else
      currentSquelch[currentMode] ^= 0x80;
    prefsRequestSave(SAVE_SETTINGS);
  }
  else
  {
    currentCmd = CMD_NONE;
  }
}

static void clickSeek(bool shortPress)
{
  if(shortPress) seekMode(true); else currentCmd = CMD_NONE;
}

static void clickScan(bool shortPress)
{
  if(shortPress)
  {
    // Clear stale parameters
    clearStationInfo();
    rssi = snr = 0;
    drawScreen();
    drawMessage("Scanning...");
    scanRun(currentFrequency, currentMode == FM ? 100000 : 10000);
  }
  else currentCmd = CMD_NONE;
}

void autoStoreAndReport(uint8_t flags)
{
  char text[16];

  // Clear stale parameters
  clearStationInfo();
  rssi = snr = 0;
  drawScreen();

  int stored = autoStoreRun(flags);

  switch(stored)
  {
    case ATS_BAD_MODE: sprintf(text, "FM/AM only"); break;
    case ATS_NO_SLOTS: sprintf(text, "Mem Full"); break;
    default:           sprintf(text, "Stored %d", stored); break;
  }

  drawMessage(text);
  delay(2000);
  identifyFrequency(currentFrequency);
}

static void clickAutoStore(bool shortPress)
{
  // Either kind of press runs the selected action, the way Update FW does
  if(autoStoreIdx) autoStoreAndReport(autoStoreIdx==2? ATS_REPLACE : ATS_KEEP_OLD);
  currentCmd = CMD_NONE;
}

static void clickMemScan(bool shortPress)
{
  // Any click parks the scanner on the current channel
  memScanStop();
  currentCmd = CMD_NONE;
}

//
// Tune to a frequency in Hz, switching bands as needed
//
bool tuneToFrequency(uint32_t freq, uint8_t mode)
{
  bool sameKind = (bands[bandIdx].bandMode==FM) == (mode==FM);
  int band = findBand(freq, mode, sameKind? bandIdx : -1);
  if(band<0) return(false);

  if(band==bandIdx && currentMode==mode)
  {
    softVolume(true);
    bool tuned = updateFrequency(freq, false);
    softVolume(false);
    clearStationInfo();
    identifyFrequency(currentFrequency);
    prefsRequestSave(SAVE_CUR_BAND);
    return(tuned);
  }

  Memory memory = {};
  memory.freq = freq;
  memory.band = band;
  memory.mode = mode;
  return(tuneToMemory(&memory));
}

//
// Stations menu: stored countries, then the stations of one country
//
static void enterStations()
{
  if(!stationsLocalCount())
  {
    drawMessage("No Lists");
    delay(1500);
    return;
  }

  stnLevel = 0;
  stnIdx = 1;
  currentCmd = CMD_STATIONS;
}

static void doStations(int16_t enc)
{
  if(stnLevel==0)
  {
    stnIdx = wrap_range(stnIdx, enc, 0, stationsLocalCount());
    return;
  }

  if(stnLevel==1)
  {
    stnIdx = wrap_range(stnIdx, enc, 0, stationsRegionsOpen());
    return;
  }

  stnIdx = wrap_range(stnIdx, enc, 0, stationsOpenCount());

  StationRecord record;
  if(stnIdx>0 && stationsOpenRecord(stnIdx - 1, &record))
    tuneToFrequency(record.freq * 10000UL, FM);
}

static void clickStations(bool shortPress)
{
  if(stnLevel==0)
  {
    StationCountry country;

    if(!stnIdx || !stationsLocalCountry(stnIdx - 1, &country))
      currentCmd = CMD_NONE;
    else if(shortPress)
    {
      // A short press deletes the list of the country
      char text[40];
      stationsLocalDelete(stnIdx - 1);
      snprintf(text, sizeof(text), "Deleted %s", country.code);
      drawMessage(text);
      delay(1500);
      stnIdx = min(stnIdx, stationsLocalCount());
      if(!stationsLocalCount()) currentCmd = CMD_NONE;
    }
    else if(stationsOpen(stnIdx - 1))
    {
      stnCountryIdx = stnIdx - 1;
      stnRegionIdx = -1;
      if(stationsRegionsOpen()>1)
      {
        // Pick the province first, a country is too long a list,
        // starting on the listener's province
        stnLevel = 1;
        stnIdx = 1;
        for(int i=0 ; i<stationsRegionsOpen() ; i++)
          if(!strcmp(stationsRegionOpenName(i), stationsHomeRegion())) stnIdx = i + 1;
      }
      else
      {
        stnLevel = 2;
        stnIdx = 1 + (currentMode==FM? stationsOpenFind(currentFrequency) : 0);
      }
    }
    return;
  }

  if(stnLevel==1)
  {
    if(!stnIdx)
    {
      stnLevel = 0;
      stnIdx = stnCountryIdx + 1;
    }
    else
    {
      stnRegionIdx = stnIdx - 1;
      stationsSelectRegion(stnRegionIdx);
      // The province picked here is where the listener is
      stationsSetHomeRegion(stationsRegionOpenName(stnRegionIdx));
      identifyFrequency(currentFrequency);
      stnLevel = 2;
      // Start on the station closest to the current frequency
      stnIdx = 1 + (currentMode==FM? stationsOpenFind(currentFrequency) : 0);
    }
    return;
  }

  if(!stnIdx)
  {
    // Back to the provinces, or to the countries when there are none
    if(stationsRegionsOpen()>1)
    {
      stnLevel = 1;
      stnIdx = stnRegionIdx + 1;
    }
    else
    {
      stnLevel = 0;
      stnIdx = stnCountryIdx + 1;
    }
  }
  else currentCmd = CMD_NONE;
}

//
// Load Stations menu: regions, then their countries
//
static void doLoadStations(int16_t enc)
{
  int count = loadLevel? stationsIndexCount(loadRegion) : stationsRegionCount();
  loadIdx = wrap_range(loadIdx, enc, 0, count);
}

static void clickLoadStations(bool shortPress)
{
  if(loadLevel==0)
  {
    if(!loadIdx) currentCmd = CMD_NONE;
    else
    {
      loadRegion = loadIdx - 1;
      loadLevel = 1;
      loadIdx = 1;
    }
    return;
  }

  if(!loadIdx)
  {
    loadLevel = 0;
    loadIdx = loadRegion + 1;
  }
  else stationsDownload(stationsIndexCountry(loadRegion, loadIdx - 1));
}

//
// RDS AF menu
//
static void enterAfList()
{
  if(currentMode!=FM)
  {
    drawMessage("FM only");
    delay(1000);
    return;
  }

  // Start on the frequency being received, when it is in the list
  afMenuIdx = afCount()? 1 : 0;
  for(int i=0 ; i<afCount() ; i++)
    if(afFrequency(i)==currentFrequency) afMenuIdx = i + 1;

  afMenuTuning = false;
  currentCmd = CMD_AFLIST;
}

static void doAfList(int16_t enc)
{
  afMenuIdx = wrap_range(afMenuIdx, enc, 0, afCount());

  // Once a frequency has been clicked, the encoder moves between them
  if(afMenuTuning && afMenuIdx>0 && afTune(afFrequency(afMenuIdx - 1)))
    prefsRequestSave(SAVE_CUR_BAND);
}

static void clickAfList(bool shortPress)
{
  if(!afMenuIdx)
  {
    afMenuTuning = false;
    currentCmd = CMD_NONE;
  }
  else if(!afMenuTuning)
  {
    afMenuTuning = true;
    if(afTune(afFrequency(afMenuIdx - 1))) prefsRequestSave(SAVE_CUR_BAND);
  }
  else afMenuTuning = false;
}

//
// RDS EON menu: other networks, then their frequencies
//
static void enterEonList()
{
  if(currentMode!=FM)
  {
    drawMessage("FM only");
    delay(1000);
    return;
  }

  eonLevel = 0;
  eonIdx = eonCount()? 1 : 0;
  eonTuning = false;
  currentCmd = CMD_EONLIST;
}

static void doEonList(int16_t enc)
{
  if(eonLevel==0)
  {
    eonIdx = wrap_range(eonIdx, enc, 0, eonCount());
    return;
  }

  eonIdx = wrap_range(eonIdx, enc, 0, eonSelected.count);
  if(eonTuning && eonIdx>0)
    tuneToFrequency(eonSelected.freq[eonIdx - 1], FM);
}

static void clickEonList(bool shortPress)
{
  if(eonLevel==0)
  {
    if(!eonIdx || !eonNetwork(eonIdx - 1, &eonSelected))
      currentCmd = CMD_NONE;
    else
    {
      eonLevel = 1;
      eonIdx = eonSelected.count? 1 : 0;
      eonTuning = false;
    }
    return;
  }

  if(!eonIdx)
  {
    eonLevel = 0;
    eonIdx = eonCount()? 1 : 0;
    eonTuning = false;
  }
  else if(!eonTuning)
  {
    eonTuning = true;
    tuneToFrequency(eonSelected.freq[eonIdx - 1], FM);
  }
  else eonTuning = false;
}

static void doTheme(int16_t enc)
{
  themeIdx = wrap_range(themeIdx, enc, 0, getTotalThemes() - 1);
  prefsRequestSave(SAVE_SETTINGS);
}

static void doUILayout(int16_t enc)
{
  uiLayoutIdx = uiLayoutIdx > LAST_ITEM(uiLayoutDesc) ? UI_DEFAULT : wrap_range(uiLayoutIdx, enc, 0, LAST_ITEM(uiLayoutDesc));
  prefsRequestSave(SAVE_SETTINGS);
}

void doAvc(int16_t enc)
{
  // Only allow for AM and SSB modes
  if(currentMode==FM) return;

  // wrap_range expects to wrap a range of incremental numbers. avc instead is a range of all even numbers
  int8_t newAvcIdx = wrap_range((isSSB() ? SsbAvcIdx : AmAvcIdx) / 2, enc, 12 / 2, 90 / 2) * 2;
  if(isSSB())
  {
    SsbAvcIdx = newAvcIdx;
    if(enc && ssbAvcHold)
    {
      ssbAvcHold = false;
      rx.setSSBAutomaticVolumeControl(1);
    }
  }
  else
  {
    AmAvcIdx = newAvcIdx;
  }
  rx.setAvcAmMaxGain(newAvcIdx);
  if(enc) prefsRequestSave(SAVE_SETTINGS);
}

static void clickAvc(bool shortPress)
{
  if(shortPress && isSSB())
  {
    ssbAvcHold = !ssbAvcHold;
    rx.setSSBAutomaticVolumeControl(!ssbAvcHold);
  }
  else currentCmd = CMD_NONE;
}

void doFmRegion(int16_t enc)
{
  FmRegionIdx = wrap_range(FmRegionIdx, enc, 0, LAST_ITEM(fmRegions));
  if(currentMode==FM)
    rx.setFMDeEmphasis(fmRegions[FmRegionIdx].value);
  if(enc) prefsRequestSave(SAVE_SETTINGS);
}

//
// Apply the stereo setting. The receiver blends down to mono on its
// own as the signal gets worse, so forcing mono is a matter of moving
// the blend thresholds out of reach. This has to run again after every
// band change, because the FM tuner starts up with the defaults.
//
void applyFmStereo()
{
  if(currentMode!=FM) return;

  if(fmStereoIdx==FM_STEREO_MONO)
    rx.setFmStereoOff();
  else if(dxMode)
    rx.setFmStereoDx();
  else
    rx.setFmStereoOn();
}

//
// Apply the DX mode to the current band: no soft mute on weak stations,
// an earlier blend to mono on FM, and seek thresholds low enough to stop
// on distant stations. Called after every band change.
//
void applyDxMode()
{
  if(currentMode==FM)
  {
    // Chip default is 16dB of soft mute on weak signals
    rx.setFmSoftMuteMaxAttenuation(dxMode? 0 : 16);
    rx.setSeekFmRssiThreshold(dxMode? 3 : 5);
    rx.setSeekFmSNRThreshold(dxMode? 1 : 2);
    applyFmStereo();
  }
  else if(currentMode==AM)
  {
    rx.setSeekAmRssiThreshold(dxMode? 5 : 10);
    rx.setSeekAmSNRThreshold(dxMode? 1 : 3);
    doSoftMute(0);
  }
}

void toggleDxMode()
{
  dxMode = !dxMode;
  applyDxMode();
  prefsRequestSave(SAVE_SETTINGS);
}

static void doDxMode() { toggleDxMode(); }

void doFmStereo(int16_t enc)
{
  fmStereoIdx = wrap_range(fmStereoIdx, enc, 0, LAST_ITEM(fmStereoDesc));
  applyFmStereo();
  if(enc) prefsRequestSave(SAVE_SETTINGS);
}

static void doAntennaCapacitor()
{
  autoAntennaCapacitor = !autoAntennaCapacitor;
  if(currentMode != FM) selectBand(bandIdx, false);
  prefsRequestSave(SAVE_SETTINGS);
}

static uint8_t dspPatchSlots(uint8_t slots[PATCH_SET_COUNT + 1], uint8_t &selected)
{
  uint8_t count = 1;
  slots[0] = DSP_PATCHES_DEFAULT;
  selected = 0;
  for(uint8_t slot = 1; slot <= PATCH_SET_COUNT; slot++)
    if(patchesModes(slot))
    {
      if(slot == dspPatchesIdx) selected = count;
      slots[count++] = slot;
    }
  return count;
}

static void doDspPatches(int16_t enc)
{
  uint8_t slots[PATCH_SET_COUNT + 1], selected;
  uint8_t count = dspPatchSlots(slots, selected);
  if(!patchesSelect(slots[wrap_range(selected, enc, 0, count - 1)]))
    statusShow("Patch request busy", "Try again when finished");
}

void doCal(int16_t enc)
{
  if (currentMode == USB)
    bands[bandIdx].usbCal = clamp_range(bands[bandIdx].usbCal, 10*enc, -MAX_CAL, MAX_CAL);
  else if (currentMode == LSB)
    bands[bandIdx].lsbCal = clamp_range(bands[bandIdx].lsbCal, 10*enc, -MAX_CAL, MAX_CAL);
  // else: no calibration change for other modes

  // If in SSB mode set the SI4732/5 BFO value
  // This adjusts the BFO while in the calibration menu
  if(isSSB())
  {
    updateFrequency(currentFrequency, true);
    if(enc) prefsRequestSave(SAVE_CUR_BAND);
  }
}

void doBrt(int16_t enc)
{
  currentBrt = clamp_range(currentBrt, 5*enc, 10, 255);
  if(!sleepOn()) ledcWrite(PIN_LCD_BL, currentBrt);
  if(enc) prefsRequestSave(SAVE_SETTINGS);
}

static void doSleep(int16_t enc)
{
  currentSleep = clamp_range(currentSleep, 5*enc, 0, 255);
  prefsRequestSave(SAVE_SETTINGS);
}

static void doOffTimer(int16_t enc)
{
  if(!enc) return;
  offTimerMinutes = clamp_range(offTimerMinutes, 5 * enc, 0, 240);
  offTimerRestart();
  prefsRequestSave(SAVE_SETTINGS);
}

static void doSleepMode(int16_t enc)
{
  sleepModeIdx = wrap_range(sleepModeIdx, enc, 0, LAST_ITEM(sleepModeDesc));
  prefsRequestSave(SAVE_SETTINGS);
}

static void doUSBMode(int16_t enc)
{
  usbModeIdx = wrap_range(usbModeIdx, enc, 0, LAST_ITEM(usbModeDesc));
  prefsRequestSave(SAVE_SETTINGS);
}

static void doTCPMode(int16_t enc)
{
  tcpModeIdx = wrap_range(tcpModeIdx, enc, 0, LAST_ITEM(tcpModeDesc));
  prefsRequestSave(SAVE_SETTINGS);
}

static void doBleMode(int16_t enc)
{
  bleModeMenuIdx = wrap_range(bleModeMenuIdx, enc, 0, LAST_ITEM(bleModeDesc));
}

static void doWiFiMode(int16_t enc)
{
  wifiModeIdx = wrap_range(wifiModeIdx, enc, 0, LAST_ITEM(wifiModeDesc));
  prefsRequestSave(SAVE_SETTINGS);
}

static void clickBleMode(uint8_t mode, bool shortPress)
{
  currentCmd = CMD_NONE;
  bleModeIdx = mode;
  prefsRequestSave(SAVE_SETTINGS);
  bleInit(mode);
}

static void clickWiFiMode(uint8_t mode, bool shortPress)
{
  currentCmd = CMD_NONE;
  netInit(mode);
}

void doRDSMode(int16_t enc)
{
  rdsModeIdx = wrap_range(rdsModeIdx, enc, 0, LAST_ITEM(rdsMode));
  if(currentMode==FM) rx.setRdsConfig(getRDSMode()!=RDS_OFF, 2, 2, 2, 2);
  if(!(getRDSMode() & RDS_CT) && !ntpIsAvailable()) clockReset();
  prefsRequestSave(SAVE_SETTINGS);
}

static void doUTCOffset(int16_t enc)
{
  utcOffsetIdx = wrap_range(utcOffsetIdx, enc, 0, LAST_ITEM(utcOffsets));
  prefsRequestSave(SAVE_SETTINGS);
}

static void doZoom(int16_t enc)
{
  zoomMenu = !zoomMenu;
  prefsRequestSave(SAVE_SETTINGS);
}

static void doEncoderAcceleration()
{
  encoderAcceleration = !encoderAcceleration;
  prefsRequestSave(SAVE_SETTINGS);
}

static void doScrollDir(int16_t enc)
{
  scrollDirection = (scrollDirection == 1) ? -1 : 1;
  prefsRequestSave(SAVE_SETTINGS);
}

uint8_t doAbout(int16_t enc)
{
  static uint8_t aboutScreen = 0;
  aboutScreen = clamp_range(aboutScreen, enc, 0, 2);
  return aboutScreen;
}

bool tuneToMemory(const Memory *memory)
{
  // Must have frequency
  if(!memory->freq) return(false);

  // Must have valid band index
  if(memory->band>=getTotalBands()) return(false);

  // Band must contain frequency and modulation
  if(!isMemoryInBand(&bands[memory->band], memory)) return(false);

  // Must differ from the current band, frequency and modulation
  if(memory->band==bandIdx && memory->freq==bands[bandIdx].currentFreq && memory->mode==bands[bandIdx].bandMode)
    return(true);

  // Within the same band and mode, a plain retune is enough and avoids
  // reconfiguring the receiver, which thumps the speaker at every step
  if(memory->band==bandIdx && memory->mode==currentMode && !isSSB())
  {
    softVolume(true);
    bool tuned = updateFrequency(memory->freq, false);
    softVolume(false);
    clearStationInfo();
    identifyFrequency(currentFrequency);
    prefsRequestSave(SAVE_CUR_BAND);
    return(tuned);
  }

  // Use default step when changing modes
  if(bands[memory->band].bandMode != memory->mode)
    bands[memory->band].currentStepIdx = defaultStepIdx[memory->mode];

  // Load frequency and modulation from memory slot
  bands[memory->band].currentFreq = memory->freq;
  bands[memory->band].bandMode    = memory->mode;

  // Enable the new band
  selectBand(memory->band);

  // Switching bands must also save pending edits to the previous band.
  prefsRequestSave(SAVE_SETTINGS | SAVE_BANDS);
  return(true);
}

// The active VFO follows live tuning; capture it before saving or switching.
VfoState vfos[2] = {};
uint8_t vfoIdx = 0;
bool vfoInitialized = false;

VfoState captureVfo()
{
  const Band *band = getCurrentBand();
  return {
    currentFrequency,
    (uint8_t)bandIdx, currentMode, band->currentStepIdx, getCurrentBandwidthIdx()
  };
}

VfoState getVfo(uint8_t index)
{
  return index == vfoIdx ? captureVfo() : vfos[index];
}

static bool validVfo(const VfoState &vfo)
{
  if(vfo.band>=getTotalBands() || vfo.mode>=getTotalModes()) return(false);
  const Band &band = bands[vfo.band];
  if((vfo.mode==FM) != (band.bandType==FM_BAND_TYPE)) return(false);
  if(!isFreqInBand(&band, vfo.freq)) return(false);
  return(vfo.stepIdx>=0 && vfo.stepIdx<=getLastStep(vfo.mode) &&
         vfo.bandwidthIdx>=0 && vfo.bandwidthIdx<=getLastBandwidth(vfo.mode));
}

static void restoreVfo(bool drawLoadingPatch = true)
{
  const VfoState &vfo = vfos[vfoIdx];
  Band &band = bands[vfo.band];
  band.currentFreq = vfo.freq;
  band.bandMode = vfo.mode;
  band.currentStepIdx = vfo.stepIdx;
  band.bandwidthIdx = vfo.bandwidthIdx;
  selectBand(vfo.band, drawLoadingPatch);
}

void initVfo()
{
  if(vfoIdx<2 && validVfo(vfos[0]) && validVfo(vfos[1]))
    restoreVfo(false);
  else
  {
    selectBand(bandIdx, false);
    vfoIdx = 0;
    vfos[0] = vfos[1] = captureVfo();
    prefsRequestSave(SAVE_CUR_BAND);
  }
  vfoInitialized = true;
}

bool selectVfo(uint8_t index)
{
  if(index > 1) return false;
  if(index == vfoIdx) return true;
  vfos[vfoIdx] = captureVfo();
  vfoIdx = index;
  restoreVfo();
  prefsRequestSave(SAVE_SETTINGS | SAVE_BANDS);
  return true;
}

bool setVfoFrequency(uint8_t index, uint32_t freq)
{
  if(index > 1) return false;
  const VfoState previous = getVfo(index);
  VfoState vfo = previous;
  const int band = findBand(freq, vfo.mode, vfo.band);
  if(band < 0) return false;
  vfo.band = band;
  if((vfo.mode == FM) != (bands[vfo.band].bandType == FM_BAND_TYPE))
  {
    vfo.mode = bands[vfo.band].defaultMode;
    vfo.stepIdx = defaultStepIdx[vfo.mode];
    vfo.bandwidthIdx = defaultBwIdx[vfo.mode];
  }
  // Match hardware resolution even when updating the inactive VFO.
  const uint32_t resolution = vfo.mode == FM ? 10000 : vfo.mode == AM ? 1000 : 1;
  vfo.freq = freq / resolution * resolution;
  if(!validVfo(vfo)) return false;
  if(vfo.freq == previous.freq && vfo.band == previous.band && vfo.mode == previous.mode &&
     vfo.stepIdx == previous.stepIdx && vfo.bandwidthIdx == previous.bandwidthIdx) return true;

  if(index != vfoIdx)
  {
    vfos[index] = vfo;
    prefsRequestSave(SAVE_CUR_BAND);
    return true;
  }
  if(vfo.band == bandIdx)
  {
    if(!updateFrequency(vfo.freq, false)) return false;
    clearStationInfo();
    identifyFrequency(currentFrequency);
    vfos[index] = captureVfo();
    prefsRequestSave(SAVE_CUR_BAND);
    return true;
  }

  vfos[index] = vfo;
  restoreVfo();
  prefsRequestSave(SAVE_SETTINGS | SAVE_BANDS);
  return true;
}

bool setMode(uint8_t mode)
{
  if(mode >= getTotalModes() || ((mode == FM) != (currentMode == FM))) return false;
  if(mode == currentMode) return true;
  bands[bandIdx].currentStepIdx = defaultStepIdx[mode];
  bands[bandIdx].bandwidthIdx = defaultBwIdx[mode];
  bands[bandIdx].bandMode = mode;
  selectBand(bandIdx);
  prefsRequestSave(SAVE_CUR_BAND);
  return true;
}

void copyVfo()
{
  vfos[vfoIdx] = captureVfo();
  vfos[vfoIdx ^ 1] = vfos[vfoIdx];
  // Save both bands: they may contain pending edits from before the switch.
  prefsRequestSave(SAVE_SETTINGS | SAVE_BANDS);
}

static void clickVfo(bool shortPress)
{
  if(shortPress) copyVfo();
  else selectVfo(vfoIdx ^ 1);
}

static void doMemory(int16_t enc)
{
  memoryIdx = wrap_range(memoryIdx, enc, 0, getTotalMemories() - 1);
  Memory memory = getMemory(memoryIdx);
  if(!tuneToMemory(&memory)) tuneToMemory(&newMemory);
}

static void clickMemory(uint8_t idx, bool shortPress)
{
  // Must have a valid index
  if(idx>=getTotalMemories()) return;

  if(shortPress)
  {
    // If clicking on an empty memory slot, save to it
    Memory memory = getMemory(idx);
    if(!memory.freq) memory = newMemory;
    // Otherwise, delete memory slot contents
    else memory.freq = 0;
    setMemory(idx, memory);
    prefsRequestSave(SAVE_MEMORIES);
  }
  // On a click, do nothing, slot already activated in doMemory()
  else currentCmd = CMD_NONE;
}

void doStep(int16_t enc)
{
  uint8_t idx = bands[bandIdx].currentStepIdx;

  idx = wrap_range(idx, enc, 0, getLastStep(currentMode));
  bands[bandIdx].currentStepIdx = idx;

  rx.setFrequencyStep(isSSB()? 0 : freqFromHz(steps[currentMode][idx].step, currentMode));

  // Set seek spacing
  if(currentMode==FM)
    rx.setSeekFmSpacing(steps[currentMode][idx].spacing);
  else
    rx.setSeekAmSpacing(steps[currentMode][idx].spacing);
  if(enc) prefsRequestSave(SAVE_CUR_BAND);
}

void doAgc(int16_t enc)
{
  if(currentMode==FM)
    agcIdx = FmAgcIdx = wrap_range(FmAgcIdx, enc, 0, 27);
  else if(isSSB())
    agcIdx = SsbAgcIdx = wrap_range(SsbAgcIdx, enc, 0, 1);
  else
    agcIdx = AmAgcIdx = wrap_range(AmAgcIdx, enc, 0, 37);

  // Process agcIdx to generate disableAgc and agcIdx
  // agcIdx     0 1 2 3 4 5 6  ..... n    (n:    FM = 27, AM = 37, SSB = 1)
  // agcNdx     0 0 1 2 3 4 5  ..... n -1 (n -1: FM = 26, AM = 36, SSB = 0)
  // disableAgc 0 1 1 1 1 1 1  ..... 1

  // if true, disable AGC; else, AGC is enabled
  disableAgc = agcIdx>0? 1 : 0;
  agcNdx     = agcIdx>1? agcIdx - 1 : 0;

  // Configure SI4732/5 (if agcNdx = 0, no attenuation)
  rx.setAutomaticGainControl(disableAgc, agcNdx);
  if(enc) prefsRequestSave(SAVE_SETTINGS);
}

void doMode(int16_t enc)
{
  // Cannot change away from FM mode
  if(currentMode==FM) return;

  // Change AM/LSB/USB modes, do not allow FM mode
  uint8_t mode = currentMode;
  do
    mode = wrap_range(mode, enc, 0, LAST_ITEM(bandModeDesc));
  while(mode==FM);
  setMode(mode);
}

void doSquelch(int16_t enc)
{
  uint8_t squelchParam = currentSquelch[currentMode] & 0x80;
  uint8_t squelchValue = currentSquelch[currentMode] & 0x7f;
  currentSquelch[currentMode] = squelchParam | clamp_range(squelchValue, enc, 0, 0x7f);
  if(enc) prefsRequestSave(SAVE_SETTINGS);
}

void doSoftMute(int16_t enc)
{
  // Nothing to do if FM mode
  if(currentMode==FM) return;

  if(isSSB())
    softMuteMaxAttIdx = SsbSoftMuteIdx = wrap_range(SsbSoftMuteIdx, enc, 0, 32);
  else
    softMuteMaxAttIdx = AmSoftMuteIdx = wrap_range(AmSoftMuteIdx, enc, 0, 32);

  // DX mode lets weak AM stations through at full volume
  rx.setAmSoftMuteMaxAttenuation(dxMode && !isSSB()? 0 : softMuteMaxAttIdx);
  if(enc) prefsRequestSave(SAVE_SETTINGS);
}

void doBand(int16_t enc)
{
  // Save current band settings
  bands[bandIdx].bandMode = currentMode;

  // Change band
  bandIdx = wrap_range(bandIdx, enc, 0, LAST_ITEM(bands));

  // Enable the new band
  selectBand(bandIdx);
  prefsRequestSave(SAVE_SETTINGS | SAVE_BANDS);
}

//
// Switch to a broadcast range: FM, MW, SW or LW. There is no LW band in
// the band table, long wave lives at the bottom of MW1.
//
bool selectBroadcastBand(const char *range)
{
  const char *name = !strcmp(range, "FM")? "VHF" : !strcmp(range, "MW")? "MW2" :
                     !strcmp(range, "SW")? "ALL" : !strcmp(range, "LW")? "MW1" : nullptr;
  if(!name) return(false);

  for(int i=0 ; i<getTotalBands() ; i++)
    if(!strcmp(bands[i].bandName, name))
    {
      if(i!=bandIdx) doBand(i - bandIdx);
      if(!strcmp(range, "LW") && (currentMode==FM || currentFrequency>=520000))
        tuneToFrequency(198000, AM);
      return(true);
    }

  return(false);
}

void doBandwidth(int16_t enc)
{
  uint8_t idx = bands[bandIdx].bandwidthIdx;

  idx = wrap_range(idx, enc, 0, getLastBandwidth(currentMode));
  bands[bandIdx].bandwidthIdx = idx;
  setBandwidth();
  if(enc) prefsRequestSave(SAVE_CUR_BAND);
}

//
// Handle encoder input in menu
//

static void doMenu(int16_t enc)
{
  menuIdx = wrap_range(menuIdx, enc, 0, LAST_ITEM(menu));
}

static void clickMenu(int cmd, bool shortPress)
{
  // No command yet
  currentCmd = CMD_NONE;

  switch(cmd)
  {
    case MENU_STEP:     currentCmd = CMD_STEP;      break;
    case MENU_SEEK:     currentCmd = CMD_SEEK;      break;
    case MENU_MODE:     currentCmd = CMD_MODE;      break;
    case MENU_BW:       currentCmd = CMD_BANDWIDTH; break;
    case MENU_AGC_ATT:  currentCmd = CMD_AGC;       break;
    case MENU_BAND:     currentCmd = CMD_BAND;      break;
    case MENU_SETTINGS: currentCmd = CMD_SETTINGS;  break;
    case MENU_SQUELCH:  currentCmd = CMD_SQUELCH;   break;
    case MENU_VOLUME:   currentCmd = CMD_VOLUME;    break;
    case MENU_VFO:      clickVfo(shortPress);       break;

    case MENU_MEMORY:
      currentCmd = CMD_MEMORY;
      newMemory.freq  = currentFrequency;
      newMemory.mode  = currentMode;
      newMemory.band  = bandIdx;
      doMemory(0);
      break;

    case MENU_SOFTMUTE:
      // No soft mute in FM mode
      if(currentMode!=FM) currentCmd = CMD_SOFTMUTE;
      break;

    case MENU_AVC:
      // No AVC in FM mode
      if(currentMode!=FM) currentCmd = CMD_AVC;
      break;

    case MENU_SCAN:
      // Run a band scan around current frequency with the same
      // step as scale resolution (10kHz for AM, 100kHz for FM)
      currentCmd = CMD_SCAN;
      clickScan(true);
      break;

    case MENU_MEMSCAN:
      // There has to be something to scan first
      if(memScanStart())
        currentCmd = CMD_MEMSCAN;
      else
      {
        drawMessage("No Memory");
        delay(1000);
      }
      break;

    case MENU_AUTOSTORE:
      // Start on the action that leaves the stored stations alone
      autoStoreIdx = 1;
      currentCmd = CMD_AUTOSTORE;
      break;

    case MENU_STATIONS: enterStations(); break;
    case MENU_DXWATCH:
      // Say what it costs before it starts
      dxwIdx = 1;
      statusShow("DX Watch may drain the battery.", "Plugging in to USB is recommended.", 0);
      currentCmd = CMD_DXWATCH;
      break;
    case MENU_AFLIST:   enterAfList();   break;
    case MENU_EONLIST:  enterEonList();  break;
  }
}

static void doSettings(int16_t enc)
{
  settingsIdx = wrap_range(settingsIdx, enc, 0, visibleSettings() - 1);
}

static void clickSettings(int cmd, bool shortPress)
{
  // No command yet
  currentCmd = CMD_NONE;

  switch(cmd)
  {
    case MENU_BRIGHTNESS: currentCmd = CMD_BRT; break;
    case MENU_CALIBRATION:
      if(isSSB()) currentCmd = CMD_CAL;
      break;
    case MENU_THEME:      currentCmd = CMD_THEME;      break;
    case MENU_UI:         currentCmd = CMD_UI;         break;
    case MENU_RDS:        currentCmd = CMD_RDS;        break;
    case MENU_DATETIME:
      dateTimeInit();
      currentCmd = CMD_DATETIME;
      break;
    case MENU_ZOOM:       currentCmd = CMD_ZOOM;       break;
    case MENU_ENC_ACCEL:  currentCmd = CMD_ENC_ACCEL;  break;
    case MENU_SCROLL:     currentCmd = CMD_SCROLL;     break;
    case MENU_OFFTIMER:
      if(shortPress) receiverShutdown();
      else currentCmd = CMD_OFFTIMER;
      break;
    case MENU_SLEEP:      currentCmd = CMD_SLEEP;      break;
    case MENU_SLEEPMODE:  currentCmd = CMD_SLEEPMODE;  break;
    case MENU_UTCOFFSET:  currentCmd = CMD_UTCOFFSET;  break;
    case MENU_USBMODE:    currentCmd = CMD_USBMODE;    break;
    case MENU_TCPMODE:    currentCmd = CMD_TCPMODE;    break;
    case MENU_BLEMODE:
      bleModeMenuIdx = bleModeIdx;
      currentCmd = CMD_BLEMODE;
      break;
    case MENU_WIFIMODE:   currentCmd = CMD_WIFIMODE;   break;
    case MENU_FM_REGION:  currentCmd = CMD_FM_REGION; break;
    case MENU_FM_STEREO:  currentCmd = CMD_FM_STEREO; break;
    case MENU_ANTCAP:     currentCmd = CMD_ANTCAP;    break;
    case MENU_DSP_PATCHES: currentCmd = CMD_DSP_PATCHES; break;
    case MENU_ABOUT:
      aboutPresses = 0;
      currentCmd = CMD_ABOUT;
      break;
    case MENU_DEBUG:
      debugIdx = 1;
      debugEdit = 0;
      currentCmd = CMD_DEBUG;
      break;
    case MENU_UPDATEFW:
      updateFwIdx = 0;
      currentCmd = CMD_UPDATEFW;
      break;

    case MENU_LOADEIBI:
      eibiLoadSchedule();
      break;

    case MENU_LOADSTN:
      // The list of countries comes from the repository first
      if(stationsLoadIndex())
      {
        loadLevel = 0;
        loadIdx = 1;
        currentCmd = CMD_LOADSTN;
      }
      break;

    case MENU_DXMODE:     currentCmd = CMD_DXMODE;    break;
  }
}

bool doSideBar(uint16_t cmd, int16_t enc, int16_t enca)
{
  // Ignore idle encoder
  if(!enc) return(false);

  switch(cmd)
  {
    // Menus and list-based options must take scrollDirection into account
    case CMD_MENU:       doMenu(scrollDirection * enc);break;
    case CMD_MODE:       doMode(scrollDirection * enc);break;
    case CMD_STEP:       doStep(scrollDirection * enc);break;
    case CMD_AGC:        doAgc(enc);break;
    case CMD_BANDWIDTH:  doBandwidth(scrollDirection * enc);break;
    case CMD_VOLUME:     doVolume(enca);break;
    case CMD_SOFTMUTE:   doSoftMute(enc);break;
    case CMD_BAND:       doBand(scrollDirection * enc);break;
    case CMD_AVC:        doAvc(enc);break;
    case CMD_FM_REGION:  doFmRegion(scrollDirection * enc);break;
    case CMD_FM_STEREO:  doFmStereo(scrollDirection * enc);break;
    case CMD_ANTCAP:     doAntennaCapacitor();break;
    case CMD_DSP_PATCHES: doDspPatches(scrollDirection * enc);break;
    case CMD_SETTINGS:   doSettings(scrollDirection * enc);break;
    case CMD_BRT:        doBrt(enca);break;
    case CMD_CAL:        doCal(enca);break;
    case CMD_THEME:      doTheme(scrollDirection * enc);break;
    case CMD_UI:         doUILayout(scrollDirection * enc);break;
    case CMD_RDS:        doRDSMode(scrollDirection * enc);break;
    case CMD_MEMORY:     doMemory(scrollDirection * enca);break;
    case CMD_OFFTIMER:   doOffTimer(enca);break;
    case CMD_SLEEP:      doSleep(enca);break;
    case CMD_SLEEPMODE:  doSleepMode(scrollDirection * enc);break;
    case CMD_USBMODE:    doUSBMode(scrollDirection * enc);break;
    case CMD_TCPMODE:    doTCPMode(scrollDirection * enc);break;
    case CMD_BLEMODE:    doBleMode(scrollDirection * enc);break;
    case CMD_WIFIMODE:   doWiFiMode(scrollDirection * enc);break;
    case CMD_ZOOM:       doZoom(enc);break;
    case CMD_ENC_ACCEL:  doEncoderAcceleration();break;
    case CMD_DXMODE:     doDxMode();break;
    case CMD_DEBUG:      doDebug(scrollDirection * enc, scrollDirection * enca);break;
    case CMD_DXWATCH:
      if(dxWatchPhase()==DXW_OFF)
        dxwIdx = wrap_range(dxwIdx, scrollDirection * enc, 0, LAST_ITEM(dxwActions));
      else
      {
        // Taking over the tuning ends the watch, a catch keeps playing
        dxWatchStop(dxWatchPhase()==DXW_CATCH);
        currentCmd = CMD_NONE;
      }
      break;
    case CMD_AUTOSTORE:  autoStoreIdx = wrap_range(autoStoreIdx, scrollDirection * enc, 0, LAST_ITEM(autoStoreActions));break;
    // Taking over the tuning cancels the memory scan
    case CMD_MEMSCAN:    memScanStop();currentCmd = CMD_NONE;break;
    case CMD_STATIONS:   doStations(scrollDirection * (stnLevel==2? enca : enc));break;
    case CMD_LOADSTN:    doLoadStations(scrollDirection * (loadLevel? enca : enc));break;
    case CMD_AFLIST:     doAfList(scrollDirection * enc);break;
    case CMD_EONLIST:    doEonList(scrollDirection * enc);break;
    case CMD_SCROLL:     doScrollDir(enc);break;
    case CMD_UTCOFFSET:  doUTCOffset(scrollDirection * enc);break;
    case CMD_DATETIME:   doDateTime(enc);break;
    case CMD_SQUELCH:    doSquelch(enca);break;
    case CMD_UPDATEFW:   updateFwIdx = wrap_range(updateFwIdx, scrollDirection * enc, 0, LAST_ITEM(updateFwActions));break;
    case CMD_ABOUT:      doAbout(enc);break;
    default:             return(false);
  }

  // Encoder input handled
  return(true);
}

bool clickHandler(uint16_t cmd, bool shortPress)
{
  switch(cmd)
  {
    case CMD_MENU:     clickMenu(menuIdx, shortPress);break;
    case CMD_SETTINGS: clickSettings(settingsIdx, shortPress);break;
    case CMD_UPDATEFW: otaRequestLatest(updateFwIdx == 1);break;
    case CMD_MEMORY:   clickMemory(memoryIdx, shortPress);break;
    case CMD_BLEMODE:  clickBleMode(bleModeMenuIdx, shortPress);break;
    case CMD_WIFIMODE: clickWiFiMode(wifiModeIdx, shortPress);break;
    case CMD_OFFTIMER:
      if(shortPress)
      {
        offTimerMinutes = 0;
        offTimerRestart();
        prefsRequestSave(SAVE_SETTINGS);
      }
      else currentCmd = CMD_NONE;
      break;
    case CMD_VOLUME:   clickVolume(shortPress);break;
    case CMD_AVC:      clickAvc(shortPress);break;
    case CMD_SQUELCH:  clickSquelch(shortPress);break;
    case CMD_SEEK:     clickSeek(shortPress);break;
    case CMD_SCAN:     clickScan(shortPress);break;
    case CMD_MEMSCAN:  clickMemScan(shortPress);break;
    case CMD_AUTOSTORE:clickAutoStore(shortPress);break;
    case CMD_STATIONS: clickStations(shortPress);break;
    case CMD_LOADSTN:  clickLoadStations(shortPress);break;
    case CMD_AFLIST:   clickAfList(shortPress);break;
    case CMD_EONLIST:  clickEonList(shortPress);break;
    case CMD_DEBUG:    clickDebug(shortPress);break;
    case CMD_ABOUT:
      // Three short presses toggle debug mode, a click leaves as before
      if(!shortPress) return(false);
      if(++aboutPresses >= 3)
      {
        aboutPresses = 0;
        debugSetMode(!debugMode);
        drawMessage(debugMode? "Debug On" : "Debug Off");
        delay(1000);
      }
      break;
    case CMD_DXWATCH:
      if(dxWatchPhase()==DXW_OFF)
      {
        statusShow(nullptr);
        if(dxwIdx==1) dxWatchStart();
        if(dxWatchPhase()==DXW_OFF) currentCmd = CMD_NONE;
      }
      else
      {
        dxWatchStop(dxWatchPhase()==DXW_CATCH);
        currentCmd = CMD_NONE;
      }
      break;
    case CMD_FREQ:     return(clickFreq(shortPress));
    case CMD_DATETIME: clickDateTime(shortPress);break;
    default:           return(false);
  }

  // Encoder input handled
  return(true);
}

//
// Selecting given band
//

void selectBand(uint8_t idx, bool drawLoadingPatch)
{
  // Silence clicks during reconfiguration using the configured hardware mute method.
  // https://github.com/esp32-si4732/ats-mini/discussions/103
  muteOn(MUTE_TEMP, true, !audioClamp);

  // Set band and mode
  bandIdx = min(idx, LAST_ITEM(bands));
  currentMode = bands[bandIdx].bandMode;

  // Load the selected DSP patch as needed
  loadDSPPatch(getCurrentBandwidth()->idx, drawLoadingPatch);

  // Switch radio to the selected band
  useBand(&bands[bandIdx]);

  // Set bandwidth for the current mode
  setBandwidth();

  // Clear current station info (RDS/CB)
  clearStationInfo();

  // Check for named frequencies
  identifyFrequency(currentFrequency);

  // Set default digit position based on the current step
  resetFreqInputPos();

  muteOn(MUTE_TEMP, false, !audioClamp);
}

//
// Draw functions
//

static void drawCommon(const char *title, int x, int y, int sx, bool cursor = false)
{
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_hdr);
  spr.fillRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_bg);
  spr.drawRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_border);

  spr.drawString(title, 40+x+(sx/2), 12+y, FONT_SMALL);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setFont(FONT_DEFAULT);
  spr.setTextColor(TH.menu_item);
  if(cursor)
    spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);
}

static void drawMenu(int x, int y, int sx)
{
  spr.setTextDatum(MC_DATUM);

  spr.fillRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_bg);
  spr.drawRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_border);
  spr.setTextColor(TH.menu_hdr);

  spr.drawString("Menu", 40+x+(sx/2), 12+y, FONT_SMALL);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setFont(FONT_DEFAULT);
  spr.setTextColor(TH.menu_item);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);

  int count = ITEM_COUNT(menu);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(menu[abs((menuIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }
    spr.setTextDatum(MC_DATUM);
    spr.drawString(menu[abs((menuIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawSettings(int x, int y, int sx)
{
  spr.setTextDatum(MC_DATUM);

  spr.fillRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_bg);
  spr.drawRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.menu_border);
  spr.setTextColor(TH.menu_hdr);
  spr.drawString("Settings", 40+x+(sx/2), 12+y, FONT_SMALL);
  spr.drawLine(1+x, 23+y, 76+sx, 23+y, TH.menu_border);

  spr.setFont(FONT_DEFAULT);
  spr.setTextColor(TH.menu_item);
  spr.fillRoundRect(6+x, 24+y+(2*16), 66+sx, 16, 2, TH.menu_hl_bg);

  int count = visibleSettings();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(settings[abs((settingsIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(settings[abs((settingsIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawMode(int x, int y, int sx)
{
  drawCommon(menu[MENU_MODE], x, y, sx, true);

  int count = ITEM_COUNT(bandModeDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bandModeDesc[abs((currentMode+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    if((currentMode!=FM) || (i==0))
     spr.drawString(bandModeDesc[abs((currentMode+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawStep(int x, int y, int sx)
{
  int count = getLastStep(currentMode) + 1;
  int idx   = bands[bandIdx].currentStepIdx + count;

  drawCommon(menu[MENU_STEP], x, y, sx, true);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(steps[currentMode][abs((idx+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(steps[currentMode][abs((idx+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawSeek(int x, int y, int sx)
{
  drawCommon(menu[MENU_SEEK], x, y, sx);
  spr.fillArc(40+x+(sx/2), 66+y, 30, 27, 135, 270, TH.menu_param);
  spr.fillTriangle(40+x+(sx/2)-5, 66+y-32, 40+x+(sx/2)+5, 66+y-27, 40+x+(sx/2)-5, 66+y-22, TH.menu_param);
  spr.fillArc(40+x+(sx/2), 66+y, 30, 27, 315, 450, TH.menu_param);
  spr.fillTriangle(40+x+(sx/2)+5, 66+y+32, 40+x+(sx/2)-5, 66+y+27, 40+x+(sx/2)+5, 66+y+22, TH.menu_param);

  if(seekMode()==SEEK_SCHEDULE)
  {
    spr.drawCircle(40+x+(sx/2), 66+y, 10, TH.menu_param);
    spr.drawLine(40+x+(sx/2), 66+y, 40+x+(sx/2), 66+y-7, TH.menu_param);
    spr.drawLine(40+x+(sx/2), 66+y, 40+x+(sx/2)+4, 66+y+4, TH.menu_param);
  }
  else if(seekMode()==SEEK_AUTO)
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(TH.menu_param);
    spr.drawString("Auto", 40+x+(sx/2), 66+y, FONT_SMALL);
  }
}

static void drawScan(int x, int y, int sx)
{
  drawCommon(menu[MENU_SCAN], x, y, sx);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.scan_rssi);
  spr.drawString("S", 40+x+(sx/2)-30, 66+y+30, FONT_SMALL);
  spr.setTextColor(TH.scan_snr);
  spr.drawString("N", 40+x+(sx/2)+30, 66+y+30, FONT_SMALL);

  spr.fillArc(40+x+(sx/2), 66+y, 30, 27, 135, 270, TH.menu_param);
  spr.fillTriangle(40+x+(sx/2)-5, 66+y-32, 40+x+(sx/2)+5, 66+y-27, 40+x+(sx/2)-5, 66+y-22, TH.menu_param);
  spr.fillArc(40+x+(sx/2), 66+y, 30, 27, 315, 450, TH.menu_param);
  spr.fillTriangle(40+x+(sx/2)+5, 66+y+32, 40+x+(sx/2)-5, 66+y+27, 40+x+(sx/2)+5, 66+y+22, TH.menu_param);

  spr.drawLine(40+x+(sx/2)-17, 66+y+5, 40+x+(sx/2)-4, 66+y+5, TH.menu_param);
  spr.drawLine(40+x+(sx/2)-4, 66+y+5, 40+x+(sx/2), 66+y-16+5, TH.menu_param);
  spr.drawLine(40+x+(sx/2), 66+y-16+5, 40+x+(sx/2)+4, 66+y+5, TH.menu_param);
  spr.drawLine(40+x+(sx/2)+4, 66+y+5, 40+x+(sx/2)+17, 66+y+5, TH.menu_param);
}

static void drawBand(int x, int y, int sx)
{
  drawCommon(menu[MENU_BAND], x, y, sx, true);

  int count = ITEM_COUNT(bands);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bands[abs((bandIdx+count+i)%count)].bandName);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(bands[abs((bandIdx+count+i)%count)].bandName, 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawBandwidth(int x, int y, int sx)
{
  int count = getLastBandwidth(currentMode) + 1;
  int idx   = bands[bandIdx].bandwidthIdx + count;

  drawCommon(menu[MENU_BW], x, y, sx, true);

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bandwidths[currentMode][abs((idx+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(bandwidths[currentMode][abs((idx+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawSleepMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_SLEEPMODE], x, y, sx, true);

  int count = ITEM_COUNT(sleepModeDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(sleepModeDesc[abs((sleepModeIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(sleepModeDesc[abs((sleepModeIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawUSBMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_USBMODE], x, y, sx, true);

  int count = ITEM_COUNT(usbModeDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(usbModeDesc[abs((usbModeIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((usbModeIdx+i) < 0 || (usbModeIdx+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(usbModeDesc[abs((usbModeIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawTCPMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_TCPMODE], x, y, sx, true);

  int count = ITEM_COUNT(tcpModeDesc);
  for(int i=0 ; i<count ; i++)
  {
    if(i == tcpModeIdx)
    {
      drawZoomedMenu(tcpModeDesc[i]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    }
    else
      spr.setTextColor(TH.menu_item);

    spr.setTextDatum(MC_DATUM);
    spr.drawString(tcpModeDesc[i], 40+x+(sx/2), 64+y+((i-tcpModeIdx)*16), FONT_SMALL);
  }
}

static void drawBleMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_BLEMODE], x, y, sx, true);

  int count = ITEM_COUNT(bleModeDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(bleModeDesc[abs((bleModeMenuIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((bleModeMenuIdx+i) < 0 || (bleModeMenuIdx+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(bleModeDesc[abs((bleModeMenuIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawUpdateFW(int x, int y, int sx)
{
  drawCommon(settings[MENU_UPDATEFW], x, y, sx, true);

  for(int i=0 ; i<ITEM_COUNT(updateFwActions) ; i++)
  {
    if(i == updateFwIdx)
    {
      drawZoomedMenu(updateFwActions[i]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    }
    else
      spr.setTextColor(TH.menu_item);

    spr.setTextDatum(MC_DATUM);
    spr.drawString(updateFwActions[i], 40+x+(sx/2), 64+y+((i-updateFwIdx)*16), FONT_SMALL);
  }
}

static void drawWiFiMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_WIFIMODE], x, y, sx, true);

  int count = ITEM_COUNT(wifiModeDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(wifiModeDesc[abs((wifiModeIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(wifiModeDesc[abs((wifiModeIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawTheme(int x, int y, int sx)
{
  drawCommon(settings[MENU_THEME], x, y, sx, true);

  int count = getTotalThemes();
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(theme[abs((themeIdx+count+i)%count)].name);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(theme[abs((themeIdx+count+i)%count)].name, 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawUILayout(int x, int y, int sx)
{
  drawCommon(settings[MENU_UI], x, y, sx, true);

  int count = ITEM_COUNT(uiLayoutDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(uiLayoutDesc[abs((uiLayoutIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((uiLayoutIdx+i) < 0 || (uiLayoutIdx+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(uiLayoutDesc[abs((uiLayoutIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawRDSMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_RDS], x, y, sx, true);

  int count = ITEM_COUNT(rdsMode);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(rdsMode[abs((rdsModeIdx+count+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(rdsMode[abs((rdsModeIdx+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawUTCOffset(int x, int y, int sx)
{
  drawCommon(settings[MENU_UTCOFFSET], x, y, sx, true);

  int count = ITEM_COUNT(utcOffsets);
  uint8_t idx = utcOffsetIdx;

  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0)
    {
      drawZoomedMenu(utcOffsets[abs((idx+count+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    }
    else
    {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(utcOffsets[abs((idx+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawDateTimeHighlight(const char *text, uint8_t start, uint8_t length, int x, int y)
{
  char prefix[11];
  char value[5];
  snprintf(prefix, sizeof(prefix), "%.*s", start, text);
  snprintf(value, sizeof(value), "%.*s", length, text + start);

  int left = x - spr.textWidth(text, FONT_SMALL) / 2;
  int valueX = left + spr.textWidth(prefix, FONT_SMALL);
  int valueWidth = spr.textWidth(value, FONT_SMALL);

  spr.fillRoundRect(valueX - 1, y - 8, valueWidth + 2, 16, 2, TH.menu_hl_bg);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
  spr.drawString(value, valueX, y, FONT_SMALL);
}

static void drawDateTime(int x, int y, int sx)
{
  static const uint8_t starts[] = { 0, 5, 8, 0, 3, 6 };
  static const uint8_t lengths[] = { 4, 2, 2, 2, 2, 2 };
  char date[16];
  char time[16];

  snprintf(date, sizeof(date), "%04u-%02u-%02u",
    (unsigned)dateTimeYear, (unsigned)dateTimeMonth, (unsigned)dateTimeDay);
  snprintf(time, sizeof(time), "%02u:%02u:%02u",
    (unsigned)dateTimeHour, (unsigned)dateTimeMinute, (unsigned)dateTimeSecond);

  drawCommon(settings[MENU_DATETIME], x, y, sx);
  drawZoomedMenu(settings[MENU_DATETIME]);

  int center = 40 + x + sx / 2;
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.menu_item);
  spr.drawString(date, center, 41 + y, FONT_SMALL);
  spr.drawString(time, center, 64 + y, FONT_SMALL);
  spr.drawString("(UTC)", center, 91 + y, FONT_SMALL);

  drawDateTimeHighlight(
    dateTimeField <= DATETIME_DAY ? date : time,
    starts[dateTimeField], lengths[dateTimeField], center,
    (dateTimeField <= DATETIME_DAY ? 41 : 64) + y
  );
}

static void drawMemory(int x, int y, int sx)
{
  char label_memory[16];
  sprintf(label_memory, "%s %2.2d", menu[MENU_MEMORY], memoryIdx + 1);
  drawCommon(label_memory, x, y, sx, true);

  int count = getTotalMemories();
  for(int i=-2 ; i<3 ; i++)
  {
    int j = abs((memoryIdx+count+i)%count);
    const Memory memory = getMemory(j);
    char buf[16];
    const char *text = buf;

    if(!memory.freq)
      text = "- - -";
    else if(memory.name[0])
      text = memory.name;
    else if(memory.mode==FM)
      sprintf(buf, "%3.2f %s", memory.freq / 1000000.0, bandModeDesc[memory.mode]);
    else
      sprintf(buf, "%5lu %s", memory.freq / 1000, bandModeDesc[memory.mode]);

    if(i==0) {
      drawZoomedMenu(text);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(text, 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawVolume(int x, int y, int sx)
{
  drawCommon(menu[MENU_VOLUME], x, y, sx);
  drawZoomedMenu(menu[MENU_VOLUME]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawNumber(volume, 40+x+(sx/2), 66+y, FONT_DIGITS);

  if(muteOn(MUTE_MAIN))
  {
    for(int i=-3; i<4; i++)
    {
      spr.drawLine(40+x+(sx/2) + 30 + i, 66 + y - 30 + i, 40+x+(sx/2) - 30 + i, 66 + y + 30 + i, TH.menu_param);
    }
  }
}

static void drawAgc(int x, int y, int sx)
{
  drawCommon(menu[MENU_AGC_ATT], x, y, sx);
  drawZoomedMenu(menu[MENU_AGC_ATT]);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.menu_param);

  // G8PTN: Read back value is not used
  // rx.getAutomaticGainControl();
  if(!agcNdx && !agcIdx)
  {
    spr.setFont(&lgfx::fonts::Orbitron_Light_24);
    spr.drawString("AGC", 40+x+(sx/2), 48+y);
    spr.drawString("On", 40+x+(sx/2), 72+y);
    spr.setFont(FONT_DEFAULT);
  }
  else
  {
    char text[16];
    sprintf(text, "%2.2d", agcNdx);
    spr.drawString(text, 40+x+(sx/2), 60+y, FONT_DIGITS);
  }
}

static void drawSquelch(int x, int y, int sx)
{
  drawCommon(menu[MENU_SQUELCH], x, y, sx);
  drawZoomedMenu(menu[MENU_SQUELCH]);
  spr.setTextDatum(MC_DATUM);

  uint8_t squelchValue = currentSquelch[currentMode] & 0x7f;
  bool squelchParam = currentSquelch[currentMode] & 0x80;
  if(squelchValue)
  {
    spr.drawNumber(squelchValue, 40+x+(sx/2), 60+y, FONT_LARGE);
    spr.drawString(squelchParam? "dB":"dBuV", 40+x+(sx/2), 90+y, FONT_LARGE);
  }
  else
  {
    spr.drawString("Off", 40+x+(sx/2), 60+y, FONT_LARGE);
    spr.drawString(squelchParam? "(snr)":"(rssi)", 40+x+(sx/2), 90+y, FONT_LARGE);
  }
}

static void drawSoftMuteMaxAtt(int x, int y, int sx)
{
  drawCommon(menu[MENU_SOFTMUTE], x, y, sx);
  drawZoomedMenu(menu[MENU_SOFTMUTE]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawString("Max Attn", 40+x+(sx/2), 32+y, FONT_SMALL);
  spr.drawNumber(softMuteMaxAttIdx, 40+x+(sx/2), 60+y, FONT_LARGE);
  spr.drawString("dB", 40+x+(sx/2), 90+y, FONT_LARGE);
}

static void drawCal(int x, int y, int sx)
{
  drawCommon(settings[MENU_CALIBRATION], x, y, sx);
  drawZoomedMenu(settings[MENU_CALIBRATION]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  if (currentMode == USB)
  {
    spr.drawString("USB", 40+x+(sx/2), 35+y, FONT_SMALL);
    spr.drawNumber(getCurrentBand()->usbCal, 40+x+(sx/2), 65+y, FONT_LARGE);
  }
  else if (currentMode == LSB)
  {
    spr.drawString("LSB", 40+x+(sx/2), 35+y, FONT_SMALL);
    spr.drawNumber(getCurrentBand()->lsbCal, 40+x+(sx/2), 65+y, FONT_LARGE);
  }
  else
    spr.drawNumber(0, 40+x+(sx/2), 65+y, FONT_LARGE);  // Display zero or nothing for other modes

  spr.drawString("Hz", 40+x+(sx/2), 95+y, FONT_LARGE);
}

static void drawAvc(int x, int y, int sx)
{
  drawCommon(menu[MENU_AVC], x, y, sx);
  drawZoomedMenu(menu[MENU_AVC]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  if(isSSB() && ssbAvcHold)
  {
    spr.drawString("Hold", 40+x+(sx/2), 60+y, FONT_LARGE);
    return;
  }
  spr.drawString("Max Gain", 40+x+(sx/2), 32+y, FONT_SMALL);

  // Only show AVC for AM and SSB modes
  if(currentMode!=FM)
  {
    int currentAvc = isSSB()? SsbAvcIdx : AmAvcIdx;
    spr.drawNumber(currentAvc, 40+x+(sx/2), 60+y, FONT_LARGE);
    spr.drawString("dB", 40+x+(sx/2), 90+y, FONT_LARGE);
  }
}

static void drawFmRegion(int x, int y, int sx)
{
  drawCommon(settings[MENU_FM_REGION], x, y, sx, true);

  int count = ITEM_COUNT(fmRegions);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(fmRegions[abs((FmRegionIdx+count+i)%count)].desc);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((FmRegionIdx+i) < 0 || (FmRegionIdx+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(fmRegions[abs((FmRegionIdx+count+i)%count)].desc, 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawFmStereo(int x, int y, int sx)
{
  drawCommon(settings[MENU_FM_STEREO], x, y, sx, true);

  int count = ITEM_COUNT(fmStereoDesc);
  for(int i=-2 ; i<3 ; i++)
  {
    if(i==0) {
      drawZoomedMenu(fmStereoDesc[abs((fmStereoIdx+count+i)%count)]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    } else {
      spr.setTextColor(TH.menu_item);
    }

    // Prevent repeats for short menus
    if (count < 5 && ((fmStereoIdx+i) < 0 || (fmStereoIdx+i) >= count)) {
      continue;
    }

    spr.setTextDatum(MC_DATUM);
    spr.drawString(fmStereoDesc[abs((fmStereoIdx+count+i)%count)], 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawDspPatches(int x, int y, int sx)
{
  drawCommon(settings[MENU_DSP_PATCHES], x, y, sx, true);
  uint8_t slots[PATCH_SET_COUNT + 1], selected;
  uint8_t count = dspPatchSlots(slots, selected);
  for(int i = -2; i < 3; i++)
  {
    int item = selected + i;
    if(item < 0 || item >= count) continue;
    uint8_t slot = slots[item];
    if(i == 0)
    {
      drawZoomedMenu(patchSetNames[slot]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    }
    else spr.setTextColor(TH.menu_item);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(patchSetNames[slot], 40+x+(sx/2), 64+y+(i*16), FONT_SMALL);
  }
}

static void drawBrt(int x, int y, int sx)
{
  drawCommon(settings[MENU_BRIGHTNESS], x, y, sx);
  drawZoomedMenu(settings[MENU_BRIGHTNESS]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawNumber(currentBrt, 40+x+(sx/2), 60+y, FONT_LARGE);
}

static void drawOffTimer(int x, int y, int sx)
{
  drawCommon(settings[MENU_OFFTIMER], x, y, sx);
  drawZoomedMenu(settings[MENU_OFFTIMER]);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.menu_param);
  uint16_t minutes = offTimerMinutes;
  char text[16];
  if(minutes) snprintf(text, sizeof(text), "%u", minutes);
  else strlcpy(text, "Off", sizeof(text));
  spr.drawString(text, 40+x+(sx/2), 60+y, FONT_LARGE);
  if(minutes) spr.drawString("min", 40+x+(sx/2), 88+y, FONT_SMALL);
}

static void drawSleep(int x, int y, int sx)
{
  drawCommon(settings[MENU_SLEEP], x, y, sx);
  drawZoomedMenu(settings[MENU_SLEEP]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  if(currentSleep)
  {
    spr.drawNumber(currentSleep, 40+x+(sx/2), 60+y, FONT_LARGE);
    spr.drawString("sec", 40+x+(sx/2), 88+y, FONT_SMALL);
  }
  else
    spr.drawString("Off", 40+x+(sx/2), 60+y, FONT_LARGE);
}

static void drawZoom(int x, int y, int sx)
{
  drawCommon(settings[MENU_ZOOM], x, y, sx);
  drawZoomedMenu(settings[MENU_ZOOM]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawString(zoomMenu ? "On" : "Off", 40+x+(sx/2), 60+y, FONT_LARGE);
}

static void drawAntennaCapacitor(int x, int y, int sx)
{
  drawCommon(settings[MENU_ANTCAP], x, y, sx);
  drawZoomedMenu(settings[MENU_ANTCAP]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawString(autoAntennaCapacitor ? "On" : "Off", 40+x+(sx/2), 60+y, FONT_LARGE);
}

static void drawEncoderAcceleration(int x, int y, int sx)
{
  drawCommon(settings[MENU_ENC_ACCEL], x, y, sx);
  drawZoomedMenu(settings[MENU_ENC_ACCEL]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawString(encoderAcceleration ? "On" : "Off", 40+x+(sx/2), 60+y, FONT_LARGE);
}

static void drawScrollDir(int x, int y, int sx)
{
  drawCommon(settings[MENU_SCROLL], x, y, sx);
  drawZoomedMenu(settings[MENU_SCROLL]);

  spr.fillRect(37+x+(sx/2), 45+y, 5, 40, TH.menu_param);
  if(scrollDirection>0)
    spr.fillTriangle(39+x+(sx/2)-5, 45+y, 39+x+(sx/2)+5, 45+y, 39+x+(sx/2), 45+y-5, TH.menu_param);
  else
    spr.fillTriangle(39+x+(sx/2)-5, 85+y, 39+x+(sx/2)+5, 85+y, 39+x+(sx/2), 85+y+5, TH.menu_param);
}

static void drawInfo(int x, int y, int sx)
{
  char text[16];

  // Info box
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(TH.box_text);
  spr.fillRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.box_bg);
  spr.drawRoundRect(1+x, 1+y, 76+sx, 110, 4, TH.box_border);

  spr.drawString("Step:", 6+x, 64+y+(-3*16), FONT_SMALL);
  spr.drawString(getCurrentStep()->desc, 48+x, 64+y+(-3*16), FONT_SMALL);

  spr.drawString("BW:", 6+x, 64+y+(-2*16), FONT_SMALL);
  spr.drawString(getCurrentBandwidth()->desc, 48+x, 64+y+(-2*16), FONT_SMALL);

  if(!agcNdx && !agcIdx)
  {
    spr.drawString("AGC:", 6+x, 64+y+(-1*16), FONT_SMALL);
    spr.drawString("On", 48+x, 64+y+(-1*16), FONT_SMALL);
  }
  else
  {
    sprintf(text, "%2.2d", agcNdx);
    spr.drawString("Att:", 6+x, 64+y+(-1*16), FONT_SMALL);
    spr.drawString(text, 48+x, 64+y+(-1*16), FONT_SMALL);
  }

  spr.drawString("Vol:", 6+x, 64+y+(0*16), FONT_SMALL);
  if(muteOn(MUTE_MAIN) || muteOn(MUTE_SQUELCH))
  {
    spr.setTextColor(TH.box_off_text, TH.box_off_bg);
    sprintf(text, muteOn(MUTE_MAIN) ? "Muted" : "%d/sq", volume);
    spr.drawString(text, 48+x, 64+y+(0*16), FONT_SMALL);
    spr.setTextColor(TH.box_text);
  }
  else
  {
    spr.setTextColor(TH.box_text);
    spr.drawNumber(volume, 48+x, 64+y+(0*16), FONT_SMALL);
  }

  // Draw RDS PI code, if present
  uint16_t piCode = getRdsPiCode();
  if(piCode && currentMode == FM)
  {
    sprintf(text, "%04X", piCode);
    spr.drawString("PI:", 6+x, 64+y + (1*16), FONT_SMALL);
    spr.drawString(text, 48+x, 64+y + (1*16), FONT_SMALL);
  }
  else
  {
    spr.drawString("AVC:", 6+x, 64+y + (1*16), FONT_SMALL);

    if(currentMode==FM)
      sprintf(text, "n/a");
    else if(isSSB() && ssbAvcHold)
      sprintf(text, "Hold");
    else if(isSSB())
      sprintf(text, "%2.2ddB", SsbAvcIdx);
    else
      sprintf(text, "%2.2ddB", AmAvcIdx);

    spr.drawString(text, 48+x, 64+y + (1*16), FONT_SMALL);
  }

  // Alternate the Off Timer countdown with the current date and time.
  const char *clock = clockGet();
  uint16_t minutes = offTimerRemainingMinutes();
  if(minutes && (!clock || millis() / BACKGROUND_REFRESH_TIME % 2))
  {
    snprintf(text, sizeof(text), "%02u:%02u", minutes / 60, minutes % 60);
    spr.drawString("Zzz:", 6+x, 64+y+(2*16), FONT_SMALL);
    spr.drawString(text, 48+x, 64+y+(2*16), FONT_SMALL);
  }
  else if(clock)
  {
    uint16_t year;
    uint8_t month, day, weekday;
    char date[6];
    const char *label = "Time:";

    if(clockGetDate(&year, &month, &day, &weekday))
    {
      if(month > 12 || day > 31) return;
      sprintf(date, "%02d.%02d", month, day);
      label = date;
    }

    spr.drawString(label, 6+x, 64+y+(2*16), FONT_SMALL);
    spr.drawString(clock, 48+x, 64+y+(2*16), FONT_SMALL);
  }
}

//
// Draw side bar (menu or information)
//
//
// Lists whose first item is "Back", drawn without wrapping around
//
static void drawList(const char *title, int count, int idx, void (*label)(int, char *, size_t),
                     int x, int y, int sx, const char *empty = nullptr)
{
  drawCommon(title, x, y, sx, true);

  for(int i=-2 ; i<3 ; i++)
  {
    int j = idx + i;
    char text[32];

    if(j<0 || j>count) continue;
    if(!j) strcpy(text, "Back");
    else label(j - 1, text, sizeof(text));

    if(i==0)
    {
      drawZoomedMenu(text);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    }
    else spr.setTextColor(TH.menu_item);

    // Long names slide on the highlighted line and are clipped on the others
    drawMarquee(text, 8+x, 64+y+(i*16)-8, 62+sx, FONT_SMALL, i==0);
  }

  // Say why the list is empty
  if(!count && empty)
  {
    spr.setTextColor(TH.menu_param);
    spr.drawString(empty, 40+x+(sx/2), 64+y+16, FONT_SMALL);
  }
}

static void fitText(char *text, size_t size, const char *src, int maxChars)
{
  snprintf(text, size, "%.*s", maxChars, src);
}

static void drawStations(int x, int y, int sx)
{
  if(stnLevel==0)
  {
    drawList("Stations", stationsLocalCount(), stnIdx, [](int i, char *text, size_t size) {
      StationCountry country;
      if(stationsLocalCountry(i, &country)) snprintf(text, size, "%s", country.name);
      else *text = '\0';
    }, x, y, sx, "No lists");
    return;
  }

  if(stnLevel==1)
  {
    StationCountry country;
    char title[12] = "Provinces";
    if(stationsLocalCountry(stnCountryIdx, &country)) fitText(title, sizeof(title), country.name, 11);
    drawList(title, stationsRegionsOpen(), stnIdx, [](int i, char *text, size_t size) {
      // Mark the listener's province
      const char *name = stationsRegionOpenName(i);
      snprintf(text, size, "%s%s", strcmp(name, stationsHomeRegion())? "" : "*", name);
    }, x, y, sx);
    return;
  }

  // The title follows the municipality of the highlighted station
  char title[STATION_CITY_LEN + 1] = "Stations";
  StationRecord record;
  if(stnIdx>0 && stationsOpenRecord(stnIdx - 1, &record))
  {
    stationsField(title, record.city, STATION_CITY_LEN);
    if(!*title) strcpy(title, "Stations");
    title[11] = '\0';
  }

  drawList(title, stationsOpenCount(), stnIdx, [](int i, char *text, size_t size) {
    StationRecord record;
    char name[STATION_NAME_LEN + 1];
    if(!stationsOpenRecord(i, &record)) { *text = '\0'; return; }
    // The whole station name, or the expected RDS name when there is none
    stationsField(name, record.name, STATION_NAME_LEN);
    if(!*name) stationsField(name, record.ps, STATION_PS_LEN);
    snprintf(text, size, "%.1f %s", record.freq / 100.0, name);
  }, x, y, sx);
}

static void drawLoadStations(int x, int y, int sx)
{
  if(loadLevel==0)
  {
    drawList("Load List", stationsRegionCount(), loadIdx, [](int i, char *text, size_t size) {
      snprintf(text, size, "%s", stationsRegionName(i));
    }, x, y, sx);
    return;
  }

  char title[12];
  fitText(title, sizeof(title), stationsRegionName(loadRegion), 11);
  drawList(title, stationsIndexCount(loadRegion), loadIdx, [](int i, char *text, size_t size) {
    const StationCountry *country = stationsIndexCountry(loadRegion, i);
    if(!country) { *text = '\0'; return; }
    // Mark the country already on the receiver
    snprintf(text, size, "%s%s", stationsIsLocal(country->code)? "*" : "", country->name);
  }, x, y, sx);
}

static void freqLabel(uint32_t freq, char *text, size_t size)
{
  snprintf(text, size, "%s%.1f", freq==currentFrequency? "> " : "", freq / 1000000.0);
}

static void drawAfList(int x, int y, int sx)
{
  drawList(afMenuTuning? "AF Tune" : "RDS AF", afCount(), afMenuIdx, [](int i, char *text, size_t size) {
    freqLabel(afFrequency(i), text, size);
  }, x, y, sx, "No AF");
}

static void drawEonList(int x, int y, int sx)
{
  if(eonLevel==0)
  {
    drawList("RDS EON", eonCount(), eonIdx, [](int i, char *text, size_t size) {
      EonNetwork network;
      if(eonNetwork(i, &network)) fitText(text, size, network.ps, 10);
      else *text = '\0';
    }, x, y, sx, "No EON");
    return;
  }

  drawList(eonTuning? "EON Tune" : eonSelected.ps, eonSelected.count, eonIdx, [](int i, char *text, size_t size) {
    freqLabel(eonSelected.freq[i], text, size);
  }, x, y, sx, "No freqs");
}

static void drawMemScan(int x, int y, int sx)
{
  const Memory memory = getMemory(memScanSlot());
  char text[16];

  sprintf(text, "Mem %2.2d", memScanSlot() + 1);
  drawCommon(text, x, y, sx);
  drawZoomedMenu(text);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.menu_param);

  if(!memory.freq)
    sprintf(text, "- - -");
  else if(memory.mode==FM)
    sprintf(text, "%3.2f", memory.freq / 1000000.0);
  else
    sprintf(text, "%lu", (unsigned long)(memory.freq / 1000));

  spr.drawString(text, 40+x+(sx/2), 50+y, FONT_LARGE);
  spr.drawString(memory.name[0]? memory.name : bandModeDesc[memory.mode], 40+x+(sx/2), 76+y, FONT_SMALL);

  if(!memScanRunning())
    spr.drawString("Done", 40+x+(sx/2), 96+y, FONT_SMALL);
  else
    spr.drawString(memScanListening()? "Holding" : "Seeking", 40+x+(sx/2), 96+y, FONT_SMALL);
}

static void drawAutoStore(int x, int y, int sx)
{
  drawCommon(menu[MENU_AUTOSTORE], x, y, sx, true);

  for(int i=0 ; i<ITEM_COUNT(autoStoreActions) ; i++)
  {
    if(i == autoStoreIdx)
    {
      drawZoomedMenu(autoStoreActions[i]);
      spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
    }
    else
      spr.setTextColor(TH.menu_item);

    spr.setTextDatum(MC_DATUM);
    spr.drawString(autoStoreActions[i], 40+x+(sx/2), 64+y+((i-autoStoreIdx)*16), FONT_SMALL);
  }
}

static void drawDxWatch(int x, int y, int sx)
{
  if(dxWatchPhase()==DXW_OFF)
  {
    drawCommon(menu[MENU_DXWATCH], x, y, sx, true);
    for(int i=0 ; i<ITEM_COUNT(dxwActions) ; i++)
    {
      if(i == dxwIdx)
      {
        drawZoomedMenu(dxwActions[i]);
        spr.setTextColor(TH.menu_hl_text, TH.menu_hl_bg);
      }
      else spr.setTextColor(TH.menu_item);
      spr.setTextDatum(MC_DATUM);
      spr.drawString(dxwActions[i], 40+x+(sx/2), 64+y+((i-dxwIdx)*16), FONT_SMALL);
    }
    return;
  }

  char line[24];
  const DxCatch *last = dxWatchLastCatch();
  bool caught = dxWatchPhase()==DXW_CATCH && last;

  drawCommon(caught? "DX Catch!" : menu[MENU_DXWATCH], x, y, sx);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.menu_param);

  if(caught)
  {
    sprintf(line, "%.1f", last->freq / 1000000.0);
    spr.drawString(line, 40+x+(sx/2), 44+y, FONT_LARGE);
    sprintf(line, "PI %04X", last->pi);
    spr.drawString(line, 40+x+(sx/2), 68+y, FONT_SMALL);
    drawMarquee(last->ps[0]? last->ps : "- - -", 8+x, 76+y, 62+sx, FONT_SMALL, true);
    spr.drawString("Click: stop", 40+x+(sx/2), 100+y, FONT_SMALL);
    return;
  }

  spr.drawString(dxWatchPhase()==DXW_LEARNING? "Learning" : "Watching", 40+x+(sx/2), 40+y, FONT_SMALL);
  sprintf(line, "%.1f", dxWatchFrequency() / 1000000.0);
  spr.drawString(line, 40+x+(sx/2), 60+y, FONT_LARGE);
  sprintf(line, "Sweep %u", dxWatchSweeps() + 1);
  spr.drawString(line, 40+x+(sx/2), 84+y, FONT_SMALL);
  sprintf(line, "Catches %u", dxWatchCatches());
  spr.drawString(line, 40+x+(sx/2), 100+y, FONT_SMALL);
}

static void drawDebug(int x, int y, int sx)
{
  bool fm = bands[bandIdx].bandMode==FM;
  char title[24];

  if(debugEdit)
  {
    char value[16];
    drawCommon(debugEdit==1? "Edit Min" : "Edit Max", x, y, sx);
    freqText(value, sizeof(value), debugValue, fm);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(TH.menu_param);
    drawMarquee(value, 8+x, 52+y, 62+sx, FONT_LARGE, false);
    spr.drawString(bands[bandIdx].bandName, 40+x+(sx/2), 90+y, FONT_SMALL);
    return;
  }

  snprintf(title, sizeof(title), "%s%s", bands[bandIdx].bandName, debugBandLimitsChanged(bandIdx)? "*" : "");
  drawList(title, ITEM_COUNT(debugItems) - 1, debugIdx, [](int i, char *text, size_t size) {
    bool fm = bands[bandIdx].bandMode==FM;
    char value[16];
    switch(i + 1)
    {
      case 1: freqText(value, sizeof(value), bands[bandIdx].minimumFreq, fm); snprintf(text, size, "Min %s", value); break;
      case 2: freqText(value, sizeof(value), bands[bandIdx].maximumFreq, fm); snprintf(text, size, "Max %s", value); break;
      case 4: snprintf(text, size, "USB Log %s", debugUsbLog? "On" : "Off"); break;
      default: snprintf(text, size, "%s", debugItems[i + 1]); break;
    }
  }, x, y, sx);
}

static void drawDxMode(int x, int y, int sx)
{
  drawCommon(settings[MENU_DXMODE], x, y, sx);
  drawZoomedMenu(settings[MENU_DXMODE]);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TH.menu_param);
  spr.drawString(dxMode ? "On" : "Off", 40+x+(sx/2), 60+y, FONT_LARGE);
}

void drawSideBar(uint16_t cmd, int x, int y, int sx)
{
  if(sleepOn()) return;

  switch(cmd)
  {
    case CMD_MENU:       drawMenu(x, y, sx);       break;
    case CMD_SETTINGS:   drawSettings(x, y, sx);   break;
    case CMD_MODE:       drawMode(x, y, sx);       break;
    case CMD_STEP:       drawStep(x, y, sx);       break;
    case CMD_SEEK:       drawSeek(x, y, sx);       break;
    case CMD_SCAN:       drawScan(x, y, sx);       break;
    case CMD_BAND:       drawBand(x, y, sx);       break;
    case CMD_BANDWIDTH:  drawBandwidth(x, y, sx);  break;
    case CMD_THEME:      drawTheme(x, y, sx);      break;
    case CMD_UI:         drawUILayout(x, y, sx);   break;
    case CMD_VOLUME:     drawVolume(x, y, sx);     break;
    case CMD_AGC:        drawAgc(x, y, sx);        break;
    case CMD_SOFTMUTE:   drawSoftMuteMaxAtt(x, y, sx);break;
    case CMD_CAL:        drawCal(x, y, sx);        break;
    case CMD_AVC:        drawAvc(x, y, sx);        break;
    case CMD_FM_REGION:  drawFmRegion(x, y, sx);   break;
    case CMD_FM_STEREO:  drawFmStereo(x, y, sx);   break;
    case CMD_ANTCAP:     drawAntennaCapacitor(x, y, sx);break;
    case CMD_DSP_PATCHES: drawDspPatches(x, y, sx); break;
    case CMD_BRT:        drawBrt(x, y, sx);        break;
    case CMD_RDS:        drawRDSMode(x, y, sx);    break;
    case CMD_MEMORY:     drawMemory(x, y, sx);     break;
    case CMD_OFFTIMER:   drawOffTimer(x, y, sx);   break;
    case CMD_SLEEP:      drawSleep(x, y, sx);      break;
    case CMD_SLEEPMODE:  drawSleepMode(x, y, sx);  break;
    case CMD_USBMODE:    drawUSBMode(x, y, sx);    break;
    case CMD_TCPMODE:    drawTCPMode(x, y, sx);    break;
    case CMD_BLEMODE:    drawBleMode(x, y, sx);    break;
    case CMD_WIFIMODE:   drawWiFiMode(x, y, sx);   break;
    case CMD_UPDATEFW:   drawUpdateFW(x, y, sx);   break;
    case CMD_ZOOM:       drawZoom(x, y, sx);       break;
    case CMD_ENC_ACCEL:  drawEncoderAcceleration(x, y, sx);break;
    case CMD_SCROLL:     drawScrollDir(x, y, sx);  break;
    case CMD_UTCOFFSET:  drawUTCOffset(x, y, sx);  break;
    case CMD_DATETIME:   drawDateTime(x, y, sx);   break;
    case CMD_SQUELCH:    drawSquelch(x, y, sx);    break;
    case CMD_MEMSCAN:    drawMemScan(x, y, sx);    break;
    case CMD_AUTOSTORE:  drawAutoStore(x, y, sx);  break;
    case CMD_STATIONS:   drawStations(x, y, sx);   break;
    case CMD_LOADSTN:    drawLoadStations(x, y, sx); break;
    case CMD_AFLIST:     drawAfList(x, y, sx);     break;
    case CMD_EONLIST:    drawEonList(x, y, sx);    break;
    case CMD_DXMODE:     drawDxMode(x, y, sx);     break;
    case CMD_DXWATCH:    drawDxWatch(x, y, sx);    break;
    case CMD_DEBUG:      drawDebug(x, y, sx);      break;
    default:             drawInfo(x, y, sx);       break;
  }
}
