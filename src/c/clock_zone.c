#include "clock_zone.h"
#include "theme.h"
#include "ui.h"
#include "icons.h"
#include "weather_data.h"
#include "anim.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char s_time_buf[8];   // "22:25" / "9:41"
static char s_date_buf[16];  // "MON JUL 6"

void clock_zone_update_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

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
// right-column idiom at label scale. x is the left edge of the arrows.
static void prv_draw_hilo(GContext *ctx, int x, int y, int w) {
  WeatherData *d = weather_data_get();
  char hi_buf[8], lo_buf[8];
  snprintf(hi_buf, sizeof(hi_buf), "%d°", d->high);
  snprintf(lo_buf, sizeof(lo_buf), "%d°", d->low);
  icon_draw_arrow_up(ctx, GPoint(x + 5, y + 9), 10, theme_accent_orange());
  graphics_context_set_text_color(ctx, theme_accent_orange());
  graphics_draw_text(ctx, hi_buf, ui_font_header(),
                     GRect(x + 12, y - 2, w - 12, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  icon_draw_arrow_down(ctx, GPoint(x + 5, y + 31), 10, theme_accent_blue());
  graphics_context_set_text_color(ctx, theme_accent_blue());
  graphics_draw_text(ctx, lo_buf, ui_font_header(),
                     GRect(x + 12, y + 20, w - 12, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

void clock_zone_draw_full(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int ox = bounds.origin.x;
  int oy = bounds.origin.y;

  // Per-screen-class vertical anchors. Weather row: icon | temp | hi-lo.
#if defined(UI_SCREEN_SMALL_RECT)
  int time_y = 6,  time_h = 42;
  int date_y = 50;
  int row_y  = 84;
  int icon_size = 30;
#elif defined(UI_SCREEN_SMALL_ROUND)
  int time_y = 22, time_h = 48;
  int date_y = 70;
  int row_y  = 100;
  int icon_size = 32;
#elif defined(UI_SCREEN_LARGE_RECT)
  int time_y = 18, time_h = 48;
  int date_y = 70;
  int row_y  = 112;
  int icon_size = 38;
#else  // UI_SCREEN_LARGE_ROUND
  int time_y = 40, time_h = 48;
  int date_y = 92;
  int row_y  = 140;
  int icon_size = 40;
#endif

  // Big time, centered. ui_font_number() = LECO (digits + ':' only — the
  // degree/minus limitation doesn't matter here).
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, s_time_buf, ui_font_number(),
                     GRect(ox, oy + time_y, W, time_h),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // Date, centered, secondary.
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, s_date_buf, ui_font_header(),
                     GRect(ox, oy + date_y, W, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);

  // Weather row: animated condition icon | big temp | hi-lo column,
  // laid out as one centered cluster.
  char temp_buf[8];
  snprintf(temp_buf, sizeof(temp_buf), "%d°", d->temp);
  GFont temp_font = ui_font_title();  // GOTHIC_28_BOLD — has ° and minus
  GSize temp_sz = graphics_text_layout_get_content_size(
      temp_buf, temp_font, GRect(0, 0, W, 36),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  int hilo_w = 40;
  int gap = 10;
  int cluster_w = icon_size + gap + temp_sz.w + gap + hilo_w;
  int cx = ox + (W - cluster_w) / 2;
  int row_cy = oy + row_y + 22;  // vertical center of the row band

  icon_draw_condition_animated(ctx, GPoint(cx + icon_size / 2, row_cy),
                               icon_size, d->condition, anim_get_frame());
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, temp_buf, temp_font,
                     GRect(cx + icon_size + gap, oy + row_y + 2,
                           temp_sz.w + 4, 36),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  prv_draw_hilo(ctx, cx + icon_size + gap + temp_sz.w + gap, oy + row_y + 1,
                hilo_w);
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
