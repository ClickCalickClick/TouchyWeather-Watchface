#pragma once
#include <pebble.h>

// The face's clock rendering. Two variants:
//   full    — CLOCK mode: big time, date, condition icon + temp + hi/lo row.
//             Leaves the bottom banner band to the caller (ui_draw_auto_banner).
//   compact — PEEK/OVERLAY modes: a single top line (time + temp) so the
//             lower band is free for a peek page.
void clock_zone_draw_full(GContext *ctx, GRect bounds);
void clock_zone_draw_compact(GContext *ctx, GRect bounds);

// Re-read the wall clock into the cached strings. Call from the minute tick
// before marking the root layer dirty.
void clock_zone_update_time(void);

// Night = current local time is after sunset or before sunrise, parsed from
// the weather data's formatted sun times. Recompute on each tick / data
// arrival; is_night reads the cached result.
void clock_zone_recompute_night(void);
bool clock_zone_is_night(void);
