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
// Each returns a LARGER font on the large screen classes (LARGE_RECT/emery,
// LARGE_ROUND/gabbro) where there is vertical slack, and falls through to
// ui_font_*() on the small classes, which are tight.
//
// Call sites in face-only files (clock_zone.c, pages/*.c, overlay.c) switch to
// face_font_* only where the size matrix bumps them; everything else keeps
// calling ui_font_* and stays pixel-identical.

GFont face_font_clock(void);    // hero time digits (LECO_42 — the classic look)
GFont face_font_header(void);   // date, page rows, overlay values
GFont face_font_hilo(void);     // resting-face hi/lo temps — like header
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
#define FACE_TIER_BASE     0
#define FACE_TIER_PROMOTED 1

GFont face_font_clock_tier(int tier);  // hero time digits
GFont face_font_temp_tier(int tier);   // weather-row temperature
GFont face_font_hilo_tier(int tier);   // weather-row hi/lo pair
int   face_icon_size_tier(int tier);   // weather-row condition/moon icon edge

// The tier the WEATHER ROW should be sized at, given the tier the clock is
// being sized at. They are the same tier — except under CLOCK_EMPHASIS_LARGE,
// where a promoted clock keeps its weather row at the BASE tier: the whole
// point of that setting is that the weather row gets out of the clock's way.
//
// The three weather accessors above already route through this, so most call
// sites need no change. clock_zone.c's prv_measure must apply it by hand to the
// two weather-row heights it takes from #defines rather than from a font
// (FZ_TEMP_INK_H, FZ_HILO_PITCH) — miss those and the row's reserved height
// never shrinks, which makes the whole setting a no-op.
int face_weather_tier(int tier);

// The clock's VISIBLE ink height and top-side internal leading at a given tier,
// mirroring face_font_clock_tier's branch structure (promoted XL / base, per
// screen class). The flow reserves the ink height for the TIME row —
// NOT the font's layout box, whose top-side leading (15px on LECO_42) is dead
// space that pushed the whole stack down and made the resting face top-heavy.
// The rise is subtracted from the draw box so the visible digits — not the
// layout box — land on the reserved band. The clock is digits + colon only, so
// both are constant per font and need no per-frame measurement.
int face_font_clock_ink_h(int tier);
int face_font_clock_rise(int tier);

// The promoted TIME tier is a bundled custom OFL numeral (the first custom font
// in the repo). It is lazy-loaded into a single static handle the first time
// the flow promotes the clock, and lives for the rest of the process — call
// face_fonts_deinit() from the app's deinit to release it. The glyph subset is
// digits + colon only (characterRegex "[0-9:]"); it carries no ° or minus, so
// it must never be reused for temperatures without widening that subset.
GFont face_font_clock_xl(void);
void  face_fonts_deinit(void);
