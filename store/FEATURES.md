# TouchyWeather Face — features, and how it differs from the app

**TouchyWeather Face v1.3.0** · UUID `2784b37d-cf9e-4b1b-80a1-1dc94ef10f53`
Companion to the **TouchyWeather** watchapp (v2.0.0), which lives in the
[TouchyWeather](../../TouchyWeather) repo.

The two ship from separate codebases that started as copies of each other.
`theme.c`, `ui.c`, `icons.c` and `weather_data.c` began verbatim and have since
diverged, so they look like siblings and behave nothing alike.

---

## The one-line version

> **The app is where you go to read the weather. The face is where the weather
> comes to you.**

The app is a twelve-card carousel you open, swipe through, and close. The face
is your clock — it is on screen all day, it shows the readings *you* chose, and
the rest of the forecast is one flick of the wrist away.

---

## What makes the watch face special

### 1. A measured flow layout, not a fixed template

This is the thing nothing else here does. The resting face has six possible
rows — **time · date · complication line · weather · badges · last-updated** —
and it does not lay them out from a template. Every draw, it *measures* the rows
that are actually present, stacks them with elastic gaps, and centres the group.

Turn a row off and the others do not just shuffle up: the whole stack
re-centres and the type **grows a tier**. Row heights are the glyphs' real ink
heights rather than the fonts' boxes, so a symmetric solve produces genuinely
equal top and bottom margins — which is what keeps the face optically centred
instead of top-heavy.

There is no out-of-flow chrome anywhere on the face. The v1.2 always-on battery
glyph — the last piece that reserved a fixed band — was deleted in v1.3, and the
charge level became a complication instead.

### 2. The clock promotes to a custom XL numeral

Strip the face back far enough and the digits swap to a **bundled OFL numeral
face** (Chakra Petch Bold), subset at build time to digits and a colon, at one
size per screen class. It is the only custom font in either project.

The promotion probe measures the *settled* stack — row presence as if the data
were already fresh — not the rows on screen, because the first couple of seconds
after a cold launch are stale and sizing to that transient stack made the XL
face flash on every launch. On the round watches a second guard re-checks that
the solved clock row actually spans its chord and demotes if it does not: a
smaller clock beats a clipped one.

### 3. Four complication slots, one menu

Two text lines under the date and two coloured pills below the weather. Any slot
takes any reading:

> feels like · wind · humidity · dew point · UV index · air quality ·
> rain chance · step count *(lines only)* · **watch battery**

- Two readings on one line draw as a **single centred group split by a hairline
  rule** — `FEELS 75° | 12 NW` — measured so the rule lands between the glyphs
  and each reading keeps its own accent colour.
- Each badge has an opt-in **"only when notable"** filter, so a pill can stay
  hidden until it is worth reading: rain ≥ 50%, UV ≥ 6 midday, battery ≤ 20% or
  charging.
- **No reading can sit in two slots at once.** The phone pickers grey out an
  option another slot holds, *and* the watch de-duplicates at draw time — so a
  config saved before that rule renders once instead of twice, and freeing the
  other slot brings the setting straight back.

**Watch battery** is the odd one out and is face-only: it is watch state, not
weather. It draws only where you put it, and it is exempt from the staleness
gate the weather badges obey — a stale forecast says nothing about your charge.

### 4. Nudge input — the only input a watch face gets

A watch face on current Pebble firmware receives **no button clicks and no touch
events** (`touch_service` is watchapp-only). So the face is built around the
accelerometer: **nudge** the watch — flick your wrist or tap the glass — and the
lower half deals a peek card.

- **Nudge Deck** — one nudge per page, through **6 Hours → Week Ahead →
  Conditions → Sun + Moon**. It auto-returns after ~7 s, and the deck
  **resumes**: your next nudge picks up after the last card you saw rather than
  restarting at page one.
- **Single peek** — one fixed view, closed by the next nudge. The dense
  everything-overlay by default, or any single page you pin.
- **Auto-rotate** — pages cycle on a timer, no nudging.
- **Off** — clock only.

**Nudge input** picks which motion counts: a wrist flick lands on X/Y, a tap on
Z, and *Either* is the reliability fallback since axis discrimination is
approximate on real hardware.

The full touch implementation (tap, swipe L/R, swipe up, swipe down) ships
compiled out behind `ENABLE_TOUCH` in `gesture.c`, using the app's exact
thresholds — ready for the firmware that delivers touch to faces.

### 5. Ambient behaviour the app has none of

- **Rain auto-peek** — the hourly page flashes up by itself when rain is due
  within the hour (edge-triggered, so it fires once per event, not in a loop).
- **Night mode** — after sunset the face forces the dark theme and swaps the
  hero icon for the current moon phase, then restores your daytime theme at
  sunrise.
- **Quick View reflow** — when the timeline Quick View band eats the bottom of
  the screen, the face sheds optional rows bottom-up rather than clipping —
  except during a rain alert, where the status row sheds *last* so the warning
  survives.
- **Rain alert takeover** — an imminent-rain alert takes over the bottom row in
  every mode, alternating `RAIN IN 15M` (orange) with `UPDATED 5M AGO` (muted),
  even when the status row is switched off.

### 6. A live schematic in the phone settings

The Clay page carries a **sketch of the resting face that reshapes as you change
settings** — boxes appear, disappear and re-centre, the clock promotes, and a
conditional pill draws dashed. It follows your connected watch's actual shape
and colour depth.

### 7. Battery discipline

At rest the face wakes **exactly once per minute**, on the tick — and that
holds *even during an active rain alert*, because the rain chrome owns no timer
at all: the `RAIN` ⇄ `UPDATED` pill alternates on the tick, and the rain
auto-peek appears on the data-arrival redraw and returns to the clock on a
later one. Every remaining timer is conditional and self-cancelling: the 10 Hz
icon animation only inside its 8 s window, the idle return only while a peek or
the update card is open, auto-rotate only in that mode.

It also needs **no background wakeups to stay fresh** — a face's PKJS runs while
the face is on screen, so the staleness refetch rides the minute tick. The app
has to schedule wakeups to do the same job.

---

## Side by side

| | **TouchyWeather Face** | **TouchyWeather** (app) |
|---|---|---|
| What it is | Watch face — always on screen | Watchapp — you open it |
| Weather views | 4 peek pages + a dense overlay | 12 cards |
| Layout | Measured flow, re-centres and grows as rows switch off | Fixed per-card layouts |
| Clock | Yes — the whole point, promotes to a custom XL numeral | None |
| Complications | 4 user-assignable slots (2 lines, 2 pills) | — |
| Watch battery | A complication you can place | — |
| Step count | A complication (declares the `health` capability) | — |
| Input | Accelerometer nudge only | Buttons + touch (swipe, pull, tap) |
| Detail charts | — | Detail sheets on 5 cards |
| Radar | — | Live RainViewer radar (colour, 128 KB models) |
| Personality | — | Touch & Go — 15 tiers of one-liners |
| Big Mode | Retired in v1.3 — the flow layout replaced it | Yes, an accessibility mode |
| Card management | Peek pages toggled from the phone | On-watch show/hide/reorder, or phone |
| Ambient | Rain auto-peek · night mode · Quick View reflow | — |
| Refresh | Rides the minute tick while on screen | Background wakeups every 30 or 60 min |
| Launcher glance | — | Current temp + condition in the launcher |
| Pollen | — | Yes (CAMS / Google Pollen) |

### Shared between them

Same Open-Meteo forecast and air-quality pipeline, same BigDataCloud reverse
geocoding, no API keys required. Same six platforms, same light/dark theme,
same icon set and accent colours. Both are configured from the phone with Clay,
both cache the last reading on the watch so something is on screen before the
first new byte arrives, both show a one-time "New on the horizon" card generated
straight from `CHANGELOG.md` at build time, and both send one anonymous
active-user ping per day (tagged `face` vs `app`), which carries no name, no
email and coordinates rounded to ~11 km.

### What you still open the app for

Radar, the Touch & Go personality engine, the detail sheets with their vector
charts, the Golden Hour / Precipitation / dedicated UV and Air Quality cards,
pollen, Big Mode, and on-watch card management. The face is deliberately the
glanceable half of the pair, not a replacement for it.

---

## Platforms

All six modern Pebbles, no aplite — the same list the app ships for.

| Platform | Model | Display | Colour |
|---|---|---|---|
| `emery` | Pebble Time 2 | 200×228 rect | ✅ |
| `gabbro` | Pebble Round 2 | 260×260 round | ✅ |
| `basalt` | Pebble Time | 144×168 rect | ✅ |
| `chalk` | Pebble Time Round | 180×180 round | ✅ |
| `diorite` | Pebble 2 | 144×168 rect | 1-bit B&W |
| `flint` | Pebble 2 Duo | 144×168 rect | 1-bit B&W |

Layouts branch across four screen classes — small-rect, small-round, large-rect
and large-round. On the round watches every row is width-clamped through the
chord solver rather than a flat margin, so nothing crowds the bezel.

---

## Settings (phone, via Clay)

- **Appearance** — light / dark theme · animations
- **Clock Face** — live preview · line 1 · line 2 · badge 1 (+ only when
  notable) · badge 2 (+ only when notable) · last-updated row
  (always / only when stale / off)
- **Nudge Gesture** — what a nudge does · what Single peek shows · nudge input
  (wrist flick / tap / either)
- **Peek Pages** — 6 Hours · Week Ahead · Conditions · Sun + Moon
- **Ambient Extras** — rain auto-peek · night mode · Quick View reflow
- **Units & Format** — imperial / metric · forecast time format · dew point
  instead of humidity
- **Location** — GPS, or pin a manual `lat,lon`
