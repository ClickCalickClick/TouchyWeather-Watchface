#include "pages.h"
#include "../theme.h"
#include "../ui.h"

// Phase 5 replaces this with the real dense everything-overlay.
void overlay_draw(GContext *ctx, GRect bounds) {
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, "RIGHT NOW", ui_font_header(),
                     GRect(bounds.origin.x,
                           bounds.origin.y + bounds.size.h / 2 - 12,
                           bounds.size.w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}
