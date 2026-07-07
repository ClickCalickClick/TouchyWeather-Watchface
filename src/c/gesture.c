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

#if ENABLE_TOUCH
#include "comm.h"
#include "anim.h"

// Gesture thresholds — identical to the app's touch_handler so the two
// feel the same the day faces get touch.
#define HSWIPE_THRESHOLD 30
#define VSWIPE_THRESHOLD 30
#define TAP_THRESHOLD 15

static int16_t s_start_x = 0, s_start_y = 0;

// Classify on liftoff from the touchdown-to-liftoff delta (the app's exact
// dx/dy discrimination). Mapping:
//   horizontal swipe  left = next page, right = previous
//   swipe up          open the everything-overlay (app's "swipe up = detail")
//   swipe down        dismiss to clock; from clock = manual refresh
//                     (the app's pull-to-refresh idiom, minus the sheet)
//   tap               lower half = nudge; upper half = wake animation only
static void prv_touch_classify(int16_t x, int16_t y) {
  int dx = x - s_start_x;
  int dy = y - s_start_y;
  int adx = dx < 0 ? -dx : dx;
  int ady = dy < 0 ? -dy : dy;

  if (adx > HSWIPE_THRESHOLD && adx > ady) {
    if (dx < 0) face_state_next_page();
    else face_state_prev_page();
  } else if (ady > VSWIPE_THRESHOLD && ady > adx) {
    if (dy < 0) {
      face_state_open_overlay();
    } else if (face_state_mode() != FACE_CLOCK) {
      face_state_dismiss();
    } else {
      anim_kick();
      comm_request_refresh();
    }
  } else if (adx < TAP_THRESHOLD && ady < TAP_THRESHOLD) {
    if (s_start_y > PBL_DISPLAY_HEIGHT / 2) {
      face_state_on_nudge();
    } else {
      anim_kick();
    }
  }
}
#endif  // ENABLE_TOUCH

static void prv_touch_handler(const TouchEvent *event, void *context) {
  (void)context;
#if TOUCH_SPIKE
  APP_LOG(APP_LOG_LEVEL_INFO, "SPIKE touch event type=%d x=%d y=%d",
          (int)event->type, (int)event->x, (int)event->y);
#endif
#if ENABLE_TOUCH
  switch (event->type) {
    case TouchEvent_Touchdown:
      s_start_x = event->x;
      s_start_y = event->y;
      break;
    case TouchEvent_PositionUpdate:
      break;
    case TouchEvent_Liftoff:
      prv_touch_classify(event->x, event->y);
      break;
    default:
      break;
  }
#else
  (void)event;
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
