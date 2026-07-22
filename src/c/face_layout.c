#include "face_layout.h"
#include "ui.h"

// Per-class frame. PAD_TOP/PAD_BOTTOM keep the stack clear of the bezel (much
// larger on the round classes, where the corners of the glass are unusable);
// GAP_MIN/GAP_MAX bound the elastic spacing between rows. INSET is the
// horizontal breathing room held back from every row's usable width.
#if defined(UI_SCREEN_LARGE_ROUND)
  #define FL_PAD_TOP     24
  #define FL_PAD_BOTTOM  24
  #define FL_GAP_MIN      4
  #define FL_GAP_MAX     18
  #define FL_INSET        8
#elif defined(UI_SCREEN_LARGE_RECT)
  #define FL_PAD_TOP     10
  #define FL_PAD_BOTTOM  10
  #define FL_GAP_MIN      4
  #define FL_GAP_MAX     18
  #define FL_INSET        6
#elif defined(UI_SCREEN_SMALL_ROUND)
  #define FL_PAD_TOP      8
  #define FL_PAD_BOTTOM   8
  #define FL_GAP_MIN      2
  #define FL_GAP_MAX     12
  #define FL_INSET        4
#else  // UI_SCREEN_SMALL_RECT
  #define FL_PAD_TOP      4
  #define FL_PAD_BOTTOM   4
  #define FL_GAP_MIN      2
  #define FL_GAP_MAX     12
  #define FL_INSET        4
#endif

// Base-tier heights of the three rows that are always present. Kept here (not
// measured) so main.c can ask for the minimum stack height before a draw
// context exists. Generous by a pixel or two rather than tight — an
// underestimate would let the Quick View cascade pick the full face for a band
// that can't hold it. The _BIG variants are the same rows at the Big-Mode ramp
// (BITHAM_42 clock, GOTHIC_24_BOLD hi/lo pair driving the weather row); the
// caller passes the mode in so this file stays settings-free.
#if defined(UI_SCREEN_LARGE_ROUND)
  #define FL_CORE_TIME_H   50
  #define FL_CORE_DATE_H   26
  #define FL_CORE_WEATHER_H 52
  #define FL_CORE_TIME_H_BIG    48
  #define FL_CORE_WEATHER_H_BIG 54
#elif defined(UI_SCREEN_LARGE_RECT)
  #define FL_CORE_TIME_H   50
  #define FL_CORE_DATE_H   26
  #define FL_CORE_WEATHER_H 50
  #define FL_CORE_TIME_H_BIG    48
  #define FL_CORE_WEATHER_H_BIG 54
#elif defined(UI_SCREEN_SMALL_ROUND)
  #define FL_CORE_TIME_H   42
  #define FL_CORE_DATE_H   22
  #define FL_CORE_WEATHER_H 36
  #define FL_CORE_TIME_H_BIG    46
  #define FL_CORE_WEATHER_H_BIG 50
#else  // UI_SCREEN_SMALL_RECT
  #define FL_CORE_TIME_H   36
  #define FL_CORE_DATE_H   22
  #define FL_CORE_WEATHER_H 36
  #define FL_CORE_TIME_H_BIG    46
  #define FL_CORE_WEATHER_H_BIG 50
#endif

// Floor for a row's usable width — see face_layout_band_w.
#define FL_MIN_ROW_W 48

static int prv_present_count(const FlowRow rows[FLOW_ROW_COUNT]) {
  int n = 0;
  for (int i = 0; i < FLOW_ROW_COUNT; i++) {
    if (rows[i].present) n++;
  }
  return n;
}

static int prv_content_h(const FlowRow rows[FLOW_ROW_COUNT]) {
  int h = 0;
  for (int i = 0; i < FLOW_ROW_COUNT; i++) {
    if (rows[i].present) h += rows[i].h;
  }
  return h;
}

int face_layout_required_h(const FlowRow rows[FLOW_ROW_COUNT]) {
  int n = prv_present_count(rows);
  if (n == 0) return FL_PAD_TOP + FL_PAD_BOTTOM;
  return FL_PAD_TOP + FL_PAD_BOTTOM + prv_content_h(rows) + (n - 1) * FL_GAP_MIN;
}

int face_layout_min_core_h(bool big_mode) {
  const int time_h = big_mode ? FL_CORE_TIME_H_BIG : FL_CORE_TIME_H;
  const int weather_h = big_mode ? FL_CORE_WEATHER_H_BIG : FL_CORE_WEATHER_H;
  return FL_PAD_TOP + FL_PAD_BOTTOM + time_h + FL_CORE_DATE_H + weather_h +
         2 * FL_GAP_MIN;
}

#if defined(PBL_ROUND)
// Integer square root (Newton). Used for the round-screen chord; a float sqrt
// would pull in the soft-FP library for six calls a redraw.
static int prv_isqrt(int v) {
  if (v <= 0) return 0;
  int x = v, y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + v / x) / 2;
  }
  return x;
}
#endif

int face_layout_band_w(GRect bounds, int y, int h) {
#if defined(PBL_ROUND)
  const int r = bounds.size.w / 2;
  const int screen_cy = bounds.origin.y + bounds.size.h / 2;
  // Chord at the row's vertical CENTER, not at its farther edge. Every row here
  // is centered text or a centered pill, so its ink is thickest across the
  // middle and tapers at the corners — measuring at the far edge was correct
  // for a rectangle but far too pessimistic for this content, and it clipped
  // the clock into an ellipsis on chalk.
  int dy = (y + h / 2) - screen_cy;
  if (dy < 0) dy = -dy;
  if (dy >= r) return FL_MIN_ROW_W;
  int half = prv_isqrt(r * r - dy * dy);
  int w = 2 * half - 2 * FL_INSET;
  // Never hand back a width a drawer can't use. A row pushed to the very edge
  // of the glass (an overflowing stack, or a tiny Quick View band) would
  // otherwise get a zero or negative width, and the pill drawers derive their
  // inner text rect by subtracting padding from it — which goes negative and
  // takes the app down. Overshooting the arc slightly is the better failure.
  return (w > FL_MIN_ROW_W) ? w : FL_MIN_ROW_W;
#else
  (void)y;
  (void)h;
  return bounds.size.w - 2 * FL_INSET;
#endif
}

bool face_layout_solve(FlowRow rows[FLOW_ROW_COUNT], GRect avail) {
  const int n = prv_present_count(rows);
  const int content = prv_content_h(rows);
  const int band_h = avail.size.h - FL_PAD_TOP - FL_PAD_BOTTOM;
  const int gaps = (n > 1) ? (n - 1) : 0;

  int gap = FL_GAP_MIN;
  bool fits = (content + gaps * FL_GAP_MIN) <= band_h;
  if (fits && gaps > 0) {
    // Spread the slack into the gaps, up to the class maximum; whatever is left
    // over after that becomes the top/bottom margin, which is what centers the
    // stack rather than letting it drift to the top.
    int slack = band_h - content - gaps * FL_GAP_MIN;
    gap += slack / gaps;
    if (gap > FL_GAP_MAX) gap = FL_GAP_MAX;
  }

  const int stack_h = content + gaps * gap;
  int y = avail.origin.y + FL_PAD_TOP;
  if (fits) y += (band_h - stack_h) / 2;

  for (int i = 0; i < FLOW_ROW_COUNT; i++) {
    if (!rows[i].present) {
      // Leave a defined position behind so a caller that reads a hidden row's
      // geometry gets something sane instead of stale values.
      rows[i].y = y;
      rows[i].cy = y;
      rows[i].x = avail.origin.x;
      rows[i].w = 0;
      continue;
    }
    rows[i].y = y;
    rows[i].cy = y + rows[i].h / 2;
    rows[i].w = face_layout_band_w(avail, y, rows[i].h);
    rows[i].x = avail.origin.x + (avail.size.w - rows[i].w) / 2;
    y += rows[i].h + gap;
  }
  return fits;
}
