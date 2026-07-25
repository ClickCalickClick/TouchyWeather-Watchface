## Project Overview

TouchyWeather Face — a Pebble **watch face** (`watchface: true`) in C,
companion to the TouchyWeather watchapp at `../TouchyWeather`. Shares that
app's design system: `theme.c`, `ui.c`, `icons.c`, `weather_data.c` started as
verbatim copies. They have since DIVERGED — the face stripped Big Mode out of
`ui.c`/`theme.c` (font tiers, high-contrast accents, the taller status banner)
and dropped `icon_draw_battery` — so `diff -q` is no longer the sync audit;
read the diff before porting anything either way. To enlarge type for
readability, edit `face_fonts.c` (the sanctioned face-only font ramp), NOT
`ui.c`.

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

- `face_state.c` — CLOCK / PEEK(page) / OVERLAY state machine; nudge deck with
  resume (a nudge from the clock continues after the last-shown page via
  `s_deck_pos`, wrapping — it doesn't restart at page 1 after the idle return),
  7s idle return, 10s auto-rotate timer, rain auto-peek (edge-triggered). Note
  the peek-page toggles only apply to Nudge Deck / Auto-rotate modes; Single
  peek opens the ONE view `settings_get_single_peek_view()` names (the dense
  `pages/overlay.c` by default, or any single peek page) and Off ignores them —
  the Clay page greys the toggles out for those modes and greys the
  Single-peek picker out for the others (`face_preview.js`). A fixed single
  peek draws no deck dots (main.c) — there is nowhere to page to.
- `gesture.c` — accel-tap nudge; full touch handler compiled out behind
  `ENABLE_TOUCH 0` (touch_service is watchapp-only on current firmware;
  `TOUCH_SPIKE 1` turns on event logging for hardware tests)
- `face_layout.c` — the resting face's flow solver (pure geometry: measure the
  active rows, stack them with elastic gaps, centre vertically, chord-clamp each
  row's width on round). Row heights are the glyphs' INK heights (the fonts'
  top-side leading is dead space and is drawn back in by clock_zone.c's per-row
  rises), so a symmetric solve yields equal top/bottom margins — that is what
  keeps the face centred rather than top-heavy. `face_layout_solve` centres in
  the whole rect it is given — the face has no out-of-flow chrome left (the
  battery glyph, which used to claim a `top_reserve` band, is gone; the charge
  level is a complication now), so the solve takes no reserve argument.
  Owns `face_layout_min_core_h(big_mode)`, which main.c's Quick View cascade
  uses — pass `settings_get_big_mode()`
  so the taller Big-Mode core is accounted for (the file stays settings-free).
  There is no longer a per-class anchor table to keep in lockstep. Rows: TIME ·
  DATE · COMPS · WEATHER · BADGES · UPDATED.
- `clock_zone.c` — resting face: builds the row list, measures each tier, and
  draws every row. Two readings on the complication line draw as ONE centred
  group split by a hairline rule ("FEELS 75° | 12 MPH"), measured so the rule
  lands between the glyphs and each reading keeps its own accent colour; a pair
  too wide for the row falls back to the old half-and-half split, whose
  per-side ellipsis is what keeps it inside a narrow chord. The XL-promotion probe measures the **settled** stack (row
  presence as if the data were fresh — `prv_badge_resolves(assume_fresh)` /
  `prv_status_row_settled`), NOT the rows on screen: the first ~2s after a cold
  launch are stale, and sizing the tier to that transient stack flashed the XL
  face on every launch. Drawn rows keep the real stale-gated presences (the
  unconditional `prv_fill_rows` refill after the tier pick — everything
  downstream must see them). After the solve, a **promoted chord guard**
  re-checks that the solved TIME row spans `m.time_w`; if a taller-than-settled
  stack lifted the XL clock into a chord it can't span (round only), it demotes
  to base and redoes shed+solve — a clipped clock is worse than a smaller one.
  Also owns the status pill (its own drawer, NOT ui.c's
  bottom-anchored banner, which the PEEK/OVERLAY modes still use) and a local
  copy of ui.c's `prv_format_ago` — keep the two in sync. When the stack still
  overflows (a Quick View band), it sheds optional rows bottom-up — but
  **status-last during a rain alert** so the warning survives. TIME/
  DATE/COMPS reserve ink heights and draw with a matching rise
  (`face_font_clock_ink_h`/`_rise`, `FZ_DATE_INK_H`/`FZ_DATE_RISE`,
  `FZ_COMP_INK_H`/`prv_comp_rise`). Every row is a flow row — there is no
  out-of-flow chrome and no top reserve.
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
- `settings.c` — face settings. Big Mode is GONE (v1.3): the flow layout
  already grows the type as slots are switched off, which is what it was for.
  Retired persist keys are deleted at init (24 battery glyph, 26 Big Mode).

Four complication slots share one menu (`ComplicationSlot`): line 1 / line 2
draw as text under the date, badge 1 / badge 2 as coloured pills. Steps is
line-only — it cannot fit a pill on the small classes. Each badge has an opt-in
"only when notable" filter; upgrading users inherit it ON, which reproduces the
pre-v1.2 automatic rain/UV badges exactly (see `prv_migrate_badges`).

No reading may sit in two slots at once: the Clay pickers disable an option
another slot already holds (`face_preview.js`), and the watch de-duplicates at
DRAW time (`prv_badge_slots` / `prv_build_comp_line`) so a config saved before
that rule renders once instead of twice. Draw-time, not storage — freeing the
other slot brings the setting straight back.

`COMPLICATION_BATTERY` is the odd one out: watch state, not weather. It
replaced the v1.2 always-on battery glyph (`BatteryDisplay`, its top-band
reserve and `icon_draw_battery` are all deleted), so it draws only where the
user puts it, and it is exempt from the badge staleness gate — a stale forecast
says nothing about the charge. Its "notable" rule (≤20%, or charging) is
exactly the old `BATTERY_WHEN_LOW` behaviour, so a user who wants the old face
sets a badge to Battery + only-when-notable. There is deliberately NO
migration: nothing appears unless they ask for it.

Persist keys: 1 theme (theme.c) · 10–29 settings (24 and 26 retired + deleted
at init, 29 = badge 1) · 30 weather cache · 31–35 settings continued (badge 2,
both notable flags, UpdatedDisplay, SinglePeekView). Key 30 sits inside the settings range — never reuse it. Bump
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

**The list is APPEND-ONLY.** Keys are numbered by POSITION (10000 + index), so
inserting or removing one renumbers every key after it — and `pebble build`
does not reliably regenerate `build/js/message_keys.json` when package.json
changes. When the phone's map and the watch's header drift, every setting is
applied to the NEXT setting's key: the config still saves, nothing logs an
error, and the failures look like unrelated UI bugs. This actually shipped —
removing `BatteryDisplay` mid-list made the Big Mode toggle write into `Temp`
(the face showed 1°), sent Line 2 to a dead legacy key, and turned Badge 2's
value into Badge 1's "only when notable" flag.

So: retire a setting by renaming its slot to `Retired<Name>` (see
`RetiredBatteryDisplay`, `RetiredBigMode`) and add new keys at the END. After
ANY messageKeys edit, `rm -rf build && pebble build`, then:

```bash
node tools/check-message-keys.js   # phone map vs watch header vs package.json
node tools/audit-settings.js       # every Clay item -> the C setter that runs it
```
