#include "face_state.h"
#include "anim.h"
#include "weather_data.h"

// Peek pages auto-return to the clock after this long with no nudges —
// watch-face etiquette: the clock is never more than a short wait away.
#define PEEK_IDLE_MS 7000
// Auto-rotate mode advances pages on this cadence (Phase 5).
#define AUTO_ROTATE_MS 10000
// Rain auto-peek returns to the clock on the minute tick, not on a timer of
// its own — the peek APPEARS on a data-arrival redraw (which happens anyway)
// and exits with the tick redraw, so a rain alert adds zero wakeups beyond the
// at-rest one-per-minute. Data arrivals are usually tick-aligned (the
// staleness refetch runs on the tick), so the dwell is naturally ~a minute;
// the hold below stops a mid-minute arrival (Clay save, launch fetch) from
// flashing the page for a second or two and yanking it away.
#define RAIN_PEEK_MIN_HOLD_S 30

static FaceMode s_mode = FACE_CLOCK;
static FacePage s_page = PAGE_HOURS;
// Deck resume: last page shown while paging the deck; -1 = never dealt, so the
// first nudge starts at the first enabled page. Lets a nudge from the resting
// clock continue where the deck left off instead of always restarting at page 1
// once the 7s idle timer has returned to the clock.
static int s_deck_pos = -1;
static AppTimer *s_idle_timer = NULL;
static AppTimer *s_rotate_timer = NULL;
static void (*s_mark_dirty)(void) = NULL;
// True while a rain auto-peek is up and owes its return to the minute tick.
// Cleared by any nudge (the user took the deck over) and by every path that
// hands the mode to another owner, so the tick never yanks a page the user
// navigated to.
static bool s_rain_peek_until_tick = false;
static time_t s_rain_peek_shown = 0;

static void prv_redraw(void) {
  if (s_mark_dirty) s_mark_dirty();
}

// The update-notes card owns the whole screen and must survive its dwell. Three
// paths below would otherwise overwrite s_mode underneath it: the auto-rotate
// tick, the gesture-mode reconciler, and the deck nudge. Each consults this.
static bool prv_notes_up(void) { return s_mode == FACE_UPDATE_NOTES; }

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
  // Skip this beat while the notes card is up, but keep the cadence running so
  // the deck resumes on its own once the card is dismissed. Without this the
  // card would be replaced by a peek page within 10s on every auto-rotate user.
  if (prv_notes_up()) {
    s_rotate_timer = app_timer_register(AUTO_ROTATE_MS, prv_rotate_fired, NULL);
    return;
  }
  int next = prv_next_enabled((int)s_page);
  if (next >= PAGE_COUNT) next = prv_next_enabled(-1);  // wrap
  if (next < PAGE_COUNT) {
    s_page = (FacePage)next;
    s_mode = FACE_PEEK;
  } else {
    s_mode = FACE_CLOCK;  // no pages enabled
  }
  prv_redraw();
  s_rotate_timer = app_timer_register(AUTO_ROTATE_MS, prv_rotate_fired, NULL);
}

// Reconcile timers/state with the current gesture mode. Idempotent — called
// on init and whenever a Clay save may have changed GestureMode.
void face_state_apply_mode(void) {
  bool want_rotate = (settings_get_gesture_mode() == GESTURE_AUTO_ROTATE) &&
                     settings_enabled_page_count() > 0;
  // The timer bookkeeping stays unconditional so a Clay save always reconciles,
  // but the mode assignments defer to a live notes card — this runs on EVERY
  // weather payload via face_state_on_data, and comm fires one ~750ms after
  // launch, right when the card is up.
  if (want_rotate && !s_rotate_timer) {
    if (!prv_notes_up()) {
      prv_cancel_idle();
      s_rain_peek_until_tick = false;  // reconfig supersedes a live auto-peek
      int first = prv_next_enabled(-1);
      s_page = (FacePage)first;
      s_mode = FACE_PEEK;
    }
    s_rotate_timer = app_timer_register(AUTO_ROTATE_MS, prv_rotate_fired, NULL);
    prv_redraw();
  } else if (!want_rotate && s_rotate_timer) {
    app_timer_cancel(s_rotate_timer);
    s_rotate_timer = NULL;
    if (!prv_notes_up()) {
      s_rain_peek_until_tick = false;
      s_mode = FACE_CLOCK;
    }
    prv_redraw();
  }
}

void face_state_init(void (*mark_dirty)(void)) {
  s_mark_dirty = mark_dirty;
  s_mode = FACE_CLOCK;
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
  // The else-branch below assumes a peek is already up: it advances s_page
  // without ever assigning s_mode. Reaching it with the notes card up would
  // leave the card on screen over a mutated page. face_state_on_nudge already
  // intercepts that case; this guards the direct callers (face_state_next_page).
  if (prv_notes_up()) {
    face_state_reset_to_clock();
    return;
  }
  if (s_mode == FACE_CLOCK) {
    // Resume after the last page shown, wrapping past the end to the first
    // enabled page so a nudge from the clock is never dead while pages exist.
    int next = prv_next_enabled(s_deck_pos);
    if (next >= PAGE_COUNT) next = prv_next_enabled(-1);
    if (next >= PAGE_COUNT) return;  // no pages enabled: stay a plain clock
    s_page = (FacePage)next;
    s_deck_pos = next;
    s_mode = FACE_PEEK;
    prv_arm_idle(PEEK_IDLE_MS);
  } else {
    int next = prv_next_enabled((int)s_page);
    if (next >= PAGE_COUNT) {
      s_mode = FACE_CLOCK;  // stepped past the last page: back to the clock
      prv_cancel_idle();
      // Leave s_deck_pos on the last page shown: the next clock nudge wraps to
      // the first page, so the deck cycles rather than dead-ending.
    } else {
      s_page = (FacePage)next;
      s_deck_pos = next;
      prv_arm_idle(PEEK_IDLE_MS);
    }
  }
  prv_redraw();
}

// Single peek: one nudge opens the ONE view the user pinned, the next closes
// it. That view is the dense everything-overlay by default, or any single peek
// page — deliberately independent of the Peek Pages toggles, which deal the
// Nudge Deck / Auto-rotate decks rather than this fixed view.
static void prv_nudge_single_peek(void) {
  if (s_mode == FACE_CLOCK) {
    SinglePeekView view = settings_get_single_peek_view();
    if (view == SINGLE_PEEK_OVERLAY) {
      s_mode = FACE_OVERLAY;
    } else {
      s_page = (FacePage)(view - 1);  // 1..4 -> PAGE_HOURS..PAGE_SUN_MOON
      s_mode = FACE_PEEK;
    }
    prv_arm_idle(PEEK_IDLE_MS);
  } else {
    s_mode = FACE_CLOCK;
    prv_cancel_idle();
  }
  prv_redraw();
}

void face_state_on_nudge(void) {
  anim_kick();  // every nudge re-wakes the decorative animation
  // Any nudge means the user took over — the rain auto-peek's tick-return
  // must not fire later and yank whatever they navigated to.
  s_rain_peek_until_tick = false;
  // Dismiss the notes card BEFORE the gesture-mode switch. gesture.c delivers
  // every accepted tap here regardless of GestureMode — it is this switch that
  // drops them for AUTO_ROTATE/OFF — so intercepting above it is what makes the
  // card dismissable in all four modes rather than only two.
  if (prv_notes_up()) {
    face_state_reset_to_clock();
    return;
  }
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
static int s_last_rain_alert = -1;

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
    // No idle timer: face_state_on_minute_tick returns to the clock. This
    // redraw rides the data arrival, the exit rides the tick — the peek costs
    // no wakeup the face wasn't already making.
    s_rain_peek_until_tick = true;
    s_rain_peek_shown = time(NULL);
  }

  prv_redraw();
}

// Called from main.c's minute tick (which redraws regardless). Returns the
// rain auto-peek to the clock once it has been up for the minimum hold —
// possibly the second tick, when the alert arrived mid-minute. Only touches
// the exact state the auto-peek set: if the user nudged onward or anything
// else changed the mode, the flag is already clear or the mode check fails.
void face_state_on_minute_tick(void) {
  if (!s_rain_peek_until_tick) return;
  if (s_mode != FACE_PEEK || s_page != PAGE_HOURS) {
    s_rain_peek_until_tick = false;
    return;
  }
  if (time(NULL) - s_rain_peek_shown < RAIN_PEEK_MIN_HOLD_S) return;
  s_rain_peek_until_tick = false;
  s_mode = FACE_CLOCK;
}

void face_state_reset_to_clock(void) {
  prv_cancel_idle();
  s_rain_peek_until_tick = false;
  s_mode = FACE_CLOCK;
  prv_redraw();
}

// Reuses the peek idle timer rather than adding a third AppTimer: it is already
// one-shot, self-nulling, cancelled in face_state_deinit, and prv_idle_fired
// unconditionally returns to FACE_CLOCK — exactly the timeout net the card needs.
void face_state_show_update_notes(uint32_t timeout_ms) {
  s_mode = FACE_UPDATE_NOTES;
  prv_arm_idle(timeout_ms);
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
  if (prv_notes_up()) { face_state_reset_to_clock(); return; }
  if (s_mode != FACE_PEEK) return;
  int prev = prv_prev_enabled((int)s_page);
  if (prev < 0) {
    s_mode = FACE_CLOCK;
    prv_cancel_idle();
  } else {
    s_page = (FacePage)prev;
    s_deck_pos = prev;
    prv_arm_idle(PEEK_IDLE_MS);
  }
  prv_redraw();
}

void face_state_open_overlay(void) {
  if (settings_get_gesture_mode() == GESTURE_AUTO_ROTATE) return;
  anim_kick();
  if (prv_notes_up()) { face_state_reset_to_clock(); return; }
  s_mode = FACE_OVERLAY;
  prv_arm_idle(PEEK_IDLE_MS);
  prv_redraw();
}

void face_state_dismiss(void) {
  if (settings_get_gesture_mode() == GESTURE_AUTO_ROTATE) return;
  anim_kick();
  face_state_reset_to_clock();
}
