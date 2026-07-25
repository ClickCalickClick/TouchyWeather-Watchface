#pragma once
#include <pebble.h>
#include "weather_data.h"

// All icons are drawn with graphics primitives (circles, lines, paths).
// `size` is the bounding-box edge in pixels; the icon is centered in `center`.
// Colors are chosen by the function based on icon meaning unless noted.

void icon_draw_condition(GContext *ctx, GPoint center, int size,
                         WeatherCondition cond);

// Same as icon_draw_condition but driven by an animation frame (incremented
// ~10x/sec). Adds subtle motion: rotating sun rays, bobbing clouds,
// falling raindrops, drifting fog, lightning flash, etc.
void icon_draw_condition_animated(GContext *ctx, GPoint center, int size,
                                  WeatherCondition cond, uint32_t frame);

void icon_draw_sun(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_cloud(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_cloud_rain(GContext *ctx, GPoint center, int size,
                          GColor cloud_color, GColor drop_color);
void icon_draw_snow(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_lightning(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_fog(GContext *ctx, GPoint center, int size, GColor color);

void icon_draw_droplet(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_wind(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_arrow_up(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_arrow_down(GContext *ctx, GPoint center, int size, GColor color);

// Compass arrow pointing toward `dir` ("N","NE",...,"NNW"). Used by the
// 6 Hours card to show per-hour wind direction. Falls back to pointing
// north for unrecognized strings.
void icon_draw_compass_arrow(GContext *ctx, GPoint center, int size,
                             GColor color, const char *dir);
void icon_draw_sunrise(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_sunset(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_pulse(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_tap(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_horizon_sun(GContext *ctx, GPoint center, int size, GColor color);

// Phase 5: clock face (header icon for 4 Hours card).
void icon_draw_clock(GContext *ctx, GPoint center, int size, GColor color);

// Phase 6: calendar with grid (header icon for Week Ahead card).
void icon_draw_calendar(GContext *ctx, GPoint center, int size, GColor color);

// Phase 7: moon glyph for Night Sky.
//   icon_draw_moon_small: stylized crescent for header icon (any size).
//   icon_draw_moon_phase: moon disc with a phase shadow whose lit
//     fraction matches `illum` (0..100). `phase` only selects waxing
//     (1..3) vs. waning (5..7) side; 0/8 = new, 4 = full.
//     bg_color is the card's background, used to draw the shadow.
void icon_draw_moon_small(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_moon_phase(GContext *ctx, GPoint center, int size,
                          uint8_t phase, uint8_t illum,
                          GColor moon_color, GColor bg_color);

// Phase 8: settings gear and lock icon.
void icon_draw_settings_gear(GContext *ctx, GPoint center, int size, GColor color);
void icon_draw_lock_small(GContext *ctx, GPoint center, int size, GColor color);
