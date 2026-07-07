#include <pebble.h>

// Phase 0 hello-clock: proves the toolchain, path, and watchface plumbing.
// Replaced by the real face (clock_zone + face_state) in Phase 1+.

static Window *s_window;
static TextLayer *s_time_layer;

static void prv_update_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  static char s_buf[8];
  strftime(s_buf, sizeof(s_buf), clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
  text_layer_set_text(s_time_layer, s_buf);
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  prv_update_time();
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_time_layer = text_layer_create(
      GRect(0, bounds.size.h / 2 - 24, bounds.size.w, 48));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer,
                      fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  prv_update_time();
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}
