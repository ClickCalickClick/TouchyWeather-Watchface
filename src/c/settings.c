#include "settings.h"

// Persist layout (face namespace — independent of the app's storage):
//   1      theme.c PERSIST_KEY_THEME (owned by theme.c, listed for the map)
//   10..29 settings below
//   30     comm.c weather cache blob (Phase 4)
#define KEY_GESTURE_MODE      10
#define KEY_PAGE_BASE         11  // 11..14 = PAGE_HOURS..PAGE_SUN_MOON
#define KEY_RAIN_AUTO_SHOW    15
#define KEY_NIGHT_MODE        16
#define KEY_UV_BADGE          17
#define KEY_QUICK_VIEW        18
#define KEY_ANIMATIONS        19
#define KEY_USE_DEW_POINT     20
#define KEY_SHOW_LOCATION     21

static GestureMode s_gesture_mode = GESTURE_NUDGE_DECK;
static bool s_page_enabled[PAGE_COUNT] = { true, true, true, true };
static bool s_rain_auto_show = true;
static bool s_night_mode = false;
static bool s_uv_badge = false;
static bool s_quick_view = true;
static bool s_animations = true;
static bool s_use_dew_point = false;
static bool s_show_location = false;

static bool prv_read_bool(uint32_t key, bool fallback) {
  return persist_exists(key) ? persist_read_bool(key) : fallback;
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
  s_uv_badge = prv_read_bool(KEY_UV_BADGE, false);
  s_quick_view = prv_read_bool(KEY_QUICK_VIEW, true);
  s_animations = prv_read_bool(KEY_ANIMATIONS, true);
  s_use_dew_point = prv_read_bool(KEY_USE_DEW_POINT, false);
  s_show_location = prv_read_bool(KEY_SHOW_LOCATION, false);
}

bool settings_get_big_mode(void) { return false; }

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

bool settings_get_uv_badge(void) { return s_uv_badge; }
void settings_set_uv_badge(bool on) {
  s_uv_badge = on;
  persist_write_bool(KEY_UV_BADGE, on);
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
