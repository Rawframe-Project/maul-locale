# Glossary

Terms as this repository uses them. Standards' own terms (UTS #35, UTS #10, ECMA-402, Temporal) keep their meanings and are not repeated here.

## Contents

- [Data](#data)
- [Locales](#locales)
- [Numbers](#numbers)
- [Dates and times](#dates-and-times)
- [Messages and catalogs](#messages-and-catalogs)
- [Grammar](#grammar)
- [Collation](#collation)
- [Proof](#proof)

## Data

- **Source:** one opened data file or the compiled-in data, an `mlocData`.
- **Data set:** the caller's ordered array of sources, `mlocDataSet`. A lookup takes the first source that has what it needs.
- **Data file:** the library's one container format, `MLOC` and a 64-byte header, then a unit table, a locale table, payloads and a string pool. The compiled-in data is a data file's bytes as a C array.
- **Unit:** one payload of a data file, named by a kind and a locale index (or no locale). Found by binary search.
- **Kind:** a unit's type, the part in the high 16 bits and the table in the low 16 (`src/kinds.h`). A kind's meaning never changes within a data version.
- **Validator:** the function a kind's part provides to check a unit when a file opens. It reads its own unit only.
- **Data version:** CLDR and Unicode versions, the data format version, the tzdb release and a hash of the generated code tables. A file of another data version is refused before anything else is read.
- **Data format version:** `MLOC_DATA_FORMAT_VERSION`, the version of the data files' and compiled messages' layouts; it changes with them, not with the library version ([data.md](data.md#versions)).
- **Zones file:** a data file holding only the zones unit, compiled from any tzdb release; the one data whose release may differ from the library's.
- **Distinct set:** a unit of no locale that stores each distinct value once (number symbols, patterns, compact tables); per-locale units name entries by index.

## Locales

- **Locale key:** the id a locale's data is filed under: its CLDR id with the likely script, dropped when it is the language's own (`zh-TW` is `zh_Hant_TW`, `sr-Cyrl-BA` is `sr_BA`, `und` is `root`).
- **Chain:** the keys a lookup walks from a locale to root in one component.
- **Component:** a part of CLDR whose locales inherit by their own parent list: main, collation, plurals, segmentation, grammar.
- **Fell back:** `mloc_fellBack`, a call that used a locale other than the one asked for, where that matters (a numbering system not taken, a name written as its code). Ordinary inheritance along the chain is not falling back.
- **Paradigm locales:** CLDR's list of locales tried early in matching (en, en-GB, es, es-419, pt-BR, pt-PT).

## Numbers

- **Decimal:** `mlocDecimal`, up to 40 significant digits, an exponent and the fraction digits shown; its bytes are canonical.
- **Plural operands:** UTS #35's `n i v w f t c e` of a decimal as shown.
- **Core:** the internal part of every number, currency and unit formatter that writes digits, symbols, pattern and grouping (`src/number_core.h`).
- **Core unit:** a unit identifier CLDR can convert (a simple unit, or a compound of them), as opposed to a mixed unit ("foot-and-inch").
- **Usage:** a unit preference category such as "road" or "person-height".
- **Rule set:** one of a spell-out group's public sets (`%spellout-cardinal`); a group is inherited whole along the chain.

## Dates and times

- **Day:** a date's count, day 1 being January 1 of year 1 in the proleptic Gregorian calendar. A date is a day and a calendar.
- **Month code:** Temporal's month name across years with leap months, `M01` to `M13` and `M05L`.
- **Instant:** seconds and nanoseconds since 1970 in universal time.
- **Gap and fold:** a local time that a forward change of clocks skips, or a backward change repeats.
- **Rearguard:** the tzdb form whose savings are never negative, which formatting needs to choose standard or daylight names.
- **Semantic skeleton:** UTS #35's closed set of field sets and options that the API takes instead of patterns.
- **Glue:** the pattern that joins a date and a time (CLDR's `atTime` or standard date-time format).
- **Metazone:** a group of zones that share names over a period ("America_Pacific").

## Messages and catalogs

- **Compiled message:** the locale-free, position-independent form `mlocCompileMessage` writes, which a catalog stores.
- **Bound message:** a compiled message bound to a formatter, with its functions found and its number formatters made; read-only.
- **Fault:** a part of a message that failed while formatting, reported with its expression; the message is written with the part's fallback.
- **Set:** a source catalog and its translations, compiled together into one key order.
- **Chain (catalog):** the catalogs of one set in fallback order, with a cache that holds bound messages.
- **Stale:** a translation whose recorded source hash differs from the source message's now.

## Grammar

- **Inflector:** a locale's object that writes suffixes or particles on runtime text and numbers.
- **Reading:** how a text's end is pronounced: as a word, by its letters' names, or by the number it writes.
- **Hint:** the spoken form of a text, given where spelling misleads.

## Collation

- **Element (CE):** a 64-bit collation element, ICU's layout: primary in the high 32 bits, then secondary and tertiary with the case bits.
- **CE32:** a 32-bit trie value that is an element or a tag (expansion, contraction, prefix, offset, implicit).
- **Root unit:** the root collation in a unit of `collation.mloc`: the code point trie, elements, expansions and contexts.
- **Tailoring:** a locale's collation type as a delta over the root; its trie falls back to the root's.
- **Fast Latin table:** ICU4C's table of mini elements for common Latin text, which the fast compare reads before the general one.
- **Unsafe backward:** a code point a compare cannot start or stop before, because a contraction or prefix may join it with what precedes.
- **Variable top:** the last primary that shifted alternate handling ignores (space, punctuation, symbol or currency).

## Proof

- **Oracle:** an independent implementation whose answers a test compares with the library's: ICU4C, ICU4X, CLDR's JSON, zic and zdump, Python's `decimal` and `zoneinfo`, test262.
- **Departure:** a listed, reasoned difference from an oracle (`tools/*-exceptions.txt`, [conformance.md](conformance.md)).
