#include <pebble.h>
#include "theme.h"
#include "ui.h"
#include "settings.h"
#include "weather_data.h"
#include "anim.h"
#include "clock_zone.h"
#include "face_layout.h"
#include "face_fonts.h"
#include "face_state.h"
#include "gesture.h"
#include "comm.h"
#include "update_notes.h"
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

// Reflow threshold for the one-line compact fallback (rect draws at ub.y+2 in a
// 30px box; round at ub.y+UI_HEADER_Y). The full face's minimum comes from
// face_layout_min_core_h() — the flow layout is the single source of truth for
// how much room the real face needs, so there is no anchor table to keep in
// lockstep here any more. COMPACT_MIN_H is below that minimum on every class,
// which keeps the cascade well-ordered.
#define COMPACT_MIN_H PBL_IF_ROUND_ELSE(56, 34)

static void prv_root_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  WeatherData *d = weather_data_get();
  int H = bounds.size.h;
  FaceMode mode = face_state_mode();

  // Quick View: while a timeline event obstructs the bottom, skip the mode
  // dispatch and the status pill (both draw into the obstructed region) and show
  // the clock instead. Never trust the reported rect verbatim — some firmwares
  // (seen on Core Devices PebbleOS with the calendar Quick View) hand back a
  // degenerate or shifted rect; standardize and clip it to our own bounds first.
  // Then pick by how much room is left, so the time is visible whenever it can be
  // and the face is never blank:
  //   - room for the core stack    -> the real face, reflowed INTO `ub` (the
  //     flow sheds its optional rows to fit rather than hiding under the card)
  //   - only a sliver              -> the one-line compact time+temp inside `ub`
  //   - degenerate (near-zero)     -> full face on full bounds; nothing else
  //     would show
  GRect ub = layer_get_unobstructed_bounds(layer);
  grect_standardize(&ub);
  grect_clip(&ub, &bounds);

  // The show-once update-notes card owns the whole screen, so it is handled
  // ahead of the Quick View cascade below (which returns early and would
  // otherwise skip the mode dispatch entirely). It draws into `ub`, so a
  // timeline card shrinks it rather than hiding it — and it may DECLINE when
  // there is too little room, in which case we fall through to the normal clock
  // and the card stays armed for a later frame. It records itself as seen only
  // once it has actually painted.
  if (mode == FACE_UPDATE_NOTES && update_notes_draw(ctx, ub)) return;

  if (ub.size.h < bounds.size.h && settings_get_quick_view_reflow()) {
    if (ub.size.h >= face_layout_min_core_h()) {
      clock_zone_draw_full(ctx, ub);
    } else if (ub.size.h < COMPACT_MIN_H) {
      clock_zone_draw_full(ctx, bounds);
    } else {
      clock_zone_draw_compact(ctx, ub);
    }
    return;
  }

  // FACE_UPDATE_NOTES joins CLOCK here: if we reach this line in that mode the
  // card declined to draw (too little unobstructed room), and the clock is the
  // right fallback — the peek chrome below would draw a page band over nothing.
  if (mode == FACE_CLOCK || mode == FACE_UPDATE_NOTES) {
    clock_zone_draw_full(ctx, bounds);
  } else {
    clock_zone_draw_compact(ctx, bounds);
    int dots_y = PEEK_DOTS_Y(H);
    GRect band = GRect(bounds.origin.x, bounds.origin.y + PEEK_BAND_TOP,
                       bounds.size.w, dots_y - 4 - PEEK_BAND_TOP);
    if (mode == FACE_PEEK) {
      page_draw(face_state_page(), ctx, band);
      // Deck dots say "there are more pages this way". Single peek pins ONE
      // page, so they would be a lie there.
      if (settings_get_gesture_mode() != GESTURE_SINGLE_PEEK) {
        page_draw_indicator(ctx,
                            GRect(bounds.origin.x, bounds.origin.y + dots_y,
                                  bounds.size.w, 6),
                            face_state_page_ordinal(),
                            face_state_enabled_count());
      }
    } else {  // FACE_OVERLAY
      overlay_draw(ctx, band);
    }
  }

  // Status pill for the PEEK/OVERLAY modes only — in CLOCK mode the resting
  // face draws its own status row inside the flow (which is what lets the user
  // make it permanent, and lets the stack recenter when they turn it off), so
  // drawing ui.c's bottom-anchored banner here too would double it. On a peek
  // the rule stays what it always was: a rain alert always shows, and the
  // last-updated stamp only once the data is actually stale.
  // PEEK/OVERLAY only — named explicitly rather than "not CLOCK", so the
  // full-screen update-notes card never gets a banner stamped across it.
  if (mode == FACE_PEEK || mode == FACE_OVERLAY) {
    bool rain = (d->rain_alert_min >= 0);
    if (rain || comm_data_is_stale()) {
      StatusBannerMode banner_mode =
          (rain && !s_banner_alt) ? STATUS_BANNER_RAIN : STATUS_BANNER_UPDATED;
      ui_draw_status_banner(ctx, bounds, banner_mode, d->rain_alert_min,
                            d->last_updated);
    }
  }
}

static void prv_banner_tick(void *ctx) {
  (void)ctx;
  s_banner_timer = NULL;
  s_banner_alt = !s_banner_alt;
  clock_zone_toggle_status_alt();  // the resting face's own status row
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
    clock_zone_reset_status_alt();
    s_banner_timer = app_timer_register(BANNER_FLIP_MS, prv_banner_tick, NULL);
  } else if (!want && s_banner_timer) {
    app_timer_cancel(s_banner_timer);
    s_banner_timer = NULL;
    s_banner_alt = false;
  }
}

// Night mode: force the dark theme between sunset and sunrise, restoring
// the user's day theme after. s_night_theme_applied tracks whether the current
// dark theme is ours (so we never clobber a deliberate choice).
//
// This MUST be persisted, not RAM-only. theme_set writes the theme to persist,
// but a watchface is reloaded constantly — open any app and come back and this
// process restarts. With the flag in RAM only, the second night-time launch
// read back the forced dark theme, saw the flag clear, and took the "entering
// night" branch again — which does settings_set_day_theme(theme_get()) and so
// recorded DARK as the user's *day* theme. Their light preference was destroyed,
// sunrise "restored" dark, and it looked like a deliberate choice rather than a
// bug. Persisting the flag keeps the two halves of the override in step.
//
// 401, from the out-of-band 400..409 block (see update_notes.c for why new keys
// go there rather than at the top of the packed 10..35 settings range).
#define PERSIST_KEY_NIGHT_APPLIED 401
static bool s_night_theme_applied = false;

static void prv_night_flag_load(void) {
  s_night_theme_applied = persist_exists(PERSIST_KEY_NIGHT_APPLIED)
                              ? persist_read_bool(PERSIST_KEY_NIGHT_APPLIED)
                              : false;
}

static void prv_night_flag_set(bool applied) {
  s_night_theme_applied = applied;
  persist_write_bool(PERSIST_KEY_NIGHT_APPLIED, applied);
}

static void prv_apply_ambient(void) {
  clock_zone_recompute_night();
  bool want_night_theme = settings_get_night_mode() && clock_zone_is_night();
  if (want_night_theme && !s_night_theme_applied) {
    // Never record DARK as the day theme. Night mode's override is the only way
    // the theme can already be DARK here with the flag clear, so recording it
    // would bake the override in as a preference — the exact corruption above.
    // A user who genuinely prefers dark is unaffected: comm.c writes the day
    // theme on every Clay theme change, so their real choice is already stored.
    // This also covers upgrading mid-night, where key 401 is absent and the
    // flag reads false against an override we set before this build existed.
    ThemeMode current = theme_get();
    if (current != THEME_DARK) settings_set_day_theme((int)current);
    theme_set(THEME_DARK);
    prv_night_flag_set(true);
  } else if (!want_night_theme && s_night_theme_applied) {
    theme_set((ThemeMode)settings_get_day_theme());
    prv_night_flag_set(false);
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

// Redraw once the obstruction settles. `change` fires per animation frame, but
// a firmware that snaps the obstruction in without animating may never call it;
// did_change is the timer-free net that guarantees a final redraw either way.
static void prv_unobstructed_did_change(void *ctx) {
  (void)ctx;
  prv_mark_dirty();
}

// Charge/plug changes. The minute tick would pick the new reading up anyway;
// this only makes a battery complication react the moment the cable goes in
// (and costs nothing when no slot is showing one — the service is
// event-driven, not a timer, so the at-rest one-wakeup-per-minute rule holds).
static void prv_battery_handler(BatteryChargeState state) {
  (void)state;
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
    .did_change = prv_unobstructed_did_change,
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
  // Before the first prv_apply_ambient: it needs to know whether the theme
  // theme_init just loaded is the user's or a night override we left behind.
  prv_night_flag_load();
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

  // After face_state_init — it resets the mode to CLOCK and may force PEEK for
  // an auto-rotate user, so the card has to claim the mode once that settled.
  // settings_init has already latched the fresh-install probe, and the window is
  // pushed, so the root layer exists to be marked dirty.
  update_notes_maybe_show();

  comm_init();

  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
  battery_state_service_subscribe(prv_battery_handler);
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  if (s_banner_timer) {
    app_timer_cancel(s_banner_timer);
    s_banner_timer = NULL;
  }
  comm_deinit();
  anim_deinit();
  gesture_deinit();
  face_state_deinit();
  face_fonts_deinit();  // release the lazily-loaded custom XL clock face
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}
