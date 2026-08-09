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
// (emery/gabbro) grow their icons and hi/lo column to match the enlarged
// fonts; the small classes keep the compact originals verbatim.
// FZ_HILO_ARROW_COL = the arrow+gap lead before the hi/lo text (prv_draw_hilo
// draws the arrow at x+5 and the text at x+12), so the measured cluster width
// is FZ_HILO_ARROW_COL + max(hi,lo text width).
// FZ_TEXT_RISE: px to lift a vertically-centered text box so the visible glyph
// (not the layout box, which carries the font's top-side internal leading)
// lands on the target midline. Used to center the temp and hi/lo on the weather
// row's midline. Larger fonts on the large classes carry more top padding.
// FZ_DATE_INK_H / FZ_COMP_INK_H are the VISIBLE ink heights of the date
// (GOTHIC_24_BOLD large / GOTHIC_18_BOLD small) and complication line
// (GOTHIC_18_BOLD large / GOTHIC_14_BOLD small), measured from screenshots. The
// flow reserves these rather than the fonts' taller layout boxes, whose dead
// top-side leading otherwise inflated the inter-row gaps. FZ_*_RISE lifts each
// draw box by that leading so the glyph — not the box — lands on the reserved
// band (the same idiom FZ_TEXT_RISE already applies to the weather temp).
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  #define FZ_HILO_ARROW_COL 12
  #define FZ_ARROW_SIZE   12
  #define FZ_HILO_BOX_H   26
  #define FZ_TEMP_BOX_H   38
  #define FZ_DATE_INK_H   14
  #define FZ_DATE_RISE    10
  #define FZ_COMP_INK_H   11
  #define FZ_COMP_RISE     7
  #define FZ_TEXT_RISE     4
#else
  #define FZ_HILO_ARROW_COL 12
  #define FZ_ARROW_SIZE   10
  #define FZ_HILO_BOX_H   22
  #define FZ_TEMP_BOX_H   36
  #define FZ_DATE_INK_H   11
  #define FZ_DATE_RISE     7
  #define FZ_COMP_INK_H    9
  #define FZ_COMP_RISE     6
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

// Vertical text offset inside a pill: the layout box carries the font's
// top-side internal leading, so the text rect starts slightly above the pill.
// -2 is the tuned value for both pill shapes (14B-in-16 on small, 18B-in-22
// on large).
#define FZ_PILL_TEXT_DY (-2)

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

// "Low" for the battery reading: the same <=20% (and never while charging)
// rule the retired battery glyph used, now shared by the reading's warning
// accent and its "only when notable" filter.
static bool prv_battery_low(BatteryChargeState bat) {
  return bat.charge_percent <= 20 && !bat.is_charging;
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
    case COMPLICATION_BATTERY: {
      // The one place the charge level reaches the face now: watch state the
      // user opted into, not the automatic glyph it replaced. Low reads in the
      // warning accent so it still catches the eye without its own chrome.
      BatteryChargeState bat = battery_state_service_peek();
      if (prv_battery_low(bat)) *color = theme_accent_orange();
      snprintf(buf, n,
               bat.is_charging ? (compact ? "CHG %d%%" : "CHARGING %d%%")
                               : (compact ? "BATT %d%%" : "BATTERY %d%%"),
               bat.charge_percent);
      return true;
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
    case COMPLICATION_BATTERY: {
      BatteryChargeState bat = battery_state_service_peek();
      if (prv_battery_low(bat)) *fill = theme_accent_orange();
      snprintf(buf, n, bat.is_charging ? "CHG %d%%" : "BATT %d%%",
               bat.charge_percent);
      return true;
    }
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
    // Keyed on the WIND unit, not `units` — those axes are independent now
    // (WindUnits in weather_data.h). 9 m/s is the sibling app's number and
    // matches its 20 mph at the top of Beaufort 5.
    //
    // The mph/kmh entries are the PRE-EXISTING 20/30, kept verbatim. The app
    // uses 32 for km/h, so the two products disagree by 2 km/h — that
    // divergence predates this change, and correcting it here would move a
    // badge threshold for existing metric users under cover of a feature whose
    // whole premise is "nothing changes unless you pick m/s". Worth fixing;
    // not worth fixing silently, and not in this commit.
    //
    // Safe to index unguarded because comm.c clamps wind_units on receipt.
    case COMPLICATION_WIND: {
      static const uint8_t wind_notable_by_unit[3] = { 20, 30, 9 };
      return d->wind_speed >= wind_notable_by_unit[d->wind_units];
    }
    case COMPLICATION_FEELS: {
      int delta = d->feels_like - d->temp;
      if (delta < 0) delta = -delta;
      return delta >= (metric ? 5 : 10);
    }
    // Notable == the retired glyph's BATTERY_WHEN_LOW rule: low, or charging
    // (a plugged-in watch is worth confirming at a glance).
    case COMPLICATION_BATTERY: {
      BatteryChargeState bat = battery_state_service_peek();
      return bat.is_charging || prv_battery_low(bat);
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
  // The Clay pickers stop a reading being chosen twice, but a config saved
  // before that rule (or a hand-sent value) can still hold a pair. Drawing
  // "FEELS 75° | FEELS 75°" is never what was meant — the second slot yields.
  // Draw-time only: the stored setting is left alone, so freeing the other
  // slot brings this one straight back.
  if (c2 == c1) c2 = COMPLICATION_OFF;
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

// Rendered width of a short label. 140x24 is a measuring box big enough for
// every string either caller passes (pill text, one complication reading).
static int prv_text_w(const char *txt, GFont f) {
  return graphics_text_layout_get_content_size(
      txt, f, GRect(0, 0, 140, 24),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter).w;
}

static void prv_draw_comp_text(GContext *ctx, GRect slot, const char *txt,
                               GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, txt, face_font_label(), slot,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

// Divider geometry for the two-reading line: gap, 1px rule, gap.
#define FZ_COMP_DIV_GAP 5
#define FZ_COMP_DIV_W   (2 * FZ_COMP_DIV_GAP + 1)

// The complication line between the date and the weather row.
//
// One reading centres across `slot`. TWO are drawn as a single centred group
// with a hairline divider between them — "FEELS 75° | 12 MPH" — rather than
// each centred in its own half of the row, which flung them to opposite edges
// and read as two unrelated things. The pair is MEASURED and laid out from the
// group's left edge so the divider sits between the actual glyphs; each
// reading keeps its own accent colour (which a single joined string could not
// express). If the pair is wider than the row, fall back to the old half-and-
// half split, whose per-side ellipsis is what keeps a long pair inside a
// narrow chord.
static void prv_draw_comp_line(GContext *ctx, GRect slot, const CompLine *c) {
  if (c->show1 && c->show2) {
    GFont f = face_font_label();
    int w1 = prv_text_w(c->buf1, f);
    int w2 = prv_text_w(c->buf2, f);
    int total = w1 + FZ_COMP_DIV_W + w2;

    if (total <= slot.size.w) {
      int x = slot.origin.x + (slot.size.w - total) / 2;
      prv_draw_comp_text(ctx, GRect(x, slot.origin.y, w1, slot.size.h),
                         c->buf1, c->col1);
      // The rule spans the row's ink band, not the taller draw box (which
      // carries the font's dead top-side leading), so it reads as centred
      // against the glyphs beside it.
      int rule_x = x + w1 + FZ_COMP_DIV_GAP;
      int rule_top = slot.origin.y + FZ_COMP_RISE;
      // theme_muted is the right weight for a divider on colour, but on 1-bit
      // it reduces to the background in BOTH themes (LightGray->white on light,
      // DarkGray->black on dark) and the rule vanishes. Step up to the always-
      // legible secondary there.
      graphics_context_set_stroke_color(
          ctx, PBL_IF_BW_ELSE(theme_secondary(), theme_muted()));
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, GPoint(rule_x, rule_top),
                         GPoint(rule_x, rule_top + FZ_COMP_INK_H - 1));
      prv_draw_comp_text(ctx,
                         GRect(x + w1 + FZ_COMP_DIV_W, slot.origin.y, w2,
                               slot.size.h),
                         c->buf2, c->col2);
      return;
    }

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

// The one rule chain for "does this badge slot resolve to a pill": the slot is
// configured, the data isn't stale (a stale reading must never be dressed up
// as a live "RAIN 70%" — the battery is exempt, it reads the watch, not the
// forecast), no imminent-rain alert is claiming the rain badge (the alert
// already says it, louder), and the notable filter passes.
// `assume_fresh` skips only the staleness gate — the tier probe below asks
// about the SETTLED face (see clock_zone_draw_full), and routing it through
// this same chain keeps probe and drawn badge from ever drifting apart.
// `buf`/`fill` are caller scratch; the probe passes throwaways.
static bool prv_badge_resolves(ComplicationSlot slot, bool notable_only,
                               bool assume_fresh, char *buf, size_t n,
                               GColor *fill) {
  WeatherData *d = weather_data_get();
  if (slot == COMPLICATION_OFF) return false;
  if (!assume_fresh && slot != COMPLICATION_BATTERY && comm_data_is_stale()) {
    return false;
  }
  if (slot == COMPLICATION_RAIN_CHANCE && d->rain_alert_min >= 0) return false;
  if (notable_only && !prv_badge_is_notable(slot)) return false;
  return prv_format_badge(slot, buf, n, fill);
}

// Resolve one badge slot into a drawable pill, honoring the staleness gate.
static void prv_build_badge(ComplicationSlot slot, bool notable_only,
                            Badge *out) {
  out->show = prv_badge_resolves(slot, notable_only, false,
                                 out->buf, sizeof(out->buf), &out->fill);
}

// The effective badge slots. Same de-duplication rule as the complication
// line above — a reading already in badge 1 does not draw a second identical
// pill — resolved in ONE place so the tier probe below and the drawn row can
// never disagree about whether the badge row exists.
static void prv_badge_slots(ComplicationSlot *s1, ComplicationSlot *s2) {
  *s1 = settings_get_badge1();
  *s2 = settings_get_badge2();
  if (*s2 == *s1) *s2 = COMPLICATION_OFF;
}

// Build both badge slots from current data + settings.
static void prv_build_badges(Badge *b1, Badge *b2) {
  ComplicationSlot s1, s2;
  prv_badge_slots(&s1, &s2);
  prv_build_badge(s1, settings_get_badge1_notable(), b1);
  prv_build_badge(s2, settings_get_badge2_notable(), b2);
}

// Would either badge slot show once the data is fresh? The tier probe uses
// this instead of the drawn Badge structs: at a cold launch the cache is stale
// for the first couple of seconds, and sizing the clock to that transient
// stack flashed the XL face on every launch and then demoted it when the
// fresh payload landed. Presence is all the probe needs (the row's height is
// a constant pill), so this stays off the stack — a few bytes of scratch, no
// Badge structs (chalk).
static bool prv_badges_would_show_fresh(void) {
  char buf[16];
  GColor fill;
  ComplicationSlot s1, s2;
  prv_badge_slots(&s1, &s2);
  return prv_badge_resolves(s1, settings_get_badge1_notable(), true,
                            buf, sizeof(buf), &fill) ||
         prv_badge_resolves(s2, settings_get_badge2_notable(), true,
                            buf, sizeof(buf), &fill);
}

static int prv_badge_w(const char *txt, GFont f) {
  // Symmetric horizontal padding inside the pill.
  return prv_text_w(txt, f) + 2 * FZ_PILL_PAD;
}

// Total width of the badge row as drawn (0 when nothing shows) — the flow
// layout measures with this before deciding whether the row fits its chord.
static int prv_badge_row_w(const Badge *b1, const Badge *b2) {
  GFont f = face_font_label();
  int w1 = b1->show ? prv_badge_w(b1->buf, f) : 0;
  int w2 = b2->show ? prv_badge_w(b2->buf, f) : 0;
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

// The status row's SETTLED presence — where prv_status_row_visible lands once
// fresh data arrives. A rain alert claims the row in every mode; otherwise
// only UPDATED_ALWAYS keeps it (fresh data means STALE_OR_RAIN's stamp is
// down, and NEVER never shows one). The tier probe sizes the clock against
// this rather than the transiently-stale launch state.
static bool prv_status_row_settled(void) {
  if (weather_data_get()->rain_alert_min >= 0) return true;
  return settings_get_updated_display() == UPDATED_ALWAYS;
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
  // gets orange for the alert and muted for the stamp.
  GColor fill, txt;
#if defined(PBL_BW)
  fill = theme_fg();
  txt = theme_bg();
#else
  if (rain_mode) {
    fill = theme_accent_orange();
    txt = GColorBlack;
  } else {
    fill = theme_muted();
    txt = theme_fg();
  }
#endif

  // Clamp to the row's usable width, but never below the padding the text rect
  // subtracts — a narrower pill than that yields a negative-width text rect.
  GFont f = face_font_label();
  const int ph = FZ_PILL_H;
  int w = prv_badge_w(buf, f);
  if (w > W) w = W;
  if (w < 2 * FZ_PILL_PAD + 8) w = 2 * FZ_PILL_PAD + 8;
  GRect r = GRect(ox + (W - w) / 2, cy - ph / 2, w, ph);
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, r, ph / 2, GCornersAll);
  graphics_context_set_text_color(ctx, txt);
  graphics_draw_text(ctx, buf, f,
                     GRect(r.origin.x + 4,
                           r.origin.y + FZ_PILL_TEXT_DY,
                           r.size.w - 8, r.size.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

static void prv_draw_pill(GContext *ctx, GRect r, const char *txt, GColor fill) {
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, r, r.size.h / 2, GCornersAll);
  graphics_context_set_text_color(ctx, theme_bg());
  graphics_draw_text(ctx, txt, face_font_label(),
                     GRect(r.origin.x, r.origin.y + FZ_PILL_TEXT_DY,
                           r.size.w, r.size.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

// Draw the active badges as one centered row at `cy`. Both pills sit
// side-by-side when both slots resolve; a single pill centers on its own.
static void prv_draw_badge_row(GContext *ctx, int ox, int W, int cy,
                               const Badge *b1, const Badge *b2) {
  GFont f = face_font_label();
  const int ph = FZ_PILL_H;
  int total = prv_badge_row_w(b1, b2);
  if (total == 0) return;
  int x = ox + (W - total) / 2;
  int y = cy - ph / 2;
  if (b1->show) {
    int w = prv_badge_w(b1->buf, f);
    prv_draw_pill(ctx, GRect(x, y, w, ph), b1->buf, b1->fill);
    x += w + FZ_PILL_GAP;
  }
  if (b2->show) {
    prv_draw_pill(ctx, GRect(x, y, prv_badge_w(b2->buf, f), ph),
                  b2->buf, b2->fill);
  }
}

// Everything the weather row needs, measured once per tier so the flow can try
// a tier on for size before committing to drawing it.
#define FZ_CLUSTER_GAP 10
// Smallest icon the width giveback may leave — below this a sun is a speck.
#define FZ_ICON_MIN 18
typedef struct {
  GFont time_font;
  int   time_h;     // reserved TIME-row height = the numeral's ink height
  int   time_rise;  // top-side leading to lift the draw box by
  int   time_w;     // measured numeral width — the chord guard checks the
                    // solved TIME row spans it (round can come back narrower)
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
  // The clock is digits + colon only, so its ink height is constant per font —
  // reserve that (not the layout box, whose top-side leading is dead space) and
  // remember the rise to lift the draw box by. Drops a per-frame text measure.
  m->time_h = face_font_clock_ink_h(tier);
  m->time_rise = face_font_clock_rise(tier);
  m->time_w = graphics_text_layout_get_content_size(
      s_time_buf, m->time_font, GRect(0, 0, W, 60),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft).w;

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
  int temp_h = (tier >= FACE_TIER_PROMOTED) ? FZ_TEMP_INK_H + 6
                                            : FZ_TEMP_INK_H;
  m->weather_h = m->icon_size;
  if (temp_h > m->weather_h) m->weather_h = temp_h;
  if (hilo_h > m->weather_h) m->weather_h = hilo_h;
}

static void prv_fill_rows(FlowRow rows[FLOW_ROW_COUNT], const FaceMetrics *m,
                          bool has_comps, bool has_badges, bool has_status) {
  rows[FLOW_ROW_TIME].present = true;
  rows[FLOW_ROW_TIME].h = m->time_h;
  rows[FLOW_ROW_DATE].present = true;
  rows[FLOW_ROW_DATE].h = FZ_DATE_INK_H;
  rows[FLOW_ROW_COMPS].present = has_comps;
  rows[FLOW_ROW_COMPS].h = FZ_COMP_INK_H;
  rows[FLOW_ROW_WEATHER].present = true;
  rows[FLOW_ROW_WEATHER].h = m->weather_h;
  rows[FLOW_ROW_BADGES].present = has_badges;
  rows[FLOW_ROW_BADGES].h = FZ_PILL_H;
  rows[FLOW_ROW_UPDATED].present = has_status;
  rows[FLOW_ROW_UPDATED].h = FZ_PILL_H;
}

// Extra slack the promoted tier must leave behind before we take it. Two jobs:
// keep a promoted stack off the bezel, AND hold the design line that the FULL
// six-row stack rests at the base clock on every class (its honest ink stack is
// now short enough that the XL clock would otherwise fit). Sized per class to
// sit above the six-row promoted required_h but below the required_h once a row
// or two is switched off — so turning rows off still grows the clock, as
// designed. Retune alongside the FZ_CLOCK_INK_XL constants.
#if defined(UI_SCREEN_LARGE_ROUND)
  #define FZ_PROMOTE_HEADROOM 20
#elif defined(UI_SCREEN_LARGE_RECT)
  #define FZ_PROMOTE_HEADROOM 24
#elif defined(UI_SCREEN_SMALL_ROUND)
  #define FZ_PROMOTE_HEADROOM 28
#else  // UI_SCREEN_SMALL_RECT
  #define FZ_PROMOTE_HEADROOM 20
#endif

void clock_zone_draw_full(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  const int W = bounds.size.w;
  const int ox = bounds.origin.x;

  // Every row on the resting face is a flow row now — the battery glyph was the
  // one piece of out-of-flow chrome, and it is gone (the charge level is a
  // complication the user assigns to a slot). So the whole rect is the flow's
  // to use, and every fit test below (promotion, the Big-Mode ladder, the shed
  // backstop) compares against it.
  const int avail_h = bounds.size.h;

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
  // fall back and re-measure.
  //
  // The probe measures the SETTLED stack (row presence as if the data were
  // fresh), not the one on screen right now: at a cold launch the cache is
  // stale for the first ~2s, which hides the badges and (under STALE_OR_RAIN)
  // raises the stamp, and sizing the tier to that transient stack promoted
  // the clock to XL and then demoted it the moment the fresh payload landed —
  // a visible flash on every launch. The tier holds steady; only the rows
  // themselves swap in when the data freshens.
  int tier = FACE_TIER_BASE;
  prv_measure(&m, FACE_TIER_PROMOTED, W);
  prv_fill_rows(rows, &m, has_comps, prv_badges_would_show_fresh(),
                prv_status_row_settled());
  if (face_layout_required_h(rows) + FZ_PROMOTE_HEADROOM <= avail_h &&
      m.cluster_w <= face_layout_band_w(bounds, bounds.origin.y +
                                        bounds.size.h / 2, m.weather_h)) {
    tier = FACE_TIER_PROMOTED;
  }
  if (tier == FACE_TIER_BASE) {
    prv_measure(&m, FACE_TIER_BASE, W);
  }
  // The probe filled `rows` with the settled presences; everything downstream
  // — the shed backstop, the solve and the drawers' rows[*].present checks —
  // must see the real ones.
  prv_fill_rows(rows, &m, has_comps, has_badges, has_status);

  // Too tall (a Quick View band)? Shed optional rows rather than clip the
  // clock; face_layout_solve top-aligns if even that loses, so the clock
  // stays whole. Normal order is bottom-up —
  // the status stamp is the cheapest loss — but during an imminent-rain alert
  // the status row IS the alert, so it flips to shedding last instead.
  // The solve also arbitrates the promoted tier's width: on the round classes
  // a taller-than-settled stack (the stale stamp riding under a settled-XL
  // face, or badges simply switched off with the stamp kept) lifts the TIME
  // row into a chord the XL numerals don't span, and the clock draws clipped.
  // The promotion gate can't see that — it probes the settled stack, and the
  // chord depends on where the solve actually lands the row — so the guard
  // lives here: if the solved TIME row comes back narrower than the numerals,
  // fall to the base tier and redo from the refill. A clipped clock is worse
  // than a smaller one. Terminates in two passes at most (the tier only ever
  // steps down); on the rect classes the band is constant and the XL fonts
  // were sized to it, so the guard never trips there.
  for (;;) {
    {
      static const FlowRowId shed_normal[3] = {FLOW_ROW_UPDATED,
                                               FLOW_ROW_BADGES, FLOW_ROW_COMPS};
      static const FlowRowId shed_alert[3] = {FLOW_ROW_BADGES, FLOW_ROW_COMPS,
                                              FLOW_ROW_UPDATED};
      const FlowRowId *shed_order =
          (d->rain_alert_min >= 0) ? shed_alert : shed_normal;
      for (int i = 0; i < 3; i++) {
        if (face_layout_required_h(rows) <= avail_h) break;
        rows[shed_order[i]].present = false;
      }
    }

    face_layout_solve(rows, bounds);

    if (tier == FACE_TIER_PROMOTED && m.time_w > rows[FLOW_ROW_TIME].w) {
      tier = FACE_TIER_BASE;
      prv_measure(&m, FACE_TIER_BASE, W);
      prv_fill_rows(rows, &m, has_comps, has_badges, has_status);
      continue;
    }
    break;
  }

  // --- Draw ---
  // Each text row draws in a box lifted by its font's top-side leading so the
  // visible glyph lands on the ink band the flow reserved (rows[*].h == ink);
  // the box carries extra slack below the ink, never above it.
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, s_time_buf, m.time_font,
                     GRect(rows[FLOW_ROW_TIME].x,
                           rows[FLOW_ROW_TIME].y - m.time_rise,
                           rows[FLOW_ROW_TIME].w,
                           m.time_rise + rows[FLOW_ROW_TIME].h + 6),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, s_date_buf, face_font_header(),
                     GRect(rows[FLOW_ROW_DATE].x,
                           rows[FLOW_ROW_DATE].y - FZ_DATE_RISE,
                           rows[FLOW_ROW_DATE].w,
                           FZ_DATE_RISE + rows[FLOW_ROW_DATE].h + 4),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);

  if (rows[FLOW_ROW_COMPS].present) {
    const int crise = FZ_COMP_RISE;
    prv_draw_comp_line(ctx, GRect(rows[FLOW_ROW_COMPS].x,
                                  rows[FLOW_ROW_COMPS].y - crise,
                                  rows[FLOW_ROW_COMPS].w,
                                  crise + rows[FLOW_ROW_COMPS].h + 4),
                       &comps);
  }

  // Weather row: animated condition icon | big temp | hi-lo column, laid out as
  // one centered cluster. The temp and hi/lo column are MEASURED so the cluster
  // centers on real content — a fixed hi/lo reserve left the column padded with
  // dead space and shoved the icon too far left.
  {
    const int row_cy = rows[FLOW_ROW_WEATHER].cy;

    // If the cluster is wider than the row (a three-digit temp on a narrow
    // watch), give back width from the icon rather than let the
    // readings run off the edge — a smaller sun still reads as a sun, a clipped
    // "↑96°" does not. The icon has a floor, below which we simply accept the
    // overflow instead of drawing a speck.
    const int avail_w = rows[FLOW_ROW_WEATHER].w;
    int icon = m.icon_size;
    int cluster = m.cluster_w;
    if (cluster > avail_w) {
      const int min_icon = FZ_ICON_MIN;
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
