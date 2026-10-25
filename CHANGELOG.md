# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API, the ABI and every data format.

## [0.2.2](https://github.com/Rawframe-Project/maul-locale/compare/v0.2.1...v0.2.2) (2026-10-25)


### Fixed

* **mloccat:** refuse a NUL byte in a PO string ([#67](https://github.com/Rawframe-Project/maul-locale/issues/67)) ([0c304b0](https://github.com/Rawframe-Project/maul-locale/commit/0c304b0282ec464d7ab624d16cf99795e79e3a91)), closes [#56](https://github.com/Rawframe-Project/maul-locale/issues/56)
* **mloccat:** refuse a NUL byte in an XLIFF file or a PO comment ([#70](https://github.com/Rawframe-Project/maul-locale/issues/70)) ([83827ee](https://github.com/Rawframe-Project/maul-locale/commit/83827ee6d1dd1f539620d9d8504f58042f8b9964)), closes [#68](https://github.com/Rawframe-Project/maul-locale/issues/68)

## [0.2.1](https://github.com/Rawframe-Project/maul-locale/compare/v0.2.0...v0.2.1) (2026-10-25)

### Fixed

- `mloccat import-po` names the line of an entry it refuses for having no `msgid`. It reported line 0, so the entry could not be found in the file ([#64](https://github.com/Rawframe-Project/maul-locale/pull/64)).
- `mloccat` reads a selection message whose quoted key holds a line feed as one key. `export-xliff` offered the translator the wrong case for such a key, and the message did not come back from the XLIFF file; both now keep the key as written ([#65](https://github.com/Rawframe-Project/maul-locale/pull/65)).

## [0.2.0](https://github.com/Rawframe-Project/maul-locale/releases/tag/v0.2.0) (2026-10-24)

### Changed

- Data files carry a data format version instead of the library
  version, so a file stays valid across every release that keeps the
  format. `mlocDataVersion`'s `library` field (an `mlocVersion`) is now
  `format`, a `uint32_t` holding the new `MLOC_DATA_FORMAT_VERSION`
  (`0x80000002` in this release). Data files, catalogs and standalone
  compiled messages (from `mlocCompileMessage`) made by 0.1.0 or earlier
  are refused with `mloc_errorDataVersion`: a program must regenerate
  its data files with this release's `mlocgen`, its catalogs with its
  `mloccat`, and compile stored messages again. A host that keeps data
  files per library version should keep them per data version
  (`mlocGetDataVersion`) instead
  ([#34](https://github.com/Rawframe-Project/maul-locale/pull/34),
  [#45](https://github.com/Rawframe-Project/maul-locale/pull/45)).
- An alphabetic index takes 16 more bytes per bucket, and index bytes
  built by 0.1.0 are refused with `mloc_errorInvalid`. A program that
  sized its buffer from 0.1.0 must ask `mlocBuildIndex` for the capacity
  again (a null buffer and a capacity of 0), and must build stored
  indexes again
  ([#30](https://github.com/Rawframe-Project/maul-locale/pull/30)).
- Building an alphabetic index is about 58 times faster (German: 4.6 ms
  to 80 µs), and finding a name's bucket about 5 times faster
  ([#30](https://github.com/Rawframe-Project/maul-locale/pull/30)).
- Making a collator is faster. The data checks it repeated now run once,
  when the data file opens, which takes German from about 2,100 to
  870 ns ([#42](https://github.com/Rawframe-Project/maul-locale/pull/42)).
  A locale whose rules reorder scripts (Chinese, Japanese, Korean,
  Russian, Arabic, Hindi, Thai and others) now reads its reordering from
  the data instead of building it, which halves the time again: Chinese
  went from about 1,280 to 620 ns
  ([#45](https://github.com/Rawframe-Project/maul-locale/pull/45)).
  Results do not change.

### Added

- The locales guide shows how to match the user's languages when they
  arrive as one comma-separated list, as Maul Window's
  `mwinGetPreferredLocales` gives them, and to match again on its
  `mwin_eventLocaleChanged`. Maul Locale does not depend on Maul Window
  ([#28](https://github.com/Rawframe-Project/maul-locale/pull/28)).

### Fixed

- `mlocgen` built in Debug or with the sanitizers runs to the end. It
  no longer hands `qsort` or `memcpy` a null array for an empty list, or
  `mlocWideDivide` an output that is also an input. The data it writes
  does not change
  ([#47](https://github.com/Rawframe-Project/maul-locale/pull/47)).
- The release's SBOM (`sbom.spdx.json`) is named after the repository
  and tag; v0.1.0's was named `.`
  ([#15](https://github.com/Rawframe-Project/maul-locale/pull/15)).

## [0.1.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.0.1...v0.1.0) (2026-10-23)

The locale data moves from CLDR 49 beta 3 to the final CLDR 49 release.

### Changed

- The data comes from the final CLDR 49 release and is checked against
  ICU4C 79.1.
- Windows names of Canadian time zones follow Windows' new zones:
  `America/Vancouver` is "British Columbia Standard Time",
  `America/Edmonton` and `America/Inuvik` "Alberta Standard Time", and
  `America/Winnipeg` "Manitoba Standard Time", and
  `mlocFindTimeZoneWindows` finds these names. "Pacific Standard Time"
  with region CA now gives `America/Los_Angeles` instead of
  `America/Vancouver`.
- Data files from 0.0.1 do not open with 0.1.0: `mlocOpenData` refuses
  them with `mloc_errorDataVersion`. Take the data files from the same
  release as the library, and update the two together.

## [0.0.1](https://github.com/Rawframe-Project/maul-locale/releases/tag/v0.0.1) (2026-10-23)

The first release: locale data and formatting from CLDR 49 (beta 3)
and the IANA time zone database 2026e, checked against CLDR's
conformance data and against ICU4C 79.1 (release candidate) and ICU4X
2.3.

### Added

- Compiled-in data for locales, plural rules, numbers, currencies,
  units, lists, calendars and time zones, and data files for names,
  date patterns, zone names, currency and unit names, spell-out rules,
  relative times and collation, each validated once when opened. A
  zones file can bring a newer time zone release without a new
  library.
- Locale identifiers parsed into a fixed-size value in canonical
  syntax, canonicalized with CLDR's aliases, with likely subtags added
  and removed, fallback chains, the text direction, language matching
  as ICU's LocaleMatcher does it, and a region's preferences (hour
  cycle, first day of the week, measurement system, paper size,
  calendar).
- Display names of languages, scripts, regions, variants, keys and
  their values, measurement systems and subdivisions, and of whole
  locales, in every form CLDR has and each capitalization context.
- Exact decimals from integers, text or the shortest digits of a
  double, rounded in ECMA-402's modes, and CLDR's cardinal, ordinal and
  range plural rules.
- Number formatting with ECMA-402's options: standard, scientific,
  engineering and compact notation, percents, per mille, ranges, every
  numbering system, and parts for styling; number parsing as people
  type numbers.
- Currencies with their symbols, digits and cash rounding, by name,
  in compact notation and accounting style; the currency a locale uses
  on a day; currency parsing.
- Units: exact conversion between any of CLDR's units, the units a
  locale prefers for a usage, amounts and ranges of any unit and
  compound in each width, mixed units, numeric durations, coordinates,
  grammatical cases and genders, and unit display names.
- Numbers in words, ordinal suffixes and algorithmic numbering systems
  (Roman, Hebrew, Chinese and others) by CLDR's rules, on the exact
  value, and their parsing back.
- Lists of any items in each width, and the gender of a list of
  persons.
- Dates in 18 calendars as Temporal models them, with era and month
  codes, overflow handling, arithmetic, differences and rounding.
- Time zones from the IANA database by identifier, offset, BCP 47 id
  and Windows name, with offsets, transitions and the instants of a
  local time; times, date-times, instants and zoned date-times with
  Temporal's arithmetic and rounding; Temporal's ISO 8601 and RFC 9557
  strings read and written.
- Dates and times written in a locale by what they should show or by
  its standard formats, in its calendar and digits, with time zone
  names; ranges of dates, times and date-times; relative times; and
  durations as ECMA-402 writes them.
- Unicode MessageFormat 2: messages compiled, written back as canonical
  text, bound to a locale and formatted with named arguments, every
  default function, functions for words, relative times, durations,
  lists and grammar, the program's own functions, bidi isolation and
  parts.
- Case, plural, possessive and copula suffixes and Korean particles on
  runtime names, numbers and abbreviations for Turkish, Azerbaijani,
  Kazakh, Kyrgyz, Turkmen, Uzbek, Hungarian, Finnish and Korean, in
  code and in messages.
- Translation catalogs: the `mloccat` tool checks sources in the W3C
  message resource draft syntax, compiles them, records which source
  message each translation answers so changed ones show stale, reports
  coverage, exchanges translations as PO and XLIFF 2.2 and writes
  pseudo-locales; the library opens catalogs and formats keys through
  fallback chains that state why each text was chosen.
- Collation in every CLDR locale's order with all of ICU's options,
  sort keys with ICU4C's bytes and a version to store with them, for
  UTF-8 and UTF-16; collation search; alphabetic indexes.
- Support for Linux, macOS, Windows with `clang-cl`, and WebAssembly.
