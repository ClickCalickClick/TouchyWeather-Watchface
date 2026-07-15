## Project Overview

TouchyWeather Face — a Pebble **watch face** (`watchface: true`) in C,
companion to the TouchyWeather watchapp at `../TouchyWeather`. Shares that
app's design system: `theme.c`, `ui.c`, `icons.c`, `weather_data.c` are
verbatim copies (plus a B&W icon-color fix); when fixing bugs in those,
consider syncing with the app.

## Supported Platforms

basalt, chalk, diorite, emery, flint, gabbro (no aplite — same as the app).

## Commands

```bash
pebble build
pebble install --emulator emery --vnc
pebble screenshot --emulator emery --vnc --no-open screenshot.png
pebble emu-tap --emulator emery --vnc          # simulate a nudge (accel tap)
pebble emu-set-time --emulator emery --vnc 12:00:00
pebble emu-set-timeline-quick-view --emulator emery --vnc on
pebble logs --emulator emery --vnc
```

Always add `--vnc` to emulator commands (headless-safe). After UI changes,
screenshot and inspect before declaring success.

Send test config/data (keys are numeric; look them up in `build/appinfo.json`
appKeys). Use ONE `--int` flag with multiple pairs — repeated `--int` flags
silently drop earlier ones. Sends occasionally fail silently right after
`emu-set-time`; retry once if the screen doesn't change:

```bash
pebble send-app-message --emulator emery --vnc --int 10000=1 10032=20
```

## Architecture

Single full-screen Layer; `main.c`'s update proc dispatches on
`face_state_mode()`:

- `face_state.c` — CLOCK / PEEK(page) / OVERLAY state machine; nudge deck,
  7s idle return, 10s auto-rotate timer, rain auto-peek (edge-triggered)
- `gesture.c` — accel-tap nudge; full touch handler compiled out behind
  `ENABLE_TOUCH 0` (touch_service is watchapp-only on current firmware;
  `TOUCH_SPIKE 1` turns on event logging for hardware tests)
- `clock_zone.c` — resting face + compact line; night computation, UV badge
- `pages/` — peek pages, each `(GContext *ctx, GRect bounds)` like the
  app's cards; `overlay.c` is the single-peek grid
- `comm.c` — trimmed app pipeline: inbox parse, persist cache (keys 30–34,
  chunked — WeatherData exceeds Pebble's 256B persist cap), refresh
  sentinel, minute-tick staleness refetch with geometric backoff (no
  wakeups needed — a face's PKJS runs while the face is active)
- `settings.c` — face settings; exports `settings_get_big_mode()` (false)
  and `settings_get_animations_enabled()` so the copied modules compile

Persist keys: 1 theme (theme.c) · 10–25 settings (21 retired) · 30–34
weather cache (chunked). The cache's per-chunk size check on read is the
layout-drift guard — a struct whose size changed just fails to load and
falls back to mock, no manual key-bump needed.

## Battery rules (enforce when adding timers)

At rest with no rain alert: exactly one wakeup per minute (tick). Every
AppTimer must be conditional and self-cancelling: anim 10Hz only inside the
8s post-`anim_kick()` window; banner flip 4s only while `rain_alert_min >= 0`;
idle-return only in PEEK/OVERLAY; rotate 10s only in AUTO_ROTATE mode.

## messageKeys discipline

Every key in the PKJS payload MUST exist in `package.json` messageKeys or
the entire `Pebble.sendAppMessage` fails. Check `pebble logs` for send
errors after touching either side.
