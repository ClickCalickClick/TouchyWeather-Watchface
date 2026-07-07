#pragma once
#include <pebble.h>
#include "../settings.h"

// Peek pages, mirroring the app's card_X_draw(GContext*, GRect) signature.
// Each draws into the lower band handed to it by main.c (below the compact
// time line, above the page dots + banner).
void page_hours_draw(GContext *ctx, GRect bounds);
void page_week_draw(GContext *ctx, GRect bounds);
void page_conditions_draw(GContext *ctx, GRect bounds);
void page_sun_moon_draw(GContext *ctx, GRect bounds);

// Phase 5: the Single-peek everything-overlay.
void overlay_draw(GContext *ctx, GRect bounds);

// Dispatch helper used by main.c's update proc.
void page_draw(FacePage page, GContext *ctx, GRect bounds);

// The app's page-dot indicator idiom (nav.c): active = 16x4 pill, inactive
// = 4x4 dot, 6px gap, centered in `bounds`.
void page_draw_indicator(GContext *ctx, GRect bounds, int active_index,
                         int total);
