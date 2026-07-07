#include "pages.h"
#include "../theme.h"
#include "../ui.h"
#include "../icons.h"
#include "../weather_data.h"
#include <stdio.h>

// Sun + moon — the app's Sun Cycle rows (sunrise orange / sunset blue,
// icon + time clusters) merged with Night Sky's moon-phase disc.

void page_sun_moon_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int ox = bounds.origin.x;

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int icon_size = 22;
  int row_h = 26;
  int moon_size = 26;
#else
  int icon_size = 28;
  int row_h = 34;
  int moon_size = 36;
#endif
  const int gap = 8;
  GFont time_font = ui_font_header();

  // Left column: sunrise over sunset. Right column: moon disc + illum.
  // Measure the widest time so both rows share one cluster width.
  GSize sr = graphics_text_layout_get_content_size(d->sunrise, time_font,
      GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  GSize ss = graphics_text_layout_get_content_size(d->sunset, time_font,
      GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  int time_w = sr.w > ss.w ? sr.w : ss.w;

  int sun_col_w = icon_size + gap + time_w;
  int moon_col_w = moon_size;
  int col_gap = 18;
  int cluster_w = sun_col_w + col_gap + moon_col_w;
  int cx = ox + (W - cluster_w) / 2;
  int floor_x = ox + UI_MARGIN_X;
  if (cx < floor_x) cx = floor_x;

  int block_h = 2 * row_h;
  int top_y = bounds.origin.y + (bounds.size.h - block_h - 14) / 2;
  if (top_y < bounds.origin.y) top_y = bounds.origin.y;

  // Sunrise row.
  icon_draw_sunrise(ctx, GPoint(cx + icon_size / 2, top_y + row_h / 2),
                    icon_size, theme_accent_orange());
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, d->sunrise, time_font,
      GRect(cx + icon_size + gap, top_y + row_h / 2 - 11, time_w + 4, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Sunset row.
  int y2 = top_y + row_h;
  icon_draw_sunset(ctx, GPoint(cx + icon_size / 2, y2 + row_h / 2),
                   icon_size, theme_accent_blue());
  graphics_draw_text(ctx, d->sunset, time_font,
      GRect(cx + icon_size + gap, y2 + row_h / 2 - 11, time_w + 4, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Moon column: phase disc centered on the two rows, illum% caption below.
  int moon_cx = cx + sun_col_w + col_gap + moon_size / 2;
  int moon_cy = top_y + row_h;
  icon_draw_moon_phase(ctx, GPoint(moon_cx, moon_cy), moon_size,
                       d->moon_phase, d->moon_illum,
                       theme_fg(), theme_bg());
  char illum_buf[8];
  snprintf(illum_buf, sizeof(illum_buf), "%d%%", (int)d->moon_illum);
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, illum_buf, ui_font_caption(),
      GRect(moon_cx - 24, moon_cy + moon_size / 2 + 2, 48, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}
