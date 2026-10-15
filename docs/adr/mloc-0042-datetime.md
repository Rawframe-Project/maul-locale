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
- **Semantic skeletons map to classical ones as UTS #35's table maps
  them** (`tools/mlocgen/semantic.c`): the year, month and day letters
  of the length's `datetimeSkeleton`; a month alone as `LLLL`, `LLL` or
  `L`; the weekday `EEEE`, `EEE` or `EEE` (`EEEEE` alone and short);
  the hour `j` for the locale's own cycle (CLDR's test data; UTS #35's
  table says `C`), `h` for the 12-hour cycles and `H` for the 24-hour
  ones; minutes, seconds and fraction digits by the precision; the zone
  as `zzzz` or `vvvv` alone and not short, otherwise `z` or `v`,
  `VVVV` for the location and `O` for the offset. The year style
  "full" drops a two-digit year, "with era" adds `G`.
- **A date with a time is joined by the atTime glue**, as UTS #35 makes
  it the default, as ICU4C 79 and ICU4X do ("January 1 at 12:00:00 AM
  GMT"). CLDR's `datetime.json` expects the standard glue for its
  composites ("January 1, 12:00:00 AM GMT"); those cases are checked
  for their classical skeleton, and their text will be listed where
  it differs.
- **A chain names one element however it spells its defaults.** The
  generator's CLDR resolver looks a chain up both as written and
  without the attributes whose values are their DTD defaults, so
  root's alias to `dateTimeFormat[@type='standard']` finds the
  `dateTimeFormat` a locale writes without a type.
- **The dates data file, `dates.mloc`** (`src/date_tables.h`), which a
  program adds to its data set: a unit of no locale holds the compiled
  patterns (literal text and field items, quotes resolved, so the run
  time parses nothing), rows of a field set's patterns at the three
  lengths, and sets of a calendar's rows (the date field sets D, E,
  DE, MD, MDE and M, and YMD, YMDE, Y and YM in the three year styles)
  with its four standard date formats; a unit per locale key, filed
  where its chain would give others, names the set of each calendar,
  the ISO calendar taking the Gregorian one's, and its name set.
- **Calendar names in the same file:** a unit of no locale holds the
  names (months by context and width with the Hebrew leap year's Adar
  II, weekdays with the short width, quarters, eras in calendarData's
  order, cyclic years and leap month patterns of the Chinese and Dangi
  calendars), lists of them by kind and name sets, each name CLDR's
  value of its path (root's aliases followed from the locale); a name
  no path gives is empty. Day periods come with the times.
- **The standard formats are CLDR's, resolved per path**: ICU's data
  holds a calendar's time, date and glue patterns as one array, owned
  by the first locale giving any of them, and differs from CLDR's
  inheritance on 27 of 15,402 locale-calendars (nl_BE's Buddhist short
  date is nl_BE's, not nl's; the Chinese calendar's times are the
  locale's Gregorian ones).
- **Numbering overrides** (`numbers="hanidec"`, `"y=jpanyear"`) are
  read as CLDR's JSON gives them, the first along the chain and root's
  aliases through inheritance markers, and a length's override is
  carried to its semantic patterns' numeric year, month and day fields
  (a symbol-bound one to its symbol), as ICU4X carries it; a field it
  writes is stored with the numbering system's id.
- **The formatter** (`maul-locale/datetime.h`): `mlocInitDateFormatter`
  takes a field set (year, month and day by default), a length and a
  year style, or one of the four standard formats; the calendar is the
  locale's preference (-u-ca, else the region's first), a date of
  another calendar read in it, the ISO calendar by the Gregorian data;
  the digits are the locale's (-u-nu, else its default) and a pattern's
  override writes its fields by its system (hanidec's digits, or the
  spell-out part's hebr, jpanyear, hanidays and romanlow rules).
  `mlocFormatDate` writes the fields CLDR 49's date patterns hold (G,
  y, Y, r, U, M, L, d, E, c) with parts, refusing a pattern with any
  other when made.
- **Year style auto:** a year of a calendar counted from the Gregorian
  (Gregorian, Buddhist, ROC, Japanese) is written as its pattern writes
  it when its Gregorian year is from 1950 to 2049, as CLDR's datetime
  conformance data expects (ICU4X asks the Buddhist calendar for its
  century always); any other year takes the full year, and a year of an
  era before the calendar's main one, or a Gregorian year before 1000,
  its era too.
- **Where ICU4C and ICU4X differ, chosen one by one:** a Hebrew year of
  the current millennium in Hebrew letters leaves its thousands out
  ("תשפ״ז"), as ICU4C writes dates (ICU4X writes "ה׳תשפ״ז"); a year
  before the Hijra is a year of CLDR's "bh" era (ICU4C writes a negative
  year of AH), a Persian year before its epoch a negative year of AP;
  dates before 1582 are proleptic Gregorian, as Temporal and ICU4X have
  them (ICU4C's Gregorian calendar turns Julian).
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
- **CLDR's semantic cases:** `tools/check_datetime.py` maps each of
  the 240 semantic skeletons of `testData/datetime/datetime.json`
  (four locales, four calendars, every field set, length, year style,
  hour cycle and zone style it has) and compares the classical
  skeleton with the file's; 14 questions of its own cover the field
  sets UTS #35 does not define, the other precisions and hour cycles,
  and the year styles of a two-digit year. All agree.
- **The dates data file**, once: the standard date formats of all
  15,402 locale-calendars ICU4C 79 ships compared with its
  `DateFormat` patterns: all equal but the 27 above. The numbering
  overrides of the 11,169 locale-calendars of CLDR's JSON: all equal
  (24 with overrides). CLDR 49 gives 2,125 distinct patterns (34,630
  bytes compiled), 2,486 rows and 682 sets, and 16,359 distinct names
  (178,791 bytes) in 1,696 lists and 3,783 name sets; the file is
  583,770 bytes. `test_date_data` reads its patterns (en, en_GB, the
  ISO calendar, ja's Japanese year by jpanyear) and names (ru's two
  Januaries, Adar II, ja's Reiwa, the Chinese cycle's first year, zh's
  leap pattern) and spoils each unit field by field; `fuzz_date_data`
  checks that an accepted unit of either kind reads only inside
  itself.
- **The formatter against CLDR's datetime.json:** its 96 date cases
  (YMDE and M at every length and year style, the standard date formats,
  en, ja-JP, th-TH and ar-SA in their four calendars) give the file's
  text, on every push (`tools/check_datetime.py` with
  `conform_datetime`).
- **Against ICU4C 79's DateFormat, once:** the four standard date
  formats of its 906 locales in 16 calendars (islamic-rgsa left out) on
  14 dates from 1700 to 2033 (an era's change, Adar I and Adar II, a
  Chinese leap month): 811,776 dates, all equal but 33,103: 32,621 are
  the astronomical Islamic calendar's days (mloc-0040), the rest the
  standard formats' per-path inheritance above (nl_BE, ms_BN, es_HN,
  zh_Hans_HK and its siblings), ckb_IR's Persian months, and gl's
  Buddhist and ROC patterns, whose week-based year ICU4C counts in
  Gregorian years.
- **CLDR's own TSVs** agree with ICU on 18,812 of their 21,182 lines;
  where they differ from both (minutes kept at the requested length,
  E kept short, older data in some locales) the generator follows ICU,
  whose rules UTS #35 describes.

## Consequences

- Formatting a value never parses a pattern or a skeleton; the
  heuristics run once, in the generator, and their output is checked.
- The matcher is ICU's, so a pattern that differs from ICU's is either
  listed with its reason or a fault the check catches.
