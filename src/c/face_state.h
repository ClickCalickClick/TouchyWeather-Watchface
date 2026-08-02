#pragma once
#include <pebble.h>
#include "settings.h"

// The face's interaction state machine. Drives what the root layer renders:
//   FACE_CLOCK   — the resting watch face (big time, date, weather row)
//   FACE_PEEK    — compact time line + one peek page in the lower band
//   FACE_OVERLAY — compact time line + the dense everything-overlay
//   FACE_UPDATE_NOTES — full-screen show-once "New on the horizon" card
//
// APPEND only. Nothing persists a FaceMode, but both consumers in main.c are
// if/else chains rather than switches, so a new value silently falls into the
// PEEK/OVERLAY branch unless every site is taught about it.
typedef enum {
  FACE_CLOCK = 0,
  FACE_PEEK = 1,
  FACE_OVERLAY = 2,
  FACE_UPDATE_NOTES = 3,
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

// Show the one-time update-notes card for `timeout_ms`, after which the idle
// timer returns to the clock on its own. Idempotent — calling it again just
// re-arms the timeout, which is how the drawer restarts the reading window once
// the card actually becomes visible. update_notes.c decides whether to call it.
void face_state_show_update_notes(uint32_t timeout_ms);

// Minute tick (main.c calls this from its tick handler, which redraws
// regardless). Returns a live rain auto-peek to the clock once it has been up
// ≥30s — the peek's whole lifecycle rides redraws the face was already making,
// so a rain alert adds no wakeups. No-op in every other state.
void face_state_on_minute_tick(void);

// A nudge = accel tap / wrist flick (later: touch tap). Behavior dispatches
// on settings_get_gesture_mode(). While the update-notes card is up, ANY nudge
// dismisses it instead — in every gesture mode, including the ones that
// otherwise ignore nudges.
void face_state_on_nudge(void);
// New weather data arrived (Phase 6 wires rain auto-show through this).
void face_state_on_data(void);
// Force back to CLOCK (idle timeout does this internally; comm/config
// changes may want it too).
void face_state_reset_to_clock(void);
// Reconcile timers/state with the current gesture mode (idempotent).
void face_state_apply_mode(void);

// Direct navigation for the (future) touch gestures — swipe left/right/up/
// down map here. No-ops in AUTO_ROTATE mode.
void face_state_next_page(void);
void face_state_prev_page(void);
void face_state_open_overlay(void);
void face_state_dismiss(void);  // back to CLOCK
