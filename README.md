# Maul Locale

Locale data and formatting for every language CLDR describes, in C, with no allocation and the same output on every platform.

[![CI](https://github.com/Rawframe-Project/maul-locale/actions/workflows/ci.yml/badge.svg)](https://github.com/Rawframe-Project/maul-locale/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/github/license/Rawframe-Project/maul-locale)](LICENSE)
[![Release](https://img.shields.io/github/v/release/Rawframe-Project/maul-locale)](https://github.com/Rawframe-Project/maul-locale/releases)
[![OpenSSF Scorecard](https://api.scorecard.dev/projects/github.com/Rawframe-Project/maul-locale/badge)](https://scorecard.dev/viewer/?uri=github.com/Rawframe-Project/maul-locale)

A program that shows text to people in their own language needs more than translations. It has to pick the user's language from the ones it has, write numbers, money, units and dates as each region writes them, choose the plural form a sentence needs, sort names in the order readers expect, and put a name into a sentence with the right suffix. Maul Locale does this for games, engines and applications, from one pinned CLDR release (49, on Unicode 18):

- **Locales:** parsing and canonical forms, likely subtags, fallback, matching a user's preferred languages to a program's as ICU does, regional preferences, and display names of languages, regions and whole locales.
- **Numbers:** exact decimals, plural rules, numbers in every notation, currencies, units with exact conversion and each region's preferred units, numbers in words and in algorithmic numbering systems, and reading numbers back as people type them.
- **Dates and times:** 18 calendars, time zones from the IANA database, Temporal's values and arithmetic and its ISO 8601 and RFC 9557 strings, dates and times written by what they should show, ranges, relative times and durations.
- **Messages:** Unicode MessageFormat 2, lists, and grammatical suffixes and particles for Turkish and the other Turkic languages, Hungarian, Finnish and Korean ("Ayşe'ye", "1985'te", "Helsingissä", "책은").
- **Catalogs:** translation catalogs compiled by the `mloccat` tool, with stale translations found, PO and XLIFF exchange, pseudo-locales and fallback chains that say why each text was chosen.
- **Collation:** sorting and comparing in every CLDR locale's order with ICU4C's sort keys, collation search, and alphabetic indexes.

Every part is checked against CLDR's conformance data and against ICU4C and ICU4X. Nothing is allocated and no operating system call is made: formatters are values the caller declares, results go into caller buffers, and the program passes the data, the time and the time zone. Every function is safe from any thread.

## Quick start

It builds with CMake 3.25 or newer and GCC 14 or newer, Clang 19 or newer, or Apple Clang. On Linux and macOS:

```sh
git clone https://github.com/Rawframe-Project/maul-locale.git
cd maul-locale
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Ubuntu 24.04's default compiler is GCC 13, which the build refuses ("GCC 14 or newer is required"). Install GCC 14 and configure again in a fresh build directory, since `build/` keeps the compiler it first found; then go on from `cmake --build build`:

```sh
sudo apt install gcc-14
rm -rf build && CC=gcc-14 cmake -B build -DCMAKE_BUILD_TYPE=Release
```

On Windows the library builds with `clang-cl`, and on the web with Emscripten; [Getting started](docs/user/getting-started.md) lists what each needs.

Install it with `cmake --install build --prefix <prefix>` and use it from CMake:

```cmake
find_package(maul-locale REQUIRED CONFIG)
target_link_libraries(app PRIVATE maul-locale::maul-locale)
```

or through pkg-config as `maul-locale`. The data for locales, numbers, currencies, units, calendars and time zones is compiled in; names, date patterns, spell-out rules and collation come as data files the program loads when it needs them. [Getting started](docs/user/getting-started.md) has a first example and [Data](docs/user/data.md) explains the files.

## Platforms

Linux with GCC 14 or Clang 19 and newer, macOS with Apple Clang, Windows with `clang-cl`, and WebAssembly with Emscripten; CI builds and tests each. The public headers are C17 and C++17, so any C or C++ program can include them. The library depends only on the C standard library and [Maul Unicode](https://github.com/Rawframe-Project/maul-unicode), which the build fetches when it is not installed.

## Documentation

- [The documentation site](https://rawframe-project.github.io/maul-locale/), with the API reference generated from the headers.
- [User documentation](docs/README.md): one page per part of the library.
- [How it works](docs/internals/overview.md) and [how to develop it](docs/operations/development.md).
- [Changelog](CHANGELOG.md).

Maul Locale belongs to the Maul family of standalone C libraries, which share one set of conventions.

## Contributing

Issues are welcome. Pull requests are accepted for issues a maintainer has approved; see [CONTRIBUTING.md](https://github.com/Rawframe-Project/.github/blob/main/CONTRIBUTING.md). The project is developed with AI coding agents, directed and reviewed by its maintainers; outside contributions follow the [AI policy](https://github.com/Rawframe-Project/.github/blob/main/AI_POLICY.md).

## License

MIT; see [LICENSE](LICENSE). The library's code is its own, written from the specifications and from Reingold and Dershowitz's *Calendrical Calculations*. The data it ships and some of its tables come from others, each under its own license in [LICENSES/](LICENSES):

- CLDR's data, in `src/generated/` (the compiled-in data and the data files) and the tables taken from it, and Unihan's Hanja readings (`src/generated/hanja_finals.h`): the Unicode License v3. The Persian calendar's corrections, the Umm al-Qura month lengths and the Qing calendar's years 1900 to 1911 are published facts, taken as ICU4C and ICU4X list them, under the same notice.
- The Turkish lexicon's marks (`src/generated/turkish_marks.h`), from Zemberek-NLP's dictionaries: the Apache License 2.0.
- The Finnish inflection types (`src/generated/finnish_types.h`), from Kotus's word list of modern Finnish: Creative Commons Attribution 3.0.
- Test data only, not part of the library: test262's cases in `test/test262_*.inc` (BSD 3-Clause), and the MessageFormat 2 suite and ICU4C's answers in `test/` (Unicode License v3).
