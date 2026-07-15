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

// Which physical nudge drives the deck. Both come from AccelTapService —
// the only non-touch input a watchface gets — discriminated by axis.
// A wrist flick lands on X/Y (arm rotation); a tap on the watch body/face
// lands on Z (perpendicular impact). "Either" accepts any axis (the most
// reliable, since axis discrimination is imperfect on real hardware).
typedef enum {
  TAP_INPUT_WRIST = 0,  // wrist flick — X/Y axis (default)
  TAP_INPUT_TAP = 1,    // tap the watch — Z axis
  TAP_INPUT_EITHER = 2, // any axis (reliability fallback)
} TapInputMode;

// Persistent battery indicator on the resting face.
typedef enum {
  BATTERY_OFF = 0,
  BATTERY_ALWAYS = 1,   // default
  BATTERY_WHEN_LOW = 2, // only at <=20%
} BatteryDisplay;

// The one configurable complication slot on the resting face.
typedef enum {
  COMPLICATION_OFF = 0, // default — keep the face clean
  COMPLICATION_FEELS = 1,
  COMPLICATION_WIND = 2,
  COMPLICATION_HUMIDITY = 3,
  COMPLICATION_UV = 4,
  COMPLICATION_AQI = 5,
  COMPLICATION_STEPS = 6,
} ComplicationSlot;

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

// The user's daytime theme choice, remembered so night mode can restore it
// at sunrise (night mode force-sets dark without clobbering this).
int settings_get_day_theme(void);
void settings_set_day_theme(int theme);

TapInputMode settings_get_tap_input_mode(void);   // default TAP_INPUT_WRIST
void settings_set_tap_input_mode(TapInputMode mode);

BatteryDisplay settings_get_battery_display(void); // default BATTERY_ALWAYS
void settings_set_battery_display(BatteryDisplay mode);

ComplicationSlot settings_get_complication(void);  // default COMPLICATION_OFF
void settings_set_complication(ComplicationSlot slot);
