# Cross-platform presentation review — TouchyWeather Face

_My (Claude's) first-pass notes. Add yours below the divider and we'll combine._

## How these were captured

To compare **layout/typography** and not **content**, every screenshot renders a
**byte-identical baked dataset + config** (live weather/PKJS neutered, mock data
forced). Same on all platforms:

- Time `09:xx` · `FRI JUL 24`
- Complications: **FEELS 75°** (line 1) · **12 NW** (line 2)
- Weather: ☀ **72°** · ↑78° ↓58°
- Badges: **RAIN 80%** · **UV 4**
- **UPDATED 5M AGO** + battery glyph (BatteryDisplay forced ON)

Two throwaway builds were used (`settings_get_big_mode()` forced false/true) since
`send-app-message` doesn't deliver reliably in this env. See `_contact_sheet.png`
(all pairs) and `_clock_strip.png` (clock typefaces).

## Coverage

| Platform | Class | Color | Regular | Big |
|---|---|---|---|---|
| basalt  | SMALL_RECT | color | ✅ | ⚠️ emulator wouldn't apply the Big build (behaves like **diorite**, same class) |
| chalk   | SMALL_ROUND | color | ❌ firmware never boots past logo in this env | ❌ |
| diorite | SMALL_RECT | B&W | ✅ | ✅ |
| flint   | SMALL_RECT | B&W | ✅ | ✅ |
| emery   | LARGE_RECT | color | ✅ | ✅ |
| gabbro  | LARGE_ROUND | color | ✅ | ✅ |

> **chalk** and **basalt-big** are emulator/environment gaps, not layout results —
> both older-arch qemu machines refused to boot/re-install here after many retries.
> Worth re-running later; chalk (the only SMALL_ROUND) is the real hole.

---

## What's consistent / working well

- **Row structure is uniform** across every class: TIME · DATE · COMPS · WEATHER ·
  BADGES · UPDATED. The full 6-row stack **fits without shedding** on every captured
  class, incl. 144-px B&W — matches the design intent.
- **Date + secondary rows** use the GOTHIC family everywhere — consistent.
- **Vertical centering** reads balanced on emery/gabbro; small-rect fills more of the
  height but doesn't look top-heavy.
- Round chord-clamping on gabbro looks clean (nothing clipped at the circle edge).

## Normalization issues found

### 1. The clock is THREE different typefaces depending on screen × mode  ← biggest
See `_clock_strip.png`.

| | Regular | Big Mode |
|---|---|---|
| **Large** (emery/gabbro) | **Custom XL numeral** (Chakra Petch — the signature font) | **BITHAM_42_BOLD** |
| **Small** (basalt/diorite/flint) | **LECO_42** (thin) | **BITHAM_42_BOLD** |

- The **signature custom clock font only appears on 2 of 6 platforms** (large,
  regular). A Pebble Time (basalt) or any B&W user **never sees it**, and it
  **disappears the moment Big Mode is on**.
- Heights are ~equal across modes, so Big Mode doesn't shrink the clock — but it
  swaps the identity from the distinctive numeral to generic bold BITHAM.
- **Direction:** pick one clock identity. Either promote the custom XL on *all*
  classes and *both* modes (it's digits+colon only, so it fits the clock's needs),
  or drop it and standardize. Right now the custom font is the exception, not the rule.

### 2. Big Mode delivers uneven value per class
- **Large color (emery/gabbro):** clear win — secondary text collapses gray→black,
  accents darken for contrast, weather grows. Genuinely more legible.
- **Small B&W (diorite/flint):** only the clock weight (LECO→BITHAM) + a reflow +
  bigger hi/lo. Accents can't darken (already fg in B&W), so the contrast benefit is
  mostly absent.
- **Small color (basalt):** should get the accent-darkening too (unverified here).
- **Direction:** define what Big Mode guarantees on *every* class, so toggling it is
  never a near-no-op.

### 3. Regular-mode secondary text is faint on color screens
`FEELS 75°` / `12 NW` render in gray (theme_secondary = DarkGray) on emery/gabbro —
noticeably lower-contrast than everything around them. Big Mode fixes this by forcing
black; consider a darker secondary **by default**, not only in Big Mode.

### 4. B&W badge pills read as heavy black lozenges
On diorite/flint the RAIN/UV pills are solid-black fills with light text; two side by
side look blocky. On color they're clean tinted pills. **Direction:** review an
outlined/hairline pill treatment for the 1-bit platforms.

### 5. Battery glyph placement differs by class
Top-**center** (reserved band) on emery/gabbro vs top-**right corner** on the small
classes. Intentional per the layout code, but worth a deliberate decision on whether
placement should be consistent.

### 6. Verify the sunny icon between diorite & flint
Same COND_SUNNY renders slightly differently (diorite = filled disc + rays; flint =
spikier asterisk). Same class/color depth, so this may be a glyph/scale quirk to check.

---

## YOUR NOTES (add below)

-
