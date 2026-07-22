#include "clock_zone.h"
#include "theme.h"
#include "ui.h"
#include "face_fonts.h"
#include "face_layout.h"
#include "icons.h"
#include "weather_data.h"
#include "anim.h"
#include "settings.h"
#include "comm.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Weather-row / chrome geometry (readability pass). The large screen classes
// (emery/gabbro) grow their icons, hi/lo column and battery glyph to match the
// enlarged fonts; the small classes keep the compact originals verbatim.
// FZ_HILO_ARROW_COL = the arrow+gap lead before the hi/lo text (prv_draw_hilo
// draws the arrow at x+5 and the text at x+12), so the measured cluster width
// is FZ_HILO_ARROW_COL + max(hi,lo text width).
// FZ_TEXT_RISE: px to lift a vertically-centered text box so the visible glyph
// (not the layout box, which carries the font's top-side internal leading)
// lands on the target midline. Used to center the temp and hi/lo on the weather
// row's midline. Larger fonts on the large classes carry more top padding.
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  #define FZ_HILO_ARROW_COL 12
  #define FZ_ARROW_SIZE   12
  #define FZ_HILO_BOX_H   26
  #define FZ_BATTERY_SZ   24
  #define FZ_TEMP_BOX_H   38
  #define FZ_DATE_BOX_H   26
  #define FZ_COMP_BOX_H   22
  #define FZ_TEXT_RISE     4
#else
  #define FZ_HILO_ARROW_COL 12
  #define FZ_ARROW_SIZE   10
  #define FZ_HILO_BOX_H   22
  #define FZ_BATTERY_SZ   20
  #define FZ_TEMP_BOX_H   36
  #define FZ_DATE_BOX_H   22
  #define FZ_COMP_BOX_H   18
  #define FZ_TEXT_RISE     3
#endif

// Hi/lo row-to-row pitch, and the height the weather row reserves for the temp.
// Both are stated outright rather than taken from the font's reported line box:
// the "°" glyph drags that box several pixels taller than the ink it actually
// puts on screen, which made the weather row reserve space it did not need and
// squeezed the rows below it. FZ_TEXT_RISE already handles the matching
// horizontal-centering offset for the same reason.
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  #define FZ_HILO_PITCH  26
  #define FZ_TEMP_INK_H  40
#elif defined(UI_SCREEN_SMALL_ROUND)
  #define FZ_HILO_PITCH  18
  #define FZ_TEMP_INK_H  30
#else  // UI_SCREEN_SMALL_RECT
  #define FZ_HILO_PITCH  20
  #define FZ_TEMP_INK_H  32
#endif

// Pill geometry (badge row + status row). Chalk trims its padding and gap so
// two full-width pills ("RAIN 100%" twice) still fit the ~149px chord at the
// badge row's height; every other class keeps the roomier original spacing.
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  #define FZ_PILL_H      22
  #define FZ_PILL_PAD     8
  #define FZ_PILL_GAP     8
#elif defined(UI_SCREEN_SMALL_ROUND)
  #define FZ_PILL_H      18
  #define FZ_PILL_PAD     4
  #define FZ_PILL_GAP     6
#else  // UI_SCREEN_SMALL_RECT
  #define FZ_PILL_H      16
  #define FZ_PILL_PAD     7
  #define FZ_PILL_GAP     6
#endif

static char s_time_buf[8];   // "22:25" / "9:41"
static char s_date_buf[16];  // "MON JUL 6"
static int s_now_min = 0;    // minutes since local midnight (badge/night calc)
static bool s_is_night = false;

// Parse a PKJS-formatted sun time — "6:14 AM", "12:05 PM" or 24h "05:18" —
// into minutes since midnight. Returns -1 when unparseable.
static int prv_parse_minutes(const char *s) {
  if (!s || !s[0]) return -1;
  int h = 0, m = 0, i = 0;
  while (s[i] >= '0' && s[i] <= '9') { h = h * 10 + (s[i] - '0'); i++; }
  if (s[i] != ':') return -1;
  i++;
  while (s[i] >= '0' && s[i] <= '9') { m = m * 10 + (s[i] - '0'); i++; }
  if (s[i] == ' ') {
    if (s[i + 1] == 'P' && h != 12) h += 12;
    if (s[i + 1] == 'A' && h == 12) h = 0;
  }
  if (h > 23 || m > 59) return -1;
  return h * 60 + m;
}

void clock_zone_recompute_night(void) {
  WeatherData *d = weather_data_get();
  int rise = prv_parse_minutes(d->sunrise);
  int set = prv_parse_minutes(d->sunset);
  if (rise < 0 || set < 0) {
    s_is_night = false;  // no usable sun data: fail to the day look
    return;
  }
  s_is_night = (s_now_min < rise || s_now_min >= set);
}

bool clock_zone_is_night(void) { return s_is_night; }

void clock_zone_update_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  s_now_min = t->tm_hour * 60 + t->tm_min;

  strftime(s_time_buf, sizeof(s_time_buf),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
  // 12h: drop the leading zero ("09:41" -> "9:41").
  if (!clock_is_24h_style() && s_time_buf[0] == '0') {
    memmove(s_time_buf, s_time_buf + 1, strlen(s_time_buf));
  }

  char dow[4], mon[4];
  strftime(dow, sizeof(dow), "%a", t);
  strftime(mon, sizeof(mon), "%b", t);
  for (char *p = dow; *p; p++) *p = toupper((unsigned char)*p);
  for (char *p = mon; *p; p++) *p = toupper((unsigned char)*p);
  snprintf(s_date_buf, sizeof(s_date_buf), "%s %s %d", dow, mon, t->tm_mday);
}

// Hi/lo mini column: ↑high (orange) over ↓low (blue) — the main card's
// right-column idiom at label scale. `x` is the left edge of the arrows; `cy`
// is the vertical CENTER of the two-row stack. The two rows are placed
// symmetrically above/below cy (high in the upper row, low in the lower) and
// each glyph is centered on its own row using the font's measured line height,
// so the pair straddles the same midline as the icon/temp instead of hanging
// below it. FZ_HILO_ROW_RISE trims the font's top-side internal padding so the
// visible glyph — not the layout box — lands on the row center.
static void prv_draw_hilo(GContext *ctx, int x, int cy, int w, GFont f,
                          int line_h) {
  WeatherData *d = weather_data_get();
  char hi_buf[8], lo_buf[8];
  snprintf(hi_buf, sizeof(hi_buf), "%d°", d->high);
  snprintf(lo_buf, sizeof(lo_buf), "%d°", d->low);
  // The font and line height come from the layout pass, so the space reserved
  // for this pair and the space it actually paints can never drift apart.
  GSize ts = GSize(w - 12, line_h);
  const int pitch = ts.h;                 // row-to-row spacing = one line height
  const int hi_cy = cy - pitch / 2;       // high row center
  const int lo_cy = cy + pitch / 2;       // low row center
  icon_draw_arrow_up(ctx, GPoint(x + 5, hi_cy), FZ_ARROW_SIZE,
                     theme_accent_orange());
  graphics_context_set_text_color(ctx, theme_accent_orange());
  graphics_draw_text(ctx, hi_buf, f,
                     GRect(x + 12, hi_cy - ts.h / 2 - FZ_TEXT_RISE, w - 12, ts.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  icon_draw_arrow_down(ctx, GPoint(x + 5, lo_cy), FZ_ARROW_SIZE,
                       theme_accent_blue());
  graphics_context_set_text_color(ctx, theme_accent_blue());
  graphics_draw_text(ctx, lo_buf, f,
                     GRect(x + 12, lo_cy - ts.h / 2 - FZ_TEXT_RISE, w - 12, ts.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

// Battery pill in the top area, per the BatteryDisplay setting. Rect classes
// get a top-right corner; round classes a top-center glyph clear of the
// bezel curve. `bounds` is the full face rect.
static void prv_draw_battery(GContext *ctx, GRect bounds) {
  BatteryDisplay mode = settings_get_battery_display();
  if (mode == BATTERY_OFF) return;
  BatteryChargeState bat = battery_state_service_peek();
  if (mode == BATTERY_WHEN_LOW && bat.charge_percent > 20 && !bat.is_charging) {
    return;
  }
  int ox = bounds.origin.x, oy = bounds.origin.y, W = bounds.size.w;
  const int bw = FZ_BATTERY_SZ;
  GPoint c;
#if defined(UI_SCREEN_SMALL_RECT)
  c = GPoint(ox + W - UI_MARGIN_X - bw / 2 - 2, oy + 10);
#elif defined(UI_SCREEN_SMALL_ROUND)
  c = GPoint(ox + W / 2, oy + 13);
#elif defined(UI_SCREEN_LARGE_RECT)
  c = GPoint(ox + W / 2, oy + 8);
#else  // UI_SCREEN_LARGE_ROUND
  c = GPoint(ox + W / 2, oy + 22);
#endif
  icon_draw_battery(ctx, c, bw, bat.charge_percent, bat.is_charging,
                    theme_secondary(), theme_accent_orange());
}

// Peak precipitation probability over the next ~6 hours — the number behind
// both the "Rain chance" complication and the old automatic rain badge.
static int prv_peak_pop(void) {
  WeatherData *d = weather_data_get();
  int pop = 0;
  for (int i = 0; i < 6; i++) {
    if (d->hours_pop[i] > pop) pop = d->hours_pop[i];
  }
  return pop;
}

// Format one complication into `buf`, setting its accent `*color`. `compact`
// trims the label words so two readings fit side-by-side on the narrow classes.
// Returns false when the slot is Off or unavailable (e.g. steps sans health).
static bool prv_format_complication(ComplicationSlot which, bool compact,
                                    char *buf, size_t n, GColor *color) {
  WeatherData *d = weather_data_get();
  *color = theme_secondary();
  switch (which) {
    case COMPLICATION_FEELS:
      snprintf(buf, n, "FEELS %d°", d->feels_like);
      return true;
    case COMPLICATION_WIND:
      snprintf(buf, n, compact ? "%d %s" : "WIND %d %s",
               d->wind_speed, d->wind_dir);
      return true;
    case COMPLICATION_HUMIDITY:
      snprintf(buf, n, compact ? "HUM %d%%" : "HUMIDITY %d%%", d->humidity);
      return true;
    case COMPLICATION_DEW:
      snprintf(buf, n, compact ? "DEW %d°" : "DEW POINT %d°", d->dew_point);
      return true;
    case COMPLICATION_UV:
      *color = theme_accent_orange();
      snprintf(buf, n, compact ? "UV %d" : "UV %d %s", d->uv, uv_label(d->uv));
      return true;
    case COMPLICATION_AQI:
      *color = theme_accent_blue();
      snprintf(buf, n, compact ? "AIR %d" : "AIR %d %s", d->aqi, aqi_label(d->aqi));
      return true;
    case COMPLICATION_RAIN_CHANCE:
      *color = theme_accent_blue();
      snprintf(buf, n, compact ? "RAIN %d%%" : "RAIN %d%%", prv_peak_pop());
      return true;
    case COMPLICATION_STEPS: {
#if defined(PBL_HEALTH)
      int steps = (int)health_service_sum_today(HealthMetricStepCount);
      snprintf(buf, n, compact ? "%d" : "%d STEPS", steps);
      return true;
#else
      return false;  // no health service on this platform
#endif
    }
    default:
      return false;
  }
}

// Pill text for a badge slot, plus the fill color. The vocabulary is short and
// IDENTICAL on every screen class — two pills have to sit side-by-side inside
// chalk's 149px chord, and a reading that changes wording per watch reads as a
// different feature. Colors follow the metric, matching the line variants
// above; on B&W the theme accessors collapse them to fg (an inverted pill).
static bool prv_format_badge(ComplicationSlot which, char *buf, size_t n,
                             GColor *fill) {
  WeatherData *d = weather_data_get();
  *fill = theme_fg();
  switch (which) {
    case COMPLICATION_RAIN_CHANCE:
      *fill = theme_accent_blue();
      snprintf(buf, n, "RAIN %d%%", prv_peak_pop());
      return true;
    case COMPLICATION_FEELS:
      *fill = theme_accent_orange();
      snprintf(buf, n, "FEELS %d°", d->feels_like);
      return true;
    case COMPLICATION_WIND:
      snprintf(buf, n, "WIND %d %s", d->wind_speed, d->wind_dir);
      return true;
    case COMPLICATION_HUMIDITY:
      *fill = theme_accent_blue();
      snprintf(buf, n, "HUM %d%%", d->humidity);
      return true;
    case COMPLICATION_DEW:
      *fill = theme_accent_blue();
      snprintf(buf, n, "DEW %d°", d->dew_point);
      return true;
    case COMPLICATION_UV:
      *fill = theme_accent_orange();
      snprintf(buf, n, "UV %d", d->uv);
      return true;
    case COMPLICATION_AQI:
      *fill = theme_accent_blue();
      snprintf(buf, n, "AIR %d", d->aqi);
      return true;
    default:
      return false;  // Off, or STEPS (line-only — see ComplicationSlot)
  }
}

// "Only when notable" thresholds — the opt-in filter that reproduces the v1.1
// automatic badges (rain >= 50%, UV >= 6 during the midday window) and extends
// the same idea to the other readings. Metric thresholds differ by unit, so
// each case reads d->units. Steps has no meaningful threshold; it can't reach
// a badge slot anyway.
static bool prv_badge_is_notable(ComplicationSlot which) {
  WeatherData *d = weather_data_get();
  bool metric = (d->units == UNITS_METRIC);
  switch (which) {
    case COMPLICATION_RAIN_CHANCE: return prv_peak_pop() >= 50;
    case COMPLICATION_UV:
      return d->uv >= 6 && s_now_min >= 10 * 60 && s_now_min < 16 * 60;
    case COMPLICATION_AQI:      return d->aqi >= 101;   // "unhealthy for some"
    case COMPLICATION_HUMIDITY: return d->humidity >= 80;
    case COMPLICATION_DEW:      return d->dew_point >= (metric ? 18 : 65);
    case COMPLICATION_WIND:     return d->wind_speed >= (metric ? 30 : 20);
    case COMPLICATION_FEELS: {
      int delta = d->feels_like - d->temp;
      if (delta < 0) delta = -delta;
      return delta >= (metric ? 5 : 10);
    }
    default: return false;
  }
}

// The two line slots, resolved to text once per draw. Resolving up front is
// what lets the flow drop the whole row when neither slot produces anything
// (Off, or steps on a watch without a health service) instead of reserving an
// empty band.
typedef struct {
  bool   show1, show2;
  char   buf1[24], buf2[24];
  GColor col1, col2;
} CompLine;

static void prv_build_comp_line(CompLine *out) {
  ComplicationSlot c1 = settings_get_complication();
  ComplicationSlot c2 = settings_get_complication2();
  out->show1 = (c1 != COMPLICATION_OFF) &&
               prv_format_complication(c1, false, out->buf1, sizeof(out->buf1),
                                       &out->col1);
  out->show2 = (c2 != COMPLICATION_OFF) &&
               prv_format_complication(c2, false, out->buf2, sizeof(out->buf2),
                                       &out->col2);
  // Both readings on one line: re-format with the trimmed labels so the pair
  // fits side by side.
  if (out->show1 && out->show2) {
    prv_format_complication(c1, true, out->buf1, sizeof(out->buf1), &out->col1);
    prv_format_complication(c2, true, out->buf2, sizeof(out->buf2), &out->col2);
  }
}

static void prv_draw_comp_text(GContext *ctx, GRect slot, const char *txt,
                               GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, txt, face_font_label(), slot,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

// The complication line between the date and the weather row. One reading
// centers across `slot`; two split it left/right.
static void prv_draw_comp_line(GContext *ctx, GRect slot, const CompLine *c) {
  if (c->show1 && c->show2) {
    int half = slot.size.w / 2;
    prv_draw_comp_text(ctx, GRect(slot.origin.x, slot.origin.y, half,
                                  slot.size.h), c->buf1, c->col1);
    prv_draw_comp_text(ctx, GRect(slot.origin.x + half, slot.origin.y,
                                  slot.size.w - half, slot.size.h),
                       c->buf2, c->col2);
  } else if (c->show1) {
    prv_draw_comp_text(ctx, slot, c->buf1, c->col1);
  } else if (c->show2) {
    prv_draw_comp_text(ctx, slot, c->buf2, c->col2);
  }
}

// A small pill badge (rain chance / UV). Filled rounded rect, bg-colored text.
typedef struct {
  bool show;
  char buf[16];  // "FEELS -100°" — note ° is two bytes in UTF-8
  GColor fill;
} Badge;

// Resolve one badge slot into a drawable pill. Hidden when: the slot is Off,
// "only when notable" is on and the reading isn't, the data is stale (a stale
// reading must never be dressed up as a live "RAIN 70%"), or an imminent-rain
// alert is up and this is the rain badge (the alert already says it, louder).
static void prv_build_badge(ComplicationSlot slot, bool notable_only,
                            Badge *out) {
  WeatherData *d = weather_data_get();
  out->show = false;
  if (slot == COMPLICATION_OFF) return;
  if (comm_data_is_stale()) return;
  if (slot == COMPLICATION_RAIN_CHANCE && d->rain_alert_min >= 0) return;
  if (notable_only && !prv_badge_is_notable(slot)) return;
  out->show = prv_format_badge(slot, out->buf, sizeof(out->buf), &out->fill);
}

// Build both badge slots from current data + settings.
static void prv_build_badges(Badge *b1, Badge *b2) {
  prv_build_badge(settings_get_badge1(), settings_get_badge1_notable(), b1);
  prv_build_badge(settings_get_badge2(), settings_get_badge2_notable(), b2);
}

static int prv_badge_w(const char *txt) {
  GSize s = graphics_text_layout_get_content_size(
      txt, face_font_label(), GRect(0, 0, 140, 24),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
  return s.w + 2 * FZ_PILL_PAD;  // symmetric horizontal padding inside the pill
}

// Total width of the badge row as drawn (0 when nothing shows) — the flow
// layout measures with this before deciding whether the row fits its chord.
static int prv_badge_row_w(const Badge *b1, const Badge *b2) {
  int w1 = b1->show ? prv_badge_w(b1->buf) : 0;
  int w2 = b2->show ? prv_badge_w(b2->buf) : 0;
  return w1 + w2 + ((b1->show && b2->show) ? FZ_PILL_GAP : 0);
}

// --- Status row ("UPDATED 5M AGO" / "RAIN IN 12M") ------------------------
//
// The resting face draws its own status pill as a flow row rather than calling
// ui.c's bottom-anchored ui_draw_status_banner: the flow decides where the row
// goes, and the pill has to size to its text so it can't overrun chalk's chord.
// PEEK/OVERLAY still use ui.c's version. Keep the two visually in sync — the
// text formatting below mirrors ui.c's static prv_format_ago.

static bool s_status_alt = false;  // during a rain alert: false = RAIN, true = UPDATED

void clock_zone_toggle_status_alt(void) { s_status_alt = !s_status_alt; }
void clock_zone_reset_status_alt(void) { s_status_alt = false; }

static void prv_format_ago(uint32_t when, char *out, size_t n) {
  if (!when) { snprintf(out, n, "UPDATED --"); return; }
  uint32_t now = (uint32_t)time(NULL);
  if (now < when) { snprintf(out, n, "UPDATED NOW"); return; }
  uint32_t delta = now - when;
  if (delta < 60)                snprintf(out, n, "UPDATED NOW");
  else if (delta < 60 * 60)      snprintf(out, n, "UPDATED %luM AGO", (unsigned long)(delta / 60));
  else if (delta < 24 * 60 * 60) snprintf(out, n, "UPDATED %luH AGO", (unsigned long)(delta / 3600));
  else                           snprintf(out, n, "UPDATED %luD AGO", (unsigned long)(delta / 86400));
}

// Whether the status row is drawn right now, and in which mode. An imminent
// -rain alert always claims the row — including under UPDATED_NEVER, because a
// user who hid a housekeeping stamp did not ask to hide a weather warning.
static bool prv_status_row_visible(bool *out_rain) {
  WeatherData *d = weather_data_get();
  bool rain = (d->rain_alert_min >= 0);
  if (out_rain) *out_rain = rain && !s_status_alt;
  if (rain) return true;
  switch (settings_get_updated_display()) {
    case UPDATED_ALWAYS:        return true;
    case UPDATED_STALE_OR_RAIN: return comm_data_is_stale();
    default:                    return false;
  }
}

static void prv_draw_status_row(GContext *ctx, int ox, int W, int cy,
                                bool rain_mode) {
  WeatherData *d = weather_data_get();
  char buf[28];
  if (rain_mode) {
    int m = d->rain_alert_min;
    if (m >= 60) snprintf(buf, sizeof(buf), "RAIN IN %dH", m / 60);
    else         snprintf(buf, sizeof(buf), "RAIN IN %dM", m);
  } else {
    prv_format_ago(d->last_updated, buf, sizeof(buf));
  }

  // Colors follow ui.c's banner exactly: B&W gets a solid inverted pill, color
  // gets orange for the alert and muted for the stamp, with the Big-Mode
  // inversion guard on the alert text.
  GColor fill, txt;
#if defined(PBL_BW)
  fill = theme_fg();
  txt = theme_bg();
#else
  if (rain_mode) {
    fill = theme_accent_orange();
    txt = settings_get_big_mode() ? theme_bg() : GColorBlack;
  } else {
    fill = theme_muted();
    txt = theme_fg();
  }
#endif

  // Clamp to the row's usable width, but never below the padding the text rect
  // subtracts — a narrower pill than that yields a negative-width text rect.
  int w = prv_badge_w(buf);
  if (w > W) w = W;
  if (w < 2 * FZ_PILL_PAD + 8) w = 2 * FZ_PILL_PAD + 8;
  GRect r = GRect(ox + (W - w) / 2, cy - FZ_PILL_H / 2, w, FZ_PILL_H);
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, r, FZ_PILL_H / 2, GCornersAll);
  graphics_context_set_text_color(ctx, txt);
  graphics_draw_text(ctx, buf, face_font_label(),
                     GRect(r.origin.x + 4, r.origin.y - 2, r.size.w - 8, r.size.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

static void prv_draw_pill(GContext *ctx, GRect r, const char *txt, GColor fill) {
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, r, r.size.h / 2, GCornersAll);
  graphics_context_set_text_color(ctx, theme_bg());
  graphics_draw_text(ctx, txt, face_font_label(),
                     GRect(r.origin.x, r.origin.y - 2, r.size.w, r.size.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

// Draw the active badges as one centered row at `cy`. Both pills sit
// side-by-side when both slots resolve; a single pill centers on its own.
static void prv_draw_badge_row(GContext *ctx, int ox, int W, int cy,
                               const Badge *b1, const Badge *b2) {
  int total = prv_badge_row_w(b1, b2);
  if (total == 0) return;
  int x = ox + (W - total) / 2;
  int y = cy - FZ_PILL_H / 2;
  if (b1->show) {
    int w = prv_badge_w(b1->buf);
    prv_draw_pill(ctx, GRect(x, y, w, FZ_PILL_H), b1->buf, b1->fill);
    x += w + FZ_PILL_GAP;
  }
  if (b2->show) {
    prv_draw_pill(ctx, GRect(x, y, prv_badge_w(b2->buf), FZ_PILL_H),
                  b2->buf, b2->fill);
  }
}

// Everything the weather row needs, measured once per tier so the flow can try
// a tier on for size before committing to drawing it.
#define FZ_CLUSTER_GAP 10
typedef struct {
  GFont time_font;
  int   time_h;
  GFont temp_font;
  GSize temp_sz;
  int   icon_size;
  GFont hilo_font;
  int   hilo_w;
  int   hilo_line_h;  // one hi/lo row's line height; the pair is twice this
  int   cluster_w;
  int   weather_h;
} FaceMetrics;

static void prv_measure(FaceMetrics *m, int tier, int W) {
  WeatherData *d = weather_data_get();

  m->time_font = face_font_clock_tier(tier);
  GSize ts = graphics_text_layout_get_content_size(
      s_time_buf, m->time_font, GRect(0, 0, W, 90),
      GTextOverflowModeFill, GTextAlignmentCenter);
  m->time_h = ts.h;

  char temp_buf[8];
  snprintf(temp_buf, sizeof(temp_buf), "%d°", d->temp);
  m->temp_font = face_font_temp_tier(tier);
  m->temp_sz = graphics_text_layout_get_content_size(
      temp_buf, m->temp_font, GRect(0, 0, W, 60),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

  char hi_b[8], lo_b[8];
  snprintf(hi_b, sizeof(hi_b), "%d°", d->high);
  snprintf(lo_b, sizeof(lo_b), "%d°", d->low);
  m->hilo_font = face_font_hilo_tier(tier);
  GFont hl = m->hilo_font;
  GSize hi_sz = graphics_text_layout_get_content_size(hi_b, hl,
      GRect(0, 0, W, FZ_HILO_BOX_H), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  GSize lo_sz = graphics_text_layout_get_content_size(lo_b, hl,
      GRect(0, 0, W, FZ_HILO_BOX_H), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  int hilo_text_w = hi_sz.w > lo_sz.w ? hi_sz.w : lo_sz.w;
  m->hilo_w = FZ_HILO_ARROW_COL + hilo_text_w;  // arrow+gap lead, then text
  // Widths come from measurement (they must fit real digits); heights come from
  // the ink constants above.
  m->hilo_line_h = (tier >= FACE_TIER_PROMOTED) ? FZ_HILO_PITCH
                                                : FZ_HILO_PITCH - 2;

  m->icon_size = face_icon_size_tier(tier);
  m->cluster_w = m->icon_size + FZ_CLUSTER_GAP + m->temp_sz.w +
                 FZ_CLUSTER_GAP + m->hilo_w;

  // The row is as tall as its tallest member: the icon, the temp, or the
  // stacked hi/lo pair (two line heights).
  int hilo_h = 2 * m->hilo_line_h;
  int temp_h = (tier >= FACE_TIER_PROMOTED) ? FZ_TEMP_INK_H + 6 : FZ_TEMP_INK_H;
  m->weather_h = m->icon_size;
  if (temp_h > m->weather_h) m->weather_h = temp_h;
  if (hilo_h > m->weather_h) m->weather_h = hilo_h;
}

static void prv_fill_rows(FlowRow rows[FLOW_ROW_COUNT], const FaceMetrics *m,
                          bool has_comps, bool has_badges, bool has_status) {
  rows[FLOW_ROW_TIME].present = true;
  rows[FLOW_ROW_TIME].h = m->time_h;
  rows[FLOW_ROW_DATE].present = true;
  rows[FLOW_ROW_DATE].h = FZ_DATE_BOX_H;
  rows[FLOW_ROW_COMPS].present = has_comps;
  rows[FLOW_ROW_COMPS].h = FZ_COMP_BOX_H;
  rows[FLOW_ROW_WEATHER].present = true;
  rows[FLOW_ROW_WEATHER].h = m->weather_h;
  rows[FLOW_ROW_BADGES].present = has_badges;
  rows[FLOW_ROW_BADGES].h = FZ_PILL_H;
  rows[FLOW_ROW_UPDATED].present = has_status;
  rows[FLOW_ROW_UPDATED].h = FZ_PILL_H;
}

// Extra slack the promoted tier must leave behind before we take it. Without
// it, promotion could win by a single pixel and leave the stack visually
// crammed against the bezel.
#define FZ_PROMOTE_HEADROOM 6

void clock_zone_draw_full(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  const int W = bounds.size.w;
  const int ox = bounds.origin.x;

  // Battery glyph is screen chrome, not a flow row: it stays pinned to the top
  // of the physical face and is measured out of the flow entirely.
  prv_draw_battery(ctx, bounds);

  // --- Resolve content, which is what decides row presence ---
  CompLine comps;
  prv_build_comp_line(&comps);
  Badge b1, b2;
  prv_build_badges(&b1, &b2);
  bool rain_mode = false;
  const bool has_status = prv_status_row_visible(&rain_mode);
  const bool has_comps = comps.show1 || comps.show2;
  const bool has_badges = b1.show || b2.show;

  // --- Choose a tier, then solve ---
  // These are static rather than locals: the draw path is single-threaded and
  // runs one frame at a time, and holding two of each on the stack (a candidate
  // plus a probe) was enough to blow the app stack on the smaller-RAM watches.
  static FaceMetrics m;
  static FlowRow rows[FLOW_ROW_COUNT];

  // Try the taller tier first and keep it only if it still leaves comfortable
  // margins and its weather cluster fits the width at that height; otherwise
  // fall back and re-measure. Big Mode never promotes — its ramp is already the
  // accessibility ceiling, and growing further would overflow the small classes.
  int tier = FACE_TIER_BASE;
  if (!settings_get_big_mode()) {
    prv_measure(&m, FACE_TIER_PROMOTED, W);
    prv_fill_rows(rows, &m, has_comps, has_badges, has_status);
    if (face_layout_required_h(rows) + FZ_PROMOTE_HEADROOM <= bounds.size.h &&
        m.cluster_w <= face_layout_band_w(bounds, bounds.origin.y +
                                          bounds.size.h / 2, m.weather_h)) {
      tier = FACE_TIER_PROMOTED;
    }
  }
  if (tier == FACE_TIER_BASE) {
    prv_measure(&m, FACE_TIER_BASE, W);
    prv_fill_rows(rows, &m, has_comps, has_badges, has_status);
  }

  // Still too tall (Big Mode on a small class, or a Quick View band)? Shed the
  // optional rows from the bottom up — the status stamp first, then the badge
  // pills, then the complication line — rather than clipping the clock.
  if (face_layout_required_h(rows) > bounds.size.h) {
    rows[FLOW_ROW_UPDATED].present = false;
    if (face_layout_required_h(rows) > bounds.size.h) {
      rows[FLOW_ROW_BADGES].present = false;
    }
    if (face_layout_required_h(rows) > bounds.size.h) {
      rows[FLOW_ROW_COMPS].present = false;
    }
  }

  face_layout_solve(rows, bounds);

  // --- Draw ---
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, s_time_buf, m.time_font,
                     GRect(rows[FLOW_ROW_TIME].x, rows[FLOW_ROW_TIME].y,
                           rows[FLOW_ROW_TIME].w, rows[FLOW_ROW_TIME].h + 4),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, s_date_buf, face_font_header(),
                     GRect(rows[FLOW_ROW_DATE].x, rows[FLOW_ROW_DATE].y,
                           rows[FLOW_ROW_DATE].w, rows[FLOW_ROW_DATE].h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);

  if (rows[FLOW_ROW_COMPS].present) {
    prv_draw_comp_line(ctx, GRect(rows[FLOW_ROW_COMPS].x,
                                  rows[FLOW_ROW_COMPS].y,
                                  rows[FLOW_ROW_COMPS].w,
                                  rows[FLOW_ROW_COMPS].h),
                       &comps);
  }

  // Weather row: animated condition icon | big temp | hi-lo column, laid out as
  // one centered cluster. The temp and hi/lo column are MEASURED so the cluster
  // centers on real content — a fixed hi/lo reserve left the column padded with
  // dead space and shoved the icon too far left.
  {
    const int row_cy = rows[FLOW_ROW_WEATHER].cy;

    // If the cluster is wider than the row (Big Mode on a narrow watch, or a
    // three-digit temp), give back width from the icon rather than let the
    // readings run off the edge — a smaller sun still reads as a sun, a clipped
    // "↑96°" does not. The icon has a floor, below which we simply accept the
    // overflow instead of drawing a speck.
    const int avail_w = rows[FLOW_ROW_WEATHER].w;
    int icon = m.icon_size;
    int cluster = m.cluster_w;
    if (cluster > avail_w) {
      const int min_icon = 18;
      int over = cluster - avail_w;
      int room = icon - min_icon;
      int shrink = (room < over) ? room : over;
      if (shrink > 0) {
        icon -= shrink;
        cluster -= shrink;
      }
    }
    int cx = ox + (W - cluster) / 2;

    // Night mode swaps the condition icon for tonight's moon phase.
    if (settings_get_night_mode() && s_is_night) {
      icon_draw_moon_phase(ctx, GPoint(cx + icon / 2, row_cy),
                           icon - 6, d->moon_phase, d->moon_illum,
                           theme_fg(), theme_bg());
    } else {
      icon_draw_condition_animated(ctx, GPoint(cx + icon / 2, row_cy),
                                   icon, d->condition, anim_get_frame());
    }

    char temp_buf[8];
    snprintf(temp_buf, sizeof(temp_buf), "%d°", d->temp);
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, temp_buf, m.temp_font,
                       GRect(cx + icon + FZ_CLUSTER_GAP,
                             row_cy - m.temp_sz.h / 2 - FZ_TEXT_RISE,
                             m.temp_sz.w + 4, m.temp_sz.h),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    prv_draw_hilo(ctx,
                  cx + icon + FZ_CLUSTER_GAP + m.temp_sz.w + FZ_CLUSTER_GAP,
                  row_cy, m.hilo_w, m.hilo_font, m.hilo_line_h);
  }

  if (rows[FLOW_ROW_BADGES].present) {
    prv_draw_badge_row(ctx, rows[FLOW_ROW_BADGES].x, rows[FLOW_ROW_BADGES].w,
                       rows[FLOW_ROW_BADGES].cy, &b1, &b2);
  }
  if (rows[FLOW_ROW_UPDATED].present) {
    prv_draw_status_row(ctx, rows[FLOW_ROW_UPDATED].x, rows[FLOW_ROW_UPDATED].w,
                        rows[FLOW_ROW_UPDATED].cy, rain_mode);
  }
}

void clock_zone_draw_compact(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int ox = bounds.origin.x;
  int oy = bounds.origin.y;

  // One top line: time left, temp right (round centers them closer in).
  char temp_buf[8];
  snprintf(temp_buf, sizeof(temp_buf), "%d°", d->temp);
#if defined(PBL_ROUND)
  int y = oy + UI_HEADER_Y;
  // Centered "9:41  72°" cluster reads better inside the circle.
  char line[24];
  snprintf(line, sizeof(line), "%s  %s", s_time_buf, temp_buf);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, line, ui_font_title(),
                     GRect(ox, y, W, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
#else
  int margin = UI_MARGIN_X;
  int y = oy + 2;
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, s_time_buf, ui_font_title(),
                     GRect(ox + margin, y, W / 2, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     NULL);
  graphics_draw_text(ctx, temp_buf, ui_font_title(),
                     GRect(ox + W / 2, y, W / 2 - margin, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
                     NULL);
#endif
}
