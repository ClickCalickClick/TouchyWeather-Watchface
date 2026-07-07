#include "comm.h"
#include "weather_data.h"
#include "theme.h"
#include "settings.h"
#include "anim.h"
#include <string.h>
#include <stdlib.h>

static CommUpdateCb s_update_cb = NULL;

// Face persist namespace (see settings.c for the full key map). Bump this
// key whenever WeatherData's layout changes so an old blob can't misalign —
// the app's 100→108 discipline.
#define PERSIST_KEY_CACHE 30

// Refetch when data is older than this (checked on the minute tick — the
// face is always running, so this replaces the app's wakeup machinery).
#define STALE_REFETCH_SECS (30 * 60)
// On-launch refresh threshold (matches the app).
#define LAUNCH_REFRESH_SECS (15 * 60)
// Don't spam PKJS: minimum spacing between staleness-triggered requests.
#define REQUEST_COOLDOWN_SECS 60

static uint32_t s_last_request = 0;

static void prv_save_cache(void) {
  WeatherData *d = weather_data_get();
  if (d->valid) {
    persist_write_data(PERSIST_KEY_CACHE, d, sizeof(WeatherData));
  }
}

// Clay toggles arrive as int 0/1 or (radiogroup/select) as CSTRING digits.
static bool prv_tuple_bool(Tuple *t) {
  return (t->type == TUPLE_CSTRING) ? (atoi(t->value->cstring) != 0)
                                    : (t->value->int32 != 0);
}

static int prv_tuple_int(Tuple *t) {
  return (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring)
                                    : (int)t->value->int32;
}

static void prv_copy_str(char *dst, size_t n, Tuple *t) {
  strncpy(dst, t->value->cstring, n - 1);
  dst[n - 1] = '\0';
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  WeatherData *d = weather_data_get();
  bool got_anything = false;
  bool config_changed = false;
  Tuple *t;

  // --- Config (Clay) ---
  if ((t = dict_find(iter, MESSAGE_KEY_Theme))) {
    int theme_val = prv_tuple_int(t);
    theme_set(theme_val ? THEME_DARK : THEME_LIGHT);
    // Remember as the day theme so night mode restores this at sunrise.
    settings_set_day_theme(theme_val ? 1 : 0);
    config_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_AnimationsEnabled))) {
    settings_set_animations_enabled(prv_tuple_bool(t));
    anim_kick();
    config_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_GestureMode))) {
    settings_set_gesture_mode((GestureMode)prv_tuple_int(t));
    config_changed = true;
  }
  const struct { uint32_t key; FacePage page; } page_map[] = {
    { MESSAGE_KEY_PageEnabledHours,      PAGE_HOURS },
    { MESSAGE_KEY_PageEnabledWeek,       PAGE_WEEK },
    { MESSAGE_KEY_PageEnabledConditions, PAGE_CONDITIONS },
    { MESSAGE_KEY_PageEnabledSunMoon,    PAGE_SUN_MOON },
  };
  for (unsigned i = 0; i < sizeof(page_map) / sizeof(page_map[0]); ++i) {
    if ((t = dict_find(iter, page_map[i].key))) {
      settings_set_page_enabled(page_map[i].page, prv_tuple_bool(t));
      config_changed = true;
    }
  }
  if ((t = dict_find(iter, MESSAGE_KEY_RainAutoShow))) {
    settings_set_rain_auto_show(prv_tuple_bool(t));
    config_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_NightMode))) {
    settings_set_night_mode(prv_tuple_bool(t));
    config_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_UVBadge))) {
    settings_set_uv_badge(prv_tuple_bool(t));
    config_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_QuickViewReflow))) {
    settings_set_quick_view_reflow(prv_tuple_bool(t));
    config_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_UseDewPoint))) {
    bool on = prv_tuple_bool(t);
    settings_set_use_dew_point(on);
    d->use_dew_point = on;
    config_changed = true;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_ShowLocation))) {
    bool on = prv_tuple_bool(t);
    settings_set_show_location(on);
    d->show_location = on;
    config_changed = true;
  }

  // --- Weather payload ---
  if ((t = dict_find(iter, MESSAGE_KEY_Temp))) { d->temp = t->value->int32; got_anything = true; }
  if ((t = dict_find(iter, MESSAGE_KEY_FeelsLike))) { d->feels_like = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_High))) { d->high = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Low))) { d->low = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Condition))) { d->condition = (WeatherCondition)t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Wind))) { d->wind_speed = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_WindDir))) { prv_copy_str(d->wind_dir, sizeof(d->wind_dir), t); }
  if ((t = dict_find(iter, MESSAGE_KEY_Humidity))) { d->humidity = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_DewPoint))) { d->dew_point = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_UV))) { d->uv = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_UVMax))) { d->uv_max = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_AQI))) { d->aqi = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Sunrise))) { prv_copy_str(d->sunrise, sizeof(d->sunrise), t); }
  if ((t = dict_find(iter, MESSAGE_KEY_Sunset))) { prv_copy_str(d->sunset, sizeof(d->sunset), t); }
  if ((t = dict_find(iter, MESSAGE_KEY_LocationName))) { prv_copy_str(d->location_name, sizeof(d->location_name), t); }
  if ((t = dict_find(iter, MESSAGE_KEY_RainAlertMinutes))) { d->rain_alert_min = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Units))) { d->units = (Units)t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_LastUpdated))) {
    // PKJS sends the fetch's unix-second timestamp; anything < 100000 is a
    // sentinel echo, not a real time — use the device clock instead.
    uint32_t v = (uint32_t)t->value->int32;
    d->last_updated = (v >= 100000U) ? v : (uint32_t)time(NULL);
    got_anything = true;
  }

  uint32_t hour_label_keys[6] = {
    MESSAGE_KEY_Hour1Label, MESSAGE_KEY_Hour2Label, MESSAGE_KEY_Hour3Label,
    MESSAGE_KEY_Hour4Label, MESSAGE_KEY_Hour5Label, MESSAGE_KEY_Hour6Label
  };
  uint32_t hour_temp_keys[6] = {
    MESSAGE_KEY_Hour1Temp, MESSAGE_KEY_Hour2Temp, MESSAGE_KEY_Hour3Temp,
    MESSAGE_KEY_Hour4Temp, MESSAGE_KEY_Hour5Temp, MESSAGE_KEY_Hour6Temp
  };
  uint32_t hour_cond_keys[6] = {
    MESSAGE_KEY_Hour1Cond, MESSAGE_KEY_Hour2Cond, MESSAGE_KEY_Hour3Cond,
    MESSAGE_KEY_Hour4Cond, MESSAGE_KEY_Hour5Cond, MESSAGE_KEY_Hour6Cond
  };
  uint32_t hour_pop_keys[6] = {
    MESSAGE_KEY_Hour1Pop, MESSAGE_KEY_Hour2Pop, MESSAGE_KEY_Hour3Pop,
    MESSAGE_KEY_Hour4Pop, MESSAGE_KEY_Hour5Pop, MESSAGE_KEY_Hour6Pop
  };
  for (int i = 0; i < 6; i++) {
    if ((t = dict_find(iter, hour_label_keys[i]))) {
      prv_copy_str(d->hours_label[i], sizeof(d->hours_label[i]), t);
    }
    if ((t = dict_find(iter, hour_temp_keys[i]))) d->hours_temp[i] = t->value->int32;
    if ((t = dict_find(iter, hour_cond_keys[i]))) d->hours_cond[i] = (WeatherCondition)t->value->int32;
    if ((t = dict_find(iter, hour_pop_keys[i]))) d->hours_pop[i] = (uint8_t)t->value->int32;
  }

  uint32_t day_label_keys[5] = {
    MESSAGE_KEY_Day0Label, MESSAGE_KEY_Day1Label, MESSAGE_KEY_Day2Label,
    MESSAGE_KEY_Day3Label, MESSAGE_KEY_Day4Label
  };
  uint32_t day_high_keys[5] = {
    MESSAGE_KEY_Day0High, MESSAGE_KEY_Day1High, MESSAGE_KEY_Day2High,
    MESSAGE_KEY_Day3High, MESSAGE_KEY_Day4High
  };
  uint32_t day_low_keys[5] = {
    MESSAGE_KEY_Day0Low, MESSAGE_KEY_Day1Low, MESSAGE_KEY_Day2Low,
    MESSAGE_KEY_Day3Low, MESSAGE_KEY_Day4Low
  };
  uint32_t day_cond_keys[5] = {
    MESSAGE_KEY_Day0Cond, MESSAGE_KEY_Day1Cond, MESSAGE_KEY_Day2Cond,
    MESSAGE_KEY_Day3Cond, MESSAGE_KEY_Day4Cond
  };
  for (int i = 0; i < 5; i++) {
    if ((t = dict_find(iter, day_label_keys[i]))) {
      prv_copy_str(d->days_label[i], sizeof(d->days_label[i]), t);
    }
    if ((t = dict_find(iter, day_high_keys[i]))) d->days_high[i] = t->value->int32;
    if ((t = dict_find(iter, day_low_keys[i])))  d->days_low[i]  = t->value->int32;
    if ((t = dict_find(iter, day_cond_keys[i]))) d->days_cond[i] = (WeatherCondition)t->value->int32;
  }

  if ((t = dict_find(iter, MESSAGE_KEY_MoonPhase))) d->moon_phase = (uint8_t)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_MoonIllum))) d->moon_illum = (uint8_t)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_MoonName1))) { prv_copy_str(d->moon_name1, sizeof(d->moon_name1), t); }
  if ((t = dict_find(iter, MESSAGE_KEY_MoonName2))) { prv_copy_str(d->moon_name2, sizeof(d->moon_name2), t); }

  if (got_anything) {
    d->valid = true;
    prv_save_cache();
    anim_kick();  // fresh data: wake the hero icon for another window
  }
  if ((got_anything || config_changed) && s_update_cb) {
    s_update_cb();
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "AppMessage dropped: %d", (int)reason);
}

void comm_request_refresh(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  // Sentinel to trigger a PKJS fetch, plus the watch's clock style so the
  // "Match watch" time format works (PKJS does all time formatting).
  dict_write_uint8(iter, MESSAGE_KEY_LastUpdated, 1);
  dict_write_uint8(iter, MESSAGE_KEY_ClockIs24h, clock_is_24h_style() ? 1 : 0);
  dict_write_end(iter);
  app_message_outbox_send();
  s_last_request = (uint32_t)time(NULL);
}

// One-shot retry when the refresh-sentinel send fails (typically PKJS not
// up yet at the 750ms initial request) — the app's exact pattern.
static bool s_refresh_retry_done = false;

static void prv_refresh_retry(void *ctx) {
  (void)ctx;
  APP_LOG(APP_LOG_LEVEL_INFO, "Refresh sentinel send failed, retrying once");
  comm_request_refresh();
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason,
                              void *context) {
  (void)reason; (void)context;
  if (s_refresh_retry_done) return;
  if (!iter || !dict_find(iter, MESSAGE_KEY_LastUpdated)) return;
  s_refresh_retry_done = true;  // per-launch: one retry, never a loop
  app_timer_register(2000, prv_refresh_retry, NULL);
}

void comm_check_staleness(void) {
  WeatherData *d = weather_data_get();
  uint32_t now = (uint32_t)time(NULL);
  if (now - s_last_request < REQUEST_COOLDOWN_SECS) return;
  if (!d->valid || d->last_updated == 0 ||
      now - d->last_updated > STALE_REFETCH_SECS) {
    comm_request_refresh();
  }
}

static void prv_initial_refresh(void *ctx) {
  (void)ctx;
  WeatherData *d = weather_data_get();
  if (d->valid && d->last_updated != 0) {
    uint32_t age = (uint32_t)time(NULL) - d->last_updated;
    if (age < LAUNCH_REFRESH_SECS) return;  // cache fresh enough
  }
  comm_request_refresh();
}

void comm_load_cache(void) {
  if (persist_exists(PERSIST_KEY_CACHE)) {
    WeatherData *d = weather_data_get();
    persist_read_data(PERSIST_KEY_CACHE, d, sizeof(WeatherData));
  }
  if (s_update_cb && weather_data_get()->valid) {
    s_update_cb();
  }
}

void comm_set_update_callback(CommUpdateCb cb) { s_update_cb = cb; }

void comm_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(1024, 256);
  // Refresh-on-open after a short delay so AppMessage is fully open first.
  app_timer_register(750, prv_initial_refresh, NULL);
}

void comm_deinit(void) {
  app_message_deregister_callbacks();
}
