#ifndef THEMES_H
#define THEMES_H

#include <stdint.h>

// Only named here, so that the host unit tests can include this header
class Print;

// This is our current theme
#define TH (*currentTheme)

#define THEME_CUSTOM_MAX   8    // Themes made in the web theme editor
#define THEME_NAME_LEN    15    // Longest theme name
#define THEME_CUSTOM_ID  100    // Saved theme ids from here on are custom themes

typedef struct __attribute__ ((packed))
{
  const char *name;
  uint16_t bg;
  uint16_t text;
  uint16_t text_muted;
  uint16_t text_warn;

  uint16_t smeter_icon;
  uint16_t smeter_bar;
  uint16_t smeter_bar_plus;
  uint16_t smeter_bar_empty;

  uint16_t save_icon;

  uint16_t stereo_icon;

  uint16_t rf_icon;
  uint16_t rf_icon_conn;

  uint16_t batt_voltage;
  uint16_t batt_border;
  uint16_t batt_full;
  uint16_t batt_low;
  uint16_t batt_charge;
  uint16_t batt_icon;

  uint16_t band_text;

  uint16_t mode_text;
  uint16_t mode_border;

  uint16_t box_bg;
  uint16_t box_border;
  uint16_t box_text;
  uint16_t box_off_bg;
  uint16_t box_off_text;

  uint16_t menu_bg;
  uint16_t menu_border;
  uint16_t menu_hdr;
  uint16_t menu_item;
  uint16_t menu_hl_bg;
  uint16_t menu_hl_text;
  uint16_t menu_param;

  uint16_t freq_text;
  uint16_t funit_text;
  uint16_t freq_hl;
  uint16_t freq_hl_sel;

  uint16_t rds_text;

  uint16_t scale_text;
  uint16_t scale_pointer;
  uint16_t scale_line;

  uint16_t scan_grid;
  uint16_t scan_snr;
  uint16_t scan_rssi;
} ColorTheme;

// Built-in themes
extern ColorTheme theme[];
// The theme in use, a built-in one, a custom one or a preview
extern ColorTheme *currentTheme;
// Position of the current theme among the listed ones
extern uint8_t themeIdx;

// Listed themes: built-in ones that are not hidden, then the custom ones
int getTotalThemes();
const char *themeName(int idx);
void themeSelect(int idx);
// Stable identifiers, kept in the settings: built-in themes are numbered
// from 0, custom themes from THEME_CUSTOM_ID
int themeId(int idx);
void themeSelectId(int id);

// Load the custom themes and the hidden built-in ones
void themesInit();

// Web theme editor. Requests come from the network task and are carried
// out by themesTick() on the main loop.
enum ThemeOp
{
  THEME_OP_SELECT,   // Use theme <id>
  THEME_OP_SAVE,     // Store <name> and <colors> as custom theme <id>, or as a new one when <id> is not custom
  THEME_OP_DELETE,   // Delete custom theme <id>
  THEME_OP_HIDE,     // Take built-in theme <id> off the list
  THEME_OP_SHOW,     // Put built-in theme <id> back on the list
  THEME_OP_PREVIEW,  // Show <colors> without saving them
  THEME_OP_CANCEL,   // Leave the preview
};
int themeColorCount();
const char *themeColorName(int idx);
bool themesRequest(uint8_t op, int id, const char *name, const uint16_t *colors);
bool themesTick();
void themesWriteJson(Print &out);

bool switchThemeEditor(int8_t state = 2);

#endif // THEMES_H
