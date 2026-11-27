#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "UpdateScreen.h"

#include <atomic>
#include <mutex>
#include <math.h>

#define FRAME_MS    40   // 25 frames per second
#define ICON_X      66   // Center of the animated icon
#define ICON_Y      82
#define ICON_R      40
#define TEXT_X     124   // Text column
#define TEXT_W     190

static std::mutex screenMutex;
static struct
{
  UpdateIcon icon;
  char title[32];
  char line1[64];
  char line2[64];
  int percent;
  char footer[64];
  int shownPercent;   // Eased towards percent, for a smooth bar
} screen;

static std::atomic<bool> running{false};
static std::atomic<bool> stopped{true};
static TaskHandle_t task = nullptr;

static void copyText(char *dst, const char *src, size_t size)
{
  strlcpy(dst, src? src : "", size);
}

// Up to two lines of text in the column, broken between words
static int drawWrapped(const char *text, int y, uint16_t color, int maxLines)
{
  char line[64];
  int lines = 0;

  spr.setTextColor(color);
  while(*text && lines < maxLines)
  {
    int fit = 0, last = 0;
    for(int n = 1 ; text[n-1] ; n++)
    {
      snprintf(line, sizeof(line), "%.*s", n, text);
      if(spr.textWidth(line, FONT_SMALL) > TEXT_W) break;
      fit = n;
      if(text[n]==' ' || !text[n]) last = n;
    }
    int take = (last && text[fit]) ? last : fit;
    if(!take) break;
    snprintf(line, sizeof(line), "%.*s", take, text);
    spr.drawString(line, TEXT_X, y, FONT_SMALL);
    text += take;
    while(*text==' ') text++;
    y += 16;
    lines++;
  }
  return(y);
}

static void drawIcon(UpdateIcon icon, int percent, uint32_t t)
{
  const int cx = ICON_X, cy = ICON_Y, r = ICON_R;
  uint16_t accent = TH.smeter_bar;
  uint16_t track  = TH.smeter_bar_empty;

  // The ring: progress when known, a spinning arc otherwise
  spr.fillArc(cx, cy, r - 5, r, 0, 360, track);
  if(icon == UPDATE_ICON_OK)        spr.fillArc(cx, cy, r - 5, r, 0, 360, accent);
  else if(icon == UPDATE_ICON_FAIL) spr.fillArc(cx, cy, r - 5, r, 0, 360, TH.text_warn);
  else if(icon == UPDATE_ICON_NEW)  spr.fillArc(cx, cy, r - 5, r, 0, 360, TH.freq_hl_sel);
  else if(percent >= 0)
  {
    if(percent > 0) spr.fillArc(cx, cy, r - 5, r, -90, -90 + 3.6f * percent, accent);
    // A bright spark running round the ring
    float a = (t / 4) % 360;
    spr.fillArc(cx, cy, r - 5, r, a, a + 14, TH.text);
  }
  else
  {
    float a = (t / 3) % 360;
    spr.fillArc(cx, cy, r - 5, r, a, a + 80, accent);
    spr.fillArc(cx, cy, r - 5, r, a + 180, a + 260, accent);
  }

  switch(icon)
  {
    case UPDATE_ICON_SEARCH:
    {
      // An antenna sending waves out to both sides
      spr.fillTriangle(cx - 7, cy + 18, cx + 7, cy + 18, cx, cy - 4, TH.text_muted);
      spr.fillCircle(cx, cy - 6, 3, TH.text);
      for(int k=0 ; k<3 ; k++)
      {
        int radius = 8 + (int)((t / 18 + k * 9) % 27);
        uint16_t color = radius < 20? TH.text : TH.text_muted;
        spr.fillArc(cx, cy - 6, radius, radius + 2, -40, 40, color);
        spr.fillArc(cx, cy - 6, radius, radius + 2, 140, 220, color);
      }
      break;
    }

    case UPDATE_ICON_DOWNLOAD:
    {
      // Arrows falling into a tray
      spr.setClipRect(cx - 16, cy - 24, 32, 36);
      int offset = (int)((t / 22) % 24);
      for(int k=-1 ; k<1 ; k++)
      {
        int y = cy - 22 + offset + k * 24;
        spr.fillRect(cx - 3, y, 7, 12, TH.text);
        spr.fillTriangle(cx - 10, y + 11, cx + 10, y + 11, cx, y + 21, TH.text);
      }
      spr.clearClipRect();
      spr.fillRect(cx - 16, cy + 14, 33, 3, TH.text_muted);
      spr.fillRect(cx - 16, cy + 6, 3, 9, TH.text_muted);
      spr.fillRect(cx + 14, cy + 6, 3, 9, TH.text_muted);
      break;
    }

    case UPDATE_ICON_VERIFY:
    {
      // A chip, with its pins lighting up in turn
      spr.fillRoundRect(cx - 14, cy - 14, 29, 29, 3, TH.text_muted);
      spr.fillRect(cx - 7, cy - 7, 15, 15, TH.bg);
      int lit = (t / 120) % 12;
      for(int k=0 ; k<12 ; k++)
      {
        uint16_t color = k == lit? accent : TH.text;
        int pos = -10 + (k % 3) * 9;
        switch(k / 3)
        {
          case 0: spr.fillRect(cx + pos, cy - 20, 3, 5, color); break;
          case 1: spr.fillRect(cx + 16, cy + pos, 5, 3, color); break;
          case 2: spr.fillRect(cx - pos, cy + 16, 3, 5, color); break;
          case 3: spr.fillRect(cx - 20, cy - pos, 5, 3, color); break;
        }
      }
      break;
    }

    case UPDATE_ICON_OK:
      spr.drawWideLine(cx - 16, cy + 1, cx - 5, cy + 13, 3.5f, accent);
      spr.drawWideLine(cx - 5, cy + 13, cx + 17, cy - 12, 3.5f, accent);
      break;

    case UPDATE_ICON_NEW:
      // An arrow pointing up, to the newer version
      spr.fillTriangle(cx - 14, cy - 2, cx + 14, cy - 2, cx, cy - 18, TH.freq_hl_sel);
      spr.fillRect(cx - 5, cy - 3, 11, 20, TH.freq_hl_sel);
      break;

    case UPDATE_ICON_FAIL:
      spr.drawWideLine(cx - 13, cy - 13, cx + 13, cy + 13, 3.5f, TH.text_warn);
      spr.drawWideLine(cx - 13, cy + 13, cx + 13, cy - 13, 3.5f, TH.text_warn);
      break;
  }
}

static void drawFrame()
{
  uint32_t t = millis();
  std::lock_guard<std::mutex> lock(screenMutex);

  // The bar eases towards the real progress
  if(screen.percent < 0) screen.shownPercent = -1;
  else if(screen.shownPercent < 0 || screen.shownPercent > screen.percent) screen.shownPercent = screen.percent;
  else if(screen.shownPercent < screen.percent) screen.shownPercent += max(1, (screen.percent - screen.shownPercent) / 4);

  spr.fillSprite(TH.bg);
  drawIcon(screen.icon, screen.shownPercent, t);

  // Title, with dots that keep moving while something is going on
  bool busy = screen.icon == UPDATE_ICON_SEARCH || screen.icon == UPDATE_ICON_DOWNLOAD || screen.icon == UPDATE_ICON_VERIFY;
  char title[40];
  snprintf(title, sizeof(title), "%s%s", screen.title, busy? &"..."[3 - (t / 400) % 4] : "");
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_hdr);
  spr.drawString(title, TEXT_X, 34, FONT_SMALL);
  spr.drawFastHLine(TEXT_X, 52, TEXT_W, TH.menu_border);

  int y = drawWrapped(screen.line1, 60, TH.text, 2);
  drawWrapped(screen.line2, y, TH.text_muted, 2);

  if(screen.shownPercent >= 0)
  {
    // Progress bar with the percentage
    int barY = 104, fill = (TEXT_W - 40) * screen.shownPercent / 100;
    spr.drawRoundRect(TEXT_X, barY, TEXT_W - 40, 12, 3, TH.box_border);
    if(fill > 4) spr.fillRoundRect(TEXT_X + 2, barY + 2, fill - 4, 8, 2, TH.smeter_bar);
    char percent[8];
    snprintf(percent, sizeof(percent), "%d%%", screen.shownPercent);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(TH.text);
    spr.drawString(percent, TEXT_X + TEXT_W, barY - 2, FONT_SMALL);
  }

  if(screen.footer[0])
  {
    spr.fillRect(0, 148, DISPLAY_WIDTH, 22, TH.box_bg);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(TH.box_text);
    spr.drawString(screen.footer, DISPLAY_WIDTH / 2, 159, FONT_SMALL);
  }

  spr.pushSprite(0, 0);
}

static void animate(void *)
{
  while(running)
  {
    drawFrame();
    vTaskDelay(pdMS_TO_TICKS(FRAME_MS));
  }
  stopped = true;
  task = nullptr;
  vTaskDelete(nullptr);
}

void updateScreenShow(UpdateIcon icon, const char *title, const char *line1, const char *line2,
                      int percent, const char *footer)
{
  {
    std::lock_guard<std::mutex> lock(screenMutex);
    if(icon != screen.icon || !running) screen.shownPercent = -1;
    screen.icon = icon;
    copyText(screen.title, title, sizeof(screen.title));
    copyText(screen.line1, line1, sizeof(screen.line1));
    copyText(screen.line2, line2, sizeof(screen.line2));
    copyText(screen.footer, footer, sizeof(screen.footer));
    screen.percent = percent > 100? 100 : percent;
  }

  if(running) return;

  // The update screen wakes the display
  if(sleepOn()) sleepOn(false);
  ledcWrite(PIN_LCD_BL, currentBrt);

  // Drawing passes to the animation task until updateScreenHide()
  running = true;
  stopped = false;
  if(xTaskCreatePinnedToCore(animate, "UpdateScreen", 6144, nullptr, 1, &task, ARDUINO_RUNNING_CORE) != pdPASS)
  {
    running = false;
    stopped = true;
    drawFrame();
  }
}

void updateScreenHide()
{
  if(!running) return;
  running = false;
  for(int i=0 ; i<100 && !stopped ; i++) delay(5);
}

void updateScreenResult(UpdateIcon icon, const char *title, const char *line1, const char *line2,
                        uint32_t holdMs)
{
  updateScreenHide();
  {
    std::lock_guard<std::mutex> lock(screenMutex);
    screen.icon = icon;
    copyText(screen.title, title, sizeof(screen.title));
    copyText(screen.line1, line1, sizeof(screen.line1));
    copyText(screen.line2, line2, sizeof(screen.line2));
    copyText(screen.footer, "Click to continue", sizeof(screen.footer));
    screen.percent = screen.shownPercent = -1;
  }
  if(sleepOn()) sleepOn(false);
  ledcWrite(PIN_LCD_BL, currentBrt);
  drawFrame();

  // Wait for the time, or a click: press and release
  uint32_t start = millis();
  bool pressed = false;
  while(millis() - start < holdMs)
  {
    bool down = digitalRead(ENCODER_PUSH_BUTTON) == LOW;
    if(pressed && !down) break;
    pressed = down;
    delay(20);
  }
}
