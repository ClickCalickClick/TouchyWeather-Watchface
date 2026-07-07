#include <pebble.h>
#include "theme.h"
#include "ui.h"
#include "settings.h"
#include "weather_data.h"
#include "anim.h"
#include "clock_zone.h"
#include "face_state.h"
#include "gesture.h"
#include "comm.h"
#include "pages/pages.h"

static Window *s_window;
static Layer *s_root_layer;

// Banner alternation. The app drove the rain/updated flip off the anim
// frame, which is fine there (constant interaction keeps anim alive) but
// on an idle face anim freezes after ~8s and could freeze the banner on
// UPDATED, hiding an active rain alert. So the face owns the flip with a
// dedicated 4s timer that only exists while a rain alert is active.
#define BANNER_FLIP_MS 4000
static AppTimer *s_banner_timer = NULL;
static bool s_banner_alt = false;  // false = RAIN pill, true = UPDATED pill

// Single redraw funnel: every driver (minute tick, anim ticker, data
// arrival, state changes) goes through here — one canvas, one dirty bit.
static void prv_mark_dirty(void) {
  if (s_root_layer) layer_mark_dirty(s_root_layer);
}

// PEEK/OVERLAY vertical split: compact time line on top, page band in the
// middle, page dots just above the banner pill (whose top sits at
// H - pad_bottom - 22; see ui_draw_status_banner).
#if defined(UI_SCREEN_SMALL_RECT)
  #define PEEK_BAND_TOP   34
  #define PEEK_DOTS_Y(H)  ((H) - 50)
#elif defined(UI_SCREEN_SMALL_ROUND)
  #define PEEK_BAND_TOP   56
  #define PEEK_DOTS_Y(H)  ((H) - 48)
#elif defined(UI_SCREEN_LARGE_RECT)
  #define PEEK_BAND_TOP   36
  #define PEEK_DOTS_Y(H)  ((H) - 50)
#else  // UI_SCREEN_LARGE_ROUND
  #define PEEK_BAND_TOP   56
  #define PEEK_DOTS_Y(H)  ((H) - 64)
#endif

static void prv_root_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  WeatherData *d = weather_data_get();
  int H = bounds.size.h;
  FaceMode mode = face_state_mode();

  // Quick View reflow: when a timeline event obstructs the bottom, degrade
  // to the compact time+temp line inside the unobstructed area.
  GRect ub = layer_get_unobstructed_bounds(layer);
  if (ub.size.h < bounds.size.h && settings_get_quick_view_reflow()) {
    clock_zone_draw_compact(ctx, ub);
    return;
  }

  if (mode == FACE_CLOCK) {
    clock_zone_draw_full(ctx, bounds);
  } else {
    clock_zone_draw_compact(ctx, bounds);
    int dots_y = PEEK_DOTS_Y(H);
    GRect band = GRect(bounds.origin.x, bounds.origin.y + PEEK_BAND_TOP,
                       bounds.size.w, dots_y - 4 - PEEK_BAND_TOP);
    if (mode == FACE_PEEK) {
      page_draw(face_state_page(), ctx, band);
      page_draw_indicator(ctx,
                          GRect(bounds.origin.x, bounds.origin.y + dots_y,
                                bounds.size.w, 6),
                          face_state_page_ordinal(),
                          face_state_enabled_count());
    } else {  // FACE_OVERLAY
      overlay_draw(ctx, band);
    }
  }

  StatusBannerMode banner_mode =
      (d->rain_alert_min >= 0 && !s_banner_alt) ? STATUS_BANNER_RAIN
                                                : STATUS_BANNER_UPDATED;
  ui_draw_status_banner(ctx, bounds, banner_mode, d->rain_alert_min,
                        d->last_updated);
}

static void prv_banner_tick(void *ctx) {
  (void)ctx;
  s_banner_timer = NULL;
  s_banner_alt = !s_banner_alt;
  prv_mark_dirty();
  if (weather_data_get()->rain_alert_min >= 0) {
    s_banner_timer = app_timer_register(BANNER_FLIP_MS, prv_banner_tick, NULL);
  }
}

// Start/stop the flip timer to match the current rain-alert state.
static void prv_banner_reconcile(void) {
  bool want = weather_data_get()->rain_alert_min >= 0;
  if (want && !s_banner_timer) {
    s_banner_alt = false;  // lead with the rain pill
    s_banner_timer = app_timer_register(BANNER_FLIP_MS, prv_banner_tick, NULL);
  } else if (!want && s_banner_timer) {
    app_timer_cancel(s_banner_timer);
    s_banner_timer = NULL;
    s_banner_alt = false;
  }
}

// Night mode: force the dark theme between sunset and sunrise, restoring
// the user's day theme after. s_night_theme_applied tracks whether the
// current dark theme is ours (so we never clobber a deliberate choice).
static bool s_night_theme_applied = false;

static void prv_apply_ambient(void) {
  clock_zone_recompute_night();
  bool want_night_theme = settings_get_night_mode() && clock_zone_is_night();
  if (want_night_theme && !s_night_theme_applied) {
    settings_set_day_theme((int)theme_get());
    theme_set(THEME_DARK);
    s_night_theme_applied = true;
  } else if (!want_night_theme && s_night_theme_applied) {
    theme_set((ThemeMode)settings_get_day_theme());
    s_night_theme_applied = false;
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  clock_zone_update_time();
  prv_apply_ambient();
  comm_check_staleness();  // refetch if data is >30 min old
  prv_mark_dirty();
}

// Data or config arrived (weather fields, theme, gesture mode, ...).
static void prv_on_data(void) {
  prv_apply_ambient();
  prv_banner_reconcile();
  face_state_on_data();
}

static void prv_unobstructed_change(AnimationProgress progress, void *ctx) {
  (void)progress; (void)ctx;
  prv_mark_dirty();
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_root_layer = layer_create(bounds);
  layer_set_update_proc(s_root_layer, prv_root_update_proc);
  layer_add_child(root, s_root_layer);

  UnobstructedAreaHandlers handlers = {
    .change = prv_unobstructed_change,
  };
  unobstructed_area_service_subscribe(handlers, NULL);
}

static void prv_window_unload(Window *window) {
  unobstructed_area_service_unsubscribe();
  layer_destroy(s_root_layer);
  s_root_layer = NULL;
}

static void prv_init(void) {
  settings_init();
  theme_init();
  weather_data_init_mock();
  // Load the cached blob over the mock BEFORE the first draw so units and
  // values don't flash from mock to real (the app's units-flash fix).
  comm_set_update_callback(prv_on_data);
  comm_load_cache();
  clock_zone_update_time();
  prv_apply_ambient();
  prv_banner_reconcile();  // the cache may carry an active rain alert

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  theme_apply_to_window(s_window);
  window_stack_push(s_window, true);

  face_state_init(prv_mark_dirty);
  gesture_init();
  anim_set_redraw_callback(prv_mark_dirty);
  anim_init();
  comm_init();

  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  if (s_banner_timer) {
    app_timer_cancel(s_banner_timer);
    s_banner_timer = NULL;
  }
  comm_deinit();
  anim_deinit();
  gesture_deinit();
  face_state_deinit();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}
