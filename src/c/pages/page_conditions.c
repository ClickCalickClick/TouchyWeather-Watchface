#include "pages.h"
#include "../theme.h"
#include "../ui.h"
#include "../icons.h"
#include "../settings.h"
#include "../weather_data.h"
#include <stdio.h>

// Conditions detail — the Main card's secondary stats: FEELS line on top,
// then the app's signature split row (wind | humidity-or-dew-point) with a
// vertical divider, plus a UV line underneath.

void page_conditions_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int ox = bounds.origin.x;

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int feels_h = 24;
  int row_gap = 6;
  int split_h = 40;
#else
  int feels_h = 30;
  int row_gap = 12;
  int split_h = 44;
#endif
  int uv_h = 20;
  int total_h = feels_h + row_gap + split_h + row_gap + uv_h;
  int y = bounds.origin.y + (bounds.size.h - total_h) / 2;
  if (y < bounds.origin.y) y = bounds.origin.y;

  // "FEELS 75°" centered (main card idiom, ui_font_body).
  char feels_buf[16];
  snprintf(feels_buf, sizeof(feels_buf), "FEELS %d°", d->feels_like);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, feels_buf, ui_font_body(),
                     GRect(ox, y - 4, W, feels_h + 6),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
  y += feels_h + row_gap;

  // Split row: wind (left) | humidity or dew point (right), divider between.
  // theme_muted() is a gray that quantizes to the background on 1-bit
  // displays (diorite/flint), erasing the divider stroke — use fg there.
  graphics_context_set_stroke_color(ctx, PBL_IF_BW_ELSE(theme_fg(), theme_muted()));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(ox + W / 2, y), GPoint(ox + W / 2, y + split_h - 4));

  icon_draw_wind(ctx, GPoint(ox + W / 4, y + 8), 22, theme_fg());
  char wind_buf[16];
  const char *wind_unit = (d->units == UNITS_METRIC) ? "KMH" : "MPH";
  snprintf(wind_buf, sizeof(wind_buf), "%d%s %s",
           d->wind_speed, wind_unit, d->wind_dir);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, wind_buf, ui_font_header(),
                     GRect(ox, y + 16, W / 2, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);

  icon_draw_droplet(ctx, GPoint(ox + W * 3 / 4, y + 8), 18,
                    theme_accent_blue());
  char hum_buf[8];
  if (settings_get_use_dew_point()) {
    snprintf(hum_buf, sizeof(hum_buf), "%d°", d->dew_point);
  } else {
    snprintf(hum_buf, sizeof(hum_buf), "%d%%", d->humidity);
  }
  graphics_draw_text(ctx, hum_buf, ui_font_header(),
                     GRect(ox + W / 2, y + 16, W / 2, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
  y += split_h + row_gap;

  // "UV 4 MODERATE" caption line, orange like the app's UV identity.
  char uv_buf[24];
  snprintf(uv_buf, sizeof(uv_buf), "UV %d %s", d->uv, uv_label(d->uv));
  graphics_context_set_text_color(ctx, theme_accent_orange());
  graphics_draw_text(ctx, uv_buf, ui_font_label(),
                     GRect(ox, y, W, uv_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}
