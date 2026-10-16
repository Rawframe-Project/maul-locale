# mloc-0043. Relative time: CLDR's fields filed per key, a decimal and a unit in

Status: Accepted

## Context

"Yesterday", "in 3 days", "2 hr. ago", "next Monday": CLDR's `fields`
give each locale, per unit and width, names of small offsets and future
and past patterns by plural category. Root aliases the narrow width to
the short one and the short to the long, so a width without its own
value takes the wider one's along the chain.

- **ICU4C's RelativeDateTimeFormatter** takes a double: numeric always
  writes the magnitude by its number format and picks the pattern by the
  plural category of the number as written; its auto mode takes a name
  for an offset within 1% of -2 to 2. It joins a relative day and a time
  with the atTime glue, and refuses parts when it capitalizes.
- **ECMA-402's Intl.RelativeTimeFormat** takes the units year to second
  with quarter and week, auto by the value's exact string, the plural of
  the value, parts with each number part's unit.
- **The units:** year, quarter, month, week, day, hour, minute, second,
  and the seven weekdays (ICU's), in three widths; offsets -2 and 2 named
  in 125 locales' days, 0 alone for hour and minute, "now" for second.

## Decision

- **The data:** a file of its own, `relative.mloc`, a unit per locale
  key: entries keyed by unit, width and slot (the names of -2 to 2, the
  future patterns zero to other, the past ones), each distinct text of a
  unit stored once. The lookup takes a width's slot from the first key
  along the locale's chain that files it, else the next wider width's
  (narrow, short, long), as root's aliases send them; an empty text ends
  it. The generator files a slot where that lookup would give another
  value than the one it should:
  - a name or other's pattern: CLDR's value resolved along the chain and
    through root's aliases;
  - a plural category's pattern: as ICU's data and fallback give it, the
    first width from the slot's to the long one where a file along the
    chain gives that category (a value, or the inheritance marker CLDR
    resolves, count fallback included); none when no width does, the
    formatter then taking other's. CLDR's own resolution would send a
    short "many" the locale lacks to long's "other" through its count
    fallback (ca's "d'aquí a 1.000.000 setmanes" where ICU writes
    "setm.").
  A pattern holds "{0}" at most once (ar's "خلال سنة واحدة", "in one
  year", writes no number) and a name none, which the validator checks
  with the layout; the fuzz target keeps every read of an accepted unit
  inside it. `relative.mloc` was 2,810,104 bytes filed width by width
  with each text per entry; it is 631,696 with the width lookup and
  shared texts. A titlecase unit holds the letters its texts begin with.
- **The formatter** (`mlocInitRelativeFormatter`, `mlocFormatRelative`,
  `maul-locale/relative.h`): a decimal and a unit in (ECMA-402's eight
  and the weekdays), never a clock. Negative amounts, -0 included, are
  past. The magnitude is written by the formatter's number options
  (decimal style, sign display auto) in the pattern of its direction and
  the plural category of the number as written, which ECMA-402's plural
  of the value agrees with under its defaults. Auto takes a name for an
  amount equal to -2 to 2 where the width chain has one, exactly as
  ECMA-402 (no 1% epsilon as ICU's; "1.0" is 1, -0 is 0). The patterns
  of a call are copied to the stack with the number marked and written
  by the number part's outer pattern, so parts come for free: the
  number's, and literal for the rest.
  - **Capitalization** at a sentence's start always, in a list or menu
    or standing alone by CLDR's `relative` context transforms (a usage
    the context unit now holds), the first letter titlecased in the
    copy, parts kept. ICU's RelativeDateTimeFormatter titlecases only
    at a sentence's start and refuses parts then.
- **Still to come:** a relative day joined with a time by CLDR's
  relative glue (research 41's sixth decision).

## Proof

- **Against ICU4C 79's RelativeDateTimeFormatter, once:** 1,060,020
  texts (906 locales, three widths, numeric always and auto, the 15
  units, amounts -2, -1, -0, 0, 1, 2, 3, 21, 100, 1.5, -1.5, 1,000,000
  and 0.5): all equal but 53, where ICU's data resolves an inheritance
  marker in the bundle that holds it, which a child then inherits, and
  through draft values, where CLDR resolves it from the locale asked for
  at the draft threshold, as the generator does: ur_IN's short and
  narrow weeks (ur's long pattern in ICU, ur_IN's in CLDR, 31), ckb's
  short and narrow years and fr_CA's short quarter (unconfirmed values,
  22).
- The tests check each kind of unit, width and mode against ICU's text,
  auto's cases without the epsilon, capitalization with parts, the
  refusals; the whitebox test refuses each field of a built unit
  spoiled and reads en's, es's and en_GB's names and patterns along
  their chains.

## Consequences

- A program that formats relative time adds one file to its data set;
  one that does not pays nothing.
- The lookup walks the locale's chain per slot, at most a few binary
  searches.
