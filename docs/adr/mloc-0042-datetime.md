# mloc-0042. Dates and times formatted: semantic skeletons, patterns compiled by the generator

Status: Accepted

## Context

The `datetime` part formats the values the `calendar` (mloc-0040) and
`zone` (mloc-0041) parts make. CLDR gives each locale and calendar its
standard date and time lengths, the glue that joins a date and a time,
a few hundred `availableFormats` (skeleton to pattern), interval
patterns, names, and appendItems for fields no pattern holds.

UTS #35 finds the pattern for a request by "Matching Skeletons": a
distance between the requested fields and each available skeleton,
field lengths adjusted to the request, missing fields appended, and a
date and a time glued by the request's month width. ICU's
DateTimePatternGenerator runs it at run time for each formatter. ICU4X
runs it while building its data and ships, per locale, calendar and
semantic field set, a pattern and its variants, so its formatter only
selects.

UTS #35's semantic skeletons (a technical preview since CLDR 46) are a
closed set of field sets (date, calendar period, time, zone, and their
composites) with a few options (length, alignment, year style, hour
cycle, time precision, zone style).

## Decision

- **The API takes meaning, never letters:** semantic skeletons and the
  four standard lengths. Pattern strings and classical skeleton strings
  are not accepted at run time.
- **The generator matches.** `tools/mlocgen/skeleton.c` follows ICU's
  DateTimePatternGenerator step by step: ICU's field table and
  distances, the pattern map in ICU's order (canonical single fields,
  then the standard time and date formats, then `availableFormats`
  from the locale up its parents and through root's calendar aliases,
  each skeleton the first time it is met), ties to the larger missing
  mask, field lengths adjusted as ICU adjusts them (hours, minutes and
  seconds keep the pattern's length, numeric and text never swap),
  `j`, `J` and `C` from the region's `timeData`, fractions appended
  after the decimal separator, missing fields by appendItems, and a
  date and a time joined by the calendar's atTime glue chosen by the
  month's width.
- **Where ICU's data build and CLDR's inheritance part, the generator
  follows CLDR** (UTS #35: root's aliases are resolved from the locale
  asked for, and "↑↑↑" inherits along that path):
  - a calendar other than the Gregorian has its own atTime glue,
    through root's alias to its standard glue; ICU's data has none for
    those calendars and takes the Gregorian glue;
  - the Chinese and Dangi calendars take the locale's Gregorian time
    formats through root's alias; ICU takes root's own;
  - a locale that inherits its glue ("↑↑↑") inherits it chain by chain
    (fr_ML's own standard glue); ICU inherits its parent's whole array;
  - a pattern is read whole: ICU's pattern reader keeps 50 tokens and
    drops the fields after them (tok's long date loses its day);
  - an item CLDR gives per plural count ("MMMMW", "yw") takes its
    count "other" form; ICU returns no pattern for it.
- **A chain names one element however it spells its defaults.** The
  generator's CLDR resolver looks a chain up both as written and
  without the attributes whose values are their DTD defaults, so
  root's alias to `dateTimeFormat[@type='standard']` finds the
  `dateTimeFormat` a locale writes without a type.
- **Still to come in this part** (research 39's slices): the data units
  of compiled patterns per locale and calendar; dates in all 18
  calendars; times, hour cycles and day periods; time zone names by
  metazone; intervals; relative time; durations; speed.

## Proof

- **Against ICU4C 79** (CLDR 49.0), on every push:
  `tools/check_skeletons.py` asks `mlocgen skeletons` 26,970
  questions and compares the answers with ICU's, kept in
  `test/skeletons_icu.tsv`: the 21,182 of CLDR's
  `testData/datetime/skeletons*.tsv` (113 locales, 16 calendars, 132
  skeletons), and every field letter at each length it takes, alone
  and with others, with the special cases (minutes and fractions
  without seconds, the hour metacharacters, every zone form), in
  eight locales of the Gregorian calendar. All agree; 347 agree with
  `tools/skeleton-exceptions.txt`, which lists, with mlocgen's
  pattern, the answers where CLDR's inheritance and ICU's data part
  (the two non-Gregorian causes above) and the plural items. An
  exception that stops differing from ICU fails the check.
- **Every locale ICU ships, once:** 141,336 questions (906 locales,
  the Gregorian calendar, 156 skeletons: CLDR's and those the semantic
  field sets ask for). All agree but 14, the fr_ML glue and tok's
  long date above.
- **CLDR's own TSVs** agree with ICU on 18,812 of their 21,182 lines;
  where they differ from both (minutes kept at the requested length,
  E kept short, older data in some locales) the generator follows ICU,
  whose rules UTS #35 describes.

## Consequences

- Formatting a value never parses a pattern or a skeleton; the
  heuristics run once, in the generator, and their output is checked.
- The matcher is ICU's, so a pattern that differs from ICU's is either
  listed with its reason or a fault the check catches.
