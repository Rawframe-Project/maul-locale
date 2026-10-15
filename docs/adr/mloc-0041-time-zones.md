# mloc-0041. Time zones: the library's own tzdb compiler, Temporal's exact time

Status: Accepted

## Context

The time zone database (tzdb) is a set of rules. A program needs it
either as these rules or as transitions compiled from them.

- **zic's TZif output:** a table of transitions, then a POSIX TZ string
  for the times after it. It is what the C library, Go and cctz read.
- **ICU:** compiles its own `zoneinfo64` (148 KB). It stores 32-bit
  transition times with a list of pairs for the few outside, each
  type as a raw offset and a daylight saving, and a final yearly rule.
- **Java:** compiles transitions and up to two yearly rules per zone.

The tzdb 2026e release has, in the default build's files:

- 344 Zones and 253 Links;
- 16,948 transitions once compiled, 389 of them outside 32-bit
  seconds;
- 105 zones that still change each year.

Its source also has negative daylight savings in four places: Ireland
since 1971, Namibia from 1994 to 2017, Czechoslovakia in the winter of
1946, and Morocco from 2018 to 2026. Its "rearguard" form keeps
savings positive. Formatting relies on the rearguard periods to choose
a standard or a daylight name, and ICU follows them.

Temporal, now in ECMA-262, needs four answers from a named zone: the
offset at an instant, the possible instants of a local date-time, and
the next and previous transitions. Instants span ±10⁸ days in
nanoseconds.

## Decision

- **Compiler:** `mlocgen` compiles the pinned tzdb source itself, as
  zic compiles it:
  - Zone, Rule and Link lines, every day and time form the release
    uses, and any other form refused;
  - each line's rules walked year by year;
  - a line's start given the offset of the last rule before it;
  - transitions whose local time does not move forward merged, as zic
    merges them.

  It never reads TZif files, the system's zones or zic's output.
- **Default build:** Links resolve to their Zones' data, as in ICU and
  the browsers; backzone is not read for data. `Factory` is left out.
- **Standard and saving:** each offset is a standard offset and a
  saving in seconds, never negative.
  - The source is read in its rearguard form: the vanguard lines of a
    marked section are skipped, and its commented alternative is read
    instead.
  - A zone line that still gives a negative saving (Morocco's, which
    tzdb rewrites by script) has its standard offset lowered and every
    saving raised by the most negative.
  - The generator checks that the rearguard form gives the main form's
    offsets for every zone.
- **A unit of the data file** (`src/zone_tables.h`, kind zones): the
  compiled-in data carries it, its validator checks every index,
  range and order on open, and the zone functions will take a data set
  as the formatters do. It holds, per zone:
  - its transitions as `int32_t` seconds, with `int64_t` for the
    first or last few outside that range;
  - a one-byte place in the zone's own offset list, whose entries
    index shared offset pairs;
  - and a closing rule of two yearly changes (month, day rule,
    universal time) that runs to the end of Temporal's range, or none
    when the last offset stays.

  The generator checks that the closing rule gives the two years after
  the table as the source does, and that each of its changes changes
  the offset. Changes fall in February to November, so a year's
  changes stay within that year.
- **Identifiers:** every Zone and Link name, sorted with ASCII letters
  folded, so a lookup ignores case as ECMA-262 does.
  - Each name carries its primary identifier, as ECMA-402's
    `AvailableNamedTimeZoneIdentifiers` resolves it from zone.tab,
    backzone's Links and the UTC names.
  - A Link outside zone.tab takes its country from CLDR's
    `bcp47/timezone.xml`, which groups each place's names; the
    generator refuses a primary that differs from CLDR's for a name
    CLDR has, so a rename that needs ECMA-402's two-year wait shows
    when the pin moves.
  - A name reads its primary's data, so equal identifiers give equal
    offsets; where the two are different Zones (Etc/GMT resolves to
    UTC, a Link to Etc/UTC), the generator checks that they compile
    the same.
- **Version:** the release is pinned by name and the archive's SHA-256
  (`tools/tzdb-release.txt`, fetched by `tools/fetch_tzdb.sh`). The
  generator refuses a source directory holding another release.
- **The public functions** (`maul-locale/zone.h`):
  - `mlocFindTimeZone`, from a data set, by name (ASCII letters folded)
    or by offset (UTCOffset without seconds: `+05`, `+0530`, `-05:30`,
    U+2212 too);
  - `mlocFormatTimeZoneId` (the name as given, or `+HH:MM`) and
    `mlocFormatPrimaryTimeZoneId`;
  - `mlocTimeZonesEqual`, by primary identifier, as TimeZoneEquals;
  - `mlocGetTimeZoneCount` and `mlocGetTimeZone`, the primaries in
    byte order (a section of the unit lists them), as
    Intl.supportedValuesOf;
  - `mlocGetTimeZoneOffset` at an `mlocInstant` (seconds and
    nanoseconds over Temporal's range);
  - `mlocGetTimeZoneTransition`, next or previous, strictly, counting
    only changes of offset (London's 1968 change from daylight to
    standard time at one offset is none).

  An `mlocTimeZone` is 16 bytes: the zones unit and an identifier's
  place, or an offset in minutes; it points into its data.
- **Local times** (amended 2026-10-15, slice 12e):
  - `mlocTime` (8 bytes: hour, minute, second, and the nanoseconds
    past the second, which hold Temporal's millisecond, microsecond and
    nanosecond) is made by `mlocMakeTime`, as RegulateTime does, with
    `mlocOverflow`'s constrain or reject;
  - `mlocDateTime` (16 bytes: an `mlocDate` and an `mlocTime`) is made
    by `mlocMakeDateTime`, within ISODateTimeWithinLimits: everything
    but the first day's midnight;
  - `mlocGetPossibleInstants` gives a local date-time's count of
    instants with the earliest and the latest, which is all that
    disambiguation reads. A zone's offsets are under a day, so every
    instant lies within a day of the local time read as universal
    time. The spans of one offset over those two days are walked, each
    giving the local time minus its offset when that falls inside the
    span. Exact, with no assumption about how far apart changes are;
  - `mlocGetInstantFor` takes `mlocDisambiguation`. In a gap, the time
    moves by the difference of the offsets on its two sides and that
    time's instants are found again (DisambiguatePossibleEpochNanoseconds);
  - `mlocGetStartOfDay` is midnight's earliest instant, or the change
    ending a gap that skips midnight;
  - `mlocGetDateTimeFor` is the instant plus its offset, in a calendar.

  An instant outside Temporal's range among the possible ones refuses
  the call, as ECMA-262 refuses the list. Changes past the range's end
  are still read, so a local time on its last day is placed exactly.
  The C function mlocGetInstantFor refuses with mloc_errorInvalid where
  ECMA-262 throws a RangeError, as the calendar's reject does.
- **Time durations and times of day** (amended 2026-10-15, slice 12f):
  - `mlocTimeDuration` is Temporal's time duration as seconds and the
    nanoseconds past them, of one sign, the seconds below 2^53, so no
    wider integer is needed. `mlocMakeTimeDuration` sums
    `mlocTimeDurationFields` (days of 24 hours, then hours to
    nanoseconds, int64_t each), and `mlocBalanceTimeDuration` gives
    them back up to an `mlocTimeUnit`. Temporal keeps a field as a
    float64, so a valid duration may hold more nanoseconds than int64_t
    can: such a field is the host's to form, from the duration, and
    the balance refuses it (beyond about 292 years in nanoseconds, or
    292,000 in microseconds);
  - `mlocAddToTime` is AddTime, giving the days passed for
    PlainDateTime;
  - `mlocTimeUntil` is DifferenceTemporalPlainTime with
    `mlocTimeRounding`; since is until with the mode negated and the
    fields negated, as for dates;
  - `mlocRoundTime` is RoundTime with ValidateTemporalRoundingIncrement's
    rules, a day taking 1 and giving the next midnight's day.

  Temporal's rounding of integers moved out of the calendar's rounding
  into `src/rounding.c` for both.
- **Date-times** (amended 2026-10-15, slice 12g):
  - `mlocUnit` names Temporal's ten units in its table's order, and
    `mlocDateTimeDuration` is its Internal Duration Record (date fields
    and a time duration);
  - `mlocAddToDateTime` is AddDurationToDateTime: the time first, its
    days passed joining the date part's, then mlocAddToDate;
  - `mlocDateTimeUntil` is DifferencePlainDateTimeWithRounding with
    `mlocDateTimeRounding`; the host balances the time part with
    mlocBalanceTimeDuration and keeps each field as a float64, as
    TemporalDurationFromInternal does;
  - `mlocRoundDateTime` is RoundISODateTime.

  RoundRelativeDuration without a time zone lives in
  `src/duration_round.c`, and the calendar's mlocDateUntilRounded is its
  case of midnight at both ends (its 20,743 test262 rows unchanged). A
  moment is a day and the nanoseconds past midnight. Twice the progress
  is compared with the window by days first, so ranges of 10^8 days need
  no integer wider than 64 bits. A time duration is rounded by whole
  seconds or by a divisor of a second. With a divisor, half-even's
  parity also counts the seconds when a second holds an odd number of
  steps (200 ms).
- **Instants** (amended 2026-10-15, slice 12h):
  - `mlocAddToInstant` is AddDurationToInstant: an instant plus a time
    duration, refused past the range;
  - `mlocInstantUntil` is DifferenceTemporalInstant's rounding of
    DifferenceEpochNanoseconds, with `mlocTimeRounding` from hours down
    and an increment below and dividing the next larger unit; the host
    balances it as for times;
  - `mlocRoundInstant` is RoundTemporalInstant: the increment divides a
    day of the unit, and the nanoseconds are rounded as if positive
    (RoundNumberToIncrementAsIfPositive). The nanoseconds past the
    floored day's midnight are rounded, so half-even's parity also
    counts the days when a day holds an odd number of steps (24 hours).
- **Later in this part:**
  - a zones file whose tzdb release may differ from the library's, so
    a program takes a new release without the rest of the data;
  - CLDR's BCP 47 ids and Windows names;
  - Temporal's zoned date-times with their arithmetic and rounding;
  - IXDTF strings.

## Proof

- **Against zic and zdump:** `tools/check_zones.py` compiles the same
  source with zic into fat TZif files. It compares zdump's transitions
  with the library's for every zone, from 1800 to 2500 and from 275700
  to the range's end. Every change of offset must match in time and
  offset, and so must every change of the daylight flag, outside the
  five zones with negative savings. The cldr CI job runs it.
- **Against ICU:** for those five zones, ICU's pairs of raw offset and
  saving match the library's. ICU's data is one release older, and
  the one difference is Dublin's autumn of 1925, which 2026e corrected.
- **The unit test** (`test/test_zone_rules.c`) covers:
  - known times, at both ends of a table and near the range's end;
  - the rearguard periods;
  - every transition from 1800 to 2100 seen as a change of offset;
  - names found with case ignored, Links, and ECMA-402's primaries
    (Asia/Calcutta, Etc/GMT, Europe/Bratislava, Atlantic/Jan_Mayen);
  - the validator refusing a changed count, offset place, order,
    closing month and saving.
- **Local times against zoneinfo:** `tools/check_local.py` asks
  `test/conform_local.c` about every change of offset from 1800 to 2500
  in every zone. It covers the local times at the edges and in the
  middle of each gap or fold, and every disambiguation of them. It also
  covers the start of those days and the local time at the change and a
  second before. Python's zoneinfo reads zic's fat files for the same
  source. A local time's instant under each of its folds counts when
  the offset at that instant agrees. In a gap the shift is the
  difference of the two folds' offsets. All 1,441,140 answers agree.
  The cldr CI job runs it.
- **PlainTime against test262:** `test/test_time_test262.c` replays
  613 rows and 3,615 checks from 115 of test262's Temporal.PlainTime
  files: times made, added to, rounded, and their differences until and
  since. They are in `test/test262_time.inc`. Files that need ISO time
  strings wait for the IXDTF slice.
- **PlainDateTime against test262:** `test/test_datetime_test262.c`
  replays 7,532 rows and 73,614 checks from 404 of test262's
  Temporal.PlainDateTime files, in every calendar: date-times made,
  added to, rounded, and their differences until and since. They are in
  `test/test262_datetime.inc`.
- **Instant against test262:** `test/test_instant_test262.c` replays
  377 rows and 1,583 checks from 67 of test262's Temporal.Instant
  files: instants added to, rounded, and their differences until and
  since, in `test/test262_instant.inc`. A largest unit of microseconds
  or nanoseconds past `int64_t` takes its field from the exact decimal,
  as a host does.
- **Against test262:** `test/test_zone_test262.c` replays 3,138 rows
  from 27 of test262's Temporal.ZonedDateTime files: identifiers kept
  or refused, equality, offsets and transitions. The rows are in
  `test/test262_zone.inc`, under test262's BSD license. Three tests
  loop over `Intl.supportedValuesOf("timeZone")`, and the replay runs
  their loops over the library's list: each primary keeps its
  identifier, no two are equal, and none has a transition beyond the
  range. Tests that need date-times, ISO strings or other calendars'
  dates wait for those parts. `links.js` is one of them; it expects
  CST6CDT, EST5EDT, MST7MDT and PST8PDT to equal America/Chicago and
  the others. Since 2026e they are Zones again, and the library follows
  tzdb there.

## Consequences

The zones answer the same as zic for the pinned source and give ICU's
daylight periods. A new tzdb release is a new pin, a generator run, and
the zic check. The zones unit takes 120,536 bytes with its 597
identifiers, against the 147,792 of ICU's `zoneinfo64.res`.
