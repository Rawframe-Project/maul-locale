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
operands, and CLDR's cardinal, ordinal and range plural rules compiled
to C (every sample of CLDR's files passes on every push). The library
has
32 public functions.

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
