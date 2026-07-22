#include "face_fonts.h"
#include "ui.h"
#include "settings.h"

// See face_fonts.h for the policy. Each accessor: Big Mode branch first (never
// smaller than the enlarged normal path), then the enlarged normal path on the
// large screen classes, then a fall-through to the shared ui_font_* on the
// small classes (which are too tight to grow).

GFont face_font_clock(void) {
  // The clock is digits + colon only. Big Mode keeps the app's full-height hero
  // numeral; the normal path uses LECO_42 (the signature thin Pebble clock at
  // its largest) on every class — including small-rect, where the app's old
  // LECO_36 was for a narrow card column, not this full-width centered face.
  if (settings_get_big_mode()) return ui_font_number();  // BITHAM_42_BOLD
  return fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
}

GFont face_font_header(void) {
  if (settings_get_big_mode()) return ui_font_header();  // large 24B (==normal), small 18B
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
#else
  return ui_font_header();
#endif
}

GFont face_font_hilo(void) {
  // Resting-face hi/lo temps. Same size as face_font_header on the NORMAL path
  // (large: GOTHIC_24_BOLD, small: 18B), but in Big Mode it grows to
  // GOTHIC_24_BOLD on EVERY class — including the small screens, where the date
  // and page rows stay compact but the secondary temps should still be legible
  // in the accessibility mode.
  if (settings_get_big_mode()) {
    return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  }
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
#else
  return ui_font_header();
#endif
}

GFont face_font_body(void) {
  if (settings_get_big_mode()) return ui_font_body();  // large 28B (==normal), small 24B
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
#else
  return ui_font_body();
#endif
}

GFont face_font_title(void) {
  if (settings_get_big_mode()) return ui_font_title();  // BITHAM_30_BLACK (==normal on large)
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);  // has ° / minus
#else
  return ui_font_title();
#endif
}

GFont face_font_hero(void) {
  // Resting-face weather-row temperature. LECO_42_NUMBERS on every class — the
  // same technical numeral as the clock and the companion app's hero temp, and
  // it carries the ° / minus glyphs. Already the largest numeral, so it also
  // satisfies the Big-Mode "never smaller than normal" invariant unchanged.
  return fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
}

GFont face_font_label_big(bool big) {
  // Explicit-tier variant: the resting face's Big-Mode overflow ladder demotes
  // chrome rows back to the normal tier while the setting is still ON, so it
  // cannot go through the settings-driven accessors — name the fonts directly.
  if (big) return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#else
  return fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
#endif
}

GFont face_font_label(void) {
  return face_font_label_big(settings_get_big_mode());
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

GFont face_font_clock_tier(int tier) {
  if (settings_get_big_mode()) return face_font_clock();
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

GFont face_font_temp_tier(int tier) {
  if (settings_get_big_mode()) return face_font_hero();
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
  if (settings_get_big_mode()) return face_font_hilo();
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
  bool up = (tier >= FACE_TIER_PROMOTED) && !settings_get_big_mode();
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
  // Inversion guard: ui.c's Big Mode caption is GOTHIC_14_BOLD, which is
  // SMALLER than our enlarged normal (GOTHIC_18) on the large classes — so in
  // Big Mode on large we lift to GOTHIC_18_BOLD to keep Big Mode >= normal.
  if (settings_get_big_mode()) {
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
    return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#else
    return ui_font_caption();
#endif
  }
#if defined(UI_SCREEN_LARGE_RECT) || defined(UI_SCREEN_LARGE_ROUND)
  return fonts_get_system_font(FONT_KEY_GOTHIC_18);
#else
  return ui_font_caption();
#endif
}
