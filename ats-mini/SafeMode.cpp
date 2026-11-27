#include "Common.h"
#include "Themes.h"
#include "Storage.h"
#include "Utils.h"
#include "Ota.h"
#include "Debug.h"
#include "SafeMode.h"
#include "Recovery.h"

#include <WiFi.h>

// Encoder handling lives in ats-mini.ino
uint32_t consumeEncoderCounts();
void rotaryEncoder();

enum SafeItem
{
  SAFE_CONTINUE, SAFE_SETTINGS, SAFE_MEMORIES, SAFE_NETWORK, SAFE_FILES, SAFE_FACTORY,
  SAFE_PREVIOUS, SAFE_RECOVERY, SAFE_WIFI, SAFE_REINSTALL, SAFE_DOWNLOAD, SAFE_RESTART, SAFE_COUNT
};

static const struct { const char *name; const char *help; bool confirm; } items[SAFE_COUNT] =
{
  { "Continue",           "Start the receiver normally",                         false },
  { "Reset settings",     "Keeps memories, station lists, logs and Wi-Fi",       true  },
  { "Erase memories",     "Clears the memory slots",                             true  },
  { "Forget Wi-Fi",       "Clears the networks and the web login",               true  },
  { "Erase files",        "Station lists, DX log, splash image and patches",     true  },
  { "Factory reset",      "Erases everything, as on a new receiver",             true  },
  { "Previous firmware",  "",                                                    true  },
  { "Recovery app",       "",                                                    false },
  { "Recovery Wi-Fi",     "Starts with Wi-Fi on: open 10.1.1.1/update",          false },
  { "Reinstall latest",   "Installs the latest release over a saved network",    true  },
  { "USB flashing",       "Waits for esptool, no BOOT button needed",            false },
  { "Restart",            "Restarts the receiver",                               false },
};

#define SAFE_ROWS 7   // Items on screen at once

static bool wifiRequested = false;
static bool reinstallRequested = false;
static char previousLabel[32] = "";
static bool previousAvailable = false;
static bool recoveryInstalled = false;

bool safeModeWifi() { return(wifiRequested || reinstallRequested); }

void safeModeTick()
{
  if(!reinstallRequested || WiFi.status() != WL_CONNECTED) return;
  // Secure connections need the time, which arrives from the network
  if(!clockGetDate(nullptr, nullptr, nullptr, nullptr)) return;
  reinstallRequested = false;
  otaRequestReinstall();
}

static void clearNamespace(const char *name)
{
  prefs.begin(name, false, STORAGE_PARTITION);
  prefs.clear();
  prefs.end();
}

static void draw(int selected, bool armed, const char *note)
{
  spr.fillSprite(TH.bg);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.text_warn);
  spr.drawString("Safe mode", 6, 2, FONT_SMALL);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(TH.text_muted);
  spr.drawString(getVersion(true), 314, 2, FONT_SMALL);
  spr.drawFastHLine(0, 19, 320, TH.menu_border);

  int first = selected - SAFE_ROWS / 2;
  first = first < 0? 0 : first > SAFE_COUNT - SAFE_ROWS? SAFE_COUNT - SAFE_ROWS : first;

  for(int i=0 ; i<SAFE_ROWS ; i++)
  {
    int item = first + i, y = 24 + i * 17;
    bool disabled = (item == SAFE_PREVIOUS && !previousAvailable) || (item == SAFE_RECOVERY && !recoveryInstalled);
    if(item == selected)
    {
      spr.fillRect(0, y - 1, 320, 17, TH.menu_hl_bg);
      spr.setTextColor(TH.menu_hl_text);
    }
    else spr.setTextColor(disabled? TH.text_muted : TH.menu_item);
    spr.setTextDatum(TL_DATUM);
    spr.drawString(items[item].name, 12, y, FONT_SMALL);
    if(item == SAFE_PREVIOUS)
    {
      spr.setTextDatum(TR_DATUM);
      spr.drawString(previousAvailable? previousLabel : "none kept", 308, y, FONT_SMALL);
    }
  }

  // What the selected item does, or what just happened
  const char *help = note? note : armed? "Click again to confirm, rotate to cancel" :
                     selected == SAFE_PREVIOUS? (previousAvailable? "Starts the firmware kept from before the last update" : "No previous firmware on this receiver") :
                     selected == SAFE_RECOVERY? (recoveryInstalled? "Restarts into the recovery app" : "Not installed: flash the full firmware over USB") :
                     items[selected].help;
  spr.fillRect(0, 146, 320, 24, TH.box_bg);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(armed || note? TH.text_warn : TH.text);
  spr.drawString(help, 160, 158, FONT_SMALL);

  spr.pushSprite(0, 0);
}

static bool pressed() { return(digitalRead(ENCODER_PUSH_BUTTON) == LOW); }

void safeModeRun(bool afterCrashes)
{
  // Whatever is installed starts far enough to get here, keep it
  otaConfirmRunning();
  previousAvailable = otaPreviousFirmware(previousLabel, sizeof(previousLabel));
  recoveryInstalled = recoveryAvailable();

  ledcWrite(PIN_LCD_BL, 160);
  int selected = SAFE_CONTINUE;
  bool armed = false;
  const char *note = "Release the button";
  uint32_t noteTime = 0;
  draw(selected, armed, note);

  // The button is still held from switching on
  while(pressed()) delay(20);
  delay(50);
  note = afterCrashes? "The firmware kept failing to start" : nullptr;
  noteTime = millis();

  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);
  consumeEncoderCounts();

  bool redraw = true;
  for(;;)
  {
    int16_t enc = (int16_t)(consumeEncoderCounts() & 0xFFFF);
    if(enc)
    {
      selected = (selected + enc) % SAFE_COUNT;
      if(selected < 0) selected += SAFE_COUNT;
      armed = false;
      note = nullptr;
      afterCrashes = false;
      redraw = true;
    }

    if(note && !afterCrashes && millis() - noteTime > 2500) { note = nullptr; redraw = true; }
    if(redraw) { draw(selected, armed, note); redraw = false; }

    if(!pressed()) { delay(10); continue; }

    // A click: wait for the release
    delay(30);
    while(pressed()) delay(10);
    redraw = true;

    if(items[selected].confirm && !armed)
    {
      if(selected == SAFE_PREVIOUS && !previousAvailable) continue;
      armed = true;
      continue;
    }
    armed = false;
    note = "Done";
    noteTime = millis();

    switch(selected)
    {
      case SAFE_CONTINUE:
        detachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A));
        detachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B));
        spr.fillSprite(TH.bg);
        spr.pushSprite(0, 0);
        return;

      case SAFE_SETTINGS:
      {
        // The battery calibration belongs to the hardware, keep it
        prefs.begin("debug", true, STORAGE_PARTITION);
        int16_t batteryCal = prefs.getShort("BattCal", 0);
        prefs.end();
        clearNamespace("settings");
        clearNamespace("bands");
        clearNamespace("debug");
        prefs.begin("debug", false, STORAGE_PARTITION);
        prefs.putShort("BattCal", batteryCal);
        prefs.end();
        break;
      }

      case SAFE_MEMORIES: clearNamespace("memories"); break;
      case SAFE_NETWORK:  clearNamespace("network"); break;

      case SAFE_FILES:
        note = diskInit(true)? "Files erased" : "Unable to format the file system";
        clearNamespace("stations");
        break;

      case SAFE_FACTORY:
        // What holding the button at power on always did
        nvsErase();
        diskInit(true);
        note = "Everything erased";
        break;

      case SAFE_PREVIOUS:
        draw(selected, false, "Checking and starting...");
        note = otaBootPrevious();
        previousAvailable = otaPreviousFirmware(previousLabel, sizeof(previousLabel));
        break;

      case SAFE_RECOVERY:
        if(!recoveryInstalled) { note = "The recovery app is not installed"; break; }
        draw(selected, false, "Starting the recovery app...");
        recoveryRestart();
        break;

      case SAFE_WIFI:
        wifiRequested = true;
        note = "Wi-Fi starts with the receiver";
        break;

      case SAFE_REINSTALL:
        reinstallRequested = true;
        note = "Installs once the receiver is online";
        break;

      case SAFE_DOWNLOAD:
        // The receiver chip is not started yet in safe mode
        debugRebootToDownload(false);
        break;

      case SAFE_RESTART:
        esp_restart();
        break;
    }
  }
}
