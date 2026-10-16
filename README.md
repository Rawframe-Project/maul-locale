# Maul Locale

Locale-aware formatting, parsing, collation and messages for every
language CLDR describes. Written in C23 with public headers any C17 or
C++17 program can include, with no dependency beyond Maul Unicode and
an MIT license.

It covers, from one pinned CLDR release (49, on Unicode 18):

- **Locales:** identifiers, canonical forms, likely subtags, fallback,
  and matching a user's preferred languages to a program's.
- **Numbers:** plural rules, decimal, percent, scientific and compact
  notation, currencies, units, ranges, spell-out, and parsing what a
  user types.
- **Dates and times:** 18 calendars, time zones from the IANA database,
  date arithmetic, formatting by semantic skeletons, intervals,
  relative times and durations.
- **Text:** lists, display names, person names, collation, word
  breaking for languages written without spaces, and transliteration.
- **Messages:** MessageFormat 2, grammatical functions for case
  suffixes and particles, and translation catalogs.

Nothing is allocated, no operating system call is made, and output is
identical on every platform. Formatting writes into the caller's
buffer.

## Status

Not released. The design is decided. Built so far: the generator's
reading of CLDR (its inheritance checked against CLDR's own JSON on
every push), the data layer that opens compiled-in data and data
files, locale ids parsed strictly into canonical syntax and
canonicalized with CLDR's aliases, likely subtags added and removed,
fallback chains per component, language matching as ICU's
LocaleMatcher does it, and regional preferences (CLDR's conformance
files, its own inheritance and JSON, and ICU's matcher test pass on
every push), and decimal values (from text, integers and the shortest
digits of a double, rounded in ECMA-402's modes) with their plural
operands, CLDR's cardinal, ordinal and range plural rules compiled to C
(every sample of CLDR's files passes on every push), and numbers,
percents and per mille in standard, scientific, engineering and compact
notation and ranges, with every numbering system and ECMA-402's options
(CLDR's decimal test data passes on every push) and read back as a
person types them, as ICU parses, and amounts of money
with each currency's symbols, digits and cash rounding, in compact
notation and by name (the names are a data file of their own,
`src/generated/currency-names.mloc`), read back as ICU reads them
(the formatter's currency or any the locale names), and the currency a
locale or region uses on a day; and exact conversion between CLDR's units and
the units a locale prefers for a usage (every conversion and preference
CLDR's unit test data gives passes on every push), and amounts and
ranges of any core unit in each width (a name CLDR lacks built from its
compound patterns), amounts of mixed units ("5 ft, 6 in") and of a usage
in the unit the locale prefers ("0.62 mi" for 1000 m on a US road),
numeric durations ("1:05:09") and coordinates ("12.5°N"), long names
in a grammatical case ("2 Metern", German's dative) and units'
grammatical genders, with
the display names of the units CLDR names (the
names are a data file of their own, `src/generated/units.mloc`); and
numbers in words, ordinal suffixes and Roman, Hebrew and the other
algorithmic numerals by CLDR's rule-based formats, as ICU runs them but
on the exact decimal, by rule set or numbering system, capitalized as
the context asks, and read back as ICU's strict parse reads them (CLDR's rule-based test data passes on every push;
the rules are a data file of their own, `src/generated/spellout.mloc`);
lists of any items ("a, b, and c", "a, b, or c") in each width, as
ICU joins them; and the display names of languages, scripts, regions,
variants, keys and their values and measurement systems, in every form
CLDR has and each capitalization context, as ICU4C's LocaleDisplayNames
gives them, and of whole locales by UTS #35's algorithm, dialect names,
extensions and transforms included, as CLDR's own tools build them
(CLDR's locale display name test data passes on every push; the names
are a data file of their own, `src/generated/names.mloc`, and the
subdivision names another, `src/generated/subdivisions.mloc`); and
dates as values in CLDR's calendars, as Temporal models them, with era
and month codes and overflow constrained or refused, all 18 of
them (each day from 1600 to 2400 as ICU4X computes it, and the
Chinese and Dangi calendars from integer astronomy), with durations
added to them, their differences taken and rounded, their fields
changed and month-day reference dates found, as Temporal does it; and
time zones compiled from the pinned IANA release (every zone's offsets
and transitions checked against zic), with times of day, date-times,
instants and zoned date-times and their arithmetic, differences,
rounding, totals and durations as ECMA-262's Temporal computes them,
read from and written to Temporal's ISO 8601 and RFC 9557 strings
(test262's Temporal tests replayed on every push), zones files that
bring a newer IANA release without the rest of the data, and zones by
their BCP 47 ids and Windows names as CLDR maps them; and dates
written in a locale by what they should show (a field set at a length,
the year with its century or era as it needs) or by its standard
formats, in its calendar and digits, and times of day by their hour
cycle and precision or a standard format, with day periods, and both
joined by the calendar's glue, as ICU4X's
semantic skeletons and ICU4C's patterns write them (CLDR's datetime
test data passes on every push; the patterns and calendar names are a
data file of their own, `src/generated/dates.mloc`); and time zones
written by their specific or generic names, locations, cities or
offsets at an instant, falling back as ICU4C's TimeZoneFormat does,
alone or with a date, a time or both (the zone names are a data file of
their own, `src/generated/zone-names.mloc`); and ranges of two dates
("Jan 5 – 8, 2026") as ICU4C's DateIntervalFormat writes them.
The library has 138 public functions.

## Building

Requirements: CMake 3.25 and GCC 14 or Clang 19 or newer; on Windows,
`clang-cl` (the Visual Studio component "C++ Clang tools for Windows").

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Design

The rules every Maul library follows are in `docs/conventions.md` and
`docs/adr/`; the records particular to this library are listed in
`docs/adr/mloc.md`.

## License

MIT; see `LICENSE`.

The data the library ships is generated from the Unicode Common Locale
Data Repository (CLDR), which is covered by the Unicode License v3; its
notice is in `LICENSES/Unicode-3.0.txt`. That covers:
- the compiled-in data, `src/generated/builtin_data.c`;
- the data files in `src/generated`;
- the tables taken from CLDR's supplemental data;
- the skeleton test table `test/skeletons_icu.tsv` (CLDR's test
  questions with ICU4C's answers, ICU being under the same license).

The library's code is its own, written from the specifications and
from Reingold and Dershowitz's Calendrical Calculations. The Persian
calendar's corrections, KACST's Umm al-Qura month lengths and the Qing
calendar's years 1900 to 1911 are published facts. Maul Locale takes
them as ICU4C and ICU4X list them, under the same notice.

The test cases in `test/test262_calendar.inc`,
`test/test262_zone.inc`, `test/test262_time.inc`,
`test/test262_datetime.inc`, `test/test262_instant.inc`,
`test/test262_zoned.inc`, `test/test262_duration.inc` and
`test/test262_ixdtf.inc` are taken from
test262,
the ECMAScript test suite, under its BSD license; its notice is in
`LICENSES/BSD-3-Clause-test262.txt`. They are tests only and are not
part of the library.
