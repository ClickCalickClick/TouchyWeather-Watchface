#include "update_notes.h"
#include "face_state.h"
#include "face_layout.h"
#include "settings.h"
#include "theme.h"
#include "ui.h"
#include "icons.h"
#include "weather_data.h"
#include "version_gen.h"
#include <string.h>

// Persisted APP_VERSION_CODE of the last release whose notes were shown.
//
// 400, deliberately far outside this face's dense low range (1 theme, 10..29 and
// 31..35 settings, 30 weather cache, 200 legacy theme). The watchapp learned
// this the hard way: its key was originally 106 and collided once its weather
// cache reached 106, so every weather save clobbered the seen-version int and
// the card re-popped on every launch. This face's cache key is pinned at 30 and
// validates by blob size rather than incrementing, so that exact trap does not
// transfer — but keep new keys out of the packed range anyway. 400..409 is
// reserved for out-of-band state like this.
#define PERSIST_KEY_NOTES_VERSION 400

// Temporary on-screen instrumentation. `pebble logs` does not flush reliably in
// this environment, so the working technique is to paint the numbers onto the
// face and read them off a screenshot. Set to 0 for real builds.
#define UPDATE_NOTES_DIAG 0

// How long the card holds before the idle timer returns to the clock. Longer
// than a peek (7s) because this is prose the user has never seen; short enough
// that a face is never held hostage by it.
#define NOTES_IDLE_MS 20000

// "New on the horizon" wraps to two lines against chalk's ~137px chord even at
// the smaller header font, and two lines of headline is most of the body budget
// on a 180px round screen. The small classes get a short headline instead.
#if defined(UI_SCREEN_SMALL_ROUND) || defined(UI_SCREEN_SMALL_RECT)
#define HEADLINE     "What's new"
#define SUN_SIZE     20
#define NOTES_MAX_BULLETS 3
#else
#define HEADLINE     "New on the horizon"
#define SUN_SIZE     30
#define NOTES_MAX_BULLETS 4
#endif

// Shown instead of the changelog on a genuinely new watch — a first-run user has
// no "what's new". Static intro copy rather than release notes, which is why it
// lives here and not in CHANGELOG.md.
#define WELCOME_NOTES \
  "Nudge your wrist to page through the forecast.\n" \
  "Pick what the face shows from your phone."

#define MARKER_W       5   // triangle marker width
#define MARKER_INDENT 12   // text indent past the marker
#define NOTES_BUF      600 // must hold APP_UPDATE_NOTES; overflow truncates
#define NOTES_MAX_LINES 16
#define MORE_LINE_H     18 // room reserved for the "+N more" line when it shows

// Which copy this showing uses. Latched in update_notes_maybe_show so the draw
// path never has to re-derive it.
static bool s_welcome = false;

// ---------------------------------------------------------------------------
// Body
// ---------------------------------------------------------------------------

// Split into lines in place. The watchapp records that strtok proved unreliable
// here — likely clobbered by graphics_text_layout's own string handling — so
// this is a manual scan, kept verbatim in spirit.
static int prv_split(const char *src, char *buf, char **lines, int max_lines) {
  strncpy(buf, src, NOTES_BUF - 1);
  buf[NOTES_BUF - 1] = '\0';
  int n = 0;
  lines[n++] = buf;
  for (char *p = buf; *p && n < max_lines; p++) {
    if (*p == '\n') {
      *p = '\0';
      lines[n++] = p + 1;
    }
  }
  return n;
}

// Right-pointing accent triangle, drawn as shrinking 1px columns. The watchapp
// builds a GPath per line per frame; this needs no allocation.
static void prv_draw_marker(GContext *ctx, int x, int cy, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  for (int i = 0; i < MARKER_W; i++) {
    int half = MARKER_W - 1 - i;
    graphics_fill_rect(ctx, GRect(x + i, cy - half, 1, 2 * half + 1), 0,
                       GCornerNone);
  }
}

// Measure (ctx == NULL) or draw the bullet list, one accent-marked line each.
// Returns the height consumed; `*fitted` gets how many lines were placed inside
// `max_h`. Row widths come from face_layout_band_w, so on the round classes each
// line is clamped to its own chord instead of a flat margin — that is what stops
// the lower bullets running under the bezel.
static int prv_layout_body(GContext *ctx, GRect avail, int top_y, char **lines,
                           int n, GFont f, int gap, int max_h, int *fitted) {
  int line_h = graphics_text_layout_get_content_size(
      "Ag", f, GRect(0, 0, 200, 40), GTextOverflowModeWordWrap,
      GTextAlignmentLeft).h;
  int y = top_y;
  int placed = 0;

  for (int i = 0; i < n; i++) {
    if (!lines[i][0]) continue;  // skip blanks from a trailing newline

    // Chord for this row, then re-query once if the text wrapped taller than a
    // single line — a two-line row reaches further down the curve and gets a
    // narrower chord than the first estimate assumed.
    int band = face_layout_band_w(avail, y, line_h);
    int w_text = band - MARKER_INDENT;
    if (w_text < 20) w_text = 20;
    GSize ls = graphics_text_layout_get_content_size(
        lines[i], f, GRect(0, 0, w_text, 1000), GTextOverflowModeWordWrap,
        GTextAlignmentLeft);
    if (ls.h > line_h) {
      band = face_layout_band_w(avail, y, ls.h);
      w_text = band - MARKER_INDENT;
      if (w_text < 20) w_text = 20;
      ls = graphics_text_layout_get_content_size(
          lines[i], f, GRect(0, 0, w_text, 1000), GTextOverflowModeWordWrap,
          GTextAlignmentLeft);
    }

    if (y + ls.h > top_y + max_h) break;  // no room for this one

    if (ctx) {
      int x_text = avail.origin.x + (avail.size.w - band) / 2 + MARKER_INDENT;
      prv_draw_marker(ctx, x_text - MARKER_INDENT, y + line_h / 2,
                      theme_accent_orange());
      graphics_context_set_text_color(ctx, theme_fg());
      graphics_draw_text(ctx, lines[i], f, GRect(x_text, y, w_text, ls.h + 4),
                         GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    }
    y += ls.h + gap;
    placed++;
  }

  if (fitted) *fitted = placed;
  return y - top_y;
}

// ---------------------------------------------------------------------------
// Header: static sun, headline, and a "····  v1.3.0  ····" divider
// ---------------------------------------------------------------------------

// The watchapp animates the sun off a private 10Hz AppTimer. That is exactly the
// kind of unconditional timer this face's battery rules forbid, and anim_kick()
// is not a substitute: its window is 8s, so a 20s card would freeze mid-read,
// and it goes dark entirely when the user turns animations off. Static sun.
static int prv_draw_header(GContext *ctx, GRect avail, bool measure_only) {
  int cx = avail.origin.x + avail.size.w / 2;
  int y = avail.origin.y + 4;

  if (!measure_only) {
    icon_draw_condition(ctx, GPoint(cx, y + SUN_SIZE / 2), SUN_SIZE, COND_SUNNY);
  }
  y += SUN_SIZE + 4;

  GFont hf = ui_font_header();
  int head_w = face_layout_band_w(avail, y, 24);
  GSize hs = graphics_text_layout_get_content_size(
      HEADLINE, hf, GRect(0, 0, head_w, 64), GTextOverflowModeWordWrap,
      GTextAlignmentCenter);
  if (!measure_only) {
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, HEADLINE, hf,
                       GRect(cx - head_w / 2, y, head_w, hs.h + 4),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
  y += hs.h + 5;

  // Version centered in a dotted divider. The welcome card has no release to
  // name, so it gets the rule without the label.
  GFont vf = ui_font_label();
  const char *ver = "v" APP_VERSION_LABEL;
  int rule_w = face_layout_band_w(avail, y, 16);
  int rx1 = cx - rule_w / 2;
  int rx2 = cx + rule_w / 2;
  // theme_secondary, not theme_muted: muted is GColorLightGray in light mode,
  // which the SDK reduces to GColorWhite on the 1-bit classes — invisible on the
  // light background. theme.c calls this out explicitly; the rule and the two
  // muted text runs below all hit it.
  if (s_welcome) {
    if (!measure_only) {
      ui_draw_dotted_hline(ctx, rx1, rx2, y + 6, theme_secondary());
    }
    y += 14;
  } else {
    GSize vs = graphics_text_layout_get_content_size(
        ver, vf, GRect(0, 0, rule_w, 20), GTextOverflowModeFill,
        GTextAlignmentCenter);
    if (!measure_only) {
      int line_y = y + vs.h / 2;
      ui_draw_dotted_hline(ctx, rx1, cx - vs.w / 2 - 6, line_y, theme_secondary());
      ui_draw_dotted_hline(ctx, cx + vs.w / 2 + 6, rx2, line_y, theme_secondary());
      graphics_context_set_text_color(ctx, theme_secondary());
      graphics_draw_text(ctx, ver, vf,
                         GRect(cx - vs.w / 2 - 6, y, vs.w + 12, vs.h + 4),
                         GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
    y += vs.h + 4;
  }
  return y - avail.origin.y;
}

// The card has no buttons and the user has no prior for dismissing it, so say
// so. Which motion actually works depends on their nudge-input setting.
//
// The small classes get one word: at the foot of a 180px round screen the chord
// is far too narrow for "FLICK TO DISMISS", which clipped mid-phrase.
static const char *prv_hint_text(void) {
  switch (settings_get_tap_input_mode()) {
#if defined(UI_SCREEN_SMALL_ROUND) || defined(UI_SCREEN_SMALL_RECT)
    case TAP_INPUT_TAP:    return "TAP";
    case TAP_INPUT_EITHER: return "NUDGE";
    default:               return "FLICK";
#else
    case TAP_INPUT_TAP:    return "TAP TO DISMISS";
    case TAP_INPUT_EITHER: return "FLICK OR TAP";
    default:               return "FLICK TO DISMISS";
#endif
  }
}

// How far the hint sits off the bottom edge. The round classes need real
// clearance: the bezel curve eats the last rows, and a centred run of text there
// is exactly where it bites.
#if defined(PBL_ROUND)
#define HINT_BOTTOM_INSET 16
#else
#define HINT_BOTTOM_INSET 4
#endif

static int prv_draw_hint(GContext *ctx, GRect avail, int y, bool measure_only) {
  GFont f = ui_font_label();
  const char *t = prv_hint_text();
  int w = face_layout_band_w(avail, y, 18);
  GSize s = graphics_text_layout_get_content_size(
      t, f, GRect(0, 0, w, 24), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter);
  if (!measure_only) {
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, t, f,
                       GRect(avail.origin.x + (avail.size.w - w) / 2, y, w,
                             s.h + 4),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
  }
  // Height reserved must match the box actually drawn above, plus the bottom
  // clearance — the body budget is computed from this.
  return s.h + 4 + HINT_BOTTOM_INSET;
}

// ---------------------------------------------------------------------------

bool update_notes_draw(GContext *ctx, GRect avail) {
  static char buf[NOTES_BUF];
  static char *lines[NOTES_MAX_LINES];

#if UPDATE_NOTES_DIAG
  {
    graphics_context_set_fill_color(ctx, theme_bg());
    graphics_fill_rect(ctx, avail, 0, GCornerNone);
    static char d[64];
    int hh = prv_draw_header(ctx, avail, true);
    int nh = prv_draw_hint(ctx, avail, avail.origin.y, true);
    int nn = prv_split(s_welcome ? WELCOME_NOTES : APP_UPDATE_NOTES, buf, lines,
                       NOTES_MAX_LINES);
    graphics_context_set_text_color(ctx, theme_fg());
    snprintf(d, sizeof(d), "h%d n%d b%d", hh, nh,
             avail.size.h - hh - nh - 8);
    graphics_draw_text(ctx, d, ui_font_label(),
                       GRect(avail.origin.x, avail.origin.y + 40,
                             avail.size.w, 24),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    static char d2[64];
    snprintf(d2, sizeof(d2), "n=%d w=%d h=%d", nn, avail.size.w, avail.size.h);
    graphics_draw_text(ctx, d2, ui_font_label(),
                       GRect(avail.origin.x, avail.origin.y + 70,
                             avail.size.w, 24),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    return true;
  }
#endif

  // Header + one bullet + the hint is the floor. Below it, decline and let the
  // caller draw the clock instead — a Quick View can shrink `avail` arbitrarily.
  int head_h = prv_draw_header(ctx, avail, true);
  int hint_h = prv_draw_hint(ctx, avail, avail.origin.y, true);
  int body_max = avail.size.h - head_h - hint_h - 8;
  if (body_max < 24) return false;

  int n = prv_split(s_welcome ? WELCOME_NOTES : APP_UPDATE_NOTES, buf, lines,
                    NOTES_MAX_LINES);
  if (n > NOTES_MAX_BULLETS) n = NOTES_MAX_BULLETS;

  // Fallback ladder. The card cannot scroll, so instead of clipping we step
  // down: normal font, then the small font with a tighter rhythm, then let
  // prv_layout_body place only what fits and say how many were dropped.
  int body_top = avail.origin.y + head_h;
  GFont f = ui_font_header();
  int gap = 8;
  int fitted = 0;
  int used = prv_layout_body(NULL, avail, body_top, lines, n, f, gap, body_max,
                             &fitted);
  if (fitted < n) {
    f = ui_font_label();
    gap = 5;
    used = prv_layout_body(NULL, avail, body_top, lines, n, f, gap, body_max,
                           &fitted);
  }
  // If entries are going to be dropped, the "+N more" line needs its own room —
  // it draws after the body, so without this it runs into the dismissal hint.
  // Re-measure against the smaller budget rather than just shifting it up.
  int more_h = (fitted < n) ? MORE_LINE_H : 0;
  if (more_h) {
    body_max -= more_h;
    used = prv_layout_body(NULL, avail, body_top, lines, n, f, gap, body_max,
                           &fitted);
  }
  if (fitted < 1) return false;  // not even one bullet: not worth showing

  graphics_context_set_fill_color(ctx, theme_bg());
  graphics_fill_rect(ctx, avail, 0, GCornerNone);
  prv_draw_header(ctx, avail, false);
  used = prv_layout_body(ctx, avail, body_top, lines, fitted, f, gap, body_max,
                         &fitted);

  // "+N more" when the ladder had to drop entries, so a truncated card never
  // reads as the whole story.
  int y = body_top + used;
  if (fitted < n) {
    static char more[24];  // "+N more"; sized for any int the compiler fears
    snprintf(more, sizeof(more), "+%d more", n - fitted);
    int w = face_layout_band_w(avail, y, 18);
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, more, ui_font_label(),
                       GRect(avail.origin.x + (avail.size.w - w) / 2, y, w, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
  }

  prv_draw_hint(ctx, avail, avail.origin.y + avail.size.h - hint_h, false);
  return true;
}

void update_notes_maybe_show(void) {
  int32_t seen = persist_exists(PERSIST_KEY_NOTES_VERSION)
                     ? persist_read_int(PERSIST_KEY_NOTES_VERSION)
                     : -1;
  if (seen == APP_VERSION_CODE) return;  // already seen this release

  // A genuinely new watch gets the welcome; anyone with prior state gets the
  // release notes — including the very first build to carry this feature, where
  // nobody has the key yet. settings_is_fresh_install() is latched before any
  // key is written, which is the only point at which the two are separable.
  s_welcome = settings_is_fresh_install();

  // A release with no bullets is a silent one: nothing to say, so say nothing —
  // but still record it, or every launch re-evaluates. The welcome path has its
  // own copy and ignores the flag.
  if (!s_welcome && !APP_HAS_UPDATE_NOTES) {
    persist_write_int(PERSIST_KEY_NOTES_VERSION, APP_VERSION_CODE);
    return;
  }

  face_state_show_update_notes(NOTES_IDLE_MS);

  // Recorded here rather than from the draw path. Doing it on first paint would
  // handle a Quick View stealing the one showing, but it puts a persist write
  // and an app_timer_register inside a layer update proc — side effects in a
  // render pass, which is exactly the kind of thing this face's tightest class
  // (chalk) punishes. The watchapp records on show for its own reasons (surviving
  // a force-quit before dismissal); the same call is the safer one here.
  persist_write_int(PERSIST_KEY_NOTES_VERSION, APP_VERSION_CODE);
}
