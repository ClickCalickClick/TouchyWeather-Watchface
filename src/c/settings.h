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

// A reading the user can place in any of the four resting-face slots: two
// text lines under the date (line 1 / line 2) and two colored pills (badge 1 /
// badge 2). One menu drives all four; the slot only decides how it's drawn.
// COMPLICATION_STEPS is line-only — a step count can't fit a pill on the small
// screens, so the badge pickers omit it and badge slots treat it as Off.
// COMPLICATION_BATTERY is watch state rather than weather: it replaced the
// v1.2 always-on battery glyph, so it never draws unless the user assigns it
// to a slot, and it is exempt from the badge staleness gate (a stale forecast
// says nothing about the charge level).
typedef enum {
  COMPLICATION_OFF = 0,
  COMPLICATION_FEELS = 1, // default for line 1 (useful out-of-box reading)
  COMPLICATION_WIND = 2,
  COMPLICATION_HUMIDITY = 3,
  COMPLICATION_UV = 4,
  COMPLICATION_AQI = 5,
  COMPLICATION_STEPS = 6,     // line slots only
  COMPLICATION_DEW = 7,
  COMPLICATION_RAIN_CHANCE = 8, // next ~6h peak precip probability
  COMPLICATION_BATTERY = 9,     // watch charge level (+ charging state)
  COMPLICATION_MAX = COMPLICATION_BATTERY,
} ComplicationSlot;

// What a Single-peek nudge actually shows. The mode used to mandate the dense
// everything-overlay; now that is just the default, and the user can pin any
// one peek page instead. Independent of the Peek Pages toggles — those deal
// the Nudge Deck / Auto-rotate decks, while this is one fixed view.
typedef enum {
  SINGLE_PEEK_OVERLAY = 0,  // dense everything-overlay (default)
  SINGLE_PEEK_HOURS = 1,    // 1..4 == FacePage + 1
  SINGLE_PEEK_WEEK = 2,
  SINGLE_PEEK_CONDITIONS = 3,
  SINGLE_PEEK_SUN_MOON = 4,
  SINGLE_PEEK_MAX = SINGLE_PEEK_SUN_MOON,
} SinglePeekView;

// The bottom status row ("UPDATED 5M AGO"). Permanent by default; an imminent
// -rain alert takes the row over in every mode, including UPDATED_NEVER.
typedef enum {
  UPDATED_ALWAYS = 0,        // default
  UPDATED_STALE_OR_RAIN = 1, // pre-v1.2 behavior
  UPDATED_NEVER = 2,
} UpdatedDisplay;

void settings_init(void);

// True when this launch found NO prior persist key — i.e. a genuinely new watch,
// not an upgrade. Latched at the top of settings_init before anything writes, so
// it stays meaningful for the rest of the session. update_notes.c uses it to
// choose between the welcome card and the release notes.
bool settings_is_fresh_install(void);

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

TapInputMode settings_get_tap_input_mode(void);   // default TAP_INPUT_WRIST
void settings_set_tap_input_mode(TapInputMode mode);

// Line slot 1 — text under the date. Default COMPLICATION_FEELS.
ComplicationSlot settings_get_complication(void);
void settings_set_complication(ComplicationSlot slot);

// Line slot 2. When both line slots are set the resting face draws them
// side-by-side on the complication line; when only one is set it centers.
ComplicationSlot settings_get_complication2(void); // default COMPLICATION_OFF
void settings_set_complication2(ComplicationSlot slot);

// Badge slots — the same readings rendered as colored pills in their own row.
// Badge 1 defaults to the rain chance (the pre-v1.2 auto badge, now always-on
// unless the "only when notable" toggle is set).
ComplicationSlot settings_get_badge1(void);        // default RAIN_CHANCE
void settings_set_badge1(ComplicationSlot slot);
ComplicationSlot settings_get_badge2(void);        // default COMPLICATION_OFF
void settings_set_badge2(ComplicationSlot slot);

// Per-badge "only when notable": when set, the pill hides unless the reading
// crosses its attention threshold (rain >= 50%, UV >= 6 midday, and so on —
// see prv_badge_is_notable in clock_zone.c). Off by default: a badge the user
// picked shows its reading. Upgrading users inherit it ON, which reproduces
// the old automatic rain/UV badges exactly.
bool settings_get_badge1_notable(void);            // default false
void settings_set_badge1_notable(bool on);
bool settings_get_badge2_notable(void);            // default false
void settings_set_badge2_notable(bool on);

UpdatedDisplay settings_get_updated_display(void); // default UPDATED_ALWAYS
void settings_set_updated_display(UpdatedDisplay mode);

SinglePeekView settings_get_single_peek_view(void); // default SINGLE_PEEK_OVERLAY
void settings_set_single_peek_view(SinglePeekView view);
