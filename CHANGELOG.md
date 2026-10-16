# Changelog

All notable changes to this project are recorded here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/). Before
1.0.0, any minor release may change the API, the ABI and every data
format.

## [Unreleased]

### Added

- The library skeleton: the build, the family rules and tools, the
  version and result API (`mlocGetVersion`, `mlocGetCldrVersion`,
  `mlocResultName`) and the library profile.
- The data layer: compiled-in data and data files opened, validated
  and searched as one set (`maul-locale/data.h`, mloc-0005, mloc-0014).
- Locales: strict parsing into canonical syntax and formatting,
  canonicalization with CLDR's aliases, likely subtags added and
  removed, fallback chains per component, language matching as ICU's
  LocaleMatcher does it, and regional preferences (mloc-0015 to
  mloc-0018).
- Decimal values from text, integers and the shortest digits of a
  double, rounded in ECMA-402's modes, with their plural operands, and
  CLDR's cardinal, ordinal and range plural rules (mloc-0019, mloc-0020).
- Number formatting in standard, scientific, engineering and compact
  notation, percents, per mille and ranges, with every numbering system
  and ECMA-402's options, and number parsing (mloc-0021, mloc-0032).
- Currencies: amounts with symbols, digits and cash rounding, compact
  and by name (`src/generated/currency-names.mloc`), the currencies of
  a region on a day, and currency parsing (mloc-0022, mloc-0033).
- Units: exact conversion, the units a locale prefers for a usage,
  amounts and ranges in each width, compound and mixed units, numeric
  durations, coordinates, grammatical cases and genders, and unit
  display names (`src/generated/units.mloc`; mloc-0023 to mloc-0025,
  mloc-0027 to mloc-0031).
- Spell-out: numbers in words, ordinals and algorithmic numerals by
  CLDR's rule-based formats, and their strict parse
  (`src/generated/spellout.mloc`; mloc-0034).
- Lists of any items in each width (mloc-0026, mloc-0035).
- Display names of languages, scripts, regions, variants, keys and
  values, whole locales and subdivisions (`src/generated/names.mloc`,
  `src/generated/subdivisions.mloc`; mloc-0036 to mloc-0039).
- Calendars: dates as values in CLDR's 18 calendars as Temporal models
  them, with era and month codes, overflow, durations added,
  differences taken and rounded, fields changed and reference dates
  found (`maul-locale/calendar.h`, mloc-0040).
- Time zones compiled from the pinned IANA release: identifiers and
  primaries, offsets, possible instants, transitions and starts of
  days (`maul-locale/zone.h`, mloc-0041).
- Temporal's exact-time values: times of day, date-times, instants and
  zoned date-times with their arithmetic, differences, rounding and
  totals, and durations as float64 fields, as ECMA-262 computes them
  (mloc-0041).
- Temporal's strings: ISO 8601 and RFC 9557 date-times, offsets, time
  zone and calendar strings and durations read by ECMA-262's grammar,
  and every type's toString written (`maul-locale/ixdtf.h`,
  mloc-0041).
- Zones files: a data file of zones alone may bring another IANA
  release, built by `mlocgen zonesfile`, and `mlocGetTimeZoneRelease`
  names the release a data set's zones are (mloc-0041).
- Time zones by their BCP 47 ids and Windows names, both ways, as CLDR
  maps them (`mlocFindTimeZoneBcp47`, `mlocFormatTimeZoneBcp47`,
  `mlocFindTimeZoneWindows`, `mlocFormatTimeZoneWindows`; mloc-0041).
- Dates formatted (`maul-locale/datetime.h`): `mlocInitDateFormatter`
  and `mlocFormatDate` write a date by a field set, length and year
  style or a standard format, in the locale's calendar and digits, with
  parts (mloc-0042).
- Times formatted: `mlocInitTimeFormatter` and `mlocFormatTime` write a
  time of day by its hour cycle (the locale's, -u-hc, H11 to H24, either
  clock) and precision (hour, minute, minute unless zero, second, 1 to 9
  fraction digits) or a standard format, with day periods (am and pm,
  noon, the locale's flexible periods) and parts (mloc-0042).
- Dates with times: `mlocInitDateTimeFormatter` and `mlocFormatDateTime`
  join a date field set or standard date format and a time by the
  calendar's atTime or standard glue of the date's length, a weekday
  with a time matched whole (mloc-0042).
- Regional preferences give the calendar: the region's first in CLDR's
  calendarPreferenceData, or -u-ca's whole value (mloc-0018).
- The dates data file, `dates.mloc`: every locale's semantic date
  patterns and standard date formats in the 18 calendars, compiled, and
  its calendar names (months, weekdays, quarters, eras, cyclic years,
  leap month patterns), for the date formatter (mloc-0042).
- Times in `dates.mloc`: each locale's time patterns per calendar (hour,
  minutes and seconds on both clocks, the standard time formats, the
  date-time glue), its day period names and its day period rules
  (mloc-0042).
- Zones formatted: `mlocInitZoneFormatter` and `mlocFormatZone` write a
  zone at an instant by its specific or generic name, location, city or
  offset, falling back and qualifying generic names as ICU4C does, with
  CLDR's daylight offsets read over the zones unit's (mloc-0042).
- Dates and times in a zone: `mlocFormatDateInZone`,
  `mlocFormatTimeInZone` and `mlocFormatDateTimeInZone` write an
  instant's local date, time or both with its zone in any of the zone
  styles, and the full and long standard time formats, which name the
  zone, are now taken (mloc-0042).
- The zone names data file, `zone-names.mloc`: each BCP 47 zone id's
  region and metazone periods, each metazone's golden and preferred
  zones, and every locale's zone formats, exemplar cities and zone and
  metazone names, as CLDR resolves them (mloc-0042).
- Date ranges: `mlocFormatDateRange` writes two dates by a date
  formatter as ICU4C's DateIntervalFormat writes its skeleton ("Jan
  5 – 8, 2026"), with parts marking each date's own
  (`mloc_dateRangeStart`, `mloc_dateRangeEnd`); `dates.mloc` holds
  every locale's interval patterns for each date and time option, and
  `mlocDateFormatter` grows to hold a formatter's (mloc-0042).
- Time ranges: `mlocFormatTimeRange` and `mlocFormatTimeRangeInZone`
  write two times of day, or two instants in a zone (across dates each
  with its short date), by a time formatter ("10:00 – 11:30 AM"); the
  time and date-time formatters' values grow (mloc-0042).
- Date-time ranges and dates in a zone: `mlocFormatDateTimeRange`,
  `mlocFormatDateTimeRangeInZone` and `mlocFormatDateRangeInZone`: on
  one day the date once and the time's range, on two each whole, a date
  that lacks the greatest difference's field written with it
  (mloc-0042).
- Date patterns follow ICU's item order where a locale's own file has
  a draft or inherited item: en_JM's month and day "05/1" and es_HN's
  "05-enero", as ICU writes them (mloc-0042).
- The relative time data file, `src/generated/relative.mloc`: each
  locale's names of near offsets ("yesterday", "next Monday") and future
  and past patterns by plural category, for every unit and width, filed
  per key along the chain (mloc-0043).
- Relative time: `mlocInitRelativeFormatter` and `mlocFormatRelative`
  write a signed amount of a unit (year to second, quarter, week, the
  weekdays) in three widths, numeric always or auto ("yesterday"),
  capitalized by context, with parts (`maul-locale/relative.h`,
  mloc-0043).
- A compact number's unit, currency or relative time name takes the
  plural category of its whole value, as ICU4C does ("1M meters", "1
  million US dollars"; it took the category of "1").
- A relative day with a time: `mlocFormatRelativeDayTime` joins the
  relative formatter's day and a time formatter's time by CLDR's
  relative date-time glue ("tomorrow at 3:00 PM"), the day one part
  (`mloc_datePartRelativeDay`); `dates.mloc`'s time sets hold that glue
  (mloc-0043).
- Durations: `mlocInitDurationFormatter`, `mlocGetDurationOptions` and
  `mlocFormatDuration` write a Temporal duration's fields as ECMA-402's
  Intl.DurationFormat does, each field's style and display, the
  fractions exact and truncated, the locale's digital separators and
  hour padding, with parts (`maul-locale/duration.h`, mloc-0044).
- The IXDTF duration writer is `mlocFormatTemporalDuration`, beside
  `mlocFormatTemporalDate` and the others (it was `mlocFormatDuration`,
  which now names the locale's duration formatter; mloc-0044).
