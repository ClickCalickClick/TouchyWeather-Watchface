#include "pages.h"
#include "../theme.h"
#include "../ui.h"

void page_draw(FacePage page, GContext *ctx, GRect bounds) {
  switch (page) {
    case PAGE_HOURS:      page_hours_draw(ctx, bounds); break;
    case PAGE_WEEK:       page_week_draw(ctx, bounds); break;
    case PAGE_CONDITIONS: page_conditions_draw(ctx, bounds); break;
    case PAGE_SUN_MOON:   page_sun_moon_draw(ctx, bounds); break;
    default: break;
  }
}

void page_draw_indicator(GContext *ctx, GRect bounds, int active_index,
                         int total) {
  if (total <= 0) return;
  // Active dot = pill (16w x 4h, radius 2). Inactive = circle 4x4. (nav.c)
  const int dot_size = 4;
  const int active_w = 16;
  const int gap = 6;
  int total_w = total * dot_size + (total - 1) * gap + (active_w - dot_size);
  int x = bounds.origin.x + (bounds.size.w - total_w) / 2;
  int y = bounds.origin.y + (bounds.size.h - dot_size) / 2;

  for (int i = 0; i < total; ++i) {
    if (i == active_index) {
      GRect r = GRect(x, y, active_w, dot_size);
      graphics_context_set_fill_color(ctx, theme_indicator_active());
      graphics_fill_rect(ctx, r, 2, GCornersAll);
      x += active_w + gap;
    } else {
      GRect r = GRect(x, y, dot_size, dot_size);
      graphics_context_set_fill_color(ctx, theme_indicator_inactive());
      graphics_fill_rect(ctx, r, 2, GCornersAll);
      x += dot_size + gap;
    }
  }
}
