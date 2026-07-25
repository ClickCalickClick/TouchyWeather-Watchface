#include "pages.h"
#include "../theme.h"
#include "../ui.h"
#include "../face_fonts.h"
#include "../icons.h"
#include "../settings.h"
#include "../weather_data.h"
#include <stdio.h>

// The Single-peek everything-overlay: one nudge shows the whole current
// picture at once — a 2-column grid of label+value cells under a
// "RIGHT NOW" header (dropped on small classes for room).

typedef struct {
  const char *label;
  char value[16];
  GColor color;
} OverlayCell;

void overlay_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int ox = bounds.origin.x;

  OverlayCell cells[6];
  cells[0].label = "FEELS";
  snprintf(cells[0].value, sizeof(cells[0].value), "%d°", d->feels_like);
  cells[0].color = theme_fg();
  cells[1].label = "WIND";
  snprintf(cells[1].value, sizeof(cells[1].value), "%d %s",
           d->wind_speed, d->wind_dir);
  cells[1].color = theme_fg();
  if (settings_get_use_dew_point()) {
    cells[2].label = "DEW PT";
    snprintf(cells[2].value, sizeof(cells[2].value), "%d°", d->dew_point);
  } else {
    cells[2].label = "HUMIDITY";
    snprintf(cells[2].value, sizeof(cells[2].value), "%d%%", d->humidity);
  }
  cells[2].color = theme_accent_blue();
  cells[3].label = "UV";
  snprintf(cells[3].value, sizeof(cells[3].value), "%d %s",
           d->uv, uv_label(d->uv));
  cells[3].color = theme_accent_orange();
  cells[4].label = "AIR";
  snprintf(cells[4].value, sizeof(cells[4].value), "%d %s",
           d->aqi, aqi_label(d->aqi));
  cells[4].color = theme_accent_blue();
  cells[5].label = "SUN";
  // "6:14-7:45" — strip the AM/PM to fit a half-width cell.
  {
    char sr[8], ss[8];
    snprintf(sr, sizeof(sr), "%s", d->sunrise);
    snprintf(ss, sizeof(ss), "%s", d->sunset);
    for (char *p = sr; *p; p++) if (*p == ' ') { *p = '\0'; break; }
    for (char *p = ss; *p; p++) if (*p == ' ') { *p = '\0'; break; }
    snprintf(cells[5].value, sizeof(cells[5].value), "%s-%s", sr, ss);
  }
  cells[5].color = theme_accent_orange();

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  const bool show_header = false;
  const int cell_h = 26;
  const int lbl_y = -3, lbl_h = 16, val_y = 9, val_h = 22;
#else
  const bool show_header = true;
  const int cell_h = 38;
  const int lbl_y = -4, lbl_h = 18, val_y = 12, val_h = 26;
#endif
  int header_h = show_header ? 20 : 0;
  int grid_h = header_h + 3 * cell_h;
  int y = bounds.origin.y + (bounds.size.h - grid_h) / 2;
  if (y < bounds.origin.y) y = bounds.origin.y;

  if (show_header) {
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, "RIGHT NOW", face_font_label(),
                       GRect(ox, y - 2, W, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
    y += header_h;
  }

  int margin = UI_MARGIN_X;
#if defined(PBL_ROUND)
  margin += 14;  // keep the grid clear of the circle's curve
#endif
  int col_w = (W - 2 * margin) / 2;
  for (int i = 0; i < 6; i++) {
    int col = i % 2;
    int row = i / 2;
    int cx = ox + margin + col * col_w;
    int cy = y + row * cell_h;
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, cells[i].label, face_font_caption(),
                       GRect(cx, cy + lbl_y, col_w, lbl_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
    graphics_context_set_text_color(ctx, cells[i].color);
    graphics_draw_text(ctx, cells[i].value, face_font_header(),
                       GRect(cx, cy + val_y, col_w, val_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
  }
}
