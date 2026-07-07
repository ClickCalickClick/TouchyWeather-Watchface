#pragma once
#include <pebble.h>

// Trimmed AppMessage pipeline (from the app's comm.c): weather ingestion,
// whole-struct persist cache, refresh sentinel. The app's radar streaming,
// card-management protocol, and wakeup/background machinery are dropped —
// a watch face's PKJS runs whenever the face is active, so freshness is a
// minute-tick staleness check instead of scheduled wakeups.

typedef void (*CommUpdateCb)(void);

void comm_init(void);
void comm_deinit(void);

// Load the cached WeatherData blob. Call BEFORE the first draw so units and
// values don't flash from mock to real (the app's units-flash fix).
void comm_load_cache(void);

// Ask PKJS for a fetch (sends the LastUpdated=1 sentinel + ClockIs24h).
void comm_request_refresh(void);

// Called from the minute tick: refetch if the data is older than 30 min.
void comm_check_staleness(void);

// Invoked on any data/config arrival that changed what's on screen.
void comm_set_update_callback(CommUpdateCb cb);
