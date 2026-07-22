## Project Overview

TouchyWeather Face — a Pebble **watch face** (`watchface: true`) in C,
companion to the TouchyWeather watchapp at `../TouchyWeather`. Shares that
app's design system: `theme.c`, `ui.c`, `icons.c`, `weather_data.c` are
verbatim copies (plus a B&W icon-color fix, and a Big-Mode rain-banner
contrast fix in `ui.c` — both worth syncing back); when fixing bugs in those,
consider syncing with the app. To enlarge type for readability, edit
`face_fonts.c` (the sanctioned face-only font ramp), NOT `ui.c`.

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
- `face_layout.c` — the resting face's flow solver (pure geometry: measure the
  active rows, stack them with elastic gaps, centre vertically, chord-clamp each
  row's width on round). Owns `face_layout_min_core_h(big_mode)`, which main.c's
  Quick View cascade uses — pass `settings_get_big_mode()` so the taller
  Big-Mode core is accounted for (the file stays settings-free). There is no
  longer a per-class anchor table to keep in lockstep. Rows: TIME · DATE · COMPS
  · WEATHER · BADGES · UPDATED.
- `clock_zone.c` — resting face: builds the row list, measures each tier, and
  draws every row. Also owns the status pill (its own drawer, NOT ui.c's
  bottom-anchored banner, which the PEEK/OVERLAY modes still use) and a local
  copy of ui.c's `prv_format_ago` — keep the two in sync. Owns the **Big-Mode
  overflow ladder** in `clock_zone_draw_full` (Big never promotes; TIME/DATE/
  WEATHER always keep the big fonts): (1) width — if the big weather cluster
  can't fit even after the icon giveback, demote just that row to the
  enlarged-normal ramp (`s_big_weather_demoted`, still ≥ normal); (2) a round
  badge-chord guard; (3) height — demote chrome rows (COMPS/BADGES/UPDATED) to
  the normal tier bottom-up via the `s_chrome` flags (`prv_comp_box_h` etc.);
  (4) backstop — shed optional rows, bottom-up normally but **status-last
  during a rain alert** so the warning survives. `s_chrome`/`s_big_weather_
  demoted` are `static` draw-path scratch (chalk stack is tight). On the tight
  classes the maximal Big stack still can't hold UPDATED (chalk 180 / rect 144
  with both badges) — that shed is expected, not a bug.
- `face_fonts.c` — face-only font ramp over the shared `ui_font_*` roles:
  enlarges type a tier on the large screen classes (emery/gabbro), falls
  through to `ui_font_*` on the tight small classes, and keeps Big Mode ≥ the
  enlarged normal path. Enlarge readability here, never in the shared `ui.c`.
  `face_font_label_big(bool)` is the explicit-tier label accessor the overflow
  ladder uses to draw a demoted chrome row at the normal tier while Big Mode is
  still on (the settings-driven `face_font_label()` can't express that).
  Owns the promoted XL clock tier: `face_font_clock_xl()` lazy-loads a bundled
  custom OFL numeral (Chakra Petch Bold, `resources/fonts/`, subset via
  `characterRegex "[0-9:]"` — digits + colon only, no ° / minus, so it must
  never be reused for temps without widening the subset) into one static
  handle the first time the flow promotes the clock. One size per screen class
  (`FONT_CLOCK_XL_48/52/56/64` in package.json, scoped by `targetPlatforms`);
  `face_fonts_deinit()` unloads it and is called from main.c's `prv_deinit`.
  This is the repo's only custom font — that whole lifetime lives here.
- `pages/` — peek pages, each `(GContext *ctx, GRect bounds)` like the
  app's cards; `overlay.c` is the single-peek grid
- `comm.c` — trimmed app pipeline: inbox parse, persist cache (key 30),
  refresh sentinel, minute-tick staleness refetch (no wakeups needed —
  a face's PKJS runs while the face is active)
- `settings.c` — face settings; `settings_get_big_mode()` is now a real
  opt-in toggle (accessibility: bigger fonts + high-contrast colors, drives
  `face_fonts.c`/`theme.c`/`ui.c`), plus `settings_get_animations_enabled()`

Four complication slots share one menu (`ComplicationSlot`): line 1 / line 2
draw as text under the date, badge 1 / badge 2 as coloured pills. Steps is
line-only — it cannot fit a pill on the small classes. Each badge has an opt-in
"only when notable" filter; upgrading users inherit it ON, which reproduces the
pre-v1.2 automatic rain/UV badges exactly (see `prv_migrate_badges`).

Persist keys: 1 theme (theme.c) · 10–29 settings (26 = Big Mode, 29 = badge 1) ·
30 weather cache · 31–34 settings continued (badge 2, both notable flags,
UpdatedDisplay). Key 30 sits inside the settings range — never reuse it. Bump
30's comment trail whenever WeatherData changes layout; `comm_load_cache` also
rejects any blob whose size doesn't match the struct.

## Battery rules (enforce when adding timers)

At rest with no rain alert: exactly one wakeup per minute (tick). Every
AppTimer must be conditional and self-cancelling: anim 10Hz only inside the
8s post-`anim_kick()` window; banner flip 4s only while `rain_alert_min >= 0`;
idle-return only in PEEK/OVERLAY; rotate 10s only in AUTO_ROTATE mode.

## messageKeys discipline

Every key in the PKJS payload MUST exist in `package.json` messageKeys or
the entire `Pebble.sendAppMessage` fails. Check `pebble logs` for send
errors after touching either side.
