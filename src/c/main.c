#include <pebble.h>
#include "theme.h"
#include "ui.h"
#include "settings.h"
#include "weather_data.h"
#include "anim.h"
#include "clock_zone.h"

static Window *s_window;
static Layer *s_root_layer;

// Single redraw funnel: every driver (minute tick, anim ticker, data
// arrival, state changes) goes through here — one canvas, one dirty bit.
static void prv_mark_dirty(void) {
  if (s_root_layer) layer_mark_dirty(s_root_layer);
}

static void prv_root_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  WeatherData *d = weather_data_get();

  clock_zone_draw_full(ctx, bounds);
  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  clock_zone_update_time();
  prv_mark_dirty();
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_root_layer = layer_create(bounds);
  layer_set_update_proc(s_root_layer, prv_root_update_proc);
  layer_add_child(root, s_root_layer);
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_root_layer);
  s_root_layer = NULL;
}

static void prv_init(void) {
  settings_init();
  theme_init();
  weather_data_init_mock();
  clock_zone_update_time();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  theme_apply_to_window(s_window);
  window_stack_push(s_window, true);

  anim_set_redraw_callback(prv_mark_dirty);
  anim_init();

  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  anim_deinit();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}
