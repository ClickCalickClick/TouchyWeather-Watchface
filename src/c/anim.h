#pragma once
#include <pebble.h>

void anim_init(void);
void anim_deinit(void);

// Reset the decorative-animation idle deadline and ensure the ticker is
// running. Call on any user/data activity (nudge, data arrival) so
// decorative animation resumes for another window and then re-settles.
// Cheap and idempotent.
void anim_kick(void);

uint32_t anim_get_frame(void);

// Face adaptation: the app's anim.c called nav_redraw()/refresh_sheet_is_active()
// directly. The face decouples that into two hooks wired up in main.c:
//   redraw callback  — invoked when an animation tick wants a repaint
//   active predicate — extra "keep ticking even past the idle deadline"
//     condition (e.g. a peek transition in flight); may be NULL.
void anim_set_redraw_callback(void (*cb)(void));
void anim_set_active_predicate(bool (*fn)(void));
