#pragma once
#include <pebble.h>

// Gesture input for the face.
//   Today: AccelTapService — the "nudge" (wrist flick / firm tap on the
//   watch body or glass). The only on-demand gesture watch faces get.
//   Future: touch_service, written in gesture.c behind ENABLE_TOUCH
//   (watchapp-only on current firmware; see README spike notes).
void gesture_init(void);
void gesture_deinit(void);
