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
  no path gives is empty; day period names likewise, by context, width
  and CLDR's twelve periods.
- **Times in the same file:** per locale and calendar a time set holds
  the time field set matched at the hour, minute and second on the
  12-hour (`h`) and 24-hour (`H`) clocks (the `j` pattern is always one
  of the two), the four standard time formats and the standard and
  atTime glue of each length, as CLDR resolves them under the calendar
  (am's Gregorian minutes are its own "a h:mm", its Chinese ones root's
  "h:mm a", as ICU4C reads them; ICU4X takes the generic calendar's for
  every calendar). Fractional seconds are not stored: the run time
  writes them after the seconds with the locale's decimal separator, as
  ICU4C writes them from its numbering system's symbols. Glue holds
  only literal text, `{0}` and `{1}`, which the generator checks.
- **Day period rules** (`dayPeriods.xml`'s format rules) are stored per
  locale as each hour's period and whether noon and midnight are named
  at their instant, looked up by cutting the id's last subtag until a
  rule list is found, as ICU's DayPeriodRules does (zh_Hant takes zh's,
  where CLDR's parent locales would give root's), root's at the end.
- **The zone names data file, `zone-names.mloc`**
  (`src/zone_name_tables.h`), which a program adds for zone names: a
  unit of no locale holds each BCP 47 id's region (TR35's first two
  letters of a five-letter id, or its region attribute), whether the
  location format names the region (its only zone, or a primary zone),
  and its metazone periods in minutes since 1970 with the offsets CLDR
  gives some, and each metazone's golden zone and preferred zones by
  region; a unit per locale key holds its zone formats (the hour
  format's halves apart), Etc/Unknown's city and each zone's and
  metazone's names, filed where the chain would give others, as the
  names are, "∅∅∅" as an empty name that ends the chain. Root's unit
  holds the city each identifier implies (its last part, '_' as
  spaces), as ICU4C derives it, so the run time derives nothing. Both
  units index the zone codes unit's ids, which one CLDR release gives
  both; a zones file of another tzdb release reaches them through its
  identifiers' names.
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
- **The time formatter** (`mlocInitTimeFormatter`, `mlocFormatTime` on
  `mlocTime`): an hour cycle and a precision, or a standard format.
  - **Hour cycles** as UTS #35's semantic options: Clock12 and Clock24
    take the locale's pattern of that clock as matched; H11 to H24 take
    it with the hour letter replaced by K, h, H or k, its width kept
    (en's "HH:mm" under h24 is "kk:mm"). Auto takes -u-hc's h11 to h24
    exactly, else the clock of the preferences' cycle (-u-hc's c12 and
    c24, -u-rg, the region subtag, the likely region). ICU4C answers h24
    by matching a `k` skeleton, which can find another item than the
    24-hour pattern (fur's "kk:mm" where its `Hm` is "H:mm"); UTS #35's
    substitution is kept, a known difference.
  - **Precision:** hour, minute, minute unless it is zero (then the
    hour's pattern), second, or 1 to 9 fraction digits written after the
    seconds with the locale's decimal separator (its numbering
    system's, fa's "٫"), truncated as ICU4C truncates.
  - **Standard formats:** the medium and short ones; full and long name
    a zone, which a time of day lacks, and are refused (the zoned
    formatter writes them). As ICU4C builds them, a cycle asked for (the
    option, -u-hc) or a region (-u-rg), or a region whose clock is not
    the one the locale's data was written for (de-US: de's 24-hour
    data, the US's 12-hour clock), takes the clock's minutes or seconds
    matched as `hmm`, `hmmss`, `Hmm` or `Hmmss` without the standard
    formats (ICU's createInstanceNoStdPat: am's "a h:mm", seh's
    "HH:mm"). A time set records whether the locale's own hour (j) is
    on the 12-hour clock for this. ICU also rematches when only its
    valid locale differs with the same clock (fr-DE); the stored format
    stays there.
  - **Day periods** as ICU4C writes them: `a` by the half of the day;
    `b` as noon at 12:00 exactly as the pattern shows it; `B` by the
    rules, noon at its instant where the rules and names have it,
    midnight never (ICU leaves it out), the hour's period otherwise, and
    am or pm where the period is one of them or has no name. Names are
    format names: abbreviated up to three letters, wide at four, narrow
    at five.
  - **Known differences with CLDR's datetime.json:** th's abbreviated am
    and pm are "↑↑↑" in CLDR 49 (root's "AM" and "PM", as the JSON
    resolves them and ICU4C writes them); the file expects th's wide
    names. Its nine Thai Clock12 cases are listed in
    `tools/check_datetime.py`.
- **The date-time formatter** (`mlocInitDateTimeFormatter`,
  `mlocFormatDateTime` on `mlocDateTime`): a date field set showing a
  day or a weekday, or a standard date format, with a time's options.
  - **Composed as ICU4C composes a skeleton it has no single item for:**
    the date's pattern, the calendar's glue, the time's pattern, each
    written as when alone. The glue's style comes from the field set's
    month in its length's datetimeSkeleton (four letters with a weekday
    full, four letters long, three medium, else short), a standard date
    format's from its length; the time set records each length's month
    for this. atTime by default, the standard glue when asked
    (`mloc_dateTimeGlueStandard`, CLDR's dateTimeFormatType).
  - **A weekday with a time** is the one composite CLDR 49's items cover
    whole (Eh, Ehm, Ehms, EHm, EHms and their B forms): matched as such
    at the time's clock, the weekday's width and the precision, and
    stored in the time set ("Thursday 4:03 PM", ja's "16:03 (木曜日)").
  - Calendar periods take no time and a standard format on one side
    only is refused.
- **Still to come in this part** (research 39's slices): time zone
  names by metazone; intervals; relative time; durations; speed.

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
- **The time formatter against CLDR's datetime.json:** its 72 time
  cases (the time field set under Clock12, Clock24 and the locale's own
  cycle at three lengths, the standard short time) give the file's text
  but the nine Thai cases listed above, on every push.
- **Against ICU4C 79, once:** 761,040 times (the 906 locales ICU ships,
  7 calendars, 8 times from midnight to 23:59:59.999, 15 options: each
  precision under the locale's cycle, both clocks, h11, h12, h23 and
  h24, the short and medium formats with and without -u-hc). All equal
  but 15,745: 315 are h24's minutes, where ICU matches a `k` skeleton;
  15,430 are the standard formats of non-Gregorian calendars in 992
  locale-calendars, where CLDR's per-path inheritance gives the
  locale's own Gregorian times and ICU's array ownership a parent's or
  root's (CLDR's JSON agrees with the generator on all 13,633 of them
  it has a file for). The time patterns add 314 compiled patterns and
  399 time sets; day periods 257 name lists and 65 rule sets; the file
  is 701,316 bytes.
- **The date-time formatter against datetime.json:** its 24 standard
  date-time cases without a zone (full date, short time, either glue)
  give the file's text, on every push.
- **Against ICU4C 79's skeleton composites, once:** 684,936 date-times
  (the 902 locales ICU and CLDR share, 4 calendars, the 7 field sets
  that take a time at 3 lengths, 3 clocks, 3 precisions). All equal but
  87,654. In the Gregorian calendar 195: fr_ML's glue, tok's long date
  and es_HN's per-path pattern above, and haw's month in Roman numerals
  (the numbering override ICU4X carries to semantic patterns and ICU's
  matcher drops). In the other calendars 87,459: 81,140 are the glue
  alone (ICU's data takes the Gregorian glue; CLDR gives each calendar
  its own, above), confirmed by gluing the formatter's own date and
  time with the Gregorian glue; the rest are the per-path inheritance
  of the calendars' items (the Chinese calendar's EHm, which ICU's
  data lacks) and the Chinese calendar's numbering overrides.
- **CLDR's own TSVs** agree with ICU on 18,812 of their 21,182 lines;
  where they differ from both (minutes kept at the requested length,
  E kept short, older data in some locales) the generator follows ICU,
  whose rules UTS #35 describes.
- **The zone names data file:** 468 BCP 47 ids (451 live), 636
  metazone periods of 419 zones over 155 metazones, 155 golden and 128
  preferred zones, 11 primary zones; 296 locale units, 2,836,588 bytes
  of names, the file 2,858,565 bytes. The validators refuse each field
  spoiled in a whitebox test, and the fuzz target keeps every read of
  an accepted unit inside it.

## Consequences

- Formatting a value never parses a pattern or a skeleton; the
  heuristics run once, in the generator, and their output is checked.
- The matcher is ICU's, so a pattern that differs from ICU's is either
  listed with its reason or a fault the check catches.
