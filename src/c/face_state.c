#include "face_state.h"
#include "anim.h"
#include "weather_data.h"

// Peek pages auto-return to the clock after this long with no nudges —
// watch-face etiquette: the clock is never more than a short wait away.
#define PEEK_IDLE_MS 7000
// Auto-rotate mode advances pages on this cadence (Phase 5).
#define AUTO_ROTATE_MS 10000
// Rain auto-peek shows the hours page a little longer than a normal peek.
#define RAIN_AUTOSHOW_MS 10000

static FaceMode s_mode = FACE_CLOCK;
static FacePage s_page = PAGE_HOURS;
static AppTimer *s_idle_timer = NULL;
static AppTimer *s_rotate_timer = NULL;
static void (*s_mark_dirty)(void) = NULL;
// Rain auto-peek edge state (declared here so face_state_init can reset it).
static int s_last_rain_alert = -1;

static void prv_redraw(void) {
  if (s_mark_dirty) s_mark_dirty();
}

static void prv_cancel_idle(void) {
  if (s_idle_timer) {
    app_timer_cancel(s_idle_timer);
    s_idle_timer = NULL;
  }
}

static void prv_idle_fired(void *ctx) {
  (void)ctx;
  s_idle_timer = NULL;
  s_mode = FACE_CLOCK;
  prv_redraw();
}

static void prv_arm_idle(uint32_t ms) {
  prv_cancel_idle();
  s_idle_timer = app_timer_register(ms, prv_idle_fired, NULL);
}

// Next enabled page strictly after `from` (in deck order); PAGE_COUNT means
// "past the end" (wrap to clock). Pass -1 to get the first enabled page.
static int prv_next_enabled(int from) {
  for (int i = from + 1; i < PAGE_COUNT; i++) {
    if (settings_get_page_enabled((FacePage)i)) return i;
  }
  return PAGE_COUNT;
}

// Last enabled page strictly before `from`; -1 means "before the start".
static int prv_prev_enabled(int from) {
  for (int i = from - 1; i >= 0; i--) {
    if (settings_get_page_enabled((FacePage)i)) return i;
  }
  return -1;
}

// Auto-rotate: the lower zone cycles enabled pages forever on a timer.
static void prv_rotate_fired(void *ctx) {
  (void)ctx;
  s_rotate_timer = NULL;
  int next = prv_next_enabled((int)s_page);
  if (next >= PAGE_COUNT) next = prv_next_enabled(-1);  // wrap
  if (next < PAGE_COUNT) {
    s_page = (FacePage)next;
    s_mode = FACE_PEEK;
    // Re-arm only while at least one page exists. With none enabled we stop
    // entirely (no recurring wakeup); face_state_apply_mode() restarts
    // rotation when a page is re-enabled.
    s_rotate_timer = app_timer_register(AUTO_ROTATE_MS, prv_rotate_fired, NULL);
  } else {
    s_mode = FACE_CLOCK;  // no pages enabled: leave the timer stopped
  }
  prv_redraw();
}

// Reconcile timers/state with the current gesture mode. Idempotent — called
// on init and whenever a Clay save may have changed GestureMode.
void face_state_apply_mode(void) {
  bool want_rotate = (settings_get_gesture_mode() == GESTURE_AUTO_ROTATE) &&
                     settings_enabled_page_count() > 0;
  if (want_rotate && !s_rotate_timer) {
    prv_cancel_idle();
    int first = prv_next_enabled(-1);
    s_page = (FacePage)first;
    s_mode = FACE_PEEK;
    s_rotate_timer = app_timer_register(AUTO_ROTATE_MS, prv_rotate_fired, NULL);
    prv_redraw();
  } else if (!want_rotate && s_rotate_timer) {
    app_timer_cancel(s_rotate_timer);
    s_rotate_timer = NULL;
    s_mode = FACE_CLOCK;
    prv_redraw();
  }

  // If the page currently being peeked was disabled out from under us (Clay
  // save), snap to the first enabled page — otherwise we'd render a disabled
  // page and face_state_page_ordinal() could index past the dot count.
  if (s_mode == FACE_PEEK && !settings_get_page_enabled(s_page)) {
    int first = prv_next_enabled(-1);
    if (first < PAGE_COUNT) {
      s_page = (FacePage)first;
    } else {
      s_mode = FACE_CLOCK;
      prv_cancel_idle();
    }
  }
}

void face_state_init(void (*mark_dirty)(void)) {
  s_mark_dirty = mark_dirty;
  s_mode = FACE_CLOCK;
  s_last_rain_alert = -1;  // clean edge-detector slate
  prv_cancel_idle();       // no stray idle-return timer in CLOCK
  face_state_apply_mode();
}

void face_state_deinit(void) {
  prv_cancel_idle();
  if (s_rotate_timer) {
    app_timer_cancel(s_rotate_timer);
    s_rotate_timer = NULL;
  }
}

FaceMode face_state_mode(void) { return s_mode; }
FacePage face_state_page(void) { return s_page; }

int face_state_page_ordinal(void) {
  int ord = 0;
  for (int i = 0; i < (int)s_page; i++) {
    if (settings_get_page_enabled((FacePage)i)) ord++;
  }
  return ord;
}

int face_state_enabled_count(void) { return settings_enabled_page_count(); }

static void prv_nudge_deck(void) {
  if (s_mode == FACE_CLOCK) {
    int first = prv_next_enabled(-1);
    if (first >= PAGE_COUNT) return;  // no pages enabled: stay a plain clock
    s_page = (FacePage)first;
    s_mode = FACE_PEEK;
    prv_arm_idle(PEEK_IDLE_MS);
  } else {
    int next = prv_next_enabled((int)s_page);
    if (next >= PAGE_COUNT) {
      s_mode = FACE_CLOCK;  // stepped past the last page: back to the clock
      prv_cancel_idle();
    } else {
      s_page = (FacePage)next;
      prv_arm_idle(PEEK_IDLE_MS);
    }
  }
  prv_redraw();
}

static void prv_nudge_single_peek(void) {
  if (s_mode == FACE_CLOCK) {
    s_mode = FACE_OVERLAY;
    prv_arm_idle(PEEK_IDLE_MS);
  } else {
    s_mode = FACE_CLOCK;
    prv_cancel_idle();
  }
  prv_redraw();
}

void face_state_on_nudge(void) {
  anim_kick();  // every nudge re-wakes the decorative animation
  switch (settings_get_gesture_mode()) {
    case GESTURE_NUDGE_DECK:  prv_nudge_deck(); break;
    case GESTURE_SINGLE_PEEK: prv_nudge_single_peek(); break;
    case GESTURE_AUTO_ROTATE: // pages rotate on their own; nudge = anim only
    case GESTURE_OFF:
      break;
  }
}

// Rain auto-peek is edge-triggered: it fires when a data arrival newly
// reports rain within the hour, not on every refresh while rain persists.
// (s_last_rain_alert is declared at file scope above so init can reset it.)

void face_state_on_data(void) {
  anim_kick();
  face_state_apply_mode();  // a Clay save may have changed GestureMode

  WeatherData *d = weather_data_get();
  int r = d->rain_alert_min;
  bool imminent = (r >= 0 && r < 60);
  bool was_imminent = (s_last_rain_alert >= 0 && s_last_rain_alert < 60);
  s_last_rain_alert = r;
  if (imminent && !was_imminent &&
      settings_get_rain_auto_show() &&
      s_mode == FACE_CLOCK &&
      settings_get_gesture_mode() != GESTURE_AUTO_ROTATE &&
      settings_get_page_enabled(PAGE_HOURS)) {
    s_page = PAGE_HOURS;
    s_mode = FACE_PEEK;
    prv_arm_idle(RAIN_AUTOSHOW_MS);
  }

  prv_redraw();
}

void face_state_reset_to_clock(void) {
  prv_cancel_idle();
  s_mode = FACE_CLOCK;
  prv_redraw();
}

// --- Direct navigation (future touch gestures) ---

void face_state_next_page(void) {
  if (settings_get_gesture_mode() == GESTURE_AUTO_ROTATE) return;
  anim_kick();
  prv_nudge_deck();  // same semantics: advance, wrap past the end to clock
}

void face_state_prev_page(void) {
  if (settings_get_gesture_mode() == GESTURE_AUTO_ROTATE) return;
  anim_kick();
  if (s_mode != FACE_PEEK) return;
  int prev = prv_prev_enabled((int)s_page);
  if (prev < 0) {
    s_mode = FACE_CLOCK;
    prv_cancel_idle();
  } else {
    s_page = (FacePage)prev;
    prv_arm_idle(PEEK_IDLE_MS);
  }
  prv_redraw();
}

void face_state_open_overlay(void) {
  if (settings_get_gesture_mode() == GESTURE_AUTO_ROTATE) return;
  anim_kick();
  s_mode = FACE_OVERLAY;
  prv_arm_idle(PEEK_IDLE_MS);
  prv_redraw();
}

void face_state_dismiss(void) {
  if (settings_get_gesture_mode() == GESTURE_AUTO_ROTATE) return;
  anim_kick();
  face_state_reset_to_clock();
}
