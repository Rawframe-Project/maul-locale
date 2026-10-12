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
  differs from latn's the indexes of its symbols and its decimal and
  percent patterns. Scientific notation writes the decimal pattern's
  affixes with the exponential symbol and the minus sign, as ICU's
  NumberFormatter does; the locales' scientific patterns (Hindi's
  `[#E0]`) are not stored. Each
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
- **Styles and notations:** percent and per mille multiply the decimal
  by 10^2 or 10^3 before rounding, moving the fraction digits shown
  with the point, and use the percent pattern (per mille in the percent
  sign's place). Scientific and engineering notation round the
  mantissa, choosing the exponent again when rounding carries it to
  the next power (ECMA-402's ComputeExponent); a mantissa keeps the
  places its decimal shows.
- **Compact notation:** two more units: the distinct compact patterns
  (affixes and a flag for a pattern of words alone, Italian's "mille")
  and the distinct compact tables (a row per power from 10^3 to 10^14:
  the other pattern's zeros, which give the divisor, a mask of the
  categories present and their pattern indexes). The generator reads
  compact patterns by plain inheritance: CLDR's resolution would give a
  long table's missing category the short table's pattern, through
  root's alias, or the other category's. A value is divided by its
  magnitude's row, rounded (by default to an integer when two integer
  digits or more show, else to two significant digits), the row chosen
  again when rounding carries it, and the pattern taken by the plural
  category of the digits shown with c the exponent, the exact value 1
  taking its own pattern where the locale has one. That is what ICU,
  ICU4X and CLDR's decimal test data do ("1,2 million" in French); CLDR's
  plural samples, which take the whole value (1.1c6 is many), serve the
  plural part's `mlocSelectPluralOperands`. Compact numbers keep the
  decimal pattern's grouping, with two digits before the first
  separator by default, and take the decimal style only. The compact
  word is its own part, the spaces around it literal.
- **Ranges:** `mlocFormatNumberRange` follows ICU's NumberRangeFormatter
  with its defaults, which ECMA-402's formatRange takes: the locale's
  range separator (stored with the symbols; every CLDR 49 range
  pattern is the two ends around it); an affix both ends share, sign
  included, written once when it is longer than one character ("3–5
  thousand", "3–5 %" in French, Arabic's two-character minus sign);
  spaces around the separator when an affix is written for each end or
  an exponent shows ("-5 – -3", "3K – 5K", "1E3 – 5E3"); and ends that
  write alike give the approximately sign in the sign's place ("~5",
  "~-5"). A range's parts carry the end they belong to. ICU4C 74,
  given the same input for 105 locales, agrees on every case whose
  data CLDR 49 has not changed.
- **No default rounding:** the zero precision writes the digits the
  decimal shows (mloc-0019), as ICU4X's decimal formatter does; a
  precision, once set, rounds and cuts zeros to its minimum as
  ECMA-402 does. Compact notation, whose scaled value has no digits
  the caller chose, rounds by default.
- **Proof:** `conform_numbers` runs every line of `testData/decimal`
  in the cldr CI job with ICU4J's defaults set as options (six
  fraction digits at most, half to even). Where the files predate the
  pinned data (Galician's group separator, Urdu's percent sign),
  `tools/number-exceptions.txt`
  states the replacement and its reason, and each entry must be used.
  Rows whose expected text alone predates the data (compact patterns
  in Afrikaans, Igbo, Pashto and Tamil) are listed one by one.

## Consequences

The number data costs about 65 KB of the compiled-in data, compact
notation 43 KB of it (ICU4X's compact data for its locales: 457 KB). Every formatter shares one core, so
currency and unit formatting add only their own data and code. A
caller who wants ECMA-402's three fraction digits sets them.
