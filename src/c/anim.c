#include "anim.h"
#include "settings.h"

#define ANIM_PERIOD_MS 100

// Decorative animation (hero condition icon, rotating banner) freezes this
// many frames (~8 s at 10 Hz) after the last activity, so the continuous
// 10 Hz redraw doesn't drain the battery on a face that is always
// foregrounded. Tunable 50-100 (5-10 s).
#define ANIM_TIMEOUT_FRAMES 80

static AppTimer *s_timer = NULL;
static uint32_t s_frame = 0;
// Decorative animation is allowed while s_frame < s_deadline_frame. anim_kick()
// pushes this forward on activity; when the deadline passes (and the active
// predicate is quiet) the ticker stops entirely and re-arms on the next kick.
static uint32_t s_deadline_frame = 0;

static void (*s_redraw_cb)(void) = NULL;
static bool (*s_active_pred)(void) = NULL;

static void prv_tick(void *ctx);

static bool prv_decorative_active(void) {
  if (!settings_get_animations_enabled()) return false;
  return s_frame < s_deadline_frame;
}

static void prv_ensure_running(void) {
  if (!s_timer) {
    s_timer = app_timer_register(ANIM_PERIOD_MS, prv_tick, NULL);
  }
}

static void prv_tick(void *ctx) {
  (void)ctx;
  s_timer = NULL;  // the timer that just fired is spent
  s_frame++;

  bool decorative = prv_decorative_active();
  bool extra = s_active_pred ? s_active_pred() : false;

  if ((decorative || extra) && s_redraw_cb) {
    s_redraw_cb();
  }

  // Keep the timer alive only while something still needs it. Once decorative
  // animation has frozen and nothing else is animating, stop entirely (zero
  // idle cost); anim_kick() re-arms on the next activity.
  if (decorative || extra) {
    s_timer = app_timer_register(ANIM_PERIOD_MS, prv_tick, NULL);
  }
}

void anim_init(void) {
  s_frame = 0;
  // Animate through the first post-launch window, then settle.
  s_deadline_frame = ANIM_TIMEOUT_FRAMES;
  s_timer = app_timer_register(ANIM_PERIOD_MS, prv_tick, NULL);
}

void anim_deinit(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
}

void anim_kick(void) {
  // Reset the decorative idle deadline and make sure the ticker is running.
  // Safe to call even when animations are disabled: the next tick
  // re-evaluates and stops the ticker again if nothing needs it.
  s_deadline_frame = s_frame + ANIM_TIMEOUT_FRAMES;
  prv_ensure_running();
}

uint32_t anim_get_frame(void) { return s_frame; }

void anim_set_redraw_callback(void (*cb)(void)) { s_redraw_cb = cb; }
void anim_set_active_predicate(bool (*fn)(void)) { s_active_pred = fn; }
