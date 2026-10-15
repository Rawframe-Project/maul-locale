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
