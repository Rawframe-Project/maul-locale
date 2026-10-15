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
- **Later in this part:**
  - the public functions: a zone by name (ignoring case) or offset,
    equality by primary identifier, the list of primaries, the offset
    at an instant, possible instants, transitions, the start of a day;
  - a zones file whose tzdb release may differ from the library's, so
    a program takes a new release without the rest of the data;
  - CLDR's BCP 47 ids and Windows names;
  - Temporal's instants, date-times and zoned date-times with their
    arithmetic and rounding;
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

## Consequences

The zones answer the same as zic for the pinned source and give ICU's
daylight periods. A new tzdb release is a new pin, a generator run, and
the zic check. The zones unit takes 120,536 bytes with its 597
identifiers, against the 147,792 of ICU's `zoneinfo64.res`.
