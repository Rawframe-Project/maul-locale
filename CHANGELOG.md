# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API, the ABI and every data format.

## [0.5.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.4.0...v0.5.0) (2026-10-31)


### ⚠ BREAKING CHANGES

* `mlocFavor`, `mlocComponent`, `mlocHourFormat`, `mlocMeasurementSystem` and `mlocPaperSize` are `int32_t`. `mlocDate.calendar`, `mlocLocalePreferences.calendar`, `mlocDecimal.kind` and the three kinds of `mlocTemporalStringOptions` take their kinds' typedefs. `mlocLocalePreferences` reorders its fields and grows from 19 to 52 bytes, and `mlocDecimal` moves `kind` before `count` and grows from 52 to 56 bytes. A program recompiles against the new headers, and a program that stored either struct's bytes stores them again.
* **relative:** an `mlocRelativeFormatter_FormatDayTime` whose day fails no longer writes the glue and the time into the output or sets its length; both are left as they were. A program that read the length after such a failure reads the value it set before.
* **unit:** an `mlocUnitFormatter_Format` of a coordinate formatter whose amount fails no longer writes the direction's text into the output's buffer; it is left as it was. A program that read the buffer after such a failure finds what it held before.
* **spellout:** an `mlocSpelloutFormatter_Format` that fails with anything but `mloc_errorCapacity` no longer writes text into the output's buffer or sets its length; both are left as they were. A program that read the length after such a failure reads the length it set before.
* **spellout:** an `mlocSpelloutFormatter_Parse` that fails with anything but `mloc_errorSyntax` no longer sets the end offset to 0; it is left as it was. A program that read the end as 0 after such a failure reads the value it set before.
* **number:** an `mlocNumberFormatter_Parse` or `mlocCurrencyFormatter_Parse` that fails with `mloc_errorLimit` no longer writes the end offset or the currency code; both are left as they were. A program that read the end after such a failure reads the value it set before.
* **locale:** the output of `mlocMaximizeLocale`, `mlocMinimizeLocale` and `mlocCombineMatch` must be a locale apart from their inputs; passing an input as the output now returns `mloc_errorInvalid`. To get the result in place, write it to another locale and copy it back.
* the public enumerations are no longer C enum types. Each `mlocX` is `int32_t` (`mlocTemporalFormat` `uint32_t`) with its constants unchanged, so C++ code that overloads on these types or converts to them as enums changes, and a part function takes `mlocPart` in place of `uint32_t`. Struct layouts are unchanged.
* **message:** a strict `mlocFormatMessage` that returns `mloc_errorFallback` no longer sets the output's length to 0 or writes into its buffer; both are left as they were. A program that read the length as 0 after such a failure reads the length it set before.
* **segment:** a call of `mlocSegmenter_NextBreak` from the segmenter's own complex breaker, which is refused with `mloc_errorInvalid`, now leaves `offsetOut` and `mandatoryOut` as they were instead of setting them to 0 and false. A breaker that reads them after such a call reads its own values.
* **segment:** after mloc_done, mloc_needMoreText or a failure, mlocSegmenter_NextBreak's offset is 0 and the mandatory flag false. A program that read the last boundary from the offset after its loop ended keeps it in a variable of its own while the call returns mloc_success.

### Fixed

* **catalog:** report a chain's reason only once the text is written ([#324](https://github.com/Rawframe-Project/maul-locale/issues/324)) ([df3a327](https://github.com/Rawframe-Project/maul-locale/commit/df3a3276c800de0ea44c3a475bdfa6460aa91f11)), closes [#322](https://github.com/Rawframe-Project/maul-locale/issues/322)
* **collate:** leave a sort key buffer too small as it was ([#310](https://github.com/Rawframe-Project/maul-locale/issues/310)) ([e31915e](https://github.com/Rawframe-Project/maul-locale/commit/e31915e2f2ec1162251984976188b3ff81ad5eef)), closes [#303](https://github.com/Rawframe-Project/maul-locale/issues/303)
* hold every kind in its int32_t type ([#313](https://github.com/Rawframe-Project/maul-locale/issues/313)) ([f9a1e5c](https://github.com/Rawframe-Project/maul-locale/commit/f9a1e5cbb9656e1e1a40e16feeeff10a0fe2c93c)), closes [#296](https://github.com/Rawframe-Project/maul-locale/issues/296)
* leave a buffer too small for a format as it was ([#314](https://github.com/Rawframe-Project/maul-locale/issues/314)) ([4e5f4cb](https://github.com/Rawframe-Project/maul-locale/commit/4e5f4cb581afc898d4b284c40df67c0e2ebf29bb)), closes [#308](https://github.com/Rawframe-Project/maul-locale/issues/308) [#305](https://github.com/Rawframe-Project/maul-locale/issues/305) [#306](https://github.com/Rawframe-Project/maul-locale/issues/306)
* **locale:** refuse a locale output that aliases an input ([#265](https://github.com/Rawframe-Project/maul-locale/issues/265)) ([deba3fc](https://github.com/Rawframe-Project/maul-locale/commit/deba3fce36ab53a93dc5f3c2ad50ac31a7610a93)), closes [#264](https://github.com/Rawframe-Project/maul-locale/issues/264)
* **message:** clear what a refused compile wrote ([#323](https://github.com/Rawframe-Project/maul-locale/issues/323)) ([42c2bfa](https://github.com/Rawframe-Project/maul-locale/commit/42c2bfa7bf96c249d82c774ab3822f99cb460649)), closes [#321](https://github.com/Rawframe-Project/maul-locale/issues/321)
* **message:** leave a buffer too small for a message or string as it was ([#315](https://github.com/Rawframe-Project/maul-locale/issues/315)) ([575b64d](https://github.com/Rawframe-Project/maul-locale/commit/575b64d6ef7dc773d37185de17ba068f8c06345b)), closes [#308](https://github.com/Rawframe-Project/maul-locale/issues/308)
* **message:** leave the output of a failed strict format as it was ([#251](https://github.com/Rawframe-Project/maul-locale/issues/251)) ([1c45cc4](https://github.com/Rawframe-Project/maul-locale/commit/1c45cc450171eba124e584620724c4904a99873b)), closes [#249](https://github.com/Rawframe-Project/maul-locale/issues/249)
* **message:** refuse a bind buffer that overlaps its message ([#298](https://github.com/Rawframe-Project/maul-locale/issues/298)) ([c082c5b](https://github.com/Rawframe-Project/maul-locale/commit/c082c5bf2d958267b3edb543b4fff7a8a69b67a0)), closes [#297](https://github.com/Rawframe-Project/maul-locale/issues/297)
* **message:** refuse a parts pass that writes another length ([#317](https://github.com/Rawframe-Project/maul-locale/issues/317)) ([8080a8f](https://github.com/Rawframe-Project/maul-locale/commit/8080a8f66d984f88cabdfdbd18dc513c52ac11b5)), closes [#316](https://github.com/Rawframe-Project/maul-locale/issues/316)
* **names:** stop reading past a language code's full tail ([#229](https://github.com/Rawframe-Project/maul-locale/issues/229)) ([efe13d8](https://github.com/Rawframe-Project/maul-locale/commit/efe13d86df3898be3ac74070c149155c7152d02f)), closes [#228](https://github.com/Rawframe-Project/maul-locale/issues/228)
* **number:** leave the outputs of a parse past the decimal bounds ([#270](https://github.com/Rawframe-Project/maul-locale/issues/270)) ([b8d9f1a](https://github.com/Rawframe-Project/maul-locale/commit/b8d9f1abfbb3e322261832faded621dea43b03f0)), closes [#269](https://github.com/Rawframe-Project/maul-locale/issues/269)
* **relative:** leave the output of a failed day and time format ([#276](https://github.com/Rawframe-Project/maul-locale/issues/276)) ([197f9e0](https://github.com/Rawframe-Project/maul-locale/commit/197f9e0d7bd6d2380ac805208a92a287c818bd18)), closes [#275](https://github.com/Rawframe-Project/maul-locale/issues/275)
* **search:** keep a refused candidate's bounds out of a failed search ([#320](https://github.com/Rawframe-Project/maul-locale/issues/320)) ([e2389df](https://github.com/Rawframe-Project/maul-locale/commit/e2389dfad203b92f8d1a15bd84cf5142b2fe2683)), closes [#319](https://github.com/Rawframe-Project/maul-locale/issues/319)
* **search:** leave the searcher of a refused init as it was ([#307](https://github.com/Rawframe-Project/maul-locale/issues/307)) ([1355273](https://github.com/Rawframe-Project/maul-locale/commit/1355273665d2e2d77a60720b85b4759d3024c1ad)), closes [#304](https://github.com/Rawframe-Project/maul-locale/issues/304)
* **segment:** clear NextBreak's outputs when it finds no boundary ([#211](https://github.com/Rawframe-Project/maul-locale/issues/211)) ([86584bf](https://github.com/Rawframe-Project/maul-locale/commit/86584bfad34b130fd218ddf40e14a180d40ff3d1)), closes [#209](https://github.com/Rawframe-Project/maul-locale/issues/209)
* **segment:** leave NextBreak's outputs when its breaker calls back in ([#220](https://github.com/Rawframe-Project/maul-locale/issues/220)) ([b21ad3c](https://github.com/Rawframe-Project/maul-locale/commit/b21ad3cebc2526709a6dfd4f2dedefa0d4647309)), closes [#218](https://github.com/Rawframe-Project/maul-locale/issues/218)
* **segment:** leave the outputs of a refused NextBreak as they were ([#302](https://github.com/Rawframe-Project/maul-locale/issues/302)) ([c97e6ca](https://github.com/Rawframe-Project/maul-locale/commit/c97e6cad95375e363f9a06a42e00ca65d636139a)), closes [#301](https://github.com/Rawframe-Project/maul-locale/issues/301)
* **spellout:** leave the end of a parse past its limits ([#272](https://github.com/Rawframe-Project/maul-locale/issues/272)) ([00e5f79](https://github.com/Rawframe-Project/maul-locale/commit/00e5f79705edd66d88e513aa4f6b357570b95755)), closes [#271](https://github.com/Rawframe-Project/maul-locale/issues/271)
* **spellout:** leave the output of a failed format as it was ([#268](https://github.com/Rawframe-Project/maul-locale/issues/268)) ([2449298](https://github.com/Rawframe-Project/maul-locale/commit/2449298bd6d95db34c9b2a2c0680b438c5b5fe85)), closes [#267](https://github.com/Rawframe-Project/maul-locale/issues/267)
* **unit:** leave the output of a failed coordinate format ([#281](https://github.com/Rawframe-Project/maul-locale/issues/281)) ([aa89f07](https://github.com/Rawframe-Project/maul-locale/commit/aa89f077acf2db336f5c280e7bd3a8c11f59ea5a)), closes [#279](https://github.com/Rawframe-Project/maul-locale/issues/279)
* **unit:** refuse a negative width or coordinate ([#254](https://github.com/Rawframe-Project/maul-locale/issues/254)) ([6eeff3c](https://github.com/Rawframe-Project/maul-locale/commit/6eeff3ced40cd31e205468f77fea4aaf53949091)), closes [#253](https://github.com/Rawframe-Project/maul-locale/issues/253)


### Changed

* **calendar:** compute each new moon of a search once ([#247](https://github.com/Rawframe-Project/maul-locale/issues/247)) ([d33d066](https://github.com/Rawframe-Project/maul-locale/commit/d33d0669cd06cec3b7fdc66bab04dd44cc4dfb3d)), closes [#246](https://github.com/Rawframe-Project/maul-locale/issues/246) [#239](https://github.com/Rawframe-Project/maul-locale/issues/239)
* **calendar:** make only the months of a span that a year takes ([#245](https://github.com/Rawframe-Project/maul-locale/issues/245)) ([f6e8af4](https://github.com/Rawframe-Project/maul-locale/commit/f6e8af44388e6299a792bc2ed52ce46140cac2e8)), closes [#244](https://github.com/Rawframe-Project/maul-locale/issues/244) [#239](https://github.com/Rawframe-Project/maul-locale/issues/239)
* **calendar:** take each Chinese month's new moon as the next one ([#241](https://github.com/Rawframe-Project/maul-locale/issues/241)) ([de5c546](https://github.com/Rawframe-Project/maul-locale/commit/de5c54697d6a6b86f8eacfd538426313a5ca3961)), closes [#240](https://github.com/Rawframe-Project/maul-locale/issues/240)
* **calendar:** take the astronomy's sines together ([#243](https://github.com/Rawframe-Project/maul-locale/issues/243)) ([df9166c](https://github.com/Rawframe-Project/maul-locale/commit/df9166c9f7645356b22d76ae03f4916de0a8f109)), closes [#242](https://github.com/Rawframe-Project/maul-locale/issues/242) [#239](https://github.com/Rawframe-Project/maul-locale/issues/239)
* **calendar:** take the sine series' products as magnitudes ([#250](https://github.com/Rawframe-Project/maul-locale/issues/250)) ([110ae14](https://github.com/Rawframe-Project/maul-locale/commit/110ae149454ca1221e87726240ab38badce72334)), closes [#248](https://github.com/Rawframe-Project/maul-locale/issues/248) [#239](https://github.com/Rawframe-Project/maul-locale/issues/239)
* make the public enumerations fixed-width integers ([#255](https://github.com/Rawframe-Project/maul-locale/issues/255)) ([f43d6c0](https://github.com/Rawframe-Project/maul-locale/commit/f43d6c049cdcf36f5dbf1f6f2924249f31b1b032)), closes [#252](https://github.com/Rawframe-Project/maul-locale/issues/252)
* **unit:** write a kept output through a scratch, not twice ([#295](https://github.com/Rawframe-Project/maul-locale/issues/295)) ([a4460ba](https://github.com/Rawframe-Project/maul-locale/commit/a4460ba53be576423dc2c72c369ec50eaa5d02c8)), closes [#290](https://github.com/Rawframe-Project/maul-locale/issues/290)

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
