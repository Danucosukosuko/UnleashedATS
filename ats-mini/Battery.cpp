#include "Common.h"
#include "Themes.h"
#include "Utils.h"

#define VBAT_MON  4                 // GPIO04 -- Battery Monitor PIN

#define BATT_ADC_READS          16  // ADC reads for average calculation (at most 16 to avoid rollover)
#define BATT_ADC_FACTOR      1.702  // ADC correction factor used for the battery monitor
#define BATT_CHARGING        4.300  // Above this the receiver runs from USB (load sharing)
#define BATT_FILTER_TIME       500  // Time between filter updates (msecs)
#define BATT_FILTER_ALPHA     0.25f // Weight of a new reading in the filter
#define BATT_LOAD_SAG        0.020f // Voltage the cell loses under the receiver load (V)

// Show the battery as a percentage instead of a voltage
bool batteryShowPercent = false;
// Calibration against a multimeter, set in debug mode (mV)
int16_t batteryCalMv = 0;

// Last raw reading and the filtered voltage
static float batteryVolts = 4.0;
static float batteryFiltered = 0.0;
static uint32_t batteryFilterTime = 0;
static bool batteryOnUsb = false;

//
// Open circuit voltage of a lithium cell against its state of charge
//
static const struct { float volts; uint8_t percent; } batterySoc[] =
{
  { 4.20, 100 }, { 4.15, 95 }, { 4.11, 90 }, { 4.08, 85 }, { 4.02, 80 }, { 3.98, 75 },
  { 3.95, 70 }, { 3.91, 65 }, { 3.87, 60 }, { 3.85, 55 }, { 3.84, 50 }, { 3.82, 45 },
  { 3.80, 40 }, { 3.79, 35 }, { 3.77, 30 }, { 3.75, 25 }, { 3.73, 20 }, { 3.71, 15 },
  { 3.69, 10 }, { 3.61, 5 }, { 3.27, 0 },
};

//
// Measure the battery voltage, and return it filtered
//
float batteryMonitor()
{
  int i, j;

  // Read ADC multiple times
  for(i=j=0 ; i<BATT_ADC_READS ; i++) j += analogRead(VBAT_MON);

  // Calculate average voltage with correction factor and calibration
  batteryVolts = ((float)j / BATT_ADC_READS) * BATT_ADC_FACTOR / 1000 + batteryCalMv / 1000.0f;

  // On USB the reading is the supply, not the cell. Coming off it, the
  // filter starts again from the cell voltage
  bool onUsb = batteryVolts > BATT_CHARGING;
  uint32_t now = millis();
  if(onUsb || batteryOnUsb || batteryFiltered <= 0)
  {
    batteryFiltered = batteryVolts;
    batteryFilterTime = now;
  }
  else if(now - batteryFilterTime >= BATT_FILTER_TIME)
  {
    batteryFiltered += BATT_FILTER_ALPHA * (batteryVolts - batteryFiltered);
    batteryFilterTime = now;
  }
  batteryOnUsb = onUsb;

  return(batteryFiltered);
}

bool batteryCharging() { return(batteryOnUsb); }

//
// State of charge, interpolated over the discharge curve of the cell
//
uint8_t batteryPercent()
{
  // The curve is for a resting cell, make up for the receiver load
  float volts = batteryFiltered + BATT_LOAD_SAG;

  if(volts >= batterySoc[0].volts) return(100);
  for(size_t i=1 ; i<ITEM_COUNT(batterySoc) ; i++)
    if(volts >= batterySoc[i].volts)
    {
      float span = batterySoc[i-1].volts - batterySoc[i].volts;
      float part = (volts - batterySoc[i].volts) / span;
      return(batterySoc[i].percent + (uint8_t)(part * (batterySoc[i-1].percent - batterySoc[i].percent) + 0.5f));
    }
  return(0);
}

//
// Show last measured battery voltage and status at given screen
// coordinates. Return true if voltage was drawn.
//
bool drawBattery(int x, int y)
{
  if(sleepOn()) return false;

  // Measure battery voltage and status
  batteryMonitor();

  // Set display information
  spr.drawRoundRect(x, y + 1, 28, 14, 3, TH.batt_border);
  spr.drawLine(x + 29, y + 5, x + 29, y + 10, TH.batt_border);
  spr.drawLine(x + 30, y + 6, x + 30, y + 9, TH.batt_border);

  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(TH.batt_voltage);

  bool charging = batteryCharging();
  float volts = batteryFiltered;
  int percent = batteryPercent();

  if(switchThemeEditor())
  {
    // Alternate between five battery states every 10 seconds
    int state = (millis() % 50000u) / 10000u;
    charging = state >= 4;
    percent = state * 25 + 10;
    volts = 3.6 + state * 0.15;
  }

  // The hardware has a load sharing circuit to allow simultaneous charge and power
  // With USB(5V) connected the voltage reading will be approx. VBUS - Diode Drop = 4.65V
  // If the average voltage is greater than 4.3V, show ligtning on the display
  if(charging)
  {
    spr.fillRoundRect(x + 2, y + 3, 24, 10, 2, TH.batt_charge);
    spr.drawLine(x + 9 + 8, y + 1, x + 9 + 6, y + 1 + 5, TH.bg);
    spr.drawLine(x + 9 + 6, y + 1 + 5, x + 9 + 10, y + 1 + 5, TH.bg);
    spr.drawLine(x + 9 + 11, y + 1 + 6, x + 9 + 4, y + 1 + 13, TH.bg);
    spr.drawLine(x + 9 + 2, y + 1 + 13, x + 9 + 4, y + 1 + 8, TH.bg);
    spr.drawLine(x + 9 + 4, y + 1 + 8, x + 9 + 0, y + 1 + 8, TH.bg);
    spr.drawLine(x + 9 - 1, y + 1 + 7, x + 9 + 6, y + 1 + 0, TH.bg);
    spr.fillTriangle(x + 9 + 7, y + 1, x + 9 + 4, y + 1 + 6, x + 9, y + 1 + 7, TH.batt_icon);
    spr.fillTriangle(x + 9 + 5, y + 1 + 6, x + 9 + 10, y + 1 + 6, x + 9 + 3, y + 1 + 13, TH.batt_icon);
    spr.fillRect(x + 9 + 1, y + 1 + 6, 9, 2, TH.batt_icon);
    spr.drawPixel(x + 9 + 3, y + 1 + 12, TH.batt_icon);
    return false;
  }
  else
  {
    char text[8];

    // Voltage or state of charge, as chosen in the settings
    if(batteryShowPercent) sprintf(text, "%d%%", percent);
    else sprintf(text, "%.02fV", volts);

    // The bar follows the state of charge, red when it runs low
    int level = percent * 24 / 100;
    if(level < 2) level = 2;
    uint16_t color = percent <= 15? TH.batt_low : TH.batt_full;

    spr.fillRoundRect(x + 2, y + 3, level, 10, 2, color);
    spr.drawString(text, x - 3, y, FONT_SMALL);
    return true;
  }
}
