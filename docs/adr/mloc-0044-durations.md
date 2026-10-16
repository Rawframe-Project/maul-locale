# mloc-0044. Durations: ECMA-402's algorithm on Temporal's fields, exact

Status: Accepted

## Context

"1 hr, 2 min, 3 sec", "1:02:03.456", "2 days and 3 hours": ECMA-402's
`Intl.DurationFormat` writes a Temporal duration's ten fields, each by
its own style (long, short, narrow, or numeric for the clock fields and
the fractions) and display (auto or always), the unit-style fields as
unit numbers joined by the locale's unit list, the numeric ones as one
item joined by the locale's separators. Every piece is already in the
library: the unit names (CLDR's `duration-*` units), the plural rules,
the number formatter and the unit lists. CLDR's `durationUnit` hm, ms
and hms patterns give the separator (":" in most locales, "." in da, fi
and five others) and whether hours are padded ("hh:mm:ss" in af, be,
lt and six others); ICU4X derives both from them.

- **The fields** are float64 integers of one sign, valid as Temporal's
  `IsValidDuration`; each is its exact value. The fractions after a
  field (milliseconds to nanoseconds in the numeric style) are added to
  it exactly and truncated to the fraction digits.
- **V8** (Node 24.22, ICU 78.3) writes a field past 2^53 by its shortest
  decimal, drops the list's text before a numeric item that follows a
  unit-style one ("1 hr, 2 min:3" where the specification and test262
  say "1 hr, 2 min, 3"), rounds an exact sum past 2^53 nanoseconds, and
  never pads hours.

## Decision

- **The formatter** (`mlocInitDurationFormatter`,
  `mlocGetDurationOptions`, `mlocFormatDuration`,
  `maul-locale/duration.h`): options as ECMA-402's, the zero value its
  defaults: a style (short, long, narrow, digital), each field's style
  and display with a default that applies `GetDurationUnitOptions`, and
  fixed fraction digits (0 to 9) or none; the numbering system is the
  locale's `nu`. Conflicts are refused at init as the constructor
  throws (`ValidateDurationUnitStyle`; a style a field does not take).
  The resolved options are given back with every default applied, a
  fraction as numeric.
- **The fields** are `mlocDurationValues`, the zone part's Temporal
  fields, checked by `mlocMakeDuration`; each field is read from its
  bits as an exact integer, and a field with its fractions is summed in
  128 bits (below 2^53 seconds in nanoseconds, times 10^9 at most) and
  written as a decimal of at most 26 digits: the specification's exact
  values, which test262's `precision-exact-mathematical-values` checks.
- **Writing** is `PartitionDurationFormatPattern`: the fields up to the
  first numeric one, a unit-style field shown by its display with the
  unit's name in the plural category of the number as written, the
  first shown carrying the sign (-0 for a field of 0 in a negative
  duration) and the rest none; the numeric fields as
  `FormatNumericUnits` chooses them (minutes shown between shown hours
  and seconds), without grouping, two digits for minutes and seconds
  after a numeric field and for hours where the locale pads them; the
  items joined by the unit list of the style (digital's short), with
  the list's contextual changes. An empty list is the empty text.
- **The digital format** is read at init from the locale's hms pattern:
  hours ("h" or "hh"), a separator, minutes, a separator, seconds; a
  pattern of another shape is a data error. The hour padding applies to
  numeric hours only, as the data's definition says (ECMA-402's step
  would pad long hours too).
- **Parts:** the number part kinds, the unit's name as
  `mloc_numberPartUnit`, plus `mloc_durationPartField` times the field
  plus one; the list's text and the separators carry no field, as
  `formatToParts` gives no `unit` for them.
- **Names:** the IXDTF duration writer becomes
  `mlocFormatTemporalDuration`, beside `mlocFormatTemporalDate` and the
  others, so `mlocFormatDuration` names the locale's formatter as
  `mlocFormatDate` and `mlocFormatRelative` do.

## Proof

- **Against ECMA-402 on ICU4C 79's data, once:** 209,430 texts (895
  locales Node resolves as asked, 13 option sets, 18 durations with
  zeros, negatives, fractions, large and exact-sum fields), by a program
  of the scratch that runs the specification's steps on ICU's number
  formatter, list formatter and `durationUnits`: all equal but 163, all
  sr-Cyrl-ME, where ICU's unit data falls back to the Latin script
  (CLDR's sr_Cyrl_ME inherits sr_Cyrl).
- **Against V8 (Node 24.22):** the same rows differ in 23,119: 12,272
  V8's separator after a unit-style item, 5,560 its exact sums, 924 its
  unpadded hours, the rest CLDR 48's data against 49's (as-IN's long
  second inheriting the short name, and others). The parts differ only
  where a sign holds a bidirectional mark, which the number part gives
  inside the sign as ICU does (1,844).
- `test_duration` writes test262's DurationFormat inputs (en, es: the
  styles, negative zero, fractions, truncation, exact sums, large clock
  fields, hours display auto) and locales of each separator, padding,
  script and list, checks the parts, the resolved options and every
  refusal; `fuzz_duration` makes formatters of any options and writes
  fields from their bits or as integers, a text the same twice, its
  measure the same length, its parts covering it.

## Consequences

- A program that formats durations adds the unit names file to its
  data set, as for units; the formatter is 4 kB, its patterns copied in.
- Rounding and balancing stay with the zone part: a host balances or
  rounds a duration there and writes the result here.
