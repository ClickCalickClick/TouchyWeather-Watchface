# Round C: the "Large clock" Clay setting

**Status: DONE, shipped in 1.4.0 (2026-08-13).** Built as designed — the
solver was never forced, only made cheaper to satisfy. Verified on all six
platforms; clock ink height grew on every one, so open question 1 below
("is it a no-op on chalk and basalt?") is answered **no**:

| Platform | Clock ink, Balanced → Large |
|---|---|
| gabbro | 55 → 74 px |
| chalk | 28 → 41 px |
| emery | 29 → 39 px |
| basalt | 23 → 37 px |
| diorite / flint | 25 → 37 px |

Default stays `CLOCK_EMPHASIS_BALANCED`, where `face_weather_tier()` returns
its argument and the headroom is the original constant — so the default path
reduces to the previous code exactly.

Original plan, kept for the reasoning:

**Status:** designed, approved, NOT started. 2026-08-07.

From the r/pebble TouchyWeather 2.0 thread, u/wickedest-witch:

> I'd also appreciate being able to have a larger clock relative to the
> temperature because that's usually the information that is most important at
> a glance, but that's all personal preference.

Round C originally had a second half — light-theme accent contrast — which was
**tabled** before any code. See `ACCENT_CONTRAST_TABLED.md` in the app repo.

## Constraints set by the user

1. **It must be a Clay setting.**
2. **It must NOT change the default.** Today's balanced layout stays default.
3. The option **may need to be unique per device**, and finding fonts that work
   well **will take experimentation**. (Confirmed below — this is right.)

## Why this is mostly already built

The face has a two-tier ramp — `FACE_TIER_BASE` / `FACE_TIER_PROMOTED`
(`face_fonts.h`) — and a solver that already promotes the clock when the user
switches rows off. The probe is `clock_zone.c` ~795-830:

```c
int tier = FACE_TIER_BASE;
prv_measure(&m, FACE_TIER_PROMOTED, W);
prv_fill_rows(rows, &m, has_comps, prv_badges_would_show_fresh(),
              prv_status_row_settled());
if (face_layout_required_h(rows) + FZ_PROMOTE_HEADROOM <= avail_h &&
    m.cluster_w <= face_layout_band_w(...)) {
  tier = FACE_TIER_PROMOTED;
}
```

**The reason the time doesn't dominate is `face_fonts.c:168-169`: on the large
classes the weather-row TEMP is `face_font_hero()` — LECO_42, the same size as
the base clock.** The clock is not small; the temperature is equally large.

## The design: make the promoted tier cheaper, never force it

Do **not** have the setting force `FACE_TIER_PROMOTED`. Instead, when emphasis
is on, drop `face_font_temp_tier(FACE_TIER_PROMOTED)` (and probably
`face_font_hilo_tier`) one step. `m.weather_h` shrinks →
`face_layout_required_h(rows)` falls → the **existing** fit test starts passing
on stacks where it previously did not.

The clock grows because the weather row got out of its way, which is literally
what was asked for. Three reasons this framing matters:

- The solver stays authoritative, so the setting can **never** produce a clipped
  clock. Worst case it is a no-op.
- The file's own comments record a fight with a promote-then-demote **flash on
  every launch** when the tier was sized against a transient stack. Forcing a
  tier reopens that class of bug.
- It composes with the existing "turn rows off to grow the clock" behaviour
  instead of competing with it.

## What experimentation will actually be needed

The XL clock font is **already per-platform**, which confirms constraint 3:

| Resource | Size | Platforms |
|---|---|---|
| `FONT_CLOCK_XL_48` | 48 | chalk |
| `FONT_CLOCK_XL_52` | 52 | basalt, diorite, flint |
| `FONT_CLOCK_XL_56` | 56 | emery |
| `FONT_CLOCK_XL_64` | 64 | gabbro |

All `fonts/ChakraPetch-Bold.ttf`, `characterRegex: "[0-9:]"`.

Consequences to plan around:

- **The XL face carries digits and a colon ONLY.** No degree sign, no minus, no
  AM/PM. It can never be reused for a temperature without widening the regex,
  which costs resource weight on every platform. `face_fonts.h` already warns
  about exactly this.
- If "Large clock" needs a step *beyond* today's XL, that is **four more
  resource entries**, one per size class — and the sizes will need tuning per
  device, not scaling by a ratio.
- The demoted weather temp needs a font that **does** carry `°` and a minus.
  `BITHAM_30_BLACK` is the house substitution for this (it is what
  `ui_font_title()` uses in Big Mode in the app repo, and what chalk's promoted
  temp already falls back to at `face_fonts.c:163`).

## Open questions to settle by measuring, not by arithmetic

1. **Is it a no-op on chalk and basalt?** Both rest at LECO_36 because the
   six-row stack does not fit LECO_42 at all. Emphasis may buy nothing there
   unless the hi/lo pair is demoted too. Acceptable if so — but then the Clay
   item arguably should be hidden or relabelled on those platforms, using the
   `activeWatchInfo.platform` branch already present in `clayCustomFn`.
2. **Heap.** `face_font_clock_xl()` is lazy-loaded on first promotion and lives
   for the rest of the process. Making promotion common raises steady-state
   heap. Measure headroom before shipping; the app is already tight
   (basalt reported ~4KB free heap).
3. **Round-class width.** The probe also tests `m.cluster_w` against
   `face_layout_band_w()`. On chalk/gabbro a wider XL numeral can fail the chord
   test even when the height fits — the existing code already steps back down
   for this, so verify it still does rather than assuming.

## Clay shape

`ClockEmphasis`, radiogroup, default "Balanced":

- `"0"` Balanced (default) — today's behaviour exactly
- `"1"` Large clock — weather row demoted, clock promoted when it fits

Radiogroup rather than a toggle so a third step can be added without a config
migration. Follow the `BigMode` wiring in the app repo as the reference pattern:
messageKey → `comm.c` handler → `settings_set_*` → persist key → redraw.

## Verification

- Default must stay **pixel-identical** on all six platforms. Prove it the way
  round B did: capture with the setting off, before and after the change, and
  diff. Round B got 14/15 byte-equal with the 15th being a rotating text phrase.
- Then capture with it on, per platform, and check the clock actually grew and
  nothing clipped.
- `pebble clean && pebble build`, then `python3 tools/lock_guard.py`. This will
  fail and needs a **deliberate** re-baseline — emery and gabbro both render the
  face. Defaults unchanged means default *rendering* is identical even though
  the binary moves.
- Scaffolding for the capture walk (double install, sleep 18, screenshot every
  step, md5 distinctness in Python) is in the app repo at
  `tools/capture_walk.py`. The walk itself is app-specific; the watchface needs
  its own, but do not re-derive the traps in its header.
