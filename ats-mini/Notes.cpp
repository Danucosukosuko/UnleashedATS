#include "Common.h"
#include "Themes.h"
#include "Draw.h"
#include "Menu.h"
#include "Ota.h"
#include "Notes.h"

#define NOTES_MAX_LINES  240  // Wrapped lines kept
#define NOTES_LINE_LEN    72
#define NOTES_WIDTH      308  // Text width on screen (pixels)
#define NOTES_TOP         22  // First text row (pixels)
#define NOTES_ROW         15  // Row height (pixels)
#define NOTES_ROWS         8  // Text rows on screen

typedef struct
{
  bool heading;
  char text[NOTES_LINE_LEN];
} NoteLine;

static NoteLine *lines = nullptr;
static int lineCount = 0;
// Scroll position; the two positions past the text select Install and Cancel
static int position = 0;

static int lastTop() { return(lineCount > NOTES_ROWS? lineCount - NOTES_ROWS : 0); }

static void addLine(const char *text, bool heading)
{
  if(lineCount >= NOTES_MAX_LINES) return;
  // Never two blank lines in a row, nor one at the top
  if(!*text && (!lineCount || !lines[lineCount-1].text[0])) return;
  lines[lineCount].heading = heading;
  strlcpy(lines[lineCount].text, text, NOTES_LINE_LEN);
  lineCount++;
}

// Markdown to plain text: headings, emphasis, code and links
static void plain(const char *src, char *dst, size_t size, bool *heading)
{
  size_t n = 0;
  *heading = false;

  while(*src==' ') src++;
  if(*src=='#')
  {
    while(*src=='#') src++;
    while(*src==' ') src++;
    *heading = true;
  }
  else if((*src=='-' || *src=='*' || *src=='+') && src[1]==' ')
  {
    dst[n++] = '-';
    dst[n++] = ' ';
    src += 2;
  }

  while(*src && n < size - 1)
  {
    if(*src=='*' || *src=='_' || *src=='`') { src++; continue; }
    if(*src=='[')
    {
      // [text](url) keeps the text
      const char *close = strstr(src, "](");
      const char *end = close? strchr(close, ')') : nullptr;
      if(close && end)
      {
        for(const char *p = src + 1 ; p < close && n < size - 1 ; p++) dst[n++] = *p;
        src = end + 1;
        continue;
      }
    }
    if(*src=='<' && !strncmp(src, "<http", 5))
    {
      // <url> keeps the url
      src++;
      while(*src && *src!='>' && n < size - 1) dst[n++] = *src++;
      if(*src=='>') src++;
      continue;
    }
    // The display fonts only have printable ASCII
    dst[n++] = (*src>=' ' && *src<='~')? *src : ' ';
    src++;
  }
  dst[n] = '\0';
}

// Break a paragraph into lines that fit the screen
static void wrap(const char *text, bool heading)
{
  const lgfx::IFont *font = FONT_SMALL;
  const char *indent = !strncmp(text, "- ", 2)? "  " : "";
  char line[NOTES_LINE_LEN] = "";

  if(!*text) { addLine("", false); return; }

  while(*text)
  {
    const char *end = text;
    while(*end && *end!=' ') end++;
    char candidate[NOTES_LINE_LEN];
    snprintf(candidate, sizeof(candidate), "%s%s%.*s", line, *line? " " : "", (int)(end - text), text);

    if(*line && (spr.textWidth(candidate, font) > NOTES_WIDTH || strlen(candidate) >= NOTES_LINE_LEN - 1))
    {
      addLine(line, heading);
      snprintf(line, sizeof(line), "%s%.*s", indent, (int)(end - text), text);
    }
    else strlcpy(line, candidate, sizeof(line));

    text = end;
    while(*text==' ') text++;
  }
  if(*line) addLine(line, heading);
}

void notesOpen()
{
  if(!lines) lines = static_cast<NoteLine *>(ps_malloc(NOTES_MAX_LINES * sizeof(NoteLine)));
  if(!lines) return;
  lineCount = position = 0;

  const char *notes = otaReleaseNotes();
  char src[256], text[256];
  while(*notes && lineCount < NOTES_MAX_LINES)
  {
    const char *end = strchr(notes, '\n');
    size_t len = end? end - notes : strlen(notes);
    if(len >= sizeof(src)) len = sizeof(src) - 1;
    memcpy(src, notes, len);
    src[len] = '\0';
    if(len && src[len-1]=='\r') src[len-1] = '\0';
    notes = end? end + 1 : notes + strlen(notes);

    bool heading;
    plain(src, text, sizeof(text), &heading);
    wrap(text, heading);
  }
  if(!lineCount) addLine("This release has no notes.", false);

  currentCmd = CMD_OTANOTES;
}

void notesRotate(int16_t enc)
{
  // Past the text come the two choices
  int last = lastTop() + 2;
  position = position + enc < 0? 0 : position + enc > last? last : position + enc;
}

void notesClick()
{
  int top = lastTop();
  if(position <= top)
  {
    // A click while reading jumps to the choices
    position = top + 1;
    return;
  }

  currentCmd = CMD_NONE;
  if(position == top + 1) otaRequestLatest(true);
}

void notesDraw()
{
  char title[48];
  int top = lastTop();
  int first = position < top? position : top;
  int choice = position - top;   // 1 Install, 2 Cancel

  spr.fillSprite(TH.bg);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TH.menu_hdr);
  snprintf(title, sizeof(title), "What's new in %s", otaReleaseTag());
  spr.drawString(title, 6, 2, FONT_SMALL);
  spr.drawFastHLine(0, 19, 320, TH.menu_border);

  for(int i=0 ; i<NOTES_ROWS && first + i < lineCount ; i++)
  {
    const NoteLine *line = &lines[first + i];
    spr.setTextColor(line->heading? TH.menu_param : TH.text);
    spr.drawString(line->text, 6, NOTES_TOP + i * NOTES_ROW, FONT_SMALL);
  }

  // Scroll bar
  if(lineCount > NOTES_ROWS)
  {
    int height = NOTES_ROWS * NOTES_ROW;
    int bar = max(8, height * NOTES_ROWS / lineCount);
    int y = NOTES_TOP + (height - bar) * first / max(1, top);
    spr.fillRect(316, y, 3, bar, TH.text_muted);
  }

  // Choices
  const char *labels[] = { "Install", "Cancel" };
  for(int i=0 ; i<2 ; i++)
  {
    int x = 70 + i * 110, y = 148;
    bool selected = choice == i + 1;
    spr.fillRoundRect(x, y, 80, 20, 4, selected? TH.menu_hl_bg : TH.box_bg);
    spr.drawRoundRect(x, y, 80, 20, 4, TH.menu_border);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(selected? TH.menu_hl_text : TH.menu_item);
    spr.drawString(labels[i], x + 40, y + 10, FONT_SMALL);
  }
  spr.pushSprite(0, 0);
}
