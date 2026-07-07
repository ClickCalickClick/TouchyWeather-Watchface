#include "gesture.h"
#include "face_state.h"

// --- Touch (future) -------------------------------------------------------
// touch_service is watchapp-only on current firmware — the SDK docs state
// touch input is not supported in watchfaces. The full gesture mapping ships
// here behind ENABLE_TOUCH so the day firmware delivers touch to faces this
// flips to 1 and swipes/taps light up with a rebuild. Phase 2 spike result
// is recorded in README.md.
#define ENABLE_TOUCH 0
// Compile the spike probe (subscribe + log every event) without enabling
// any behavior. Used once on the emery emulator to test whether touch
// events reach a watchface at all; leave 0 for release builds.
#define TOUCH_SPIKE 0

#if (ENABLE_TOUCH || TOUCH_SPIKE) && defined(PBL_TOUCH)

static void prv_touch_handler(const TouchEvent *event, void *context) {
  (void)context;
#if TOUCH_SPIKE
  APP_LOG(APP_LOG_LEVEL_INFO, "SPIKE touch event type=%d x=%d y=%d",
          (int)event->type, (int)event->x, (int)event->y);
#endif
#if ENABLE_TOUCH
  // Gesture mapping (mirrors the app's touch_handler thresholds) lands in
  // Phase 7: tap lower half = nudge; swipe L/R = page nav; swipe up =
  // overlay; swipe down = dismiss / refresh.
#endif
}

static void prv_touch_init(void) {
  if (touch_service_is_enabled()) {
    touch_service_subscribe(prv_touch_handler, NULL);
    APP_LOG(APP_LOG_LEVEL_INFO, "SPIKE touch_service subscribed (enabled=1)");
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "SPIKE touch_service_is_enabled() == false");
  }
}

static void prv_touch_deinit(void) {
  touch_service_unsubscribe();
}

#else
static void prv_touch_init(void) {}
static void prv_touch_deinit(void) {}
#endif

// --- Accel tap (the nudge) -------------------------------------------------

static void prv_tap_handler(AccelAxisType axis, int32_t direction) {
  (void)axis;
  (void)direction;
  // Any-axis acceptance: real-wrist taps land on unpredictable axes, and
  // every deck state times out safely back to the clock, so false positives
  // are cheap and missed taps are the worse failure.
  face_state_on_nudge();
}

void gesture_init(void) {
  accel_tap_service_subscribe(prv_tap_handler);
  prv_touch_init();
}

void gesture_deinit(void) {
  accel_tap_service_unsubscribe();
  prv_touch_deinit();
}
