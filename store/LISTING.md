# Store listing copy — TouchyWeather Face

Paste-ready text for the appstore entry. See [FEATURES.md](FEATURES.md) for the
full feature breakdown and the comparison against the TouchyWeather app.

---

## Title

```
TouchyWeather Face
```

## Category

Watchface · Weather

## Short blurb (one line)

```
A big, clean clock with your weather under it — and the whole forecast one flick of the wrist away.
```

Alternatives:

```
The clock that grows to fit. Pick your readings; nudge for the forecast.
```

```
Weather watch face with four complication slots and a forecast deck you nudge open.
```

## Description

```
TouchyWeather Face is a weather watch face that gets out of your way.

The layout isn't a fixed template — it's measured. Time, date, your readings,
the weather, badges and the last-updated stamp are measured, stacked and
centred every time it draws. Switch a row off and the rest don't just shuffle
up — the whole face re-centres around what's left. Nothing sits out of flow, so
there's no leftover chrome band making the face look top-heavy.

FOUR SLOTS, YOUR CHOICE
Two text lines under the date and two coloured pills below the weather. Any
slot takes any reading: feels like, wind, humidity, dew point, UV, air quality,
rain chance, step count, or your watch's battery. Two readings on one line draw
as a single group split by a hairline rule, each keeping its own accent colour.

Give any badge the "only when notable" filter and it stays hidden until it's
worth reading — rain over 50%, UV 6 or higher at midday, battery at 20% or
charging.

NUDGE FOR MORE
A watch face gets no buttons and no touch, so this one listens to your wrist.
Nudge the watch — a flick, or a firm tap on the glass — and the lower half
deals a peek card: 6 Hours, Week Ahead, Conditions, Sun + Moon. It returns to
the clock on its own after a few seconds, and the deck resumes where you left
it instead of starting over.

Prefer something else? Set a nudge to open one fixed view, let the pages
auto-rotate on a timer, or turn the whole thing off and keep just the clock.

IT WATCHES THE SKY FOR YOU
Rain due within the hour flashes the hourly page up by itself, and the bottom
row takes over with the countdown. After sunset, night mode drops the face to
dark and swaps the sun for tonight's moon phase, then restores your daytime
theme at sunrise.

BUILT TO SIP BATTERY
At rest it wakes once a minute and no more. Animations run only in a short
window after a nudge, then settle to a static frame.

Settings live on your phone, with a live preview of the face that reshapes as
you change it.

Works on Pebble Time, Time Round, Pebble 2, Pebble 2 Duo, Pebble Time 2 and
Round 2. Weather from Open-Meteo — no account, no API key.

Companion to the TouchyWeather app, if you want radar, charts and a weather
app with opinions.
```

## Keywords

```
weather, watchface, forecast, complications, clock, rain, UV, air quality,
battery, minimal, customizable
```

## Screenshot order

Upload in this order — it tells the story from "what it looks like" to "what it
hides":

1. `02-face-loaded` — the face, fully loaded
2. `08-peek-hours` — nudge for the forecast
3. `07-night-mode` — night mode, moon phase and all
4. `12-overlay` — everything at once
5. `03-face-minimal` — strip it back and the face re-centres

Native-resolution PNGs for every platform are in
[screenshots/](screenshots/).

> **RESOLVED 2026-08-01 — two growth claims removed from the description.**
>
> The old copy said "switch a row off and the type grows" and "strip it right
> back and the clock promotes to a big custom numeral face". Measured against
> the shipped v1.3.0 screenshots, **neither is observable**:
>
> - Clock ink height is byte-identical across `01-face-default`,
>   `02-face-loaded` and `03-face-minimal` on **all six platforms**
>   (emery 29px, basalt/diorite/flint 25px, chalk 76, gabbro 113 in all three).
> - The big temperature reading is the same height in loaded vs minimal too
>   (emery 29/29, basalt 20/20, diorite 38/39).
>
> `03-face-minimal` already has both complication slots, both badges *and* the
> status row switched off, so it is the most stripped-back face the settings can
> reach — `FZ_PROMOTE_HEADROOM` blocks promotion while the time, date and
> weather rows remain, and nothing removes those.
>
> What *is* real and visible is the **re-centring**, which is what the
> description now claims. If the promotion is ever made reachable, the sentence
> can come back — but it must not ship as a claim a reviewer can check and
> disprove in thirty seconds. Same correction is owed to
> [FEATURES.md](FEATURES.md) and [README.md](README.md), which still say the
> type grows a tier.
