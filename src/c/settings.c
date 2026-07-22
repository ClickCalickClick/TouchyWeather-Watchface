#include "settings.h"

// Persist layout (face namespace — independent of the app's storage):
//   1      theme.c PERSIST_KEY_THEME (owned by theme.c, listed for the map)
//   10..29 settings below
//   30     comm.c weather cache blob (Phase 4) — NOT ours, never reuse
//   31..34 settings below (continued past the cache key)
#define KEY_GESTURE_MODE      10
#define KEY_PAGE_BASE         11  // 11..14 = PAGE_HOURS..PAGE_SUN_MOON
#define KEY_RAIN_AUTO_SHOW    15
#define KEY_NIGHT_MODE        16
#define KEY_UV_BADGE          17  // legacy (v1.1); migrated into KEY_BADGE2
#define KEY_QUICK_VIEW        18
#define KEY_ANIMATIONS        19
#define KEY_USE_DEW_POINT     20
#define KEY_SHOW_LOCATION     21
#define KEY_DAY_THEME         22
#define KEY_TAP_INPUT_MODE    23
#define KEY_BATTERY_DISPLAY   24
#define KEY_COMPLICATION      25
#define KEY_BIG_MODE          26
#define KEY_COMPLICATION2     27
#define KEY_RAIN_CHANCE_BADGE 28  // legacy (v1.1); migrated into KEY_BADGE1
#define KEY_BADGE1            29
#define KEY_BADGE2            31  // 30 is the weather cache — skipped
#define KEY_BADGE1_NOTABLE    32
#define KEY_BADGE2_NOTABLE    33
#define KEY_UPDATED_DISPLAY   34

// The weather cache, read here only as an "is this an upgrade?" probe during
// badge migration. comm.c owns it; we never write or delete it.
#define KEY_COMM_CACHE_PROBE  30

static GestureMode s_gesture_mode = GESTURE_NUDGE_DECK;
static bool s_page_enabled[PAGE_COUNT] = { true, true, true, true };
static bool s_rain_auto_show = true;
static bool s_night_mode = false;
static bool s_quick_view = true;
static bool s_animations = true;
static bool s_use_dew_point = false;
static bool s_show_location = false;
static TapInputMode s_tap_input_mode = TAP_INPUT_WRIST;
static BatteryDisplay s_battery_display = BATTERY_WHEN_LOW;
static ComplicationSlot s_complication = COMPLICATION_FEELS;  // useful out-of-box default
static ComplicationSlot s_complication2 = COMPLICATION_OFF;
static ComplicationSlot s_badge1 = COMPLICATION_RAIN_CHANCE;
static ComplicationSlot s_badge2 = COMPLICATION_OFF;
static bool s_badge1_notable = false;
static bool s_badge2_notable = false;
static UpdatedDisplay s_updated_display = UPDATED_ALWAYS;
static bool s_big_mode = false;

static bool prv_read_bool(uint32_t key, bool fallback) {
  return persist_exists(key) ? persist_read_bool(key) : fallback;
}

// Read a complication slot, falling back to `dflt` when absent and to OFF when
// the stored value is out of range (a downgrade from a future build, or a
// garbled write).
static ComplicationSlot prv_read_slot(uint32_t key, ComplicationSlot dflt) {
  if (!persist_exists(key)) return dflt;
  ComplicationSlot s = (ComplicationSlot)persist_read_int(key);
  return (s > COMPLICATION_MAX) ? COMPLICATION_OFF : s;
}

// v1.1 -> v1.2 badge migration. The old face had two automatic pills behind
// plain toggles (rain chance, UV); v1.2 turns them into two free-form badge
// slots. Runs once, when the new keys are absent:
//   - Upgrading user  -> reproduce their old face exactly. Each old toggle that
//     was on becomes its slot with "only when notable" ON, which is precisely
//     the old auto behavior (rain >= 50%, UV >= 6 midday).
//   - Fresh install   -> plan defaults: an always-on rain-chance badge.
// "Upgrading" is inferred from any v1.1-era persist key existing: either old
// badge key, or comm.c's weather cache (present on any watch that has run the
// face before). Pebble wipes persist on uninstall, so a fresh install has none.
// Mirrors theme.c's migrate-then-delete precedent: once read, the legacy keys
// are deleted so the two sources can't drift.
static void prv_migrate_badges(void) {
  if (persist_exists(KEY_BADGE1)) return;

  bool upgrading = persist_exists(KEY_RAIN_CHANCE_BADGE) ||
                   persist_exists(KEY_UV_BADGE) ||
                   persist_exists(KEY_COMM_CACHE_PROBE);
  if (upgrading) {
    // Absent keys mean the user never changed the v1.1 default: rain on, UV off.
    if (prv_read_bool(KEY_RAIN_CHANCE_BADGE, true)) {
      s_badge1 = COMPLICATION_RAIN_CHANCE;
      s_badge1_notable = true;
    } else {
      s_badge1 = COMPLICATION_OFF;
    }
    if (prv_read_bool(KEY_UV_BADGE, false)) {
      s_badge2 = COMPLICATION_UV;
      s_badge2_notable = true;
    } else {
      s_badge2 = COMPLICATION_OFF;
    }
  } else {
    s_badge1 = COMPLICATION_RAIN_CHANCE;
    s_badge1_notable = false;
    s_badge2 = COMPLICATION_OFF;
    s_badge2_notable = false;
  }

  persist_write_int(KEY_BADGE1, (int)s_badge1);
  persist_write_int(KEY_BADGE2, (int)s_badge2);
  persist_write_bool(KEY_BADGE1_NOTABLE, s_badge1_notable);
  persist_write_bool(KEY_BADGE2_NOTABLE, s_badge2_notable);
  persist_delete(KEY_RAIN_CHANCE_BADGE);
  persist_delete(KEY_UV_BADGE);
}

void settings_init(void) {
  if (persist_exists(KEY_GESTURE_MODE)) {
    s_gesture_mode = (GestureMode)persist_read_int(KEY_GESTURE_MODE);
    if (s_gesture_mode > GESTURE_OFF) s_gesture_mode = GESTURE_NUDGE_DECK;
  }
  for (int i = 0; i < PAGE_COUNT; i++) {
    s_page_enabled[i] = prv_read_bool(KEY_PAGE_BASE + i, true);
  }
  s_rain_auto_show = prv_read_bool(KEY_RAIN_AUTO_SHOW, true);
  s_night_mode = prv_read_bool(KEY_NIGHT_MODE, false);
  s_quick_view = prv_read_bool(KEY_QUICK_VIEW, true);
  s_animations = prv_read_bool(KEY_ANIMATIONS, true);
  s_use_dew_point = prv_read_bool(KEY_USE_DEW_POINT, false);
  s_show_location = prv_read_bool(KEY_SHOW_LOCATION, false);
  if (persist_exists(KEY_TAP_INPUT_MODE)) {
    s_tap_input_mode = (TapInputMode)persist_read_int(KEY_TAP_INPUT_MODE);
    if (s_tap_input_mode > TAP_INPUT_EITHER) s_tap_input_mode = TAP_INPUT_WRIST;
  }
  if (persist_exists(KEY_BATTERY_DISPLAY)) {
    s_battery_display = (BatteryDisplay)persist_read_int(KEY_BATTERY_DISPLAY);
    if (s_battery_display > BATTERY_WHEN_LOW) s_battery_display = BATTERY_WHEN_LOW;
  }
  s_complication = prv_read_slot(KEY_COMPLICATION, COMPLICATION_FEELS);
  s_complication2 = prv_read_slot(KEY_COMPLICATION2, COMPLICATION_OFF);

  // Badge slots: migrate from the v1.1 auto-badge toggles on first run of this
  // version, then read normally.
  prv_migrate_badges();
  s_badge1 = prv_read_slot(KEY_BADGE1, COMPLICATION_RAIN_CHANCE);
  s_badge2 = prv_read_slot(KEY_BADGE2, COMPLICATION_OFF);
  // Steps can't fit a pill; the Clay pickers omit it, but a hand-sent value
  // could still land here.
  if (s_badge1 == COMPLICATION_STEPS) s_badge1 = COMPLICATION_OFF;
  if (s_badge2 == COMPLICATION_STEPS) s_badge2 = COMPLICATION_OFF;
  s_badge1_notable = prv_read_bool(KEY_BADGE1_NOTABLE, false);
  s_badge2_notable = prv_read_bool(KEY_BADGE2_NOTABLE, false);

  if (persist_exists(KEY_UPDATED_DISPLAY)) {
    s_updated_display = (UpdatedDisplay)persist_read_int(KEY_UPDATED_DISPLAY);
    if (s_updated_display > UPDATED_NEVER) s_updated_display = UPDATED_ALWAYS;
  }
  s_big_mode = prv_read_bool(KEY_BIG_MODE, false);
}

bool settings_get_big_mode(void) { return s_big_mode; }
void settings_set_big_mode(bool on) {
  s_big_mode = on;
  persist_write_bool(KEY_BIG_MODE, on);
}

bool settings_get_animations_enabled(void) { return s_animations; }
void settings_set_animations_enabled(bool on) {
  s_animations = on;
  persist_write_bool(KEY_ANIMATIONS, on);
}

GestureMode settings_get_gesture_mode(void) { return s_gesture_mode; }
void settings_set_gesture_mode(GestureMode mode) {
  s_gesture_mode = mode;
  persist_write_int(KEY_GESTURE_MODE, (int)mode);
}

bool settings_get_page_enabled(FacePage page) {
  return ((int)page < PAGE_COUNT) ? s_page_enabled[page] : false;
}
void settings_set_page_enabled(FacePage page, bool on) {
  if ((int)page >= PAGE_COUNT) return;
  s_page_enabled[page] = on;
  persist_write_bool(KEY_PAGE_BASE + page, on);
}
int settings_enabled_page_count(void) {
  int n = 0;
  for (int i = 0; i < PAGE_COUNT; i++) {
    if (s_page_enabled[i]) n++;
  }
  return n;
}

bool settings_get_rain_auto_show(void) { return s_rain_auto_show; }
void settings_set_rain_auto_show(bool on) {
  s_rain_auto_show = on;
  persist_write_bool(KEY_RAIN_AUTO_SHOW, on);
}

bool settings_get_night_mode(void) { return s_night_mode; }
void settings_set_night_mode(bool on) {
  s_night_mode = on;
  persist_write_bool(KEY_NIGHT_MODE, on);
}

bool settings_get_quick_view_reflow(void) { return s_quick_view; }
void settings_set_quick_view_reflow(bool on) {
  s_quick_view = on;
  persist_write_bool(KEY_QUICK_VIEW, on);
}

bool settings_get_use_dew_point(void) { return s_use_dew_point; }
void settings_set_use_dew_point(bool on) {
  s_use_dew_point = on;
  persist_write_bool(KEY_USE_DEW_POINT, on);
}

bool settings_get_show_location(void) { return s_show_location; }
void settings_set_show_location(bool on) {
  s_show_location = on;
  persist_write_bool(KEY_SHOW_LOCATION, on);
}

int settings_get_day_theme(void) {
  return persist_exists(KEY_DAY_THEME) ? (int)persist_read_int(KEY_DAY_THEME) : 0;
}
void settings_set_day_theme(int theme) {
  persist_write_int(KEY_DAY_THEME, theme);
}

TapInputMode settings_get_tap_input_mode(void) { return s_tap_input_mode; }
void settings_set_tap_input_mode(TapInputMode mode) {
  s_tap_input_mode = mode;
  persist_write_int(KEY_TAP_INPUT_MODE, (int)mode);
}

BatteryDisplay settings_get_battery_display(void) { return s_battery_display; }
void settings_set_battery_display(BatteryDisplay mode) {
  s_battery_display = mode;
  persist_write_int(KEY_BATTERY_DISPLAY, (int)mode);
}

ComplicationSlot settings_get_complication(void) { return s_complication; }
void settings_set_complication(ComplicationSlot slot) {
  s_complication = slot;
  persist_write_int(KEY_COMPLICATION, (int)slot);
}

ComplicationSlot settings_get_complication2(void) { return s_complication2; }
void settings_set_complication2(ComplicationSlot slot) {
  s_complication2 = slot;
  persist_write_int(KEY_COMPLICATION2, (int)slot);
}

ComplicationSlot settings_get_badge1(void) { return s_badge1; }
void settings_set_badge1(ComplicationSlot slot) {
  // Steps is a line-only reading (see ComplicationSlot); a badge slot asked to
  // show it renders nothing, so store it as Off rather than a silent blank.
  if (slot == COMPLICATION_STEPS) slot = COMPLICATION_OFF;
  s_badge1 = slot;
  persist_write_int(KEY_BADGE1, (int)slot);
}

ComplicationSlot settings_get_badge2(void) { return s_badge2; }
void settings_set_badge2(ComplicationSlot slot) {
  if (slot == COMPLICATION_STEPS) slot = COMPLICATION_OFF;
  s_badge2 = slot;
  persist_write_int(KEY_BADGE2, (int)slot);
}

bool settings_get_badge1_notable(void) { return s_badge1_notable; }
void settings_set_badge1_notable(bool on) {
  s_badge1_notable = on;
  persist_write_bool(KEY_BADGE1_NOTABLE, on);
}

bool settings_get_badge2_notable(void) { return s_badge2_notable; }
void settings_set_badge2_notable(bool on) {
  s_badge2_notable = on;
  persist_write_bool(KEY_BADGE2_NOTABLE, on);
}

UpdatedDisplay settings_get_updated_display(void) { return s_updated_display; }
void settings_set_updated_display(UpdatedDisplay mode) {
  s_updated_display = mode;
  persist_write_int(KEY_UPDATED_DISPLAY, (int)mode);
}
