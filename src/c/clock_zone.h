#pragma once
#include <pebble.h>

// The face's clock rendering. Two variants:
//   full    — CLOCK mode: the whole flow-laid stack (time, date, complication
//             line, weather row, badge pills, status pill), measured and
//             centered in `bounds`. Owns its own status pill — callers must NOT
//             also draw ui_draw_status_banner in this mode.
//   compact — PEEK/OVERLAY modes: a single top line (time + temp) so the
//             lower band is free for a peek page; the caller still draws the
//             bottom banner there.
// Pass the UNOBSTRUCTED rect: the layout reflows into whatever it is given.
void clock_zone_draw_full(GContext *ctx, GRect bounds);
void clock_zone_draw_compact(GContext *ctx, GRect bounds);

// Status-pill flip state. During an imminent-rain alert the pill alternates
// between the rain warning and the last-updated stamp; main.c owns the 4s
// timer (so the battery rule stays in one place) and calls these to drive it.
void clock_zone_toggle_status_alt(void);
void clock_zone_reset_status_alt(void);

// Re-read the wall clock into the cached strings. Call from the minute tick
// before marking the root layer dirty.
void clock_zone_update_time(void);

// Night = current local time is after sunset or before sunrise, parsed from
// the weather data's formatted sun times. Recompute on each tick / data
// arrival; is_night reads the cached result.
void clock_zone_recompute_night(void);
bool clock_zone_is_night(void);
