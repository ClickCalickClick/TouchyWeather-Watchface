#pragma once
#include <pebble.h>

// The show-once "New on the horizon" card, ported from the TouchyWeather
// watchapp. Announces what changed after an update, and welcomes a genuinely
// new install.
//
// The port is not a copy. The app pushes a Window, scrolls with a ScrollLayer
// and dismisses on the BACK button; a watch face gets no buttons and no touch,
// so this is a FaceMode drawn into the root layer, dismissed by a nudge, with
// the peek idle timer as the safety net. Nothing can scroll, so the body has to
// fit one screen — see the fallback ladder in the .c.

// Decide whether the card is due and, if so, hand the face into
// FACE_UPDATE_NOTES. Call once at init, AFTER face_state_init (which resets the
// mode) and after settings_init (which latches the fresh-install probe).
void update_notes_maybe_show(void);

// Draw the card into `avail`. Returns false when `avail` is too short to render
// legibly — a timeline Quick View can shrink it arbitrarily — in which case the
// caller falls back to drawing the clock and the card stays armed for a later
// frame. The "seen" version is recorded on the first frame that returns true,
// so an obstruction can never silently burn the one showing.
bool update_notes_draw(GContext *ctx, GRect avail);
