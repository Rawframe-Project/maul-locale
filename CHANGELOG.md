# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API, the ABI and every data format.

## [0.6.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.5.0...v0.6.0) (2026-11-01)


### ⚠ BREAKING CHANGES

* **locale:** every function that reads an `mlocLocale` now refuses with `mloc_errorInvalid` a locale whose fields are not NUL-padded text in canonical syntax, where it used to go on and could report success. A program that fills an `mlocLocale` by hand must keep that syntax, or make the locale with `mlocParseLocale` or `mlocCanonicalizeLocale`.

### Fixed

* **catalog:** refuse a key past a replaced first catalog's keys ([#336](https://github.com/Rawframe-Project/maul-locale/issues/336)) ([e6f8cb4](https://github.com/Rawframe-Project/maul-locale/commit/e6f8cb4e058eb220c21321f3b955ee485a185e9d)), closes [#335](https://github.com/Rawframe-Project/maul-locale/issues/335)
* **catalog:** say a catalog's bytes stay as they were while it is open ([#341](https://github.com/Rawframe-Project/maul-locale/issues/341)) ([61bfb20](https://github.com/Rawframe-Project/maul-locale/commit/61bfb20dff71be0e90d1f21a9ea543dace9dbeaf)), closes [#340](https://github.com/Rawframe-Project/maul-locale/issues/340)
* **collate:** keep a long mark run's count from wrapping ([#374](https://github.com/Rawframe-Project/maul-locale/issues/374)) ([2097c4c](https://github.com/Rawframe-Project/maul-locale/commit/2097c4ceeb16002db48cff431b3abfa26ad2c295)), closes [#373](https://github.com/Rawframe-Project/maul-locale/issues/373)
* **grammar:** refuse a compact number ending in its pattern's digit ([#365](https://github.com/Rawframe-Project/maul-locale/issues/365)) ([f15b607](https://github.com/Rawframe-Project/maul-locale/commit/f15b607def232b4570dd6c928f8961124cff9059)), closes [#364](https://github.com/Rawframe-Project/maul-locale/issues/364)
* **grammar:** refuse a number whose text ends in an affix's digit ([#363](https://github.com/Rawframe-Project/maul-locale/issues/363)) ([447fe39](https://github.com/Rawframe-Project/maul-locale/commit/447fe39b9f869449eaf231fa437d52e69704c9fe)), closes [#362](https://github.com/Rawframe-Project/maul-locale/issues/362)
* **grammar:** size a stem's tail to the largest language room ([#353](https://github.com/Rawframe-Project/maul-locale/issues/353)) ([73013f0](https://github.com/Rawframe-Project/maul-locale/commit/73013f0800992d003f23787ac01fe5a02d8b1846)), closes [#352](https://github.com/Rawframe-Project/maul-locale/issues/352)
* **locale:** refuse a hand-filled locale whose bytes are no locale's ([#361](https://github.com/Rawframe-Project/maul-locale/issues/361)) ([46054b0](https://github.com/Rawframe-Project/maul-locale/commit/46054b02fbc7f996719d86d159645ad59f45e226)), closes [#360](https://github.com/Rawframe-Project/maul-locale/issues/360)
* **message:** refuse a match entry cleared during selection ([#345](https://github.com/Rawframe-Project/maul-locale/issues/345)) ([b0ed799](https://github.com/Rawframe-Project/maul-locale/commit/b0ed799e08d4dd860f16ae625ab5e9dbe1de1f2c)), closes [#344](https://github.com/Rawframe-Project/maul-locale/issues/344)
* say kept memory stays as it was; refuse changed message calls ([#343](https://github.com/Rawframe-Project/maul-locale/issues/343)) ([a096334](https://github.com/Rawframe-Project/maul-locale/commit/a09633462f315aec7abfc5184a93781a2c45c98d)), closes [#342](https://github.com/Rawframe-Project/maul-locale/issues/342)
* **spellout:** refuse a substitution its rule cannot make ([#393](https://github.com/Rawframe-Project/maul-locale/issues/393)) ([783dce8](https://github.com/Rawframe-Project/maul-locale/commit/783dce89d4e7355e82c6da5d11a79fba62638cbb)), closes [#392](https://github.com/Rawframe-Project/maul-locale/issues/392)

## [0.5.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.4.0...v0.5.0) (2026-10-31)

### Changed

- **Breaking: the public enumerations are fixed-width integers.** Each
  `mlocX` kind is an `int32_t` typedef with named constants
  (`mlocTemporalFormat`, a set of bits, is `uint32_t`), no longer a C
  `enum` type; the constants keep their values. C++ code that overloads
  on these types or converts to them as enums changes, and a part
  function takes `mlocPart` in place of `uint32_t`
  ([#255](https://github.com/Rawframe-Project/maul-locale/pull/255)).
  `mlocFavor`, `mlocComponent`, `mlocHourFormat`, `mlocMeasurementSystem`
  and `mlocPaperSize` are `int32_t` too, and every struct field of a kind
  takes its kind's typedef. `mlocLocalePreferences` puts its kinds first
  and grows from 19 to 52 bytes; `mlocDecimal` moves `kind` before
  `count` and grows from 52 to 56 bytes. Recompile against the new
  headers, and store again any `mlocLocalePreferences` or `mlocDecimal`
  kept as bytes. A negative value of any kind is refused with
  `mloc_errorInvalid`
  ([#313](https://github.com/Rawframe-Project/maul-locale/pull/313)).
- **Breaking: a failed call leaves its outputs as they were.** It writes
  only what the failure defines, which each output's documentation now
  says: the length (or size) needed with `mloc_errorCapacity`, the
  offset where a parse stopped, a message's faults. Calls that wrote
  more before:
  - a strict `mlocFormatMessage` that returns `mloc_errorFallback` no
    longer sets the length to 0 or writes into the buffer
    ([#251](https://github.com/Rawframe-Project/maul-locale/pull/251));
  - `mlocSpelloutFormatter_Format` failing with anything but
    `mloc_errorCapacity` writes neither text nor length
    ([#268](https://github.com/Rawframe-Project/maul-locale/pull/268));
  - `mlocNumberFormatter_Parse` and `mlocCurrencyFormatter_Parse` past
    the decimal's bounds (`mloc_errorLimit`) write neither the end nor
    the currency code
    ([#270](https://github.com/Rawframe-Project/maul-locale/pull/270)),
    and `mlocSpelloutFormatter_Parse` failing with anything but
    `mloc_errorSyntax` no longer sets the end to 0
    ([#272](https://github.com/Rawframe-Project/maul-locale/pull/272));
  - `mlocRelativeFormatter_FormatDayTime` whose day fails, and a
    coordinate `mlocUnitFormatter_Format` whose amount fails, write
    nothing ([#276](https://github.com/Rawframe-Project/maul-locale/pull/276),
    [#281](https://github.com/Rawframe-Project/maul-locale/pull/281)).

  A program that read an output after one of these failures reads the
  value it set before.
- **Breaking: `mlocSegmenter_NextBreak` without a boundary.** With
  `mloc_done` and `mloc_needMoreText` the offset is 0 and the mandatory
  flag false; keep the last boundary in a variable of your own while
  the call returns `mloc_success`. A call it refuses with
  `mloc_errorInvalid` (a segmenter the library did not make, a null
  pointer, a call from the segmenter's own breaker) leaves both outputs
  as they were
  ([#211](https://github.com/Rawframe-Project/maul-locale/pull/211),
  [#220](https://github.com/Rawframe-Project/maul-locale/pull/220),
  [#302](https://github.com/Rawframe-Project/maul-locale/pull/302)).
- **Breaking: a locale output may not be an input of
  `mlocMaximizeLocale`, `mlocMinimizeLocale` or `mlocCombineMatch`.**
  Such a call returns `mloc_errorInvalid`; write the result to another
  locale and copy it back
  ([#265](https://github.com/Rawframe-Project/maul-locale/pull/265)).
- A buffer too small for its text or key is left as it was: every
  formatting call, `mlocFormatMessage`, the IXDTF strings and the sort
  keys returned `mloc_errorCapacity` with as much text as fit written,
  and now write only the length needed. A call that measures with a
  null buffer is unchanged
  ([#310](https://github.com/Rawframe-Project/maul-locale/pull/310),
  [#314](https://github.com/Rawframe-Project/maul-locale/pull/314),
  [#315](https://github.com/Rawframe-Project/maul-locale/pull/315)).
- A program function must give the same text for the same value within
  one format call: a message over 512 bytes, or one whose parts are
  reported, is formatted again, and a second text of another length is
  refused with `mloc_errorInvalid`, the buffer cleared, no part reported
  past the text first written
  ([#315](https://github.com/Rawframe-Project/maul-locale/pull/315),
  [#317](https://github.com/Rawframe-Project/maul-locale/pull/317)).
- A Chinese or Dangi date outside the tables' 1600 to 2400 takes about
  a sixth of the time it did, within 1.3 times ICU4C 79.1's; results are
  unchanged
  ([#241](https://github.com/Rawframe-Project/maul-locale/pull/241),
  [#243](https://github.com/Rawframe-Project/maul-locale/pull/243),
  [#245](https://github.com/Rawframe-Project/maul-locale/pull/245),
  [#247](https://github.com/Rawframe-Project/maul-locale/pull/247),
  [#250](https://github.com/Rawframe-Project/maul-locale/pull/250)).
- Every output's documentation says what a failure leaves in it, and
  `base.h` states the rule and the three calls that clear a buffer
  instead; the headers also say which outputs may be their input
  ([#263](https://github.com/Rawframe-Project/maul-locale/pull/263),
  [#299](https://github.com/Rawframe-Project/maul-locale/pull/299),
  [#325](https://github.com/Rawframe-Project/maul-locale/pull/325)).

### Fixed

- `mlocGetName` with `mloc_nameLanguage` read past a stack variable for
  a code whose variants and extensions filled the locale's tail exactly
  ([#229](https://github.com/Rawframe-Project/maul-locale/pull/229)).
- A negative unit width or coordinate is refused; where enumerations
  are signed (MSVC) it read before a table
  ([#254](https://github.com/Rawframe-Project/maul-locale/pull/254)).
- `mlocMessageFormatter_Bind` refuses a buffer that overlaps the
  compiled message or the formatter, which it overwrote before
  ([#298](https://github.com/Rawframe-Project/maul-locale/pull/298)).
- `mlocInitSearcher` and `mlocInitSearcherUtf16` leave the searcher of a
  refused call as it was
  ([#307](https://github.com/Rawframe-Project/maul-locale/pull/307)),
  and a search that fails no longer leaves a rejected candidate's
  bounds in its outputs
  ([#320](https://github.com/Rawframe-Project/maul-locale/pull/320)).
- `mlocCompileMessage` refusing a message for an error it finds only
  once written (a data model error, a chain or resolution bound) clears
  the bytes it wrote, so the buffer holds no compiled message, instead
  of the refused records
  ([#323](https://github.com/Rawframe-Project/maul-locale/pull/323)).
- `mlocCatalogChain_FormatMessage` and `mlocCatalogChain_FormatKey`
  report the reason and position only once the text is written
  ([#324](https://github.com/Rawframe-Project/maul-locale/pull/324)).

## [0.4.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.3.0...v0.4.0) (2026-10-29)

### Added

- `mlocSpelloutWorkspace`, the 64 KiB a spell-out parse searches in,
  which the caller gives `mlocSpelloutFormatter_Parse`, so that the
  search's memory is not on the stack: under Emscripten, reading "five"
  back fits the default 64 KB stack
  ([#197](https://github.com/Rawframe-Project/maul-locale/pull/197)).

### Changed

- **Breaking: the data format is 0x80000006, and catalogs and compiled
  messages have formats of their own.** Data files of 0.3.x are refused
  with `mloc_errorVersion`: take this release's data files or
  regenerate them with its `mlocgen`. A catalog is no longer a data file:
  `mloccat compile` writes `<locale>.mlct` files, and
  `mlocOpenCatalog(bytes, size, catalogOut)` opens the file's bytes,
  which must outlive the catalog, with no `mlocOpenData` before it. A
  compiled message has its own format (magic `MLMS`, format 2). Rebuild
  catalogs with this release's `mloccat` and compile stored messages
  again. From now on a stored message survives data and Unicode updates,
  unless its keys or names hold a code point its compiler's Unicode
  version left unassigned
  ([#154](https://github.com/Rawframe-Project/maul-locale/pull/154),
  [#155](https://github.com/Rawframe-Project/maul-locale/pull/155)).
- **Breaking: public calls are renamed by the family's naming rule.** A
  call that acts on one of the library's objects is
  `mlocObject_Verb` (`mlocFormatNumber` is
  `mlocNumberFormatter_Format`, `mlocNextSegmentBreak` is
  `mlocSegmenter_NextBreak`), and `mlocResultName` is
  `mlocGetResultName`. Every `mlocInit` and `mlocOpen` call but
  `mlocOpenBuiltinData` takes its output last instead of first. 61 calls
  are renamed; the pull request lists each one, old to new
  ([#185](https://github.com/Rawframe-Project/maul-locale/pull/185)).
- **Breaking: result values change, and three are renamed.**
  `mloc_errorData` is `mloc_errorFormat` (-8), `mloc_errorDataVersion`
  is `mloc_errorVersion` (-7), and strict mode's old `mloc_errorFormat`
  is `mloc_errorFallback` (-34). `mloc_errorSyntax` is -32 and
  `mloc_errorNotFound` -33. A fixed bound (a decimal's digits, a unit's
  parts, a catalog chain's length, a message's bounds) is refused with
  the new `mloc_errorLimit` (-9) instead of `mloc_errorCapacity` or
  `mloc_errorInvalid`, and a message past a bound is refused when it is
  compiled instead of when it is bound. `mlocDecimalDigits`,
  `mlocDecimalMaxExponent`, `mlocUnitMaxParts` and
  `MLOC_CATALOG_CHAIN_MAX` are `MLOC_MAX_DECIMAL_DIGITS`,
  `MLOC_MAX_DECIMAL_EXPONENT`, `MLOC_MAX_UNIT_PARTS` and
  `MLOC_MAX_CATALOG_CHAIN`. Rebuild against the new headers, rename the
  statuses and constants, and replace any stored raw values
  ([#146](https://github.com/Rawframe-Project/maul-locale/pull/146),
  [#147](https://github.com/Rawframe-Project/maul-locale/pull/147)).
- **Breaking: `mlocDate`, `mlocTime` and `mlocDecimal` have no
  `reserved` field.** Stop setting it, and compare two decimals by their
  fields, not with `memcmp`
  ([#143](https://github.com/Rawframe-Project/maul-locale/pull/143)).
- **Breaking: spell-out parsing reads back every text a formatter
  writes.** Long texts, such as Lithuanian's long numbers, which were
  refused with `mloc_errorLimit`, now read back to their value: a
  span of text searched once is kept at its own depth and every deeper
  one, where ICU4C searches it again and can stop at its recursion
  limit. `mlocSpelloutFormatter_Parse` takes an `mlocSpelloutWorkspace*`
  after the text's length (see Added). Only text crafted against the
  search still meets `MLOC_MAX_SPELLOUT_STEPS`
  ([#197](https://github.com/Rawframe-Project/maul-locale/pull/197)).
- **Breaking: zoned differences across a day-long clock jump are
  refused.** `mlocZonedDateTimeUntil` and `mlocZonedDateTimeTotal`
  refuse with `mloc_errorInvalid` a difference rounded or totaled across
  a jump of the zone's clocks by a whole day (`Pacific/Apia` on
  2011-12-30), where they gave a wrong answer or divided by zero
  ([#170](https://github.com/Rawframe-Project/maul-locale/pull/170)).
- **Breaking: Maul Unicode 0.6.0.** Building against an installed Maul
  Unicode needs 0.6.0. A complex breaker that calls back into the
  segmenter it serves is refused with `mloc_errorInvalid`, where it
  recursed until the stack overflowed; `mlocComplexBreakFunction`'s
  documentation says so
  ([#173](https://github.com/Rawframe-Project/maul-locale/pull/173),
  [#174](https://github.com/Rawframe-Project/maul-locale/pull/174)).
- Every output parameter's name ends in `Out` (`textOut`, `bufferOut`,
  `keyOut`, `faultsOut`, `storageOut`). Calls compile unchanged
  ([#199](https://github.com/Rawframe-Project/maul-locale/pull/199)).
- An alphabetic index's bytes start with `MLIX`, and `index.h` says
  they are valid only in the process that built them, like a
  collator's: an index is not stored
  ([#121](https://github.com/Rawframe-Project/maul-locale/pull/121)).
- `docs/user/data.md` says a data file's bytes must not change once
  opened, and how to replace a file
  ([#97](https://github.com/Rawframe-Project/maul-locale/pull/97)).
- Spell-out parsing finds text faster: the fuzz corpus reads back 6%
  faster in a Release build
  ([#200](https://github.com/Rawframe-Project/maul-locale/pull/200)).

### Fixed

- An alphabetic index put a name in the wrong bucket when a hidden
  bucket came before the one a display chain ends at (labels A, Ch*, D
  and Gh* put "Gz" in A, where ICU4C puts it in D). No CLDR 49 locale
  has such labels
  ([#123](https://github.com/Rawframe-Project/maul-locale/pull/123)).
- A failed `mlocMessageFormatter_Bind` leaves no bound message in the
  buffer, as documented. Before, most refusals left an earlier message
  bound, and it still formatted
  ([#149](https://github.com/Rawframe-Project/maul-locale/pull/149)).
- On 32-bit targets, lengths that pass `SIZE_MAX` no longer wrap: a
  formatter's output length saturates at `SIZE_MAX`, and a sort key's
  length needed is counted in 64 bits. Before, a text near the limit got
  a needed size too short, or a short text reported as `mloc_success`.
  No byte was written past the capacity
  ([#139](https://github.com/Rawframe-Project/maul-locale/pull/139),
  [#141](https://github.com/Rawframe-Project/maul-locale/pull/141)).
- Hostile bytes are refused where they were read out of bounds: a
  compiled message or catalog whose counts wrap the size check, a number
  table unit whose size wraps on a 32-bit target, and a data file whose
  likely subtags table lacks `und`, which is now refused when it opens
  ([#135](https://github.com/Rawframe-Project/maul-locale/pull/135),
  [#137](https://github.com/Rawframe-Project/maul-locale/pull/137),
  [#180](https://github.com/Rawframe-Project/maul-locale/pull/180)).
- A zoned difference rounded to days across a day-long jump no longer
  stops a build with assertions on
  ([#167](https://github.com/Rawframe-Project/maul-locale/pull/167)).
- `mlocgen` refuses a CLDR or tzdb token too long for its buffer, an
  element with more than 64 distinguishing attributes, and an alias of
  64 bytes or more, naming the file and the reason, where it cut or
  skipped them; it no longer passes `qsort` a null pointer. The
  generated data does not change
  ([#92](https://github.com/Rawframe-Project/maul-locale/pull/92),
  [#106](https://github.com/Rawframe-Project/maul-locale/pull/106),
  [#110](https://github.com/Rawframe-Project/maul-locale/pull/110),
  [#113](https://github.com/Rawframe-Project/maul-locale/pull/113)).

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
