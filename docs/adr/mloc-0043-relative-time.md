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
  future patterns zero to other, the past ones), CLDR's values resolved
  along the chain and through root's aliases in the generator, filed
  where the library's lookup would give another, an empty text where
  CLDR has none but the lookup would. The lookup takes a width's slot
  from the first key along the chain that files it, else the next wider
  width's (narrow, short, long), as root's aliases send them; a
  category's pattern equal to other's is left out, the formatter taking
  other's, as ICU does. Each distinct text of a unit is stored once.
  `relative.mloc` is 2,810,104 bytes filed width by width with each text
  per entry, and 652,792 with the width lookup and shared texts. A
  pattern holds "{0}" at most once (ar's "خلال سنة واحدة", "in one
  year", writes no number) and a name none, which the validator checks
  with the layout; the fuzz target keeps every read of an accepted unit
  inside it.
- **The formatter** (research 41's decisions, built in the next slice):
  a decimal and a unit in, never a clock; width, numeric always or auto,
  capitalization, the number's options; auto exactly as ECMA-402 (the
  value equal to a named offset; no epsilon); the pattern by the plural
  of the number as written; parts kept when capitalized; a relative day
  and a time joined by CLDR's relative glue.

## Proof

- The whitebox test refuses each field of a built unit spoiled and reads
  en's, es's and en_GB's names and patterns along their chains as CLDR
  gives them.

## Consequences

- A program that formats relative time adds one file to its data set;
  one that does not pays nothing.
- The lookup walks the locale's chain per slot, at most a few binary
  searches.
