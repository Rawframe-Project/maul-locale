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
  composites ("January 1, 12:00:00 AM GMT"); the check asks those
  cases for the standard glue (`mloc_dateTimeGlueStandard`), which
  gives the file's text.
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
  and its metazone periods in seconds since 1970 (one of CLDR's starts
  falls at 00:44:30) with the offsets CLDR gives some, and each metazone's golden zone and preferred zones by
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
  - **Standard formats:** all four; full and long name a zone, which a
    time of day lacks, so a formatter of them writes a time in a zone
    only (below). As ICU4C builds them, a cycle asked for (the
    option, -u-hc) or a region (-u-rg), or a region whose clock is not
    the one the locale's data was written for (de-US: de's 24-hour
    data, the US's 12-hour clock), takes the clock's minutes or seconds
    matched as `hmm`, `hmmss`, `Hmm` or `Hmmss` (full and long:
    `hmmsszzzz`, `hmmssz`, `Hmmsszzzz`, `Hmmssz`) without the standard
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
- **The zone formatter** (`mlocInitZoneFormatter`, `mlocFormatZone`):
  a zone at an instant in one of eight styles, ICU4X's zone field sets
  (specific and generic long and short, location, city, offset long and
  short), each falling back as ICU4C's TimeZoneFormat and
  TimeZoneGenericNames do:
  - a specific name is the zone's own of its time at the instant, else
    its metazone's; else the offset;
  - a generic name is the zone's own, else, in standard time with no
    daylight time within 184 days either way, the standard name (unless
    it is the generic one), else the metazone's generic name, qualified
    by the fallback format where the metazone's preferred zone for the
    locale's region (its likely one, else 001) shows the same wall time
    with another standard offset or saving: by the zone's country where
    it is the metazone's preferred zone there, else by its city; else
    the location; else the offset;
  - the location is the region format around the country for a
    country's only or primary zone, else around the city; a zone of no
    region falls back to the offset; the city falls back to
    Etc/Unknown's;
  - the offset is gmtFormat around the hour format's half, two-digit
    hours long, seconds where they are not zero, minutes in the short
    form only where they are not zero, in the locale's digits;
  - the reference zone's offset at a wall time is the later where the
    time repeats and the earlier where it is skipped, as ICU4C reads
    one;
  - where CLDR gives a metazone period standard and daylight offsets,
    the zone's offset is read by them (Punta Arenas's permanent -03 is
    Chile Summer Time, Winnipeg's under tzdb 2026e Central Daylight
    Time), as ICU4C writes them.
- **Dates and times in a zone** (`mlocFormatDateInZone`,
  `mlocFormatTimeInZone`, `mlocFormatDateTimeInZone`): an instant's
  local date-time in the zone, in the formatter's calendar, written with
  the zone by the zone formatter above. A zone style in the time
  options (with a precision), or in the date options of a date alone
  (a field set up to the weekday), or a full or long time format, makes
  a formatter that writes through these functions only; the plain ones
  refuse it, and they refuse a formatter without a zone.
  - **A time with a zone** is matched as one skeleton (`hmz`, `Hmsv`,
    `hVVVV`: each clock, precision and of the eight forms), among the
    locale's items alone: CLDR's datetime test data writes th's `Hmsz`
    as its `Hmsv` item gives (`HH:mm:ss z`) and ja's Japanese-calendar
    `HmsVVVV` as `H:mm:ss VVVV`, where ICU4C's generator, which holds
    the standard formats too, takes th's long time format and ja's full
    one. Its zone field is written in the pattern's own form, so a full
    time format's `zzzz` stays long whatever the style asked.
  - **A date with a zone** is the date's pattern and the zone's alone
    pattern (bg's `z 'ч'.`) joined by the atTime glue of the date's
    length, as ICU4C's generator joins them.
  - **A weekday with a time and a zone** is the lone weekday, the glue
    and the time with its zone: no item covers the three, and ICU4C's
    generator splits them so.
  - **The data:** 8 zone-alone patterns, 48 zoned time patterns and the
    4 full and long rematches per time set; `dates.mloc` grows from
    724,308 to 817,900 bytes, nearly all of it the time sets' entries.
- **Intervals, the data** (research 40): `dates.mloc` holds, per locale
  and calendar, an interval set: the interval patterns of every date
  option's skeleton (each field set, length and year variant; each
  standard date format) and every time option's (each clock, precision
  and zone form; each standard time format), resolved in the generator
  as ICU4C's DateIntervalFormat resolves them, each greatest
  difference's pair of halves kept as two compiled patterns; for each
  date option that joins a time, its patterns with the larger fields it
  lacks prefixed (d, then M, y, G) and their glue styles, which a
  date-time's date differences write; the short dates yMd and GyMd a
  time's date differences write; the fallback. A time's date
  differences are not stored: ICU writes them as fallbacks of the short
  date and the time matched whole, which the run time joins as a
  date-time, so the time pairs are the same in every calendar. Sets,
  date and time parts, rows, prefixes and pairs are each kept once.
- **Date ranges** (`mlocFormatDateRange`, research 40's run-time form):
  two dates written by a date formatter as ICU4C's DateIntervalFormat
  writes the formatter's skeleton, from the interval set the locale
  unit names beside the date set (one more index at init). The greatest
  difference (the era's code, the year in its era, the month's code, the
  day) picks its pair: both halves, the start by the first and the end
  by the second (the end first where the pattern or the fallback says
  so); the second alone, the fallback around that pattern written for
  each date; neither, one date where the formatter's pattern shows no
  field as small, else the fallback around it. Both dates take the year
  style the one that needs more asks; a standard format's one date is
  its skeleton's own match, as ICU's is. Parts carry the date they
  belong to (`mloc_dateRangeStart`, `mloc_dateRangeEnd`): a date's from
  its first field of a kind the other date also writes to its last,
  the rest both's.
  - **Two choices of our own.** The month compares its code, so a leap
    month differs from the month it follows (ICU compares the number
    and writes one date). A pattern is split where a field is met again
    by its kind, a stand-alone month as a format one: fa's
    `LLLL تا MMMM y` items, which ICU leaves whole and writes with the
    first month twice ("ژانویه تا ژانویه"), give "ژانویه تا فوریه". For
    the same reason a stand-alone and a format month are one kind of
    part where ICU's spans take them as two fields.
  - **The validator** refuses a pair's first half without its second
    and a fallback without both values, so a formatter checks only that
    it writes every half of its pairs.
- **Time ranges** (`mlocFormatTimeRange`, `mlocFormatTimeRangeInZone`):
  two times of day, or two instants in one zone, by a time formatter, as
  its patterns' interval entries give them (each kept pattern's clock,
  precision and zone form; a standard format's own entry, or its
  minutes or seconds where another clock was matched anew). The
  greatest difference: am or pm, the hour, the minute, the second, the
  fraction to the digits shown (with the seconds, which have no pair).
  Under minute-optional precision both take the hour's pattern only when
  both minutes are zero. Two instants on different local dates are each
  written whole around the fallback: the short date with the full year
  (with the era across eras) and the time joined by the calendar's short
  atTime glue, as ICU's fallback of the matched yMd plus the time; the
  date is written by a date formatter's state made at that call from the
  data and the locale the zone's state keeps, so a zoned time formatter's
  init costs nothing more and a range across dates can fail as a date
  formatter's init does. Parts run across the glue: a value's from its
  date's first field to its time's last.
  - **One more choice of our own.** Across dates the offset zone forms
    (`O`, `OOOO`) stay in the time: ICU's skeleton split keeps no `O`, so
    its fallback drops the offset the formatter was asked for.
- **Date-time ranges and dates in a zone** (`mlocFormatDateTimeRange`,
  `mlocFormatDateTimeRangeInZone`, `mlocFormatDateRangeInZone`): a
  difference in the day or more writes each value whole around the
  fallback, its date by the formatter's pattern with that field
  prefixed where the date lacks it (the stored prefixes) and joined by
  the glue of the prefix's style, else by the formatter's own pattern
  and glue. A difference in the time only writes the date once and the
  time's range inside the Gregorian medium standard glue, or one value
  where the time shows no difference; a date in a zone on one local day
  is the start's. Parts run across the glue, as in times.
  - **Two choices of our own.** ICU sets no date-time pattern where the
    time's skeleton has no interval item (seconds, a zone alone) and
    then writes the whole skeleton twice, dropping the field that
    differs ("Jan 5, 10:00:00 AM – Feb 9, 10:00:00 AM" a year apart):
    the library writes the prefixed date always. ICU writes a day's
    date by its skeleton normalized (de's "5.1.2026" beside "05.01.26"
    for the same date-time alone): the library writes it as the
    formatter writes its date, so a range and its ends agree.
  - **A weekday alone at the short length** is narrow, but abbreviated
    with a time: the date-time formatter takes the weekday's medium
    patterns there ("Mon, 2:00 AM PST", where it wrote "M, 2:00 AM
    PST"), and its prefixes are those of the abbreviated skeleton, as
    ICU's.
- **Still to come in this part** (research 39's slices): relative time;
  durations; speed.

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
- **The zone formatter against ICU4C 79, once:** 20,120,448 zones (the
  906 locales ICU ships, the 450 zones CLDR names, 8 styles, 4 instants
  in 1990 and 2026 and each metazone change and the second before it).
  All equal but 23,514, of three causes: sr_Cyrl_ME (10,712; ICU's data
  reaches sr_Latn_ME's names through its sr_ME alias, CLDR's parent is
  sr_Cyrl); the British and Irish metazones' winter names (10,910; CLDR
  resolves root's alias to the GMT metazone per path, ICU's data takes
  en's British table whole and falls back to the offset); Africa/
  Monrovia in the 30 seconds after 1972-01-07 00:44:30, where CLDR's
  period starts and ICU's data starts it at 00:45. And 8,221,200 zones
  around every transition from 1990 to 2030 (15 locales; one second
  before, at, 100, 183 and 185 days after): all equal but the British
  and Irish winter names and 1,474 that follow tzdb 2026e's Manitoba
  change, which ICU's 2026d lacks.
- **Dates and times in a zone against datetime.json:** its 88 cases
  (a month and day with a time in each zone style at both lengths, in
  four calendars, and a short date with a full time) give the file's
  text, on every push; its semantic composites take the standard glue,
  as the file writes them.
- **Dates and times in a zone against ICU4C 79, once:** 1,783,008 (the
  906 locales, 12 zones, July and January 2026: the full and long time
  formats, the four date styles with each, a time in each zone form at
  each clock and precision, and a date at each length with a zone).
  All equal but 23,856: the British winter names (10,473) and
  sr_Cyrl_ME (902) above; the zoned times matched among the items alone
  (6,493; the generator holding the standard formats too, as before
  this choice, agrees with ICU on every one); the non-Gregorian
  calendars' own glue and ckb_IR's Persian months (4,838 in fa, ps,
  ckb_IR, uz_Arab_AF and shn_TH; rerun with their calendars' skeletons,
  th's differences vanish and these remain); haw's Roman-numeral
  months and tok's long date (368 each) and fr_ML's glue (230), as in
  the composites; fr_CA's `Hv` item, `HH 'h' v`, which ICU's data
  lacks (184).
- **Interval patterns against ICU4C 79** (CLDR 49.0), on every push:
  `tools/check_intervals.py` asks `mlocgen intervals` the 4,437
  questions of `test/intervals_icu.tsv` (the date and time options alone
  in 44 locales and 8 locales' own calendars, en's and ja's date-times)
  and compares each greatest difference's pattern halves with ICU's
  DateIntervalFormat's, read from its fields; 100 differ by the
  listed causes (`tools/interval-exceptions.txt`): the skeleton
  matcher's, and fa's month items split where the month is met again.
  Once: 415,854 (906 locales, 459 skeletons), all equal but 338 of the
  same causes.
- **Date ranges against ICU4C 79, once:** 4,781,868 (906 locales, 13
  calendars, every date option's skeleton and the four standard
  formats, each with equal dates and dates differing in the day, the
  month, the year, the era of Japan, in order and reversed). All texts
  equal but 9,400: the formats' numbering overrides (Hebrew numerals,
  hanidays, jpanyear's 元), which ICU's interval patterns drop (6,480,
  zh, yue, ja, he, yi); haw's Roman-numeral months (1,848); fa's split
  months (318); kok's and gl's `Y` (237), which ICU writes as the
  Gregorian year; the one-date differences already known, ckb_IR's
  Persian and cs's Dangi months, id's Islamic weekdays, tok's long date
  (336), and CLDR's per-path names and patterns in es_PY, es_VE, es_HN,
  ms_ID, ms_BN, nl_BE, sr, sv_AX and zh_Hans_HK, _MO and _SG (181).
  Spans differ in 2,526, from
  the stand-alone month (fa), the `Y` (kok) and patterns that write a
  field three times (sv, en_JP, tr, yo, qu), whose third ICU's pairing
  adds to the first date's span. Kept for every push:
  `test/date_ranges_icu.tsv`, 6,000 ranges in 30 locale-calendars, the
  316 where the library answers otherwise with its answer and cause.
- **Time ranges against ICU4C 79, once:** 866,136 (906 locales; each
  clock, precision and zone form and the four standard formats; UTC, Los
  Angeles and Kolkata; equal times, differing in the minute, the hour, am
  or pm, the second, one day and 400 days, reversed, and across the
  change to daylight time). Spans all equal; texts equal but 49,750: the
  offset forms kept across dates (43,488); the time's own pattern where
  one time already differs (5,498: zoned times matched among the items
  alone, as above); the Persian calendar's own glue and fr_ML's (760);
  fr_BE's long format, whose time ICU's generator matches anew inside
  the date fallback (4). Kept for every push:
  `test/time_ranges_icu.tsv`, 3,168 ranges in 12 locales, the 115 where
  the library answers otherwise with its answer and cause.
- **Date-time ranges and dates in a zone against ICU4C 79, once:**
  3,044,160 (906 locales in the Gregorian calendar; each date field set
  and length with each clock and precision, three zone forms with the
  minutes, and each zone form with the date alone; UTC and Los Angeles;
  equal, 30 minutes, 5 hours, 45 seconds, 3 days, 34 days and 400 days
  apart, reversed). Spans all equal; texts equal but 332,948: on two
  days the prefixed date where ICU drops the field or the offset
  (303,899); on one day the formatter's date where ICU writes its
  normalized one (24,488); the single date-time's differences already
  known, fr_ML's glue, sr_Cyrl_ME's and es_HN's per-path names, haw's
  months, tok's long date (4,456, and 85 more of fr_ML's and tok's on
  two days); en_JM's dates with the month prefixed (20), where the
  generator kept a draft item's place in the item order. Kept for every
  push: `test/datetime_ranges_icu.tsv`, 3,920 ranges in 10 locales, the
  44 where the library answers otherwise with its answer and cause.
- **The zone names data file:** 468 BCP 47 ids (451 live), 636
  metazone periods of 419 zones over 155 metazones, 155 golden and 128
  preferred zones, 11 primary zones; 296 locale units, 2,836,588 bytes
  of names, the file 2,858,565 bytes. The validators refuse each field
  spoiled in a whitebox test, and a GMT format without its {0} or an
  hour format without an H before its first mm, which the formatter
  takes apart, and the fuzz target keeps every read of
  an accepted unit inside it.

## Consequences

- Formatting a value never parses a pattern or a skeleton; the
  heuristics run once, in the generator, and their output is checked.
- The matcher is ICU's, so a pattern that differs from ICU's is either
  listed with its reason or a fault the check catches.
