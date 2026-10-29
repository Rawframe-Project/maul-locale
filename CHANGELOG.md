# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API, the ABI and every data format.

## [0.4.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.3.0...v0.4.0) (2026-10-29)


### ⚠ BREAKING CHANGES

* public parameters are renamed in the headers and their documentation: output to textOut, buffer to bufferOut, key to keyOut, faults to faultsOut and storage to storageOut. Calls compile unchanged; only text that quotes the old names needs them changed.
* **spellout:** mlocSpelloutFormatter_Parse reads back texts it refused with mloc_errorLimit before, Lithuanian's long numbers among them, and keeps a span of text's result at its own depth and every deeper one where ICU searches it again. A program that relied on mloc_errorLimit for such a text now gets the value. mlocSpelloutFormatter_Parse takes an mlocSpelloutWorkspace* after the text's length: pass the address of one (64 KiB, used by one thread at a time; it need not be set).
* **build:** maul-locale needs maul-unicode 0.6.0. A program that links maul-unicode itself, or finds it as an installed package, moves to 0.6.0 with it. `mlocComplexBreakFunction`'s signature is unchanged.
* **duration:** `mlocZonedDateTimeUntil` and `mlocZonedDateTimeTotal` refuse with `mloc_errorInvalid` a difference rounded or totaled across a jump of the zone's clocks by a whole day, where they gave a wrong answer or divided by zero. A program that calls them across such a jump must handle the refusal.
* **message:** compiled messages have their own format: magic `MLMS`, format 2, tagged fields, no longer `MLOC_DATA_FORMAT_VERSION`. Messages compiled by an earlier release are refused with `mloc_errorVersion`; compile them again with `mlocCompileMessage`. Catalogs are format 2 and must be compiled again with this release's `mloccat`. From now on, a stored message survives data updates and Unicode updates, except one whose keys or names hold a code point its compiler's Unicode version left unassigned. That one is refused under another Unicode version.
* **catalog:** catalogs are no longer data files. `mlocOpenCatalog(catalogOut, data)` becomes `mlocOpenCatalog(catalogOut, bytes, size)`: pass the catalog file's bytes, which must outlive the catalog, and drop the `mlocOpenData` call before it. `mloccat compile` writes `<locale>.mlct` files in the new format, and catalogs compiled by an earlier release are refused; compile them again. The data format is 0x80000006, so data files of earlier releases are refused too; use this release's generated files.
* fixed bounds are refused with `mloc_errorLimit` (-9) instead of `mloc_errorCapacity` or `mloc_errorInvalid`, and a compiled message past a message bound is refused when it is compiled instead of when it is bound. `mlocDecimalDigits`, `mlocDecimalMaxExponent`, `mlocUnitMaxParts` and `MLOC_CATALOG_CHAIN_MAX` are now `MLOC_MAX_DECIMAL_DIGITS`, `MLOC_MAX_DECIMAL_EXPONENT`, `MLOC_MAX_UNIT_PARTS` and `MLOC_MAX_CATALOG_CHAIN`. Compiling a message needs more room for messages with many expressions. A program must rename those constants and treat `mloc_errorLimit` where it handled Capacity or Invalid for these bounds.
* **family:** the result values change and three are renamed. `mloc_errorData` is now `mloc_errorFormat` (-8), `mloc_errorDataVersion` is `mloc_errorVersion` (-7), and the strict-mode `mloc_errorFormat` is `mloc_errorFallback` (-34). `mloc_errorSyntax` is -32 and `mloc_errorNotFound` -33. A compiled message of another message format is refused with `mloc_errorVersion`. A program must rebuild against the new header and rename the three statuses it uses, and anything that stored or compared raw values must take the new ones.
* **family:** `mlocDate`, `mlocTime` and `mlocDecimal` no longer have a `reserved` field. A program that sets it must stop doing so, and a program that compared two decimals with `memcmp` must compare their fields instead.

### Added

* **build:** move to maul-unicode 0.6.0 ([#173](https://github.com/Rawframe-Project/maul-locale/issues/173)) ([cc09985](https://github.com/Rawframe-Project/maul-locale/commit/cc09985f6ce4cb418a0a0493ab499c0dd78ad815))
* **catalog:** give catalogs their own container, opened from bytes ([#154](https://github.com/Rawframe-Project/maul-locale/issues/154)) ([daa3e9d](https://github.com/Rawframe-Project/maul-locale/commit/daa3e9d82f62a9e3508f3bd744caf6df0de4b8a0))
* **message:** give compiled messages their own tagged format ([#155](https://github.com/Rawframe-Project/maul-locale/issues/155)) ([371cfd2](https://github.com/Rawframe-Project/maul-locale/commit/371cfd260e364bf4be87e6d3ce11be23cdc46c62))


### Fixed

* **collate:** count a sort key's length in 64 bits ([#141](https://github.com/Rawframe-Project/maul-locale/issues/141)) ([ccef839](https://github.com/Rawframe-Project/maul-locale/commit/ccef839e5a6b9458fd083d3a7b34878781b1361d)), closes [#140](https://github.com/Rawframe-Project/maul-locale/issues/140)
* **duration:** drop the window assertion a day-long zone jump breaks ([#167](https://github.com/Rawframe-Project/maul-locale/issues/167)) ([97a78ef](https://github.com/Rawframe-Project/maul-locale/commit/97a78efc2e40e4b4cf6602c3788be7097300b27e)), closes [#166](https://github.com/Rawframe-Project/maul-locale/issues/166)
* **duration:** refuse zoned rounding across a day-long clock jump ([#170](https://github.com/Rawframe-Project/maul-locale/issues/170)) ([a576c62](https://github.com/Rawframe-Project/maul-locale/commit/a576c624f1c66a924ce5c40b5e5167a4aae6177b))
* **generator:** refuse a token too long for its buffer ([#92](https://github.com/Rawframe-Project/maul-locale/issues/92)) ([9dc3eb1](https://github.com/Rawframe-Project/maul-locale/commit/9dc3eb1a8454e8eef4bc9887750c3ba279a735f5)), closes [#82](https://github.com/Rawframe-Project/maul-locale/issues/82)
* **generator:** refuse too many attributes and an overlong alias ([#113](https://github.com/Rawframe-Project/maul-locale/issues/113)) ([1e14f2a](https://github.com/Rawframe-Project/maul-locale/commit/1e14f2a04c52a798f214dfe26aa4e508486984fe)), closes [#108](https://github.com/Rawframe-Project/maul-locale/issues/108) [#109](https://github.com/Rawframe-Project/maul-locale/issues/109)
* **generator:** refuse what the generators would cut short ([#106](https://github.com/Rawframe-Project/maul-locale/issues/106)) ([0c5540b](https://github.com/Rawframe-Project/maul-locale/commit/0c5540b1dd2711a94f5a807b10d07d3a78dd771a)), closes [#93](https://github.com/Rawframe-Project/maul-locale/issues/93)
* **generator:** sort nothing without a null pointer ([#110](https://github.com/Rawframe-Project/maul-locale/issues/110)) ([88b56ec](https://github.com/Rawframe-Project/maul-locale/commit/88b56ec60b5902a1d1ce56c4d7368e824a6bfe2d)), closes [#107](https://github.com/Rawframe-Project/maul-locale/issues/107)
* **index:** resolve hidden buckets before numbering the shown ones ([#123](https://github.com/Rawframe-Project/maul-locale/issues/123)) ([9087135](https://github.com/Rawframe-Project/maul-locale/commit/90871357a9b5214f1957a6b495dc0cae3e2eab8c)), closes [#122](https://github.com/Rawframe-Project/maul-locale/issues/122)
* **index:** stamp an index MLIX, with no version ([#121](https://github.com/Rawframe-Project/maul-locale/issues/121)) ([6e20ef8](https://github.com/Rawframe-Project/maul-locale/commit/6e20ef875f30e417446dbf7b3b2ad1fb535e1a3b)), closes [#120](https://github.com/Rawframe-Project/maul-locale/issues/120)
* **locale:** require und in the likely table and check every LSR made ([#180](https://github.com/Rawframe-Project/maul-locale/issues/180)) ([d555165](https://github.com/Rawframe-Project/maul-locale/commit/d55516522b5c7fa1b27693d0aa4a420fe92fa524)), closes [#179](https://github.com/Rawframe-Project/maul-locale/issues/179)
* **message:** leave no bound message after any refused bind ([#149](https://github.com/Rawframe-Project/maul-locale/issues/149)) ([835a2f2](https://github.com/Rawframe-Project/maul-locale/commit/835a2f231124989335be6e85152089e04bf15200)), closes [#148](https://github.com/Rawframe-Project/maul-locale/issues/148)
* **message:** refuse stored counts that wrap the size check ([#135](https://github.com/Rawframe-Project/maul-locale/issues/135)) ([3c8e090](https://github.com/Rawframe-Project/maul-locale/commit/3c8e0908858928ad9ada7dd9ee6409223ad71b9c)), closes [#134](https://github.com/Rawframe-Project/maul-locale/issues/134)
* **number:** saturate a formatter's output length at SIZE_MAX ([#139](https://github.com/Rawframe-Project/maul-locale/issues/139)) ([8b45901](https://github.com/Rawframe-Project/maul-locale/commit/8b45901420555bda1502a77cbdaeb2e9e67c2d6d)), closes [#138](https://github.com/Rawframe-Project/maul-locale/issues/138)
* **number:** sum a number table's size in 64 bits ([#137](https://github.com/Rawframe-Project/maul-locale/issues/137)) ([b1ec7af](https://github.com/Rawframe-Project/maul-locale/commit/b1ec7af7f66492d5746731b43ea8eeb7f45e380a)), closes [#136](https://github.com/Rawframe-Project/maul-locale/issues/136)
* **oracle:** refuse typeof of a Temporal member the interpreter lacks ([#191](https://github.com/Rawframe-Project/maul-locale/issues/191)) ([7253ade](https://github.com/Rawframe-Project/maul-locale/commit/7253adef7938ec00384dcaaf09a2882dccfb01c0)), closes [#189](https://github.com/Rawframe-Project/maul-locale/issues/189)
* **oracle:** write test262's durations with the Temporal writer ([#190](https://github.com/Rawframe-Project/maul-locale/issues/190)) ([b520b2b](https://github.com/Rawframe-Project/maul-locale/commit/b520b2beb5825be0727f585a520cb2a17cf1110b)), closes [#186](https://github.com/Rawframe-Project/maul-locale/issues/186)
* **spellout:** read back every text a formatter writes ([#197](https://github.com/Rawframe-Project/maul-locale/issues/197)) ([ed8cc05](https://github.com/Rawframe-Project/maul-locale/commit/ed8cc05202ad69d40669864e5695252244139cc6))


### Changed

* **family:** take .github v0.0.33 and the family's result values ([#146](https://github.com/Rawframe-Project/maul-locale/issues/146)) ([08db544](https://github.com/Rawframe-Project/maul-locale/commit/08db5448d50d795208606443dfcea5d5acd6214f))
* give every public output an Out suffix ([#199](https://github.com/Rawframe-Project/maul-locale/issues/199)) ([249e446](https://github.com/Rawframe-Project/maul-locale/commit/249e4465825957c3dd1322b31a4fb2c9d7a1f050))
* refuse fixed bounds with mloc_errorLimit and name each bound ([#147](https://github.com/Rawframe-Project/maul-locale/issues/147)) ([ab5cae9](https://github.com/Rawframe-Project/maul-locale/commit/ab5cae9ab0b8bb6003ac54575da0f91eb4d17ea6))
* **spellout:** find a key's first byte with memchr ([#200](https://github.com/Rawframe-Project/maul-locale/issues/200)) ([4cbab4c](https://github.com/Rawframe-Project/maul-locale/commit/4cbab4caf98d915eed4f9081f92ee788ef5b2d8b)), closes [#195](https://github.com/Rawframe-Project/maul-locale/issues/195)


### Build

* **family:** take .github v0.0.29 and drop the reserved fields ([#143](https://github.com/Rawframe-Project/maul-locale/issues/143)) ([c1264ef](https://github.com/Rawframe-Project/maul-locale/commit/c1264ef79fc712db5ac1b5f81ab31efdcc0f235d))

## [0.3.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.2.3...v0.3.0) (2026-10-25)

### Added

- Text segmentation as a locale tailors it, in `maul-locale/segment.h`.
  `mlocInitSegmenter` starts a word, sentence or line segmenter over a
  text or its first piece, `mlocNextSegmentBreak` reports each boundary
  with whether a line must end there, and `mlocFeedSegmenter` hands it
  the next piece; `base.h` gains `mloc_done` and `mloc_needMoreText`. It
  follows CLDR's segments files: the sentence suppressions of seven
  languages with `-u-ss-standard`, as ICU4C's filtered break iterator
  applies them, Greek sentences ending at a semicolon, POSIX English's
  "a.b" kept as one word, Japanese Hiragana and ideograph runs kept as
  one word, and Chinese and Japanese lines with CJ as an ideograph,
  which `-u-lb-strict`, `normal` and `loose` set. A segmenter is a
  512-byte value that may be copied, and text may come in pieces of any
  size. `mlocSetSegmenterComplexBreaker` passes a Thai, Lao, Khmer or
  Burmese word segmenter to Maul Unicode. Against ICU4C 79.1, 36 of 2031
  cases differ, each where ICU4C's own data or rules depart from CLDR's;
  `docs/user/segmentation.md` lists them
  ([#78](https://github.com/Rawframe-Project/maul-locale/pull/78),
  [#83](https://github.com/Rawframe-Project/maul-locale/pull/83),
  [#90](https://github.com/Rawframe-Project/maul-locale/pull/90)).

### Changed

- **Breaking: the data format is 0x80000005.** The new `segments.mloc`
  (12 KB) holds the segmentation tailorings. Data files, catalogs and
  compiled messages made by 0.2.x (format 0x80000002) are refused with
  `mloc_errorDataVersion`: take this release's data files or regenerate
  them with its `mlocgen`, rebuild catalogs with its `mloccat`, and
  compile stored messages again
  ([#78](https://github.com/Rawframe-Project/maul-locale/pull/78),
  [#83](https://github.com/Rawframe-Project/maul-locale/pull/83),
  [#90](https://github.com/Rawframe-Project/maul-locale/pull/90)).
- **Breaking: `en-US-u-va-posix` is POSIX English.**
  `mlocCanonicalizeLocale` gives `en-US-u-va-posix` for `en-US-posix`
  and `en_US_POSIX`, as UTS #35's legacy variant mapping says, and
  `en-US-u-va-posix` now formats and sorts as POSIX English, where it
  used to fall back to `en_US`. Its parent is `en-US`. A program that
  stored the canonical form canonicalizes it again, and one that
  compares with `en-US-posix` compares with `en-US-u-va-posix`
  ([#91](https://github.com/Rawframe-Project/maul-locale/pull/91)).
- Maul Unicode 0.3.0. Building against an installed Maul Unicode needs
  0.3.0, since before 1.0 the package accepts only its exact version,
  and a shared maul-locale built against an installed shared Maul
  Unicode loads `libmaul-unicode.so.0.3.0`; a build that fetches Maul
  Unicode embeds it. No data file, result or API changes from it
  ([#89](https://github.com/Rawframe-Project/maul-locale/pull/89)).
- On 32-bit x86 the library is built with SSE2 math
  (`-msse2 -mfpmath=sse`), so it needs a processor with SSE2, and every
  build now leaves trapping math at the compiler's default, from the
  family's compiler flags. The library computes in integers only, so no
  result depends on either
  ([#85](https://github.com/Rawframe-Project/maul-locale/pull/85)).

### Fixed

- `mlocgen` refuses a segments file with a surrogate, a repeated
  Japanese or CJ rule part, or a variable longer than it reads, naming
  the file and the reason, where it took the first two or refused the
  last for the wrong reason. The generated data does not change
  ([#81](https://github.com/Rawframe-Project/maul-locale/pull/81)).

## [0.2.3](https://github.com/Rawframe-Project/maul-locale/compare/v0.2.2...v0.2.3) (2026-10-25)

### Fixed

- `mloccat export-po` keeps a source entry's comment whole when its last line is empty. The PO writer dropped that line, so the translator saw the comment one line short ([#71](https://github.com/Rawframe-Project/maul-locale/pull/71)).
- `mloccat` reads an extracted comment line ending in several CRs as ending there, so a comment is the same after `export-po` writes it again ([#74](https://github.com/Rawframe-Project/maul-locale/pull/74)).

## [0.2.2](https://github.com/Rawframe-Project/maul-locale/compare/v0.2.1...v0.2.2) (2026-10-25)

### Fixed

- `mloccat import-po` refuses a PO file with a NUL byte in a string, written as is or as an escape, naming its line. The byte cut the string short, so the import matched the wrong key, or took the entry as the file's header ([#67](https://github.com/Rawframe-Project/maul-locale/pull/67)).
- `mloccat import-xliff` refuses an XLIFF file that holds a NUL byte, naming its line, and `import-po` refuses one in an extracted comment; the byte cut a translation, a key or a comment short ([#70](https://github.com/Rawframe-Project/maul-locale/pull/70)).

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
