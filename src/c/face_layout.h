#pragma once
#include <pebble.h>

// --- Resting-face flow layout ---
//
// The face used to place every row at a hard-coded y per screen class, which
// meant turning a row off left a hole and made the stack top-heavy. Instead the
// rows are measured, stacked with elastic gaps, and centered in whatever space
// is actually available — so the face fills the glass no matter which
// complications, badges or status row the user has enabled, and reflows for
// Timeline Quick View without a second set of anchors.
//
// This file is pure geometry: it knows nothing about weather, fonts or
// settings. clock_zone.c decides which rows exist and how tall they are;
// face_layout solves where they go.

typedef enum {
  FLOW_ROW_TIME = 0,
  FLOW_ROW_DATE,
  FLOW_ROW_COMPS,    // complication line slots
  FLOW_ROW_WEATHER,  // icon | temp | hi-lo cluster
  FLOW_ROW_BADGES,   // complication pills
  FLOW_ROW_UPDATED,  // status pill ("UPDATED 5M AGO" / "RAIN IN 12M")
  FLOW_ROW_COUNT,
} FlowRowId;

typedef struct {
  bool present;  // IN:  is this row drawn at all?
  int  h;        // IN:  measured natural height at the chosen tier
  int  y;        // OUT: absolute top edge
  int  cy;       // OUT: absolute vertical center
  int  x;        // OUT: absolute left edge of the usable width
  int  w;        // OUT: usable width (chord-clamped on the round classes)
} FlowRow;

// Stack the present rows and center them vertically in `avail`, then compute
// each row's usable width. Returns false when even minimum gaps overflow — the
// rows are still placed (top-aligned, minimum gaps) so a caller that can't
// shrink further has a defined, non-clipping-at-the-top result to draw.
//
// The whole rect is the flow's: the face has no out-of-flow chrome left (the
// battery glyph, which used to claim a top band, is now an ordinary
// complication in an ordinary row), so the stack always centers in `avail`.
bool face_layout_solve(FlowRow rows[FLOW_ROW_COUNT], GRect avail);

// Height the stack needs at minimum gaps, including the class's top/bottom
// padding. Use this to test a candidate tier before committing to it.
int face_layout_required_h(const FlowRow rows[FLOW_ROW_COUNT]);

// Usable width of a horizontal band spanning `y`..`y+h`. Full width minus
// margins on the rect classes; the inscribed chord at the band's worst edge on
// the round ones, so text and pills ellipsize instead of running under the
// bezel.
int face_layout_band_w(GRect bounds, int y, int h);

// Smallest full-face stack: time + date + weather plus padding, at the base
// tier. main.c's Quick View cascade uses this to decide whether the
// unobstructed area can host the real face or has to fall back to the compact
// line.
int face_layout_min_core_h(void);
