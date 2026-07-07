#pragma once
#include <pebble.h>
#include "settings.h"

// The face's interaction state machine. Drives what the root layer renders:
//   FACE_CLOCK   — the resting watch face (big time, date, weather row)
//   FACE_PEEK    — compact time line + one peek page in the lower band
//   FACE_OVERLAY — compact time line + the dense everything-overlay
typedef enum {
  FACE_CLOCK = 0,
  FACE_PEEK = 1,
  FACE_OVERLAY = 2,
} FaceMode;

// mark_dirty is the single redraw funnel from main.c.
void face_state_init(void (*mark_dirty)(void));
void face_state_deinit(void);

FaceMode face_state_mode(void);
// Current peek page (only meaningful in FACE_PEEK).
FacePage face_state_page(void);
// Position of the current page within the enabled set + the enabled count,
// for the page-dot indicator.
int face_state_page_ordinal(void);
int face_state_enabled_count(void);

// A nudge = accel tap / wrist flick (later: touch tap). Behavior dispatches
// on settings_get_gesture_mode().
void face_state_on_nudge(void);
// New weather data arrived (Phase 6 wires rain auto-show through this).
void face_state_on_data(void);
// Force back to CLOCK (idle timeout does this internally; comm/config
// changes may want it too).
void face_state_reset_to_clock(void);
// Reconcile timers/state with the current gesture mode (idempotent).
void face_state_apply_mode(void);
