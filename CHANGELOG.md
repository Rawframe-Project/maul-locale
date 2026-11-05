# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API, the ABI and every data format.

## [0.12.2](https://github.com/Rawframe-Project/maul-locale/compare/v0.12.1...v0.12.2) (2026-11-05)


### Fixed

* **collate:** refuse collation data whose elements carry the fourth case ([#495](https://github.com/Rawframe-Project/maul-locale/issues/495)) ([f35bc65](https://github.com/Rawframe-Project/maul-locale/commit/f35bc65583fbc96c371f5f9c52ed02cc81cb3e09)), closes [#494](https://github.com/Rawframe-Project/maul-locale/issues/494)

## [0.12.1](https://github.com/Rawframe-Project/maul-locale/compare/v0.12.0...v0.12.1) (2026-11-04)

### Fixed

- A collator, catalog chain, segmenter, searcher or alphabetic index
  whose stored counts no longer matched its arrays was read past them.
  Examples: a collator claiming more reorder ranges than it holds, a
  chain whose catalog or cache key count passed its room, a segmenter
  whose queue, window or class overrides passed theirs, a searcher with
  a ring of no elements (a division by zero), and an index whose counts,
  labels or boundaries passed the extent its header records. Each such
  value is now refused with `mloc_errorInvalid` by every call that reads
  it, and so is a searcher or index whose collator is not one. A
  segmenter whose queue starts outside the text fed, and a chain or
  segmenter in a state that stopped at an assertion in a build with
  assertions on, are refused the same way. A forged value stays outside
  the contract, but the library now checks every count and index it
  stored before reading through it
  ([#471](https://github.com/Rawframe-Project/maul-locale/pull/471),
  [#473](https://github.com/Rawframe-Project/maul-locale/pull/473),
  [#474](https://github.com/Rawframe-Project/maul-locale/pull/474),
  [#475](https://github.com/Rawframe-Project/maul-locale/pull/475),
  [#476](https://github.com/Rawframe-Project/maul-locale/pull/476)).
- Collators, catalog chains, segmenters, searchers, catalogs and the
  currency, unit, spell-out and message formatters read their `bool`
  fields without checking the stored byte. A byte other than 0 or 1 made
  that read undefined. Enumerations stored in bytes were not
  range-checked either, so a chain could hand a caller a
  `mlocCatalogResult` whose reason and position named nothing. Such a
  value is now refused with `mloc_errorInvalid` before any of its flags
  is read. The fields checked: a chain's cached reason and position; a
  segmenter's flags, filters, Japanese class and terminator run; a
  searcher's comparison; a message formatter's direction; a catalog's
  source flag; and a collator's settings, strength, case order and fast
  Latin options, including the collator a searcher or an index holds
  ([#478](https://github.com/Rawframe-Project/maul-locale/pull/478),
  [#479](https://github.com/Rawframe-Project/maul-locale/pull/479),
  [#480](https://github.com/Rawframe-Project/maul-locale/pull/480),
  [#481](https://github.com/Rawframe-Project/maul-locale/pull/481)).
- `MLOC_NODISCARD` expanded to nothing under GCC 14. GCC reports its C23
  mode as `__STDC_VERSION__` 202000L, and the macro looked for 202311L,
  so a program built with GCC that dropped a status from any of the
  library's functions got no warning. Any C dialect after C17 whose
  compiler has the attribute now gets `[[nodiscard]]`, so such a program
  warns under GCC as it already did under Clang, and fails to build with
  `-Werror`. C17 and older dialects are unchanged
  ([#484](https://github.com/Rawframe-Project/maul-locale/pull/484)).

## [0.12.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.11.0...v0.12.0) (2026-11-04)

### Changed

- **Breaking: a named time zone holds its data, not a pointer into it.**
  `mlocTimeZone` held a raw pointer and size to its zones unit
  (`zones`, `zonesSize`), so a zone pointed at bytes no open validated,
  such as a copy of the unit, was read unchecked and could read past
  them. A zone now holds a copy of the opened `mlocData` it was found
  in, the place of its zones unit there, its identifier and an offset
  zone's minutes: `{data, unit, identifier, offsetMinutes}`, 40 bytes in
  place of 24. Every call checks the unit and the identifier against
  that data, so a zone copied, assigned or put together from other
  zones' fields reads only units the data's open validated. A program
  that stored a zone, or read `zones` and `zonesSize`, finds the zone
  again with `mlocFindTimeZone` or `mlocGetTimeZone`; one that builds an
  offset zone by hand sets `offsetMinutes` alone
  ([#461](https://github.com/Rawframe-Project/maul-locale/pull/461)).

### Fixed

- A bound message whose compiled message was changed after the bind
  could be read past its bytes in a release build, and stopped at an
  assertion in a build with assertions on. The caller still keeps the
  message as it was, but formatting and the part accessors now read
  every record, string and run within the compiled message whatever it
  holds, and a chain of declarations made to loop ends as running out
  of resolutions does
  ([#460](https://github.com/Rawframe-Project/maul-locale/pull/460)).
- A data source no open filled, such as an `mlocData` left
  uninitialized or cleared, was read through whatever its words held
  when it was put in a set: a call could read through a wild pointer, or
  take a cleared source as one with no data. Only `mlocOpenData` and
  `mlocOpenBuiltinData` fill a source now, with a mark the library
  checks, together with the source's sizes, before reading it. Every
  call that takes a set refuses one with such a source with
  `mloc_errorInvalid`, or a count of 0 where it returns a count, and
  never skips it; a named zone holding such data is refused the same
  way. A source's bytes are those its open was given; a source whose
  bytes pointer was changed afterwards is outside the contract
  ([#465](https://github.com/Rawframe-Project/maul-locale/pull/465)).

## [0.11.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.10.0...v0.11.0) (2026-11-03)

### Changed

- **Breaking: a long spelled-out number in a date is written whole.**
  A date whose pattern spells out a number field (a numbering system
  override naming spell-out rules, such as `haw`'s month) wrote the
  number in digits, and reported nothing, when the spelled-out text
  passed 256 bytes. It now writes the spelled-out text whole: with
  spell-out rules that give 300 letters for October, `haw`'s short date of
  2026-10-15 gave `15/10/26` and now gives the 300 letters between
  `15/` and `/26`
  ([#449](https://github.com/Rawframe-Project/maul-locale/pull/449)).
- **Breaking: a zoned argument with a long zone identifier is written
  whole.** `mlocFormatMessage` wrote a zoned date-time argument with no
  function (`{$x}`) as its fallback, with an Unsupported Operation
  fault (`mloc_errorFallback` in strict mode), when its zone's
  identifier was longer than about 90 bytes, which only a zones file a
  host loads can give. It now writes the argument's ISO 8601 text
  whole, up to a 255-byte identifier
  ([#450](https://github.com/Rawframe-Project/maul-locale/pull/450)).
- **Breaking: relative time writes long names and patterns.**
  `mlocRelativeFormatter_Format` and
  `mlocRelativeFormatter_FormatDayTime` returned `mloc_errorFormat`, a
  status neither header lists, when the name ("tomorrow") or pattern
  ("in {0} days") they write from a relative time unit was longer than
  256 bytes. They now write it whole, with the same parts as a short
  one ([#454](https://github.com/Rawframe-Project/maul-locale/pull/454)).

## [0.10.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.9.0...v0.10.0) (2026-11-03)

### Changed

- **Breaking: a dated message value keeps its calendar option on a
  locale with a long tail.** `mlocFormatMessage` wrote a `:date`,
  `:time` or `:datetime` value with a `calendar` option as its
  fallback, with an Unsupported Operation fault (`mloc_errorFallback`
  in strict mode), when the formatter's locale had too little room left
  in its tail (`MLOC_MAX_LOCALE_TAIL`, 112 bytes) to take
  `-u-ca-<type>`, such as `en` with an `a` extension of 100 bytes. Such
  a value is now formatted in the calendar it names: a message that
  gave `{|2006-01-02|}` gives "Jan 2, 2006"
  ([#442](https://github.com/Rawframe-Project/maul-locale/pull/442)).
- **Breaking: a long relative day is joined to a time.**
  `mlocRelativeFormatter_FormatDayTime` returned `mloc_errorFormat`, a
  status its header does not list, for a finite day count whose text
  passed 1536 bytes, such as 1e1600 days in English. It now succeeds
  and writes the day whole before the time, as one relative day part
  ([#448](https://github.com/Rawframe-Project/maul-locale/pull/448)).

## [0.9.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.8.0...v0.9.0) (2026-11-03)

### Changed

- **Breaking: the package requires Maul Unicode 0.13.0.** Maul Locale
  now builds against Maul Unicode 0.13.0 in place of 0.11.0, and its
  installed CMake package asks for that version
  (`find_dependency(maul-unicode 0.13.0)`). A Maul Unicode package
  before 1.0 matches only its exact version, so a build that provides
  0.11.0 no longer configures: provide Maul Unicode 0.13.0. A build
  that fetches Maul Unicode is unaffected. No result changes; the
  Unicode version and the data files are the same as in 0.8.0
  ([#437](https://github.com/Rawframe-Project/maul-locale/pull/437)).

### Fixed

- `mlocFormatMessage` wrote a numeric argument with no function as its
  fallback `{$n}`, with an Unsupported Operation fault
  (`mloc_errorFallback` in strict mode), once its plain text passed 128
  bytes, such as 1e130 or a currency or unit amount that long. Under
  `:string` such an argument matched no key, not even its own digits,
  and raised no fault. It is now written whole and selected by its text
  ([#440](https://github.com/Rawframe-Project/maul-locale/pull/440)).

## [0.8.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.7.0...v0.8.0) (2026-11-03)

### Changed

- **Breaking: number and currency parsing publish their bounds.**
  `MLOC_MAX_NUMBER_PARSE_AFFIX` (16, `number.h`) is the most characters
  of an affix a parse reads, a symbol counting one, and
  `MLOC_MAX_CURRENCY_PARSE_PATTERNS` (4, `currency.h`) the most distinct
  long-name or ISO-append patterns a strict currency parse reads. Each
  is at least twice what CLDR 49 needs (5 characters, 2 patterns).
  `mlocNumberFormatter_Parse` and `mlocCurrencyFormatter_Parse` used to
  answer `mloc_errorFormat`, a status neither header lists, for every
  text when a formatter's affixes did not fit the parser's fixed room:
  an affix of 17 characters in lenient mode, or several distinct
  patterns in a strict currency parse. They now read every formatter
  within the bounds and return `mloc_errorLimit` past them. A strict
  parse of an affix of 17 to 32 characters, which used to be read, is
  now refused. A program that handled `mloc_errorFormat` from a parse
  handles `mloc_errorLimit` instead. The parser takes about 5.7 KB more
  stack; `docs/user/numbers.md` gives the deepest stack measured
  ([#433](https://github.com/Rawframe-Project/maul-locale/pull/433)).
- **Breaking: a long `:string` selector is compared a piece at a time.**
  A `:string` selector over text not in NFC and longer than 256 bytes
  after NFC was a bad selector for every key, so it took `*` and a
  strict formatter failed the message. It now matches a key equal to
  its NFC. The text is cut at NFC boundaries, and
  `MLOC_MAX_MESSAGE_SELECTOR_RUN` (170 bytes, `message.h`) is the
  longest stretch between two that can be compared; only a run of
  combining marks and of starters that compose with the code point
  before them passes it. Past it, the selector is still a bad one, and
  in strict mode `mlocFormatMessage` now returns `mloc_errorLimit`
  where it returned `mloc_errorFallback`. A program that tells the two
  apart handles `mloc_errorLimit` there; in fallback mode the result is
  as before
  ([#428](https://github.com/Rawframe-Project/maul-locale/pull/428)).

### Fixed

Each came from a fixed-size buffer too small for what a valid data
file a host loads may hold. CLDR 49's built-in data reaches none.

- `:mloc:spellout` in a message refused a rule set name of 64 bytes or
  more with `mloc_errorNotFound`. It now finds any name a data file can
  give
  ([#425](https://github.com/Rawframe-Project/maul-locale/pull/425)).
- A locale's display name showed a currency key's code in capitals in
  place of a symbol of 64 bytes or more. It now shows the symbol whole
  ([#426](https://github.com/Rawframe-Project/maul-locale/pull/426)).
- `mlocInitNumberingSystemFormatter` refused with `mloc_errorNotFound`
  a numbering system whose record names a locale of 16 bytes or more,
  such as `zh_Hant_HK_u_ca_chinese`. It now reads any locale that parses
  ([#427](https://github.com/Rawframe-Project/maul-locale/pull/427)).
- A year before the epoch of a calendar with no earlier era (the
  Persian) lost its minus sign when the locale's minus sign was longer
  than 12 bytes: "-121" was written "121". It now keeps the sign
  whatever its length
  ([#431](https://github.com/Rawframe-Project/maul-locale/pull/431)).

## [0.7.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.6.0...v0.7.0) (2026-11-02)

### Changed

- **Breaking: line breaks follow the levels of CSS's `line-break`.**
  `-u-lb` now picks CSS Text 3's `strict`, `normal` or `loose` level, and
  a locale without the key, or with a value CLDR's key does not have
  (`anywhere`), breaks at `normal`. Small kana and the prolonged sound
  mark (Line_Break=CJ) stay with the character before them at `normal`
  and `strict` in every locale, `ja`, `zh` and `zh-Hant` without the key
  included: "あぁあーい" is `[あぁ][あー][い]`, where it used to break
  before each of them. Only `-u-lb-loose` breaks there. `-u-lb-loose`
  used to break as `normal`; it now adds CSS's other loose breaks. Lines
  of `ja`, `zh` and `zh-Hant` may now break before 〜 (U+301C) and ゠
  (U+30A0) at `normal` and `loose`. A program that stored line breaks
  must compute them again. `docs/user/segmentation.md` describes the
  levels and the cases where ICU4C 79.1 breaks otherwise
  ([#409](https://github.com/Rawframe-Project/maul-locale/pull/409)).
- The library builds against Maul Unicode 0.11.0, up from 0.6.0. The
  build fetches it when no installed copy of that version is found
  ([#403](https://github.com/Rawframe-Project/maul-locale/pull/403),
  [#409](https://github.com/Rawframe-Project/maul-locale/pull/409),
  [#415](https://github.com/Rawframe-Project/maul-locale/pull/415)).

### Added

- The generator makes `el` and `hy` index labels from their exemplars,
  with Greek and Armenian casing, when the CLDR it reads gives them none.
  It used to stop there. The data files built from CLDR 49 do not change
  ([#412](https://github.com/Rawframe-Project/maul-locale/pull/412)).

### Fixed

- A date whose pattern writes the month number in a spelled-out
  numbering system (`numbers="M=..."`) wrote 64 bytes of uninitialized
  memory in place of a spelled-out month longer than 64 bytes. It is now
  written whole. Only a data file with such rules reaches it; CLDR 49's
  built-in data does not
  ([#417](https://github.com/Rawframe-Project/maul-locale/pull/417)).
- A unit power of a gender found its gendered patterns ("power2" of a
  feminine unit) only for genders of up to 24 bytes. A longer gender in
  a data file fell back to the plain power's patterns. CLDR 49's genders
  are at most 9 bytes
  ([#420](https://github.com/Rawframe-Project/maul-locale/pull/420)).

## [0.6.0](https://github.com/Rawframe-Project/maul-locale/compare/v0.5.0...v0.6.0) (2026-11-01)

### Changed

- **Breaking: a hand-filled `mlocLocale` must be a well-formed locale.**
  Every function that reads an `mlocLocale` (all 38) now refuses with
  `mloc_errorInvalid` a locale whose fields are not NUL-padded text in
  locale syntax: a language of 2, 3 or 5 to 8 lowercase letters, an
  empty or title-case 4-letter script, an empty, 2-letter uppercase or
  3-digit region, and a tail of subtags of 1 to 8 lowercase letters and
  digits joined by single hyphens. Before, such a locale was read as it
  was, and the call could report success with an undefined result. Only
  the syntax is checked; canonical form stays the caller's job. A
  program that fills an `mlocLocale` by hand must keep that syntax, or
  make the locale with `mlocParseLocale` or `mlocCanonicalizeLocale`.
  `mlocGetSpelloutRuleSetCount` returns 0 for such a locale, and
  `docs/user/locales.md` says what a hand-filled locale must keep
  ([#361](https://github.com/Rawframe-Project/maul-locale/pull/361)).
- The headers and guides now say that memory an object keeps a pointer
  to must stay as it was for the object's life, not only outlive it:
  data files and data sets, compiled and bound messages, the formatter
  a message is bound to, a message formatter's program functions, a
  format call's arguments, an alphabetic index's bytes, a segmenter's
  current piece of text, a catalog's bytes, and a catalog chain's
  catalogs, formatters and cache. Each is checked once and read as
  checked
  ([#336](https://github.com/Rawframe-Project/maul-locale/pull/336),
  [#341](https://github.com/Rawframe-Project/maul-locale/pull/341),
  [#343](https://github.com/Rawframe-Project/maul-locale/pull/343)).

### Fixed

- `mlocFormatMessage` refuses with `mloc_errorInvalid` memory it checked
  that changed under it, which it used to read as it then was (calling a
  null pointer, reading past the program's functions, or reading through
  an argument's cleared pointer): a bound message whose formatter was
  made again with fewer program functions, a program function's null
  resolve entry, a `match` entry set to null during selection, and
  arguments a program function changed before a later pass. The output
  is left as it was when this happens in the first pass, and cleared in
  a later one
  ([#343](https://github.com/Rawframe-Project/maul-locale/pull/343),
  [#345](https://github.com/Rawframe-Project/maul-locale/pull/345)).
- `mlocCatalogChain_FormatMessage` refuses with `mloc_errorInvalid` a
  key index past the first catalog's keys. A first catalog replaced by
  one with fewer keys sent the read past its key offsets
  ([#336](https://github.com/Rawframe-Project/maul-locale/pull/336)).
- `mlocInflector_InflectNumber` refuses with `mloc_errorInvalid` a
  number whose text does not end in the number's own digits: a percent
  sign, compact pattern or exponent separator that ends or begins in a
  digit (`12.5%9`, `129` for 12K, `1.2E93`). Such numbers, which only
  altered number data writes, were spoken as another number and
  reported success
  ([#363](https://github.com/Rawframe-Project/maul-locale/pull/363),
  [#365](https://github.com/Rawframe-Project/maul-locale/pull/365)).
- Collation and search: a run of combining marks after a starter whose
  count passed 2^32 (4 GiB of UTF-16 or more) and wrapped to a count
  that fits the 64 code point normalization window was taken to fit it
  and written past it. A run whose count wrapped to more was read as a
  long run, as it should be
  ([#374](https://github.com/Rawframe-Project/maul-locale/pull/374)).
- `mlocOpenData` refuses a spell-out rule that holds a substitution its
  kind cannot make, which no generated file has. A data file whose
  negative rule took a fractional part made formatting negative
  infinity write one byte before a buffer
  ([#393](https://github.com/Rawframe-Project/maul-locale/pull/393)).
- A Finnish stem change of 17 to 24 bytes would have written past its
  buffer in Release builds; no known input reaches it
  ([#353](https://github.com/Rawframe-Project/maul-locale/pull/353)).

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
