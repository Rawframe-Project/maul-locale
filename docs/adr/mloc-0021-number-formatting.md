# mloc-0021. Number formatting from distinct tables, without default rounding

Status: Accepted

## Context

CLDR 49 holds number data for 1,148 locale files, yet few distinct
values: 78 numeric numbering systems, 243 symbol sets, 4 decimal, 9
percent, 4 scientific and 53 currency patterns. Root aliases every
numbering system to latn, so each locale has data for every system,
mostly its latn data again. Most locales inherit their numbers
unchanged. ECMA-402's NumberFormat, with its v3 options, is the most
completely specified model of the options, and ICU4C and ICU4X follow
it; CLDR's `testData/decimal` (8,928 lines) was written by ICU4J's
NumberFormatter.

## Decision

- **Data:** four units of the number part (`src/number_tables.h`):
  the numeric systems with their digits; the distinct symbol sets;
  the distinct patterns, compiled by the generator into affixes (with
  placeholders for the minus, plus, percent, per mille and currency
  signs), digit counts and group sizes, so the library never reads a
  pattern string; and a unit per locale key, filed only where the
  locale's resolved numbers differ from the next key's along its
  chain, with its default, native, traditional and finance systems,
  minimum grouping digits, and for latn and each system whose data
  differs from latn's the indexes of its symbols and patterns. Each
  validator reads its unit alone; a formatter bounds every index when
  it is made, in the source that holds the locale's unit.
- **Formatters:** an internal core (digits, symbols, pattern, grouping)
  under `mlocNumberFormatter`, and later the currency and unit
  formatters. `mlocInitNumberFormatter` takes the numbering system from
  the locale's `-u-nu` keyword alone (`native`, `traditio`, `finance`
  as its data names them, traditional falling back to native and
  finance to the default, as UTS #35 says); a system without digits
  takes the default.
- **Status:** a formatter returns `mloc_fellBack` only when the system
  asked for was not taken. Its data is always found along its chain,
  and since units are filed only where they differ, which locale held
  them says nothing about a locale's support; that is language
  matching's question (mloc-0017).
- **Options:** ECMA-402's semantics (notation, style, grouping, sign
  display, minimum integer digits; a precision of fraction digits,
  significant digits or both with a priority, increments, the nine
  rounding modes, trailing zeros). Fields a kind does not read must be
  zero; conflicts are refused.
- **No default rounding:** the zero precision writes the digits the
  decimal shows (mloc-0019), as ICU4X's decimal formatter does; a
  precision, once set, rounds and cuts zeros to its minimum as
  ECMA-402 does. Compact notation, whose scaled value has no digits
  the caller chose, will round by default.
- **Proof:** `conform_numbers` runs every line of `testData/decimal`
  in the cldr CI job with ICU4J's defaults set as options (six
  fraction digits at most, half to even). Where the files predate the
  pinned data (Galician's group separator), `tools/number-exceptions.txt`
  states the replacement and its reason, and each entry must be used.
  Notations and styles not yet built are refused and their lines
  counted as skipped.

## Consequences

The number data costs about 23 KB of the compiled-in data, a quarter of
it the numbering systems' digits. Every formatter shares one core, so
currency and unit formatting add only their own data and code. A
caller who wants ECMA-402's three fraction digits sets them.
