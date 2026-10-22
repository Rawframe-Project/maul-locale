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
- Dates formatted (`maul-locale/datetime.h`): `mlocInitDateFormatter`
  and `mlocFormatDate` write a date by a field set, length and year
  style or a standard format, in the locale's calendar and digits, with
  parts (mloc-0042).
- Times formatted: `mlocInitTimeFormatter` and `mlocFormatTime` write a
  time of day by its hour cycle (the locale's, -u-hc, H11 to H24, either
  clock) and precision (hour, minute, minute unless zero, second, 1 to 9
  fraction digits) or a standard format, with day periods (am and pm,
  noon, the locale's flexible periods) and parts (mloc-0042).
- Dates with times: `mlocInitDateTimeFormatter` and `mlocFormatDateTime`
  join a date field set or standard date format and a time by the
  calendar's atTime or standard glue of the date's length, a weekday
  with a time matched whole (mloc-0042).
- Regional preferences give the calendar: the region's first in CLDR's
  calendarPreferenceData, or -u-ca's whole value (mloc-0018).
- The dates data file, `dates.mloc`: every locale's semantic date
  patterns and standard date formats in the 18 calendars, compiled, and
  its calendar names (months, weekdays, quarters, eras, cyclic years,
  leap month patterns), for the date formatter (mloc-0042).
- Times in `dates.mloc`: each locale's time patterns per calendar (hour,
  minutes and seconds on both clocks, the standard time formats, the
  date-time glue), its day period names and its day period rules
  (mloc-0042).
- Zones formatted: `mlocInitZoneFormatter` and `mlocFormatZone` write a
  zone at an instant by its specific or generic name, location, city or
  offset, falling back and qualifying generic names as ICU4C does, with
  CLDR's daylight offsets read over the zones unit's (mloc-0042).
- Dates and times in a zone: `mlocFormatDateInZone`,
  `mlocFormatTimeInZone` and `mlocFormatDateTimeInZone` write an
  instant's local date, time or both with its zone in any of the zone
  styles, and the full and long standard time formats, which name the
  zone, are now taken (mloc-0042).
- The zone names data file, `zone-names.mloc`: each BCP 47 zone id's
  region and metazone periods, each metazone's golden and preferred
  zones, and every locale's zone formats, exemplar cities and zone and
  metazone names, as CLDR resolves them (mloc-0042).
- Date ranges: `mlocFormatDateRange` writes two dates by a date
  formatter as ICU4C's DateIntervalFormat writes its skeleton ("Jan
  5 – 8, 2026"), with parts marking each date's own
  (`mloc_dateRangeStart`, `mloc_dateRangeEnd`); `dates.mloc` holds
  every locale's interval patterns for each date and time option, and
  `mlocDateFormatter` grows to hold a formatter's (mloc-0042).
- Time ranges: `mlocFormatTimeRange` and `mlocFormatTimeRangeInZone`
  write two times of day, or two instants in a zone (across dates each
  with its short date), by a time formatter ("10:00 – 11:30 AM"); the
  time and date-time formatters' values grow (mloc-0042).
- Date-time ranges and dates in a zone: `mlocFormatDateTimeRange`,
  `mlocFormatDateTimeRangeInZone` and `mlocFormatDateRangeInZone`: on
  one day the date once and the time's range, on two each whole, a date
  that lacks the greatest difference's field written with it
  (mloc-0042).
- Date patterns follow ICU's item order where a locale's own file has
  a draft or inherited item: en_JM's month and day "05/1" and es_HN's
  "05-enero", as ICU writes them (mloc-0042).
- The relative time data file, `src/generated/relative.mloc`: each
  locale's names of near offsets ("yesterday", "next Monday") and future
  and past patterns by plural category, for every unit and width, filed
  per key along the chain (mloc-0043).
- Relative time: `mlocInitRelativeFormatter` and `mlocFormatRelative`
  write a signed amount of a unit (year to second, quarter, week, the
  weekdays) in three widths, numeric always or auto ("yesterday"),
  capitalized by context, with parts (`maul-locale/relative.h`,
  mloc-0043).
- A compact number's unit, currency or relative time name takes the
  plural category of its whole value, as ICU4C does ("1M meters", "1
  million US dollars"; it took the category of "1").
- A relative day with a time: `mlocFormatRelativeDayTime` joins the
  relative formatter's day and a time formatter's time by CLDR's
  relative date-time glue ("tomorrow at 3:00 PM"), the day one part
  (`mloc_datePartRelativeDay`); `dates.mloc`'s time sets hold that glue
  (mloc-0043).
- Durations: `mlocInitDurationFormatter`, `mlocGetDurationOptions` and
  `mlocFormatDuration` write a Temporal duration's fields as ECMA-402's
  Intl.DurationFormat does, each field's style and display, the
  fractions exact and truncated, the locale's digital separators and
  hour padding, with parts (`maul-locale/duration.h`, mloc-0044).
- Messages: `mlocCompileMessage` parses Unicode MessageFormat 2 (UTS
  #35 Part 9) into a compiled message in caller memory, its names and
  keys in NFC, refusing syntax errors and every data model error
  (variant key count, fallback variant, selector annotation, duplicate
  declarations, options and variants) at their offset
  (`maul-locale/message.h`, mloc-0045); `mlocWriteMessageText` checks
  a compiled message, from a file too, and writes it back as canonical
  MF2 text; `mlocInitMessageFormatter`, `mlocBindMessage` and
  `mlocFormatMessage` bind a compiled message to a locale and format it
  with named arguments, each placeholder resolved where it stands, with
  fallbacks and fault records or, strict, an error; a program adds its
  own functions (resolve, format, match, better), which read their
  options and locale through `mlocGetMessageOptionCount`,
  `mlocGetMessageOption`, `mlocFindMessageOption` and
  `mlocGetMessageLocale`; `:string` and pattern selection by the
  specification's order. The library now
  links Maul Unicode 0.3.0; `mloc_withFallbacks` and `mloc_errorFormat`
  join the results.
- A locale's direction: `mlocGetLocaleDirection` gives right to left
  or left to right from the locale's script or its likely script, the
  scripts written right to left generated from CLDR's script metadata
  (`mlocDirection` in `maul-locale/base.h`, mloc-0045); it agrees with
  the character order of every CLDR locale, which CI checks.
- Messages isolate their placeholders by MF2's Default Bidi Strategy
  (`mlocMessageOptions.isolation`, on by default), the message's
  direction its locale's; values carry a direction (a program
  function's from its new `direction` entry) and `u:dir` sets it;
  `u:` options are hidden from functions, bad `u:dir` and `u:id` values
  are bad options, and markup resolves its options (mloc-0045).
- Messages format and select with `:number` and `:integer`: ECMA-402's
  options and defaults, options passed on from number values, exact and
  plural selection with bad variant keys reported; program functions
  receive number values as `mloc_messageOperandNumber`
  (`mlocMessageResolved` is now a union that can hold its decimal), and
  a declaration's faults are reported once however often it is named
  (mloc-0045).
- Messages format and select with `:offset` (exact sums, an overflow
  unsupported) and `:percent` (the value times 100, its operand's minimum
  integer digits, increment and `select` dropped) (mloc-0045).
- Messages format with `:date`, `:time` and `:datetime` (Draft): the
  specification's fields, lengths, precisions, `hour12` and `calendar`
  over the date formatters; new date, time and date-time argument kinds
  and operand kinds; date/time literals (mloc-0045).
- Messages format durations with `mloc:duration`: a duration argument
  kind, ISO 8601 literals, or seconds as MF1's duration, with ECMA-402's
  DurationFormat options; program functions receive
  `mloc_messageOperandDuration`, and `mlocMessageResolved` is 96 bytes
  (mloc-0045).
- Turkish common nouns inflect by a lexicon of 2,615 words generated
  from Zemberek-NLP's dictionaries (20 KB): stops kept or softened
  (ahlakı, kalbi, kanadı, psikoloğu), inverse harmony (katedrali,
  Kemal'in), doubling (hakkı), the dropped vowel (burnu), compounds
  (zeytinyağına, zeytinyağları), a circumflexed spelling apart from its
  plain one (hâli, halli) (mloc-0046).
- Grammatical suffixes (`maul-locale/grammar.h`): an inflector per
  locale, Turkish cases, plural, possessives, copula and question
  particle on text and numbers by their spoken end, with TDK's
  apostrophe and parts (mloc-0046).
- `mloc:duration` over seconds splits them by integer division, and
  binary64 and 128-bit quotients of 64-bit integers convert natively:
  the duration message costs 40% less (mloc-0045).
- Kazakh suffixes on text and numbers: its harmony, the first consonant
  of a suffix by the last sound, the possessives and the cases after
  them, a stop voiced before a vowel, a hyphen after abbreviations,
  digits and Latin words; the Turkic engine shared by Turkish and
  Kazakh, each a table (mloc-0047).
- Kyrgyz suffixes: four-way harmony, the onsets by the last sound, the
  possessives and the cases after them, the question particle and every
  suffix joined, a soft sign dropped, й and у written ю (mloc-0047).
- Azerbaijani suffixes: harmony with ə, no d to t, a hyphen after
  abbreviations, digits and hinted foreign names as the orthography
  norms write them, k and q written y and ğ before a vowel (mloc-0047).
- Uzbek suffixes by the 1995 spelling rules: -ka after k and -qa after
  q, every suffix joined, k and q written g and gʻ before a possessive,
  numbers read by Uzbek number words in code (mloc-0047).
- Turkmen suffixes: two-way harmony with a high vowel rounded by some
  suffixes only, the dative fused with a final vowel, a hyphen after
  abbreviations and digits, p, ç and k voiced before a vowel, numbers
  read by Turkmen number words in code (mloc-0047).
- Hungarian suffixes by the Academy's rules: 16 cases, plural and
  possessives on the word's harmony, lengthening, linking vowels and
  the assimilated v of -val and -vá; a table of the stems the rules
  cannot tell; hyphens after digits, abbreviations and as the rules
  ask for names (mloc-0048).
- Finnish suffixes: 14 cases in both numbers, possessives and the
  question clitic on Kotus's inflection types and consonant gradation,
  a table generated from Kotus's word list (CC BY 3.0) at a pinned
  commit and checked in CI; names by their ending; a colon after
  digits, abbreviations and ordinals, an apostrophe after an unsaid
  end (mloc-0049).
- Korean particles (`mlocKoreanParticle`: 은/는, 이/가, 을/를 and 13
  more) by the final of the text's spoken end: Hangul, numbers by their
  Sino-Korean reading, capitals by their letters' Korean names, Hanja
  by Unihan's readings, a hint; both forms ("은(는)") where the end
  cannot be read, or a refusal when the request is exact (mloc-0050).
- `mlocGetListGender`: the gender of a list of persons by the locale's
  rule in CLDR's genderList.xml (neutral, mixed neutral, male taints),
  the locale canonicalized first (mloc-0051).
- Messages: `mloc:case` and `mloc:particle` inflect a text or a number
  value by the locale's inflector ("{$name :mloc:case case=dative}"
  gives "Rodrigo'ya"), with the request's options and a hint that may
  be a variable (mloc-0052).
- Messages: `mloc:gender` selects on the gender of a list of persons
  ("male", "female", "other" items) by the locale's rule: keys male,
  female and other (mloc-0052).
- The inflector reads its formatters in place instead of copying them,
  reads ASCII letters' sounds by index and the Turkish table's blocks by
  their first byte: a short Turkish name inflects in about 0.7 of the
  time it took. The benchmark times inflection in each language, a
  list's gender and mloc:case.
- Finnish inflection searches its compound and place words on the
  word's letters encoded once, the type tables from an index by first
  byte, and copies only a stem's letters: a word inflects in about 0.55
  of the cycles it took, a name in about 0.65.
- Hungarian inflection encodes a word once for its compound search,
  rejects consonant spellings by their last letter and reads ASCII
  letters without decoding (about 0.77 of the cycles); the Turkish
  lexicon skips front-coded entries by the bytes they share, and the
  end of a text reads ASCII without decoding.
- `mlocInflectNumber` speaks the value the formatter shows instead of
  parsing its text back: half the work, and another locale's formatter
  now reads right (Turkish "1,000'de" for English's thousand, which
  read 1.000 before). Scientific and compact notation are still read
  from their text (mloc-0046).
- The catalog tool `mloccat` begins: catalog sources in the W3C
  Internationalization WG's message resource syntax (an Editor's Draft,
  pinned in `tools/resource-release.txt`), read rule by rule into the
  draft's data model, written back, and checked as a catalog (a locale,
  keys given once, messages that compile, the properties it reads);
  `mloccat read` and `mloccat check` (mloc-0053).
- Catalogs: `mloccat compile` writes a source and its translations as
  catalogs (a data file per locale) sharing one key order under a
  minimal perfect hash (XXH64, PTHash's pilots); `mlocOpenCatalog`,
  `mlocGetCatalogInfo`, `mlocFindCatalogKey` and `mlocGetCatalogMessage`
  find a message by key or by an index the same in every catalog of the
  set; obsolete entries, orphaned keys and `@translate no` handled; keys
  not in NFC refused (mloc-0054).
- Catalog chains: `mlocInitCatalogChain`, `mlocMeasureCatalogChain`,
  `mlocBindCatalogChain`, `mlocFormatCatalogMessage` and
  `mlocFormatCatalogKey` format a set's messages through catalogs in
  fallback order, bound once into a cache of the caller's memory, with
  a stated text and reason for every failure (mloc-0055).
- `mloccat compile` writes a regional catalog's locale as the data
  container names it (`fr_CA`); such catalogs did not open before
  (mloc-0055).
- Stale translations: `mloccat accept SOURCE TRANSLATION [KEY...]`
  records in a translation the hash of the source message each entry
  answers (`@mloc-source`, XXH64 of the canonical text), and
  `mloccat compile` counts the translations whose source changed since
  (mloc-0056).
- `mloccat report [--json] [--fail-on KINDS]` gives a set's coverage
  per locale and its missing, stale, unrecorded, orphaned and differing
  keys, and messages longer than their `@mloc-max-length` formatted
  with their `@mloc-example` arguments; text or JSON, failing on the
  kinds chosen (mloc-0057).
- `mloccat pseudo DIRECTORY SOURCE` writes the pseudo-locales `en-XA`
  (letters accented, each pattern bracketed and padded by 40%) and
  `ar-XB` (text runs between RLO and PDF) as catalog sources of the
  set, changing only pattern text (mloc-0058).
- PO exchange: `mloccat export-po` writes a translation as a PO file
  (msgctxt the key, MF2 in msgid and msgstr, `mf2-format`, `fuzzy` when
  stale, comments and `@param` extracted) and `mloccat import-po` reads
  one back, adding keys where the source has them and recording the
  source each translation answers (mloc-0059).
- XLIFF 2.2 exchange: `mloccat export-xliff` writes a translation as
  XLIFF (a unit per key, placeholders, markup and syntax as codes over
  the unit's original data, notes, the size restriction, states) and
  `mloccat import-xliff` reads a CAT tool's file back (mloc-0060).
- XLIFF selection messages by PGS: a plural, ordinal or select message
  becomes a segment per case (`pgs:switch`, `pgs:case`), expanded to
  the target locale's plural categories with example numbers as notes,
  and folds back into one message on import. Imports and pseudo-locales
  now refuse a message whose data model is wrong, as the catalog
  compiler does (mloc-0061).
- The profile states that a part function or a program function may
  call the library back, the call it is inside of included: every
  object is read only once made (mloc-0001).
- Messages format lists with `mloc:list` (`type`, `style`), selecting
  by the count as `:integer` does: a list argument kind of item texts;
  program functions receive `mloc_messageOperandList` (mloc-0045).
- Messages format with `mloc:spellout` (`rules`, `numberingSystem`;
  MF1's spellout and ordinal) and `mloc:relative` (`unit`, `style`,
  `numeric` and number options) (mloc-0045).
- Messages format to parts: with a part function, `mlocFormatMessage`
  reports text, placeholders, fallbacks, markup and isolation marks
  (`mlocMessagePart`, the expression's or markup's index in the upper
  24 bits) and, inside each placeholder, its number's or date's own
  parts; `mlocGetMessagePartInfo`, `mlocGetMessagePartParameter` and
  `mlocFindMessagePartOption` read a part's function or markup name,
  options and attributes, `u:id` among them (mloc-0045).
- Messages show exact times in zones: instant and zoned date-time
  argument kinds, literals with Z, an offset or a zone annotation,
  `timeZone` (an identifier, an offset, `UTC` or `input`),
  `timeZoneStyle` long and short, a default zone in
  `mlocMessageOptions`, and program functions receiving a value's
  instant and zone (mloc-0045).
- Messages format with `:unit` (Draft): a new argument kind,
  `mloc_messageValueUnit` (an amount and a unit identifier), or a number
  with the `unit` option; `unitDisplay`, `usage` with the locale's
  preferred unit; program functions receive its values as
  `mloc_messageOperandUnit` (mloc-0045).
- A format call costs less: no clearing of its whole state or of each
  value, record fields read inline, ASCII argument names compared
  without an NFC check (mloc-0045).
- A format call takes less stack: a chain of 64 declarations about
  24 KB instead of 108 KB (mloc-0045).
- Binding a message prepares each number function's formatter and
  plural rules where its options do not come from arguments, so a format
  call no longer makes them (mloc-0045); a bound message grows by up to
  0.9 KB per such expression.
- Messages format with `:currency`: a new argument kind,
  `mloc_messageValueCurrency` (an amount and an ISO 4217 code), or a
  number with the `currency` option; its displays, `currencySign` and
  `fractionDigits`; program functions receive its values as
  `mloc_messageOperandCurrency` (mloc-0045).
- `mlocCurrencyOptions` gains `sign` (ECMA-402's `currencySign`):
  standard or accounting over the locale's `-u-cf` choice (mloc-0022).
- Fixed: `:string` over a date function's value (`.local $d = {|2006-01-02|
  :date} {{{$d :string}}}`) took it for a program function's and called
  through the function table at an index it never set; it is now a bad
  operand, as over a number function's value.
- Fixed: `mlocFormatMessage` read a decimal argument without checking
  it; one the library does not make (an exponent past its limits) is now
  `mloc_errorInvalid`, where `:offset` could write past its work area.
- Fixed: a message whose name holds U+037E or U+1FEF compiled, but its
  NFC name (";" or "`") is no name, so `mlocWriteMessageText` refused it;
  such names are now a syntax error at that code point (mloc-0045).
- Dates: e and c of one or two letters write the day's number in the
  locale's week, as ICU4C does (kek's interval patterns, whose
  formatters were refused); `dates.mloc`'s interval patterns are
  checked once when the file is opened (the date side's pairs written
  first), which makes date and date-time formatters about a third faster
  to make and opening the file about a quarter slower (mloc-0042).
- The IXDTF duration writer is `mlocFormatTemporalDuration`, beside
  `mlocFormatTemporalDate` and the others (it was `mlocFormatDuration`,
  which now names the locale's duration formatter; mloc-0044).
- The collation data file, `src/generated/collation.mloc`: CLDR 49's
  root collation (UCA 18) as ICU's runtime holds it, its elements in a
  code point trie with their expansions, contractions and prefixes,
  checked at open (mloc-0064, mloc-0065); the element iterator reads it
  through NFD with no limit on marks (mloc-0066).
- Collation: `include/maul-locale/collate.h`, with `mlocInitCollator`
  and `mlocCompare` over the collation data file, in CLDR's root order.
  A locale's `-u-ks`, `ka`, `kv`, `kf`, `kc`, `kn`, `kb` and `kr` keys
  set the options and `mlocCollatorOptions` overrides them: every
  strength to identical, shifted alternates, case first and the case
  level, numeric ordering, backwards secondaries and script reordering,
  as ICU4C orders them (mloc-0067, mloc-0069).
- Sort keys: `mlocGetSortKey` writes ICU4C's key bytes for the same
  order and options, and `mlocGetCollationVersion` gives the version to
  store beside them, which changes with the key format or the collation
  data (mloc-0070).
- Collation: text holding U+FDD1 orders as ICU4C's, the root's script
  start markers mapped as its contractions (mloc-0065).
- The collation data file holds every CLDR 49 locale's collation types,
  built from their rules over the root as ICU4C builds them, with each
  locale's default type; import-only and alternate types are left out.
  The file is 2,341,666 bytes, zh's four types 0.6 MB of them
  (mloc-0071).
- Collators order by the locale's tailoring: the `-u-co` type along the
  collation chain, else the chain's default type, else `standard`, as
  ICU4C's loader picks it, with the tailoring's settings and reordering
  as defaults under the `-u-` keys and the options; the keys' version
  covers the tailoring. A tailored locale needs the main data's parents
  in the set. `mlocCollator` is 1,024 bytes (mloc-0071).
- Collation types honor `[suppressContractions]` where it stands and the
  settings of the rules they import, as ICU4C builds them: search types
  read Thai and Lao prevowels alone, and bs-Cyrl orders Cyrillic first
  as sr does (mloc-0071).
- zh's `pinyin`, `stroke` and `zhuyin` collations come from CLDR's
  `alt="short"` rules, as ICU4C's data takes them, so their keys are
  ICU's (mloc-0071).
- The `unihan` collations (ja, ko, zh) order Han by radical and stroke
  as ICU4C does, from a radical-stroke table in the collation data
  file; the root keeps CLDR's code-point order (mloc-0071).
- Collation search: `include/maul-locale/search.h`, with
  `mlocInitSearcher` and `mlocFindNext` (and their UTF-16 forms), a
  pattern found where a collator holds it equal, as ICU4C's usearch
  finds it: its three element comparisons, match bounds on grapheme
  clusters, a start anywhere in the text; a match whose text elements
  fill the room ICU4C keeps for them is found whole, and a longer one
  is refused with mloc_errorCapacity where ICU4C returns it cut short
  (mloc-0074).
- The collation data holds each unit's unsafe-backward ranges, the
  code points after the first of its contractions, for search
  (mloc-0074).
- Numeric ordering (kn) reads every decimal digit by its value, also
  a digit a collation type gives contractions (the emoji type's
  keycaps), as ICU4C does (mloc-0074). Collation versions change with
  it.
- A discontiguous contraction is matched past any number of marks, as
  ICU4C does; before, more than 72 code points between its parts made the
  sort keys disagree with the compare (mloc-0066).
- The collation data file holds ICU4C's fast Latin table for the root
  and each collation type, built over NFD and checked as the file is
  opened (mloc-0072). Collation versions change with it.
- `mlocCompare` compares text of the Latin and general punctuation
  ranges through ICU4C's fast Latin table, the general compare taking
  over for any other text (mloc-0072).
- `mlocCompareUtf16` and `mlocGetSortKeyUtf16`: UTF-16 texts collate
  and key as their UTF-8 forms, a lone surrogate as U+FFFD (mloc-0073).
