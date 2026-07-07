#include "pages.h"
#include "../theme.h"
#include "../ui.h"

// Phase 2 placeholders — replaced by real draw code in Phase 3.

static void prv_stub(GContext *ctx, GRect bounds, const char *name) {
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, name, ui_font_header(),
                     GRect(bounds.origin.x,
                           bounds.origin.y + bounds.size.h / 2 - 12,
                           bounds.size.w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

void page_hours_draw(GContext *ctx, GRect bounds)      { prv_stub(ctx, bounds, "6 HOURS"); }
void page_week_draw(GContext *ctx, GRect bounds)       { prv_stub(ctx, bounds, "WEEK AHEAD"); }
void page_conditions_draw(GContext *ctx, GRect bounds) { prv_stub(ctx, bounds, "CONDITIONS"); }
void page_sun_moon_draw(GContext *ctx, GRect bounds)   { prv_stub(ctx, bounds, "SUN + MOON"); }
void overlay_draw(GContext *ctx, GRect bounds)         { prv_stub(ctx, bounds, "RIGHT NOW"); }
