#pragma once
#include <pebble.h>

typedef enum {
  THEME_LIGHT = 0,
  THEME_DARK = 1,
} ThemeMode;

void theme_init(void);
void theme_set(ThemeMode mode);
ThemeMode theme_get(void);

GColor theme_bg(void);
GColor theme_fg(void);
GColor theme_muted(void);          // dotted-line / divider gray
GColor theme_secondary(void);      // legible body text on either bg
GColor theme_accent_orange(void);  // sunrise / UV / banner
GColor theme_accent_blue(void);    // sunset / precipitation / AQ
GColor theme_accent_advice(void);  // touch & go advice card identity
GColor theme_indicator_active(void);
GColor theme_indicator_inactive(void);

// Apply current theme bg color to a window AND remember it so future
// theme_set calls auto-update the window background. Call once during
// window load.
void theme_apply_to_window(Window *window);
