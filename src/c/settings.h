#pragma once
#include <pebble.h>

// Face settings. The header keeps the app's `settings.h` name and the
// settings_get_* symbol shapes so theme.c / ui.c / anim.c (copied verbatim
// from the TouchyWeather app) compile with zero call-site edits.

typedef enum {
  GESTURE_NUDGE_DECK = 0,   // each nudge advances a peek page (default)
  GESTURE_SINGLE_PEEK = 1,  // one nudge = dense everything-overlay
  GESTURE_AUTO_ROTATE = 2,  // pages rotate on a timer, nudges ignored
  GESTURE_OFF = 3,          // nudges only re-wake the icon animation
} GestureMode;

// Peek pages, in deck order (mirrors the app's card order).
typedef enum {
  PAGE_HOURS = 0,
  PAGE_WEEK = 1,
  PAGE_CONDITIONS = 2,
  PAGE_SUN_MOON = 3,
  PAGE_COUNT = 4,
} FacePage;

void settings_init(void);

// Compat shims for the copied modules. The face has no Big Mode; the
// accessor exists so theme.c/ui.c accent-collapse logic compiles unchanged.
bool settings_get_big_mode(void);
bool settings_get_animations_enabled(void);
void settings_set_animations_enabled(bool on);

GestureMode settings_get_gesture_mode(void);
void settings_set_gesture_mode(GestureMode mode);

bool settings_get_page_enabled(FacePage page);
void settings_set_page_enabled(FacePage page, bool on);
// Number of enabled pages (>=1 is enforced: with all pages off the deck
// degrades to GESTURE_OFF behavior).
int settings_enabled_page_count(void);

// Ambient behaviors ("semi-basic default, add-ons opt-in").
bool settings_get_rain_auto_show(void);   // default true
void settings_set_rain_auto_show(bool on);
bool settings_get_night_mode(void);       // default false
void settings_set_night_mode(bool on);
bool settings_get_uv_badge(void);         // default false
void settings_set_uv_badge(bool on);
bool settings_get_quick_view_reflow(void);// default true
void settings_set_quick_view_reflow(bool on);

bool settings_get_use_dew_point(void);    // conditions page: dew point vs humidity
void settings_set_use_dew_point(bool on);
bool settings_get_show_location(void);
void settings_set_show_location(bool on);

// The user's daytime theme choice, remembered so night mode can restore it
// at sunrise (night mode force-sets dark without clobbering this).
int settings_get_day_theme(void);
void settings_set_day_theme(int theme);
