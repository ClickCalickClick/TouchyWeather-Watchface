#include "pages.h"
#include "../theme.h"
#include "../ui.h"
#include "../icons.h"
#include "../weather_data.h"
#include <stdio.h>

// 6-hour strip, adapted from the app's cards/hours.c: uniform-column
// cluster centering of time | icon | temp | pop rows. The face band is
// shorter than a full card, so small classes show 4 rows, large show 6,
// and the wind/precip columns stay in the app (pop% is the face's one
// context column).

static GColor prv_cond_accent(WeatherCondition cond) {
  switch (cond) {
    case COND_SUNNY:
    case COND_PARTLY_CLOUDY:
      return theme_accent_orange();
    case COND_RAIN:
    case COND_SNOW:
    case COND_STORM:
      return theme_accent_blue();
    default:
      return theme_fg();
  }
}

void page_hours_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  const int rows = 4;
  GFont row_font = ui_font_label();
  const int icon_size = 14;
  const int row_h = 17;
#else
  const int rows = 6;
  GFont row_font = ui_font_header();
  const int icon_size = 16;
  const int row_h = 21;
#endif
  GFont pop_font = ui_font_label();
  const int gap = 6;

  // Uniform column widths across the shown rows.
  GSize time_max = GSize(0, 0), temp_max = GSize(0, 0);
  int pop_text_w = 0;
  bool any_pop = false;
  char buf[12];
  for (int i = 0; i < rows; ++i) {
    GSize ts = graphics_text_layout_get_content_size(d->hours_label[i],
        row_font, GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (ts.w > time_max.w) time_max = ts;
    snprintf(buf, sizeof(buf), "%d°", d->hours_temp[i]);
    GSize ps = graphics_text_layout_get_content_size(buf, row_font,
        GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (ps.w > temp_max.w) temp_max = ps;
    if (d->hours_pop[i] >= 30) {
      any_pop = true;
      snprintf(buf, sizeof(buf), "%d%%", (int)d->hours_pop[i]);
      GSize qs = graphics_text_layout_get_content_size(buf, pop_font,
          GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis,
          GTextAlignmentLeft);
      if (qs.w > pop_text_w) pop_text_w = qs.w;
    }
  }

  const int drop_icon = 10;
  int pop_col_w = any_pop ? (drop_icon + 3 + pop_text_w) : 0;
  int cluster_w = time_max.w + gap + icon_size + gap + temp_max.w +
                  (any_pop ? (gap + pop_col_w) : 0);
  int cluster_x = bounds.origin.x + (W - cluster_w) / 2;
  int floor_x = bounds.origin.x + UI_MARGIN_X;
  if (cluster_x < floor_x) cluster_x = floor_x;

  // Vertically center the block in the band.
  int top_y = bounds.origin.y + (bounds.size.h - rows * row_h) / 2;
  if (top_y < bounds.origin.y) top_y = bounds.origin.y;

  for (int i = 0; i < rows; ++i) {
    int row_y = top_y + i * row_h;
    int icon_cy = row_y + row_h / 2;

    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, d->hours_label[i], row_font,
        GRect(cluster_x, row_y - 2, time_max.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    int icon_cx = cluster_x + time_max.w + gap + icon_size / 2;
    icon_draw_condition(ctx, GPoint(icon_cx, icon_cy), icon_size,
                        d->hours_cond[i]);

    snprintf(buf, sizeof(buf), "%d°", d->hours_temp[i]);
    int temp_x = cluster_x + time_max.w + gap + icon_size + gap;
    graphics_context_set_text_color(ctx, prv_cond_accent(d->hours_cond[i]));
    graphics_draw_text(ctx, buf, row_font,
        GRect(temp_x, row_y - 2, temp_max.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    if (any_pop && d->hours_pop[i] >= 30) {
      int pop_x = temp_x + temp_max.w + gap;
      icon_draw_droplet(ctx, GPoint(pop_x + drop_icon / 2, icon_cy),
                        drop_icon, theme_accent_blue());
      snprintf(buf, sizeof(buf), "%d%%", (int)d->hours_pop[i]);
      graphics_context_set_text_color(ctx, theme_accent_blue());
      graphics_draw_text(ctx, buf, pop_font,
          GRect(pop_x + drop_icon + 3, row_y, pop_text_w + 4, 18),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }
}
