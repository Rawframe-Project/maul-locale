# Getting started

Maul Locale is a C library. It builds with CMake, installs as a CMake package and a pkg-config module, and depends on nothing but the C standard library and [Maul Unicode](https://github.com/Rawframe-Project/maul-unicode).

## Requirements

- CMake 3.25 or newer.
- GCC 14 or Clang 19 or newer. On Windows, `clang-cl`: the Visual Studio component "C++ Clang tools for Windows". MSVC's own `cl` compiles the public headers but not the library.
- Emscripten for WebAssembly builds.
- Maul Unicode: an installed package of the pinned version is used when CMake finds one; otherwise the build fetches the pinned release from GitHub. `-DFETCHCONTENT_SOURCE_DIR_MAUL-UNICODE=<path>` builds a local checkout instead, offline.

The public headers are plain C17 and C++17, so programs that include them need neither a C23 compiler nor C at all.

## Build and install

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix /path/to/prefix
```

Then, from CMake:

```cmake
find_package(maul-locale REQUIRED CONFIG)
target_link_libraries(app PRIVATE maul-locale::maul-locale)
```

or through pkg-config, `pkg-config --cflags --libs maul-locale`. A project can also add the source tree with `add_subdirectory`; tests, benchmarks and the tools are then off by default.

| Option | Default | Effect |
|---|---|---|
| `MAUL_LOCALE_BUILD_SHARED` | `OFF` | A shared library instead of a static one. |
| `MAUL_LOCALE_BUILD_TOOLS` | `ON` at top level | The data generator `mlocgen` and the catalog tool `mloccat` (see [Catalogs](catalogs.md)). |

The data files in `src/generated/*.mloc` are not installed; a program ships the ones it needs, see [Data](data.md). Each GitHub release also carries them as separate downloads.

## The model

- **One CLDR release.** Every table comes from CLDR 49.0 and its Unicode version; `mlocGetCldrVersion` says which. Results follow CLDR's data and, where CLDR leaves the algorithm open, ICU4C's and ECMA-402's behavior.
- **Data is passed in.** Every function that needs locale data takes an `mlocDataSet`: the data compiled into the library, data files the program holds in memory, or both, searched in order. The library reads no file, no environment variable and no system locale or clock.
- **No allocation.** Formatters, collators and compiled messages are fixed-size structs the caller declares, with nothing to destroy. They point into the data they were made from, and some also to what they were given: a message formatter to the data set and the program's functions, a bound message to its compiled message and formatter, a catalog to its bytes. The library checks such memory once and then reads it as checked, so all of it must outlive what points to it and stay as it was.
- **Any thread.** Data and formatters are read only once made, so every function is safe from any thread, and one formatter may be used by several threads at once. An output buffer is used by one call at a time.
- **Same results everywhere.** A `double` given as input is converted to its shortest decimal digits by integer arithmetic, and conversions are exact; identical input gives identical output on every platform and compiler.

## Results and output

A fallible function returns an `mlocResult`: `mloc_success` (0), a positive outcome that is not an error, or a negative error. An error from -1 to -31 has the same name, meaning and value in every Maul library, so a program that links several reads a logged value one way; this library's own errors count down from -32. `mloc_errorCapacity` means the caller's buffer or cache is too small and a larger one succeeds; `mloc_errorLimit` means an argument passes one of the library's fixed bounds, each published as an `MLOC_MAX_*` constant in its header, so no room helps. `mloc_fellBack` means the call used the data of a locale the asked one falls back to; `mloc_withFallbacks` means a message was written with fallbacks for the parts that failed. Functions that read text return an `mlocTextResult`, the status and the byte offset where the fault starts. `mlocGetResultName` names any result for logs.

Every formatting function writes UTF-8 into an `mlocOutput`: a buffer and its capacity. The call sets `length` to the bytes written, or with `mloc_errorCapacity` to the bytes needed, leaving the buffer as it was; a call that fails otherwise leaves `length` and the buffer as they were, except a message whose own function changes the text's length, an argument or the functions before the text is formatted again, which clears the buffer ([messages](messages.md#arguments-and-functions)). A null buffer with capacity 0 measures. No NUL is written.

An output never overlaps an input unless the call's header says it may. The date, time, instant and duration arithmetic, `mlocConvertUnit`, `mlocGetListGender` and `mlocGetParentLocale` may write their result over their input, so `mlocAddToDate(&date, &duration, mloc_overflowConstrain, &date)` moves a date in place.

```c
mlocDataSet set = {&builtin, 1};
mlocLocale locale;
mlocTextResult parsed = mlocParseLocale("de-CH", 5, &locale);
mlocNumberFormatter formatter;
mlocDecimal value;
char text[64];
mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
if (parsed.status == mloc_success &&
    mlocInitNumberFormatter(&set, &locale, nullptr, &formatter) >= 0 &&
    mlocMakeShortestDecimal(1234567.891, &value) == mloc_success &&
    mlocNumberFormatter_Format(&formatter, &value, &output) == mloc_success)
{
    // text holds output.length bytes: "1'234'567.891"
}
```

`builtin` is the compiled-in data, opened once with `mlocOpenBuiltinData`. Set an output's `part` function to receive the parts of the text as it is written (the integer digits, a currency symbol, a placeholder of a message) with their byte ranges, for styling or for accessibility.

Each status a function can return is listed in its header and in the [API reference](https://rawframe-project.github.io/maul-locale/api.html).

## Where to go next

- [Data](data.md): which parts need which data files, and how to add or update them.
- [Locales](locales.md): parsing, canonical forms, fallback, matching a user's languages, regional preferences, display names.
- [Numbers](numbers.md): decimals, plural rules, numbers, currencies, units, numbers in words and parsing.
- [Dates and times](dates-and-times.md): calendars, time zones, Temporal values and strings, formatting, relative time and durations.
- [Messages](messages.md): MessageFormat 2 and lists.
- [Grammar](grammar.md): case suffixes and particles on names and numbers.
- [Catalogs](catalogs.md): translation catalogs and the `mloccat` tool.
- [Collation](collation.md): sorting, sort keys, search and alphabetic indexes.
- [Segmentation](segmentation.md): words, sentences and line breaks as a language tailors them.
