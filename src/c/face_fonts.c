#include "face_fonts.h"
#include "ui.h"
#include "settings.h"

// See face_fonts.h for the policy. Each accessor: the enlarged path on the
// large screen classes, then a fall-through to the shared ui_font_* on the
// small classes (which are too tight to grow).

GFont face_font_clock(void) {
  // The clock is digits + colon only: LECO_42 (the signature thin Pebble clock
  // at its largest) on every class — including small-rect, where the app's old
  // LECO_36 was for a narrow card column, not this full-width centered face.
  return fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
}

GFont face_font_header(void) {
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
#else
  return ui_font_header();
#endif
}

GFont face_font_hilo(void) {
  // Resting-face hi/lo temps — same size as face_font_header
  // (large: GOTHIC_24_BOLD, small: 18B).
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
#else
  return ui_font_header();
#endif
}

GFont face_font_body(void) {
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
#else
  return ui_font_body();
#endif
}

GFont face_font_title(void) {
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);  // has ° / minus
#else
  return ui_font_title();
#endif
}

GFont face_font_hero(void) {
  // Resting-face weather-row temperature. LECO_42_NUMBERS on every class — the
  // same technical numeral as the clock and the companion app's hero temp, and
  // it carries the ° / minus glyphs.
  return fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
}

GFont face_font_label(void) {
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#else
  return fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
#endif
}

// --- Resting-face tier ramp ---------------------------------------------
//
// Base tier is picked so the full six-row stack fits the class; the promoted
// tier is what the flow layout reaches for once rows are switched off. At the
// promoted tier the clock jumps to the bundled XL custom face on every class
// (see below); the base tier rests at LECO_36 on the tight small classes and
// LECO_42 on the rect/large classes, where the six-row stack already fits.

// --- Promoted XL clock face (bundled custom OFL numeral) -----------------
//
// Chakra Petch Bold, subset to digits + colon (characterRegex "[0-9:]"). One
// bundled size per screen class (see package.json targetPlatforms) — the
// resource name encodes the point size and the flow layout is tuned around
// these. The RESOURCE_ID_FONT_CLOCK_XL_* symbol only exists on the class its
// size targets, so the picker is compiled per class.
#if defined(UI_SCREEN_SMALL_ROUND)      // chalk 180x180
#  define XL_RESID RESOURCE_ID_FONT_CLOCK_XL_48
#elif defined(UI_SCREEN_LARGE_RECT)     // emery 200x228
#  define XL_RESID RESOURCE_ID_FONT_CLOCK_XL_56
#elif defined(UI_SCREEN_LARGE_ROUND)    // gabbro 260x260
#  define XL_RESID RESOURCE_ID_FONT_CLOCK_XL_64
#else                                   // basalt/diorite/flint 144x168
#  define XL_RESID RESOURCE_ID_FONT_CLOCK_XL_52
#endif

// Ink height + top-side leading of each clock tier's numeral, measured from
// screenshots (see face_font_clock_ink_h in the header). Base = LECO_42 (large)
// / LECO_36 (small); XL = the bundled Chakra Petch subset at this class's point
// size. INK drives the flow's TIME-row reservation; RISE lifts the draw box so
// the glyph, not the layout box, lands on that band.
#if defined(UI_SCREEN_SMALL_ROUND)      // chalk: LECO_36 base, XL_48 promoted
#  define FZ_CLOCK_INK_BASE 25
#  define FZ_CLOCK_RISE_BASE 10
#  define FZ_CLOCK_INK_XL   34
#  define FZ_CLOCK_RISE_XL  10
#elif defined(UI_SCREEN_LARGE_RECT)     // emery: LECO_42 base, XL_56 promoted
#  define FZ_CLOCK_INK_BASE 29
#  define FZ_CLOCK_RISE_BASE 12
#  define FZ_CLOCK_INK_XL   40
#  define FZ_CLOCK_RISE_XL  12
#elif defined(UI_SCREEN_LARGE_ROUND)    // gabbro: LECO_42 base, XL_64 promoted
#  define FZ_CLOCK_INK_BASE 29
#  define FZ_CLOCK_RISE_BASE 12
#  define FZ_CLOCK_INK_XL   46
#  define FZ_CLOCK_RISE_XL  14
#else                                   // small-rect: LECO_36 base, XL_52 promoted
#  define FZ_CLOCK_INK_BASE 25
#  define FZ_CLOCK_RISE_BASE 10
#  define FZ_CLOCK_INK_XL   37
#  define FZ_CLOCK_RISE_XL  11
#endif
static GFont s_clock_xl = NULL;

GFont face_font_clock_xl(void) {
  if (!s_clock_xl) {
    s_clock_xl = fonts_load_custom_font(resource_get_handle(XL_RESID));
  }
  // If the custom face ever fails to load, fall back to the largest system
  // numeral rather than handing a NULL font to graphics_draw_text.
  return s_clock_xl ? s_clock_xl
                    : fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
}

void face_fonts_deinit(void) {
  if (s_clock_xl) {
    fonts_unload_custom_font(s_clock_xl);
    s_clock_xl = NULL;
  }
}

int face_font_clock_ink_h(int tier) {
  if (tier >= FACE_TIER_PROMOTED) return FZ_CLOCK_INK_XL;
  return FZ_CLOCK_INK_BASE;
}

int face_font_clock_rise(int tier) {
  if (tier >= FACE_TIER_PROMOTED) return FZ_CLOCK_RISE_XL;
  return FZ_CLOCK_RISE_BASE;
}

GFont face_font_clock_tier(int tier) {
  // Promoted tier is the bundled XL custom face on every class — this is what
  // fills the width when the flow switches optional rows off.
  if (tier >= FACE_TIER_PROMOTED) return face_font_clock_xl();
#if defined(UI_SCREEN_SMALL_ROUND) || defined(UI_SCREEN_SMALL_RECT)
  // Both small classes rest at LECO_36: at LECO_42 the six-row stack does not
  // fit, and the row it would push off is the status stamp — dropping a row the
  // user asked for is worse than a slightly smaller clock.
  return fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
#else
  return face_font_clock();  // LECO_42 base on the large classes
#endif
}

int face_weather_tier(int tier) {
  // Only the promoted tier has anything to give back — at base the weather row
  // is already as small as this class supports, so "Large clock" is a no-op
  // there and the default path below is bit-for-bit untouched.
  if (tier >= FACE_TIER_PROMOTED &&
      settings_get_clock_emphasis() == CLOCK_EMPHASIS_LARGE) {
    return FACE_TIER_BASE;
  }
  return tier;
}

GFont face_font_temp_tier(int tier) {
  tier = face_weather_tier(tier);
  if (tier >= FACE_TIER_PROMOTED) {
#if defined(UI_SCREEN_SMALL_ROUND)
    // Chalk's promoted temp stops at BITHAM_30 — LECO_42 alongside a promoted
    // clock would push the icon|temp|hi-lo cluster past the chord.
    return fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
#else
    return face_font_hero();  // LECO_42, carries ° / minus
#endif
  }
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return face_font_hero();    // the large classes already hero at base tier
#else
  return face_font_title();
#endif
}

GFont face_font_hilo_tier(int tier) {
  tier = face_weather_tier(tier);
#if defined(UI_SCREEN_SMALL_ROUND)
  // The hi/lo pair is two stacked lines, so it drives the weather row's height
  // more than the icon or the temp do. Chalk's 180px circle can't hold the full
  // six-row stack with a 24px line height here, and dropping a row is not an
  // option — so the base tier steps this pair down instead, and steps back up
  // the moment the flow has room.
  return (tier >= FACE_TIER_PROMOTED)
             ? fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)
             : fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
#else
  (void)tier;
  return face_font_hilo();
#endif
}

int face_icon_size_tier(int tier) {
  // The icon is the tallest thing in the weather row at the promoted tier on
  // every screen class (60/58/44/38 against a temp of 46/46/38/36), so it —
  // not the temperature — is what actually sets the row's height. Demoting the
  // fonts without demoting this would leave the row exactly as tall as before.
  const bool up = (face_weather_tier(tier) >= FACE_TIER_PROMOTED);
#if defined(UI_SCREEN_LARGE_ROUND)
  return up ? 60 : 52;
#elif defined(UI_SCREEN_LARGE_RECT)
  return up ? 58 : 50;
#elif defined(UI_SCREEN_SMALL_ROUND)
  return up ? 38 : 32;
#else
  return up ? 44 : 36;
#endif
}

GFont face_font_caption(void) {
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_GOTHIC_18);
#else
  return ui_font_caption();
#endif
}
