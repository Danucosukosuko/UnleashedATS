#include "Common.h"
#include "Themes.h"
#include "Draw.h"
#include "Utils.h"
#include "Menu.h"
#include "Storage.h"
#include "AltFreq.h"
#include "Debug.h"

#include <soc/rtc_cntl_reg.h>
#include <esp_heap_caps.h>

#define DEBUG_MAX_BANDS 48

bool debugMode = false;
bool debugUsbLog = false;
bool debugInfoView = false;
uint32_t debugLoopUs = 0;

int16_t debugFmSoftMute = DEBUG_AUTO;
int16_t debugFmSeekRssi = DEBUG_AUTO;
int16_t debugFmSeekSnr  = DEBUG_AUTO;
int16_t debugAmSeekRssi = DEBUG_AUTO;
int16_t debugAmSeekSnr  = DEBUG_AUTO;
int16_t debugAntCap     = DEBUG_AUTO;
uint16_t debugCpuMhz = 80;
uint16_t debugI2cKhz = 800;

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

void debugSaveSettings()
{
  prefs.begin("debug", false, STORAGE_PARTITION);
  prefs.putShort("FmSM", debugFmSoftMute);
  prefs.putShort("FmSR", debugFmSeekRssi);
  prefs.putShort("FmSS", debugFmSeekSnr);
  prefs.putShort("AmSR", debugAmSeekRssi);
  prefs.putShort("AmSS", debugAmSeekSnr);
  prefs.putShort("AntCap", debugAntCap);
  prefs.putUShort("Cpu", debugCpuMhz);
  prefs.putUShort("I2c", debugI2cKhz);
  prefs.putShort("BattCal", batteryCalMv);
  prefs.end();
}

void debugApplySystem()
{
  setCpuFrequencyMhz(debugMode? debugCpuMhz : 80);
  rx.setI2CFastModeCustom((debugMode? debugI2cKhz : 800) * 1000UL);
}

void debugApplyRadio()
{
  if(!debugMode) return;

  if(currentMode==FM)
  {
    if(debugFmSoftMute!=DEBUG_AUTO) rx.setFmSoftMuteMaxAttenuation(debugFmSoftMute);
    if(debugFmSeekRssi!=DEBUG_AUTO) rx.setSeekFmRssiThreshold(debugFmSeekRssi);
    if(debugFmSeekSnr!=DEBUG_AUTO)  rx.setSeekFmSNRThreshold(debugFmSeekSnr);
  }
  else if(currentMode==AM)
  {
    if(debugAmSeekRssi!=DEBUG_AUTO) rx.setSeekAmRssiThreshold(debugAmSeekRssi);
    if(debugAmSeekSnr!=DEBUG_AUTO)  rx.setSeekAmSNRThreshold(debugAmSeekSnr);
    if(debugAntCap!=DEBUG_AUTO)
    {
      // A fixed capacitor takes effect on the next tune
      rx.setTuneFrequencyAntennaCapacitor(debugAntCap);
      rx.setFrequencyConfirmed(getReceiverFrequency(currentFrequency));
    }
  }
}

void debugSetMode(bool on)
{
  debugMode = on;
  if(!on)
  {
    debugUsbLog = false;
    debugInfoView = false;
  }
  prefs.begin("debug", false, STORAGE_PARTITION);
  prefs.putBool("Enabled", on);
  prefs.end();
  debugApplySystem();
  applyDxMode();
}

void debugApplyBandLimits()
{
  keepDefaults();

  prefs.begin("debug", true, STORAGE_PARTITION);
  debugMode       = prefs.getBool("Enabled", false);
  debugFmSoftMute = prefs.getShort("FmSM", DEBUG_AUTO);
  debugFmSeekRssi = prefs.getShort("FmSR", DEBUG_AUTO);
  debugFmSeekSnr  = prefs.getShort("FmSS", DEBUG_AUTO);
  debugAmSeekRssi = prefs.getShort("AmSR", DEBUG_AUTO);
  debugAmSeekSnr  = prefs.getShort("AmSS", DEBUG_AUTO);
  debugAntCap     = prefs.getShort("AntCap", DEBUG_AUTO);
  debugCpuMhz     = prefs.getUShort("Cpu", 80);
  debugI2cKhz     = prefs.getUShort("I2c", 800);
  // The battery calibration is a property of the hardware, always used
  batteryCalMv    = prefs.getShort("BattCal", 0);
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

  if(debugMode) debugApplySystem();
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

void debugRebootToDownload()
{
  // The ROM bootloader waits for esptool, no BOOT button needed
  REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
  esp_restart();
}

//
// Live information, a full screen redrawn continuously
//
void debugDrawInfo()
{
  static bool chipRead = false;
  char line[64];
  int y = 0;

  if(!chipRead)
  {
    rx.getFirmware();
    chipRead = true;
  }

  rx.getCurrentReceivedSignalQuality();
  rx.getAutomaticGainControl();

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.text_muted);
  spr.drawString("Live Info", 0, y, FONT_SMALL);
  spr.setTextColor(TH.text_warn);
  spr.drawString("Click to leave", 220, y, FONT_SMALL);
  spr.setTextColor(TH.text);
  y += 17;

  snprintf(line, sizeof(line), "%s %.3f MHz  RSSI %u dBuV  SNR %u dB", bands[bandIdx].bandName,
           currentFrequency / 1000000.0, rx.getCurrentRSSI(), rx.getCurrentSNR());
  if(currentMode!=FM)
    snprintf(line, sizeof(line), "%s %lu kHz %s  RSSI %u dBuV  SNR %u dB", bands[bandIdx].bandName,
             (unsigned long)(currentFrequency / 1000), bandModeDesc[currentMode], rx.getCurrentRSSI(), rx.getCurrentSNR());
  spr.drawString(line, 0, y, FONT_SMALL); y += 15;

  if(currentMode==FM)
    snprintf(line, sizeof(line), "Pilot %d  Blend %u%%  Multipath %u  Offset %d kHz",
             rx.getCurrentPilot(), rx.getCurrentStereoBlend(), rx.getCurrentMultipath(),
             (int8_t)rx.getCurrentSignedFrequencyOffset());
  else
    snprintf(line, sizeof(line), "Antenna capacitor %u", rx.getAntennaTuningCapacitor());
  spr.drawString(line, 0, y, FONT_SMALL); y += 15;

  snprintf(line, sizeof(line), "AGC %s  Gain index %u  Volume %u", rx.isAgcEnabled()? "on" : "off",
           rx.getAgcGainIndex(), volume);
  spr.drawString(line, 0, y, FONT_SMALL); y += 15;

  uint32_t groups, bad;
  const uint32_t *types;
  rdsStats(&groups, &bad, &types);
  snprintf(line, sizeof(line), "RDS groups %lu  Corrupt %lu%%  PI %04X", (unsigned long)groups,
           (unsigned long)(groups? bad * 100 / groups : 0), afProgramId());
  spr.drawString(line, 0, y, FONT_SMALL); y += 15;

  // The five most frequent group types
  char *out = line;
  out += sprintf(out, "Types");
  bool used[32] = {};
  for(int n=0 ; n<5 ; n++)
  {
    int best = -1;
    for(int i=0 ; i<32 ; i++)
      if(!used[i] && types[i] && (best<0 || types[i]>types[best])) best = i;
    if(best<0) break;
    used[best] = true;
    out += sprintf(out, "  %d%c %lu", best >> 1, best & 1? 'B' : 'A', (unsigned long)types[best]);
  }
  spr.drawString(line, 0, y, FONT_SMALL); y += 15;

  snprintf(line, sizeof(line), "SI47%02u  FW %c.%c  Comp %c  Rev %c", rx.getFirmwarePN(),
           rx.getFirmwareFWMAJOR(), rx.getFirmwareFWMINOR(), rx.getFirmwareCMPMAJOR(), rx.getFirmwareCHIPREV());
  spr.drawString(line, 0, y, FONT_SMALL); y += 15;

  snprintf(line, sizeof(line), "CPU %lu MHz  I2C %u kHz  Loop %.1f ms", (unsigned long)getCpuFrequencyMhz(),
           debugMode? debugI2cKhz : 800, debugLoopUs / 1000.0);
  spr.drawString(line, 0, y, FONT_SMALL); y += 15;

  snprintf(line, sizeof(line), "Heap %luk (block %luk)  PSRAM %luk free",
           (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned long)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned long)(ESP.getFreePsram() / 1024));
  spr.drawString(line, 0, y, FONT_SMALL); y += 15;

  uint32_t s = millis() / 1000;
  snprintf(line, sizeof(line), "Up %lu:%02lu:%02lu  Battery %.2f V", (unsigned long)(s / 3600),
           (unsigned long)(s / 60 % 60), (unsigned long)(s % 60), batteryMonitor());
  spr.drawString(line, 0, y, FONT_SMALL);

  // Keep the readings moving
  marqueeActive = true;
  spr.pushSprite(0, 0);
}
