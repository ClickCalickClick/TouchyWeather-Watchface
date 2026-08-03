# Store upload kit — TouchyWeather Face

Everything needed for the Pebble appstore listing.

| File | What it is |
|---|---|
| [FEATURES.md](FEATURES.md) | Full feature list, and how the face differs from the TouchyWeather app |
| [LISTING.md](LISTING.md) | Ready-to-paste store copy — blurb, description, keywords |
| [screenshots/](screenshots/) | Per-platform PNGs at native resolution |

## Screenshots

One folder per platform, each PNG at the watch's exact native resolution — no
padding, no scaling, no emulator chrome.

| Platform | Model | Size |
|---|---|---|
| `basalt` | Pebble Time | 144×168 |
| `chalk` | Pebble Time Round | 180×180 |
| `diorite` | Pebble 2 | 144×168 |
| `emery` | Pebble Time 2 | 200×228 |
| `flint` | Pebble 2 Duo | 144×168 |
| `gabbro` | Pebble Round 2 | 260×260 |

### The shots

| File | Feature it sells |
|---|---|
| `01-face-default.png` | The resting face out of the box — feels-like on the line, rain chance as a pill |
| `02-face-loaded.png` | All four complication slots filled — two readings on the line, two pills |
| `03-face-minimal.png` | Every slot and the status row switched off: the stack sheds to clock, date and weather, and re-centres |
| `04-battery.png` | Watch battery and air quality as pills, dew point on the line |
| `05-rain-alert.png` | An imminent-rain alert taking over the status row |
| `06-dark.png` | Dark theme |
| `07-night-mode.png` | Night mode — forced dark, with the moon phase replacing the weather icon |
| `08-peek-hours.png` | Nudge deck: next 6 hours |
| `09-peek-week.png` | Nudge deck: week ahead |
| `10-peek-conditions.png` | Nudge deck: conditions detail |
| `11-peek-sunmoon.png` | Nudge deck: sun + moon |
| `12-overlay.png` | Single-peek dense everything-overlay |
| `13-metric.png` | Metric units — 22°, wind in km/h |
| `14-first-run.png` | The show-once card a fresh install opens with |

If the listing only takes a handful, the strongest set is
**02 → 08 → 07 → 12 → 03**: a loaded face, the deck it hides, night mode, the
dense overlay, and the face stripped back.

**Two captions to get right:**

- **`05-rain-alert` has no rain-chance pill, deliberately.** The face suppresses
  the rain-chance badge while a rain alert is up (`clock_zone.c`) rather than
  saying the same thing twice. It is not a dropped setting.
- **`03-face-minimal` does not show the XL clock.** The custom XL numeral
  promotion is real, but measured across all six PNGs the clock in that shot is
  the same tier as in `01-face-default`. Switching off both complication slots,
  both badges and the status row still leaves the time, date and weather rows,
  and `FZ_PROMOTE_HEADROOM` keeps the promotion off a stack that tall. Do not
  caption it as the clock growing — the difference is measurable, and it isn't
  there.

## How these were captured

These come from the emulator over the ordinary Pebble screenshot endpoint, via
a driver that holds **one libpebble2 connection per platform** and does
everything over it — install, app messages, accel taps, screenshots. Spawning
`pebble <cmd>` per action is both slow and flaky: every invocation reconnects,
and the reconnects intermittently time out.

The weather is served by a `CAPTURE_MODE` branch in `src/pkjs/index.js` that
replaces the live Open-Meteo fetch with one fixed payload, so all six platforms
show the same forecast. **It ships `false`** — only the numbers handed to the
watch change, so what is photographed is the real rendering.

Things that cost real time, in case these need re-shooting:

- **A stale `qemu_spi_flash.bin` bricks the emulator silently.** QEMU keeps
  running and installs and app messages still report success, but its control
  channel is dead and *every* screenshot times out, forever. Deleting the flash
  image per platform is the fix — and it is also what makes the first-run card
  (`14-first-run`) reproducible.
- **`pebble emu-steps` wedges the emulator outright.** The QEMU health-metric
  packet behind it kills the channel the same way; reproducible with the stock
  CLI, so it is an SDK bug. That is why there is no step-count shot: the
  complication would read 0.
- **pebble-tool's emulator registry goes stale.** `pb-emulator.json` caches the
  QEMU/pypkjs PIDs, and its liveness test is only "is this PID running" — once
  the OS recycles a dead emulator's PID, pebble-tool connects to a port nothing
  is listening on and fails with "Connection refused" on every retry. Drop the
  platform's entry before booting.
- **Set the clock format *before* installing.** The face reads
  `clock_is_24h_style()` when it draws and at rest only redraws on the minute
  tick, so flipping it after launch leaves the first shot in 24 h while the
  rest are 12 h. Installing relaunches the face, which picks it up immediately.
- **Big app-message dicts lose their tail.** A 15-key config send applied its
  first keys and silently dropped the rest — no error, no dropped-inbox log. A
  lost `BadgeComp2` just looks like a face with one pill. Send in chunks of
  ~3 keys and repeat the whole config; every setter is idempotent.
- **`NightMode` has to be reset explicitly.** It forces the dark theme and
  `prv_apply_ambient` re-asserts that whenever the theme is not already dark,
  so a plain `Theme=0` does not undo it and the next shot inherits a dark face.
- **pypkjs dies at random**, most often mid-install on the big screens. The
  watch keeps its persisted settings, so the driver tears the pair down and
  reconnects rather than losing the platform.
- **pypkjs runs on standard time** while the watch is on the host's
  DST-corrected local time, so forecast hour labels generated inside PKJS come
  out an hour behind and the first row lands on the current hour. The driver
  generates them instead.

The rain banner alternates with the last-updated stamp every 4 s, so that shot
is captured as several candidates and the frame with the least ink in the
bottom band — `RAIN IN 15M` is shorter than `UPDATED n M AGO` — is kept. That
works on the 1-bit platforms too, where there is no accent colour to look for.
