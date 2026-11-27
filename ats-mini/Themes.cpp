#include "Common.h"
#include "Themes.h"
#include "Draw.h"
#include "Storage.h"

#include <Preferences.h>
#include <mutex>
#include <stddef.h>

ColorTheme theme[] =
{
  {
    "Default",
    0x0000, // bg
    0xFFFF, // text
    0xD69A, // text_muted
    0xF800, // text_warn
    0xD69A, // smeter_icon
    0x07E0, // smeter_bar
    0xF800, // smeter_bar_plus
    0x3186, // smeter_bar_empty
    0xF800, // save_icon
    0xD69A, // stereo_icon
    0xF800, // rf_icon
    0x07E0, // rf_icon_conn
    0xFFFF, // batt_voltage
    0xFFFF, // batt_border
    0x07E0, // batt_full
    0xF800, // batt_low
    0x0000, // batt_charge
    0xFFE0, // batt_icon
    0xD69A, // band_text
    0xD69A, // mode_text
    0xD69A, // mode_border
    0x0000, // box_bg
    0xD69A, // box_border
    0xD69A, // box_text
    0xF800, // box_off_bg
    0xBEDF, // box_off_text
    0x0000, // menu_bg
    0xF800, // menu_border
    0xFFFF, // menu_hdr
    0xBEDF, // menu_item
    0x105B, // menu_hl_bg
    0xBEDF, // menu_hl_text
    0xBEDF, // menu_param
    0xFFFF, // freq_text
    0xD69A, // funit_text
    0xF800, // freq_hl
    0xFFE0, // freq_hl_sel
    0xD69A, // rds_text
    0xFFFF, // scale_text
    0xF800, // scale_pointer
    0xC638, // scale_line
    0x94B2, // scan_grid
    0x0659, // scan_snr
    0x07E0, // scan_rssi
  },

  {
    "Bluesky",
    0x2293, // bg
    0xFFFF, // text
    0xD69A, // text_muted
    0xF800, // text_warn
    0xD69A, // smeter_icon
    0x07E0, // smeter_bar
    0xF800, // smeter_bar_plus
    0x3AF3, // smeter_bar_empty
    0xF800, // save_icon
    0xD69A, // stereo_icon
    0xF800, // rf_icon
    0x07E0, // rf_icon_conn
    0xFFFF, // batt_voltage
    0xFFFF, // batt_border
    0x07E0, // batt_full
    0xF800, // batt_low
    0x2293, // batt_charge
    0xFFE0, // batt_icon
    0xD69A, // band_text
    0xD69A, // mode_text
    0xD69A, // mode_border
    0x2293, // box_bg
    0xD69A, // box_border
    0xD69A, // box_text
    0xF800, // box_off_bg
    0xBEDF, // box_off_text
    0x2293, // menu_bg
    0xF800, // menu_border
    0xFFFF, // menu_hdr
    0xBEDF, // menu_item
    0xD69A, // menu_hl_bg
    0x2293, // menu_hl_text
    0xBEDF, // menu_param
    0xFFFF, // freq_text
    0xD69A, // funit_text
    0xF800, // freq_hl
    0xFFE0, // freq_hl_sel
    0xD69A, // rds_text
    0xFFFF, // scale_text
    0xF800, // scale_pointer
    0xC638, // scale_line
    0x94B2, // scan_grid
    0x07FF, // scan_snr
    0x07E0, // scan_rssi
  },

  {
    "eInk",
    0xC616, // bg
    0x3A08, // text
    0x632C, // text_muted
    0xF800, // text_warn
    0x18C3, // smeter_icon
    0x632C, // smeter_bar
    0x18C3, // smeter_bar_plus
    0xB594, // smeter_bar_empty
    0x632C, // save_icon
    0x632C, // stereo_icon
    0x3A08, // rf_icon
    0x632C, // rf_icon_conn
    0x18C3, // batt_voltage
    0x0000, // batt_border
    0x632C, // batt_full
    0x3A08, // batt_low
    0xC616, // batt_charge
    0x3A08, // batt_icon
    0x3A08, // band_text
    0x632C, // mode_text
    0x632C, // mode_border
    0xC616, // box_bg
    0x3A08, // box_border
    0x3A08, // box_text
    0x632C, // box_off_bg
    0xC616, // box_off_text
    0xC616, // menu_bg
    0x3A08, // menu_border
    0x18C3, // menu_hdr
    0x3A08, // menu_item
    0x3A08, // menu_hl_bg
    0xC616, // menu_hl_text
    0x3A08, // menu_param
    0x3A08, // freq_text
    0x632C, // funit_text
    0x0000, // freq_hl
    0x632C, // freq_hl_sel
    0x632C, // rds_text
    0x18C3, // scale_text
    0x0000, // scale_pointer
    0x632C, // scale_line
    0x94B2, // scan_grid
    0x94B2, // scan_snr
    0x18C3, // scan_rssi
  },

  {
    "Pager",
    0x4309, // bg
    0x00C2, // text
    0x1165, // text_muted
    0xF800, // text_warn
    0x18C3, // smeter_icon
    0x1165, // smeter_bar
    0x00C2, // smeter_bar_plus
    0x3287, // smeter_bar_empty
    0x18C3, // save_icon
    0x00C2, // stereo_icon
    0x00C2, // rf_icon
    0x1165, // rf_icon_conn
    0x18C3, // batt_voltage
    0x0000, // batt_border
    0x1165, // batt_full
    0x00C2, // batt_low
    0x4309, // batt_charge
    0x00C2, // batt_icon
    0x00C2, // band_text
    0x00C2, // mode_text
    0x00C2, // mode_border
    0x4309, // box_bg
    0x00C2, // box_border
    0x00C2, // box_text
    0x00C2, // box_off_bg
    0x4309, // box_off_text
    0x4309, // menu_bg
    0x00C2, // menu_border
    0x18C3, // menu_hdr
    0x00C2, // menu_item
    0x00C2, // menu_hl_bg
    0x4309, // menu_hl_text
    0x00C2, // menu_param
    0x00C2, // freq_text
    0x1165, // funit_text
    0x0000, // freq_hl
    0x1165, // freq_hl_sel
    0x1165, // rds_text
    0x18C3, // scale_text
    0x0000, // scale_pointer
    0x1165, // scale_line
    0x18C3, // scan_grid
    0x2A25, // scan_snr
    0x00C2, // scan_rssi
  },

  {
    "Orange",
    0xF3C1, // bg
    0x18C3, // text
    0x2945, // text_muted
    0xF800, // text_warn
    0x18C3, // smeter_icon
    0x4208, // smeter_bar
    0x2945, // smeter_bar_plus
    0xE320, // smeter_bar_empty
    0x4208, // save_icon
    0x2945, // stereo_icon
    0x2945, // rf_icon
    0x4208, // rf_icon_conn
    0x18C3, // batt_voltage
    0x0000, // batt_border
    0x4208, // batt_full
    0x2945, // batt_low
    0xF3C1, // batt_charge
    0x18C3, // batt_icon
    0x18C3, // band_text
    0x2945, // mode_text
    0x2945, // mode_border
    0xF3C1, // box_bg
    0x2945, // box_border
    0x2945, // box_text
    0x2945, // box_off_bg
    0xF3C1, // box_off_text
    0xF3C1, // menu_bg
    0x18C3, // menu_border
    0x18C3, // menu_hdr
    0x2945, // menu_item
    0x2945, // menu_hl_bg
    0xF3C1, // menu_hl_text
    0x2945, // menu_param
    0x18C3, // freq_text
    0x2945, // funit_text
    0x0000, // freq_hl
    0x4208, // freq_hl_sel
    0x2945, // rds_text
    0x18C3, // scale_text
    0x0000, // scale_pointer
    0x4208, // scale_line
    0x6B4D, // scan_grid
    0x5ACB, // scan_snr
    0x2945, // scan_rssi
  },

  {
    "Night",
    0x0000, // bg
    0xD986, // text
    0xB925, // text_muted
    0xF800, // text_warn
    0xB925, // smeter_icon
    0x8925, // smeter_bar
    0xF800, // smeter_bar_plus
    0x2104, // smeter_bar_empty
    0xF800, // save_icon
    0xB925, // stereo_icon
    0xF800, // rf_icon
    0x8925, // rf_icon_conn
    0xD986, // batt_voltage
    0xD986, // batt_border
    0x8925, // batt_full
    0xF800, // batt_low
    0x0000, // batt_charge
    0xF800, // batt_icon
    0xB925, // band_text
    0xB925, // mode_text
    0xB925, // mode_border
    0x0000, // box_bg
    0xB925, // box_border
    0xB925, // box_text
    0x70C3, // box_off_bg
    0xD986, // box_off_text
    0x0000, // menu_bg
    0xF800, // menu_border
    0xD986, // menu_hdr
    0xF800, // menu_item
    0xD986, // menu_hl_bg
    0x0000, // menu_hl_text
    0xD986, // menu_param
    0xD986, // freq_text
    0xB925, // funit_text
    0xF800, // freq_hl
    0xD986, // freq_hl_sel
    0xB925, // rds_text
    0xD986, // scale_text
    0xF800, // scale_pointer
    0xB925, // scale_line
    0x8925, // scan_grid
    0x8925, // scan_snr
    0xF800, // scan_rssi
  },

  {
    "Phosphor",
    0x0060, // bg
    0x07AD, // text
    0x052D, // text_muted
    0xF800, // text_warn
    0x052D, // smeter_icon
    0x052D, // smeter_bar
    0x07AD, // smeter_bar_plus
    0x00E0, // smeter_bar_empty
    0x2364, // save_icon
    0x052D, // stereo_icon
    0x0309, // rf_icon
    0x052D, // rf_icon_conn
    0x052D, // batt_voltage
    0x07AD, // batt_border
    0x052D, // batt_full
    0x0309, // batt_low
    0x0060, // batt_charge
    0x07AD, // batt_icon
    0x052D, // band_text
    0x052D, // mode_text
    0x052D, // mode_border
    0x0060, // box_bg
    0x052D, // box_border
    0x052D, // box_text
    0x0309, // box_off_bg
    0x07AD, // box_off_text
    0x0060, // menu_bg
    0x2364, // menu_border
    0x07AD, // menu_hdr
    0x052D, // menu_item
    0x0309, // menu_hl_bg
    0x07AD, // menu_hl_text
    0x07AD, // menu_param
    0x07AD, // freq_text
    0x052D, // funit_text
    0x5CF2, // freq_hl
    0x07AD, // freq_hl_sel
    0x052D, // rds_text
    0x07AD, // scale_text
    0x5CF2, // scale_pointer
    0x2364, // scale_line
    0x2364, // scan_grid
    0x2364, // scan_snr
    0x052D, // scan_rssi
  },

  {
    "Space",
    0x0004, // bg
    0x3FE0, // text
    0xD69A, // text_muted
    0xF800, // text_warn
    0xD69A, // smeter_icon
    0x07E0, // smeter_bar
    0xF800, // smeter_bar_plus
    0x3186, // smeter_bar_empty
    0xF800, // save_icon
    0xD69A, // stereo_icon
    0xF800, // rf_icon
    0x07E0, // rf_icon_conn
    0xD69A, // batt_voltage
    0xD69A, // batt_border
    0x07E0, // batt_full
    0xF800, // batt_low
    0x0004, // batt_charge
    0xFFE0, // batt_icon
    0xD69A, // band_text
    0xD69A, // mode_text
    0xD69A, // mode_border
    0x0004, // box_bg
    0xD69A, // box_border
    0xD69A, // box_text
    0xF800, // box_off_bg
    0xBEDF, // box_off_text
    0x0004, // menu_bg
    0xF800, // menu_border
    0x3FE0, // menu_hdr
    0xBEDF, // menu_item
    0x105B, // menu_hl_bg
    0xBEDF, // menu_hl_text
    0xBEDF, // menu_param
    0x3FE0, // freq_text
    0xD69A, // funit_text
    0xF800, // freq_hl
    0xD69A, // freq_hl_sel
    0xD69A, // rds_text
    0x3FE0, // scale_text
    0xF800, // scale_pointer
    0xC638, // scale_line
    0x6B4D, // scan_grid
    0x001F, // scan_snr
    0x07E0, // scan_rssi
  },

  {
    "Magenta",
    0xA12B, // bg
    0xFFFF, // text
    0xFD95, // text_muted
    0xFD00, // text_warn
    0xC638, // smeter_icon
    0xD3F2, // smeter_bar
    0xFD95, // smeter_bar_plus
    0x8829, // smeter_bar_empty
    0x5005, // save_icon
    0xC638, // stereo_icon
    0x7007, // rf_icon
    0xFD95, // rf_icon_conn
    0xC638, // batt_voltage
    0xC638, // batt_border
    0xFD95, // batt_full
    0x7007, // batt_low
    0xA12B, // batt_charge
    0xFFE0, // batt_icon
    0xC638, // band_text
    0xC638, // mode_text
    0xC638, // mode_border
    0xA12B, // box_bg
    0xC638, // box_border
    0xC638, // box_text
    0x7007, // box_off_bg
    0xFD95, // box_off_text
    0xA12B, // menu_bg
    0x5005, // menu_border
    0xFFFF, // menu_hdr
    0xBEDF, // menu_item
    0x5005, // menu_hl_bg
    0xBEDF, // menu_hl_text
    0xBEDF, // menu_param
    0xFFFF, // freq_text
    0xC638, // funit_text
    0x5005, // freq_hl
    0xFFFF, // freq_hl_sel
    0xFD95, // rds_text
    0xFFFF, // scale_text
    0x5005, // scale_pointer
    0xC638, // scale_line
    0xD3F2, // scan_grid
    0xD3F2, // scan_snr
    0xFD95, // scan_rssi
  },

  {
    "Light",
    0xFFFF, // bg
    0x0000, // text
    0x632C, // text_muted
    0x0000, // text_warn
    0x18C3, // smeter_icon
    0x4208, // smeter_bar
    0x18C3, // smeter_bar_plus
    0xB596, // smeter_bar_empty
    0x0000, // save_icon
    0x632C, // stereo_icon
    0x4208, // rf_icon
    0x0000, // rf_icon_conn
    0x0000, // batt_voltage
    0x0000, // batt_border
    0x4208, // batt_full
    0x0000, // batt_low
    0xFFFF, // batt_charge
    0x0000, // batt_icon
    0x0000, // band_text
    0x0000, // mode_text
    0x0000, // mode_border
    0xFFFF, // box_bg
    0x0000, // box_border
    0x0000, // box_text
    0x632C, // box_off_bg
    0xFFFF, // box_off_text
    0xFFFF, // menu_bg
    0x0000, // menu_border
    0x0000, // menu_hdr
    0x0000, // menu_item
    0x0000, // menu_hl_bg
    0xFFFF, // menu_hl_text
    0x0000, // menu_param
    0x0000, // freq_text
    0x0000, // funit_text
    0x0000, // freq_hl
    0x632C, // freq_hl_sel
    0x0000, // rds_text
    0x0000, // scale_text
    0x0000, // scale_pointer
    0x0000, // scale_line
    0x632C, // scan_grid
    0x18C3, // scan_snr
    0x18C3, // scan_rssi
  },

  {
    "Dark",
    0x0000, // bg
    0xFFFF, // text
    0x9CD3, // text_muted
    0xFFFF, // text_warn
    0xE73C, // smeter_icon
    0xBDF7, // smeter_bar
    0xE73C, // smeter_bar_plus
    0x4A69, // smeter_bar_empty
    0xFFFF, // save_icon
    0x9CD3, // stereo_icon
    0xBDF7, // rf_icon
    0xFFFF, // rf_icon_conn
    0xFFFF, // batt_voltage
    0xFFFF, // batt_border
    0xBDF7, // batt_full
    0xFFFF, // batt_low
    0x0000, // batt_charge
    0xFFFF, // batt_icon
    0xFFFF, // band_text
    0xFFFF, // mode_text
    0xFFFF, // mode_border
    0x0000, // box_bg
    0xFFFF, // box_border
    0xFFFF, // box_text
    0x9CD3, // box_off_bg
    0x0000, // box_off_text
    0x0000, // menu_bg
    0xFFFF, // menu_border
    0xFFFF, // menu_hdr
    0xFFFF, // menu_item
    0xFFFF, // menu_hl_bg
    0x0000, // menu_hl_text
    0xFFFF, // menu_param
    0xFFFF, // freq_text
    0xFFFF, // funit_text
    0xFFFF, // freq_hl
    0x9CD3, // freq_hl_sel
    0xFFFF, // rds_text
    0xFFFF, // scale_text
    0xFFFF, // scale_pointer
    0xFFFF, // scale_line
    0x9CD3, // scan_grid
    0xE73C, // scan_snr
    0xE73C, // scan_rssi
  },
};

#define THEME_COLORS ((sizeof(ColorTheme) - offsetof(ColorTheme, bg)) / sizeof(uint16_t))
static_assert(THEME_COLORS == 44, "Update the color names below with the theme fields");

static const char *const colorNames[THEME_COLORS] =
{
  "bg", "text", "text_muted", "text_warn",
  "smeter_icon", "smeter_bar", "smeter_bar_plus", "smeter_bar_empty",
  "save_icon", "stereo_icon", "rf_icon", "rf_icon_conn",
  "batt_voltage", "batt_border", "batt_full", "batt_low", "batt_charge", "batt_icon",
  "band_text", "mode_text", "mode_border",
  "box_bg", "box_border", "box_text", "box_off_bg", "box_off_text",
  "menu_bg", "menu_border", "menu_hdr", "menu_item", "menu_hl_bg", "menu_hl_text", "menu_param",
  "freq_text", "funit_text", "freq_hl", "freq_hl_sel",
  "rds_text", "scale_text", "scale_pointer", "scale_line",
  "scan_grid", "scan_snr", "scan_rssi",
};

static constexpr int builtinCount = ITEM_COUNT(theme);
static_assert(builtinCount <= 32, "Hidden built-in themes are kept as a 32 bit mask");

typedef struct __attribute__((packed))
{
  char name[THEME_NAME_LEN + 1];
  uint16_t colors[THEME_COLORS];
} ThemeBlob;

static ColorTheme custom[THEME_CUSTOM_MAX];
static char customName[THEME_CUSTOM_MAX][THEME_NAME_LEN + 1];
static bool customUsed[THEME_CUSTOM_MAX];
static uint32_t hiddenMask = 0;

static ColorTheme *listed[builtinCount + THEME_CUSTOM_MAX] = { &theme[0] };
static int16_t listedId[builtinCount + THEME_CUSTOM_MAX] = { 0 };
static int listedCount = 1;

static ColorTheme preview;

uint8_t themeIdx = 0;
ColorTheme *currentTheme = &theme[0];

static uint16_t *colorsOf(ColorTheme *t)
{
  return(reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(t) + offsetof(ColorTheme, bg)));
}

int getTotalThemes() { return(listedCount); }
const char *themeName(int idx) { return(listed[idx>=0 && idx<listedCount? idx : 0]->name); }
int themeId(int idx) { return(listedId[idx>=0 && idx<listedCount? idx : 0]); }
int themeColorCount() { return(THEME_COLORS); }
const char *themeColorName(int idx) { return(idx>=0 && idx<(int)THEME_COLORS? colorNames[idx] : ""); }

void themeSelect(int idx)
{
  themeIdx = idx>=0 && idx<listedCount? idx : 0;
  currentTheme = listed[themeIdx];
}

void themeSelectId(int id)
{
  for(int i=0 ; i<listedCount ; i++)
    if(listedId[i]==id) { themeSelect(i); return; }
  themeSelect(0);
}

static void rebuild()
{
  int id = themeId(themeIdx);

  // At least one theme always stays listed
  if((hiddenMask & ((1ULL << builtinCount) - 1)) == ((1ULL << builtinCount) - 1)) hiddenMask &= ~1UL;

  listedCount = 0;
  for(int i=0 ; i<builtinCount ; i++)
    if(!(hiddenMask & (1UL << i)))
    {
      listed[listedCount] = &theme[i];
      listedId[listedCount++] = i;
    }
  for(int k=0 ; k<THEME_CUSTOM_MAX ; k++)
    if(customUsed[k])
    {
      listed[listedCount] = &custom[k];
      listedId[listedCount++] = THEME_CUSTOM_ID + k;
    }

  themeSelectId(id);
}

void themesInit()
{
  Preferences store;

  memset(customUsed, 0, sizeof(customUsed));
  hiddenMask = 0;

  if(store.begin("themes", true, STORAGE_PARTITION))
  {
    hiddenMask = store.getUInt("Hidden", 0);
    for(int k=0 ; k<THEME_CUSTOM_MAX ; k++)
    {
      char key[8];
      ThemeBlob blob;
      sprintf(key, "C%d", k);
      if(store.getBytes(key, &blob, sizeof(blob)) != sizeof(blob)) continue;
      blob.name[THEME_NAME_LEN] = '\0';
      strcpy(customName[k], blob.name);
      custom[k].name = customName[k];
      memcpy(colorsOf(&custom[k]), blob.colors, sizeof(blob.colors));
      customUsed[k] = true;
    }
    store.end();
  }

  rebuild();
}

static void storeCustom(int k)
{
  Preferences store;
  char key[8];
  sprintf(key, "C%d", k);
  if(!store.begin("themes", false, STORAGE_PARTITION)) return;
  if(customUsed[k])
  {
    ThemeBlob blob;
    memset(&blob, 0, sizeof(blob));
    strncpy(blob.name, customName[k], THEME_NAME_LEN);
    memcpy(blob.colors, colorsOf(&custom[k]), sizeof(blob.colors));
    store.putBytes(key, &blob, sizeof(blob));
  }
  else store.remove(key);
  store.end();
}

static void storeHidden()
{
  Preferences store;
  if(!store.begin("themes", false, STORAGE_PARTITION)) return;
  store.putUInt("Hidden", hiddenMask);
  store.end();
}

//
// Requests from the web theme editor
//
static std::mutex themeMutex;
static struct
{
  bool pending;
  uint8_t op;
  int16_t id;
  bool hasColors;
  char name[THEME_NAME_LEN + 1];
  uint16_t colors[THEME_COLORS];
} request;
static const char *lastMessage = "";

bool themesRequest(uint8_t op, int id, const char *name, const uint16_t *colors)
{
  std::lock_guard<std::mutex> lock(themeMutex);
  if(request.pending) return(false);

  request.op = op;
  request.id = id;
  request.hasColors = colors != nullptr;
  if(colors) memcpy(request.colors, colors, sizeof(request.colors));

  // Printable ASCII only, the display fonts have nothing else
  int n = 0;
  for(const char *p = name? name : "" ; *p && n<THEME_NAME_LEN ; p++)
    if(*p>=' ' && *p<='~') request.name[n++] = *p;
  while(n && request.name[n-1]==' ') n--;
  request.name[n] = '\0';

  request.pending = true;
  return(true);
}

bool themesTick()
{
  std::lock_guard<std::mutex> lock(themeMutex);
  if(!request.pending) return(false);
  request.pending = false;

  int id = request.id;
  bool isCustom = id>=THEME_CUSTOM_ID && id<THEME_CUSTOM_ID+THEME_CUSTOM_MAX && customUsed[id-THEME_CUSTOM_ID];
  bool isBuiltin = id>=0 && id<builtinCount;

  switch(request.op)
  {
    case THEME_OP_SELECT:
      themeSelectId(id);
      prefsRequestSave(SAVE_SETTINGS);
      lastMessage = "Theme selected.";
      break;

    case THEME_OP_SAVE:
    {
      if(!request.hasColors) { lastMessage = "No colors given."; break; }
      int k = isCustom? id - THEME_CUSTOM_ID : -1;
      for(int i=0 ; k<0 && i<THEME_CUSTOM_MAX ; i++)
        if(!customUsed[i]) k = i;
      if(k<0) { lastMessage = "All custom theme slots are taken, delete one first."; break; }

      if(request.name[0]) strcpy(customName[k], request.name);
      else sprintf(customName[k], "Custom %d", k + 1);
      custom[k].name = customName[k];
      memcpy(colorsOf(&custom[k]), request.colors, sizeof(request.colors));
      customUsed[k] = true;
      storeCustom(k);
      rebuild();
      themeSelectId(THEME_CUSTOM_ID + k);
      prefsRequestSave(SAVE_SETTINGS);
      lastMessage = "Theme saved.";
      break;
    }

    case THEME_OP_DELETE:
      if(!isCustom) { lastMessage = "Only custom themes can be deleted, built-in ones can be hidden."; break; }
      customUsed[id - THEME_CUSTOM_ID] = false;
      storeCustom(id - THEME_CUSTOM_ID);
      rebuild();
      prefsRequestSave(SAVE_SETTINGS);
      lastMessage = "Theme deleted.";
      break;

    case THEME_OP_HIDE:
    case THEME_OP_SHOW:
      if(!isBuiltin) { lastMessage = "Unknown built-in theme."; break; }
      if(request.op==THEME_OP_HIDE) hiddenMask |= 1UL << id;
      else hiddenMask &= ~(1UL << id);
      rebuild();
      storeHidden();
      prefsRequestSave(SAVE_SETTINGS);
      lastMessage = request.op==THEME_OP_HIDE? "Theme hidden." : "Theme listed again.";
      break;

    case THEME_OP_PREVIEW:
      if(!request.hasColors) break;
      preview.name = "Preview";
      memcpy(colorsOf(&preview), request.colors, sizeof(request.colors));
      currentTheme = &preview;
      lastMessage = "Previewing on the receiver.";
      break;

    case THEME_OP_CANCEL:
      themeSelect(themeIdx);
      lastMessage = "Preview closed.";
      break;
  }

  return(true);
}

static void writeColor(Print &out, uint16_t c)
{
  // RGB565 to #rrggbb, rounding each channel to 8 bits
  unsigned r = (((c >> 11) & 31) * 527 + 23) >> 6;
  unsigned g = (((c >> 5) & 63) * 259 + 33) >> 6;
  unsigned b = ((c & 31) * 527 + 23) >> 6;
  out.printf("\"#%02x%02x%02x\"", r, g, b);
}

static void writeJsonString(Print &out, const char *text)
{
  out.print('"');
  for(const char *p = text ; *p ; p++)
  {
    if(*p=='"' || *p=='\\') out.print('\\');
    if((uint8_t)*p >= ' ') out.print(*p);
  }
  out.print('"');
}

static void writeTheme(Print &out, ColorTheme *t, int id, bool builtin, bool hidden)
{
  out.printf("{\"id\":%d,\"name\":", id);
  writeJsonString(out, t->name);
  out.printf(",\"builtin\":%s,\"hidden\":%s,\"colors\":[", builtin? "true" : "false", hidden? "true" : "false");
  const uint16_t *c = colorsOf(t);
  for(size_t i=0 ; i<THEME_COLORS ; i++)
  {
    if(i) out.print(',');
    writeColor(out, c[i]);
  }
  out.print("]}");
}

void themesWriteJson(Print &out)
{
  std::lock_guard<std::mutex> lock(themeMutex);

  out.printf("{\"current\":%d,\"preview\":%s,\"slots\":%d,\"message\":", themeId(themeIdx),
             currentTheme==&preview? "true" : "false", THEME_CUSTOM_MAX);
  writeJsonString(out, lastMessage);
  out.print(",\"fields\":[");
  for(size_t i=0 ; i<THEME_COLORS ; i++)
  {
    if(i) out.print(',');
    writeJsonString(out, colorNames[i]);
  }
  out.print("],\"themes\":[");
  bool first = true;
  for(int i=0 ; i<builtinCount ; i++)
  {
    if(!first) out.print(',');
    writeTheme(out, &theme[i], i, true, hiddenMask & (1UL << i));
    first = false;
  }
  for(int k=0 ; k<THEME_CUSTOM_MAX ; k++)
    if(customUsed[k])
    {
      out.print(',');
      writeTheme(out, &custom[k], THEME_CUSTOM_ID + k, false, false);
    }
  out.print("]}");
}

//
// Turn theme editor on (1) or off (0), or get current status (2)
//
bool switchThemeEditor(int8_t state)
{
  static bool themeEditor = false;
  themeEditor = state == 0 ? false : (state == 1 ? true : themeEditor);
  return themeEditor;
}
