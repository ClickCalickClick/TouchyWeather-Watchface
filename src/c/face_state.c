#include "face_state.h"
#include "anim.h"

// Peek pages auto-return to the clock after this long with no nudges —
// watch-face etiquette: the clock is never more than a short wait away.
#define PEEK_IDLE_MS 7000
// Auto-rotate mode advances pages on this cadence (Phase 5).
#define AUTO_ROTATE_MS 10000

static FaceMode s_mode = FACE_CLOCK;
static FacePage s_page = PAGE_HOURS;
static AppTimer *s_idle_timer = NULL;
static void (*s_mark_dirty)(void) = NULL;

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

void face_state_init(void (*mark_dirty)(void)) {
  s_mark_dirty = mark_dirty;
  s_mode = FACE_CLOCK;
}

void face_state_deinit(void) {
  prv_cancel_idle();
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

void face_state_on_data(void) {
  anim_kick();
  prv_redraw();
}

void face_state_reset_to_clock(void) {
  prv_cancel_idle();
  s_mode = FACE_CLOCK;
  prv_redraw();
}
