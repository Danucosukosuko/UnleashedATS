#include "Common.h"
#include "Themes.h"
#include "Draw.h"
#include "Menu.h"
#include "Utils.h"
#include "DxWatch.h"
#include "DxLog.h"

#define DXLOG_MAX   200  // Catches shown, the newest ones
#define DXLOG_TOP    22  // First row (pixels)
#define DXLOG_ROW    16  // Row height (pixels)
#define DXLOG_ROWS    9  // Rows on screen

static DxLogEntry *entries = nullptr;
static int count = 0;
static int selected = 0;      // 0 Back, 1..count catches, count+1 Clear log
static bool armed = false;    // Clear log waits for a second click

static int rows() { return(count + 2); }

void dxLogOpen()
{
  if(!entries) entries = static_cast<DxLogEntry *>(ps_malloc(DXLOG_MAX * sizeof(DxLogEntry)));
  count = entries? dxWatchLoadLog(entries, DXLOG_MAX) : 0;
  selected = count? 1 : 0;
  armed = false;
  statusShow(nullptr);
  currentCmd = CMD_DXLOG;
}

void dxLogRotate(int16_t enc)
{
  selected = constrain(selected + enc, 0, rows() - 1);
  armed = false;
}

void dxLogClick()
{
  if(selected == 0)
  {
    currentCmd = CMD_NONE;
    return;
  }

  if(selected == rows() - 1)
  {
    if(!armed) { armed = true; return; }
    dxWatchClearLog();
    count = 0;
    selected = 0;
    armed = false;
    statusShow("DX Watch log", "Cleared");
    currentCmd = CMD_NONE;
    return;
  }

  // Back to a catch
  const DxLogEntry *e = &entries[selected - 1];
  currentCmd = CMD_NONE;
  if(e->freq < 64000000 || e->freq > 108000000) return;
  if(currentMode != FM) selectBroadcastBand("FM");
  if(currentMode == FM) updateFrequency(e->freq, false);
}

void dxLogDraw()
{
  char line[64];

  spr.fillSprite(TH.bg);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_hdr);
  spr.drawString("DX Watch log", 6, 2, FONT_SMALL);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(TH.text_muted);
  snprintf(line, sizeof(line), count == 1? "1 catch" : "%d catches", count);
  spr.drawString(line, 314, 2, FONT_SMALL);
  spr.drawFastHLine(0, 19, 320, TH.menu_border);

  int first = selected - DXLOG_ROWS / 2;
  first = first < 0? 0 : first > rows() - DXLOG_ROWS? max(0, rows() - DXLOG_ROWS) : first;

  for(int r=0 ; r<DXLOG_ROWS && first + r < rows() ; r++)
  {
    int item = first + r, y = DXLOG_TOP + r * DXLOG_ROW;
    bool hl = item == selected;
    if(hl) spr.fillRect(0, y - 1, 320, DXLOG_ROW, TH.menu_hl_bg);
    spr.setTextColor(hl? TH.menu_hl_text : TH.menu_item);
    spr.setTextDatum(TL_DATUM);

    if(item == 0)
      spr.drawString(count? "Back" : "Back - no catches yet", 8, y, FONT_SMALL);
    else if(item == rows() - 1)
    {
      spr.setTextColor(hl? TH.menu_hl_text : TH.text_warn);
      spr.drawString(armed? "Click again to clear the log" : "Clear log", 8, y, FONT_SMALL);
    }
    else
    {
      const DxLogEntry *e = &entries[item - 1];
      snprintf(line, sizeof(line), "%.1f", e->freq / 1000000.0);
      spr.drawString(line, 8, y, FONT_SMALL);
      snprintf(line, sizeof(line), "%04X", e->pi);
      spr.drawString(line, 64, y, FONT_SMALL);
      spr.drawString(e->ps[0]? e->ps : "-", 108, y, FONT_SMALL);

      // Month, day and UTC time from the ISO stamp
      spr.setTextDatum(TR_DATUM);
      if(strlen(e->time) >= 16)
        snprintf(line, sizeof(line), "%.2s/%.2s %.5s", e->time + 8, e->time + 5, e->time + 11);
      else
        snprintf(line, sizeof(line), "%u dBuV", e->rssi);
      if(!hl) spr.setTextColor(TH.text_muted);
      spr.drawString(line, 312, y, FONT_SMALL);
    }
  }

  spr.pushSprite(0, 0);
}
