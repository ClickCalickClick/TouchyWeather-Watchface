#pragma once
#include <pebble.h>

// --- Face-local font ramp (readability pass) ---
//
// ui.c / theme.c are verbatim copies of the companion app's design system
// (CLAUDE.md: keep them byte-identical so `diff -q` stays the sync audit).
// This layer is the SANCTIONED, face-only place to enlarge type for
// readability without touching those shared files — do not "fix" it back
// into ui.c.
//
// Each accessor wraps the shared ui_font_* role:
//   * Normal path: returns a LARGER font on the large screen classes
//     (LARGE_RECT/emery, LARGE_ROUND/gabbro) where there is vertical slack;
//     falls through to ui_font_*() on the small classes, which are tight.
//   * Big Mode: never smaller than the enlarged normal path (the accessibility
//     invariant). Where ui.c's Big Mode font would be smaller than our new
//     normal (the caption role on the large classes), we lift it here.
//
// Call sites in face-only files (clock_zone.c, pages/*.c, overlay.c) switch to
// face_font_* only where the size matrix bumps them; everything else keeps
// calling ui_font_* and stays pixel-identical.

GFont face_font_clock(void);    // hero time digits (LECO_42 — the classic look)
GFont face_font_header(void);   // date, page rows, overlay values
GFont face_font_hilo(void);     // resting-face hi/lo temps — like header, but Big
                                // Mode grows a tier on the small classes too
GFont face_font_body(void);     // large body copy (conditions FEELS)
GFont face_font_title(void);    // card/overlay titles (needs ° / minus)
GFont face_font_hero(void);     // resting-face weather-row temp — LECO_42 hero
                                // (matches the clock + companion app; has °)
GFont face_font_label(void);    // small bold labels / badges / complication
GFont face_font_caption(void);  // muted captions / cell labels

// --- Resting-face tier ramp (flow layout) ---
//
// The resting face lays its rows out by measurement, not fixed anchors, and
// grows the type when the user turns rows off. Two tiers:
//
//   FACE_TIER_BASE      the whole six-row stack fits on every screen class
//   FACE_TIER_PROMOTED  chosen only when the solver proves it still fits
//
// Big Mode never promotes (its own ramp is already the accessibility ceiling),
// so these accessors return the Big-Mode font at both tiers.
#define FACE_TIER_BASE     0
#define FACE_TIER_PROMOTED 1

GFont face_font_clock_tier(int tier);  // hero time digits
GFont face_font_temp_tier(int tier);   // weather-row temperature
GFont face_font_hilo_tier(int tier);   // weather-row hi/lo pair
int   face_icon_size_tier(int tier);   // weather-row condition/moon icon edge
