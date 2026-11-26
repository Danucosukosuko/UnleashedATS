#include "Common.h"
#include "Themes.h"
#include "Storage.h"

#include <esp_app_desc.h>

#define WELCOME_TIMEOUT 60000  // Carry on by itself after a minute (msecs)

static const char *const welcomeNews[] =
{
  "RDS AF & EON lists, AF tuning",
  "FM station lists by country",
  "Auto Store, Mem Scan, DX Mode",
  "S-meter peak, AF/EON indicators",
};

static void drawCentered(const char *text, int y, const lgfx::IFont *font, uint16_t color)
{
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(color);
  spr.drawString(text, DISPLAY_WIDTH / 2, y, font);
}

static void drawWelcome(const char *previous, const char *current)
{
  char text[48];

  spr.fillSprite(TH.bg);
  spr.drawRoundRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 6, TH.menu_border);

  drawCentered(previous? "Firmware updated" : "Welcome to", 6, FONT_SMALL, TH.text_muted);
  drawCentered(CFW_NAME, 22, FONT_LARGE, TH.text);

  if(!previous)
    snprintf(text, sizeof(text), "v%d.%02d, based on ATS-Mini v%d.%02d",
             VER_CFW / 100, VER_CFW % 100, VER_APP / 100, VER_APP % 100);
  else if(strcmp(previous, current))
    snprintf(text, sizeof(text), "%s -> %s", previous, current);
  else
    snprintf(text, sizeof(text), "%s, new build", current);
  drawCentered(text, 50, FONT_SMALL, TH.freq_hl_sel);

  spr.drawLine(20, 70, DISPLAY_WIDTH - 21, 70, TH.smeter_bar_empty);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_item);
  for(size_t i=0 ; i<ITEM_COUNT(welcomeNews) ; i++)
  {
    int y = 76 + i * 16;
    spr.fillRect(28, y + 6, 4, 4, TH.text_warn);
    spr.drawString(welcomeNews[i], 38, y, FONT_SMALL);
  }

  spr.fillRoundRect(60, 144, DISPLAY_WIDTH - 120, 19, 4, TH.menu_hl_bg);
  drawCentered("Click to continue", 146, FONT_SMALL, TH.menu_hl_text);

  spr.pushSprite(0, 0);
}

//
// Show the welcome screen after a new firmware image has been flashed
// over USB or installed by an update, once per image
//
void welcomeCheck()
{
  char build[9], current[16];
  esp_app_get_elf_sha256(build, sizeof(build));
  snprintf(current, sizeof(current), "v%d.%02d-%d.%02d",
           VER_CFW / 100, VER_CFW % 100, VER_APP / 100, VER_APP % 100);

  prefs.begin("cfw", true, STORAGE_PARTITION);
  String seenBuild = prefs.getString("Build", "");
  String seenVersion = prefs.getString("Version", "");
  prefs.end();

  if(seenBuild == build) return;

  ledcWrite(PIN_LCD_BL, currentBrt);
  drawWelcome(seenVersion.length()? seenVersion.c_str() : nullptr, current);

  // Wait for a click on the encoder
  uint32_t start = millis();
  while(digitalRead(ENCODER_PUSH_BUTTON)!=LOW && millis() - start < WELCOME_TIMEOUT) delay(20);
  while(digitalRead(ENCODER_PUSH_BUTTON)==LOW && millis() - start < WELCOME_TIMEOUT) delay(20);

  prefs.begin("cfw", false, STORAGE_PARTITION);
  prefs.putString("Build", build);
  prefs.putString("Version", current);
  prefs.end();
}
