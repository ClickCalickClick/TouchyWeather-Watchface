# Changelog

Newest release first. The top `## x.y.z` block is the single source of truth for the
face's version: `wscript`'s `generate_version_header` parses it into the gitignored
`src/c/version_gen.h`, whose `APP_VERSION_CODE` drives the show-once "New on the
horizon" card and whose bullets *are* the card's body.

Format contract — the parser depends on it:

- `## x.y.z` header, one `- ` bullet per line, newest block first.
- The top version must be **strictly greater** than the one below it. The build fails
  otherwise, so a non-increasing version can never ship.
- A block with no bullets is legal and means "silent release": the card is skipped but
  the version is still recorded.

Keep bullets **short and few**. The card cannot scroll — a watch face gets no buttons
and no touch — so everything must fit one screen, and the small classes are brutal:
measured at 1.3.0's copy, gabbro and emery show all three bullets, while chalk (180
round) and diorite (144x168) fit **one** and collapse the rest into "+2 more". A
~32-character bullet already wraps to two lines there. The build warns past 4 bullets
or 34 characters. Notes are also joined into a single C string literal capped at 600
bytes, where overflow truncates silently.

Put the most important change first — that is the order the drawer keeps when it has
to drop entries.

Bump `package.json`'s `version` to match the top entry when you add one.

## 1.4.0

- New: Clock size setting.
- Large clock grows the time.

## 1.3.1

- Settings now always stick.
- Rain alerts use less battery.
- Night mode holds through saves.

## 1.3.0

- Bigger type as slots switch off.
- Four slots: 2 lines, 2 badges.
- Watch battery is a complication.

## 1.2.0

- Face recentres as rows change.
- Rain and UV badges are free-form.
- New: dew point and rain chance.

## 1.1.0

- Nudge input: flick, tap, or either.
- A complication under the date.
