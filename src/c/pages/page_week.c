#include "pages.h"
#include "../theme.h"
#include "../ui.h"
#include "../icons.h"
#include "../weather_data.h"
#include <stdio.h>

// Week-ahead rows, adapted from the app's cards/week.c: day | icon |
// ↑high / ↓low, uniform columns, cluster centered. The face shows 4 days
// (tomorrow onward — today's hi/lo already lives on the clock).

#define DAY_COUNT 4

void page_week_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;

  // Day 1..4 = tomorrow onward.
  const int first = 1;

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  GFont row_font = ui_font_label();
  const int icon_size = 14;
  const int row_h = 18;
#else
  GFont row_font = ui_font_header();
  const int icon_size = 16;
  const int row_h = 24;
#endif
  const int gap = 6;
  const int arrow = 10;
  const int ag = 3;

  GSize day_max = GSize(0, 0), hi_max = GSize(0, 0), lo_max = GSize(0, 0);
  char buf[12];
  for (int k = 0; k < DAY_COUNT; ++k) {
    int i = first + k;
    GSize ds = graphics_text_layout_get_content_size(d->days_label[i],
        row_font, GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (ds.w > day_max.w) day_max = ds;
    snprintf(buf, sizeof(buf), "%d°", d->days_high[i]);
    GSize hs = graphics_text_layout_get_content_size(buf, row_font,
        GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (hs.w > hi_max.w) hi_max = hs;
    snprintf(buf, sizeof(buf), "%d°", d->days_low[i]);
    GSize ls = graphics_text_layout_get_content_size(buf, row_font,
        GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (ls.w > lo_max.w) lo_max = ls;
  }

  int cluster_w = day_max.w + gap + icon_size + gap +
                  arrow + ag + hi_max.w + gap + arrow + ag + lo_max.w;
  int cluster_x = bounds.origin.x + (W - cluster_w) / 2;
  int floor_x = bounds.origin.x + UI_MARGIN_X;
  if (cluster_x < floor_x) cluster_x = floor_x;

  int top_y = bounds.origin.y + (bounds.size.h - DAY_COUNT * row_h) / 2;
  if (top_y < bounds.origin.y) top_y = bounds.origin.y;

  for (int k = 0; k < DAY_COUNT; ++k) {
    int i = first + k;
    int row_y = top_y + k * row_h;
    int cy = row_y + row_h / 2;
    int x = cluster_x;

    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, d->days_label[i], row_font,
        GRect(x, row_y - 2, day_max.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    x += day_max.w + gap;

    icon_draw_condition(ctx, GPoint(x + icon_size / 2, cy), icon_size,
                        d->days_cond[i]);
    x += icon_size + gap;

    icon_draw_arrow_up(ctx, GPoint(x + arrow / 2, cy), arrow,
                       theme_accent_orange());
    x += arrow + ag;
    snprintf(buf, sizeof(buf), "%d°", d->days_high[i]);
    graphics_context_set_text_color(ctx, theme_accent_orange());
    graphics_draw_text(ctx, buf, row_font,
        GRect(x, row_y - 2, hi_max.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    x += hi_max.w + gap;

    icon_draw_arrow_down(ctx, GPoint(x + arrow / 2, cy), arrow,
                         theme_accent_blue());
    x += arrow + ag;
    snprintf(buf, sizeof(buf), "%d°", d->days_low[i]);
    graphics_context_set_text_color(ctx, theme_accent_blue());
    graphics_draw_text(ctx, buf, row_font,
        GRect(x, row_y - 2, lo_max.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}
