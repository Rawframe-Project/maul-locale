# Numbers

Decimals, plural rules, number, currency and unit formatting, parsing and spell-out. Sources: `src/decimal*.c`, `plural.c`, `number*.c`, `currency*.c`, `unit*.c`, `parse_*.c`, `spellout*.c`, `wide.c`.

## Decimals

A plural category depends on the digits shown (in English 1 is one and 1.0 is other), and formatting a `double` to a number of decimals answers for the binary value, not the decimal meant (1.005). So every number is an `mlocDecimal`: up to 40 significant digits, the power of ten of the last, the fraction digits shown, the sign and the kind. Trailing zeros are not stored as digits (1.50 is digits 15, exponent -1, two fraction digits shown), and every byte is defined, so `memcmp` compares value and digits shown; -0 keeps its sign, as ECMA-402 formats it. A value past 40 digits or 10^±999,999 is refused with `mloc_errorCapacity`, never rounded silently.

- **Shortest digits of a double** (`decimal_shortest.c`): Ryu, in integer arithmetic only, so every platform gives the same digits. Its 125-bit powers of five are written by `tools/gen_pow5_tables.py`, which CI checks. It was compared with Python's `repr` on 31 million doubles.
- **Rounding** (`decimal_round.c`): ECMA-402's nine modes, increments 1, 2, 5 or 25 at any power of ten, which give all of ECMA-402's increments. Compared with Python's `decimal` module on 1.3 million cases.
- **Plural operands:** an integer of more than 18 digits is held as 10^18 plus its low 18 digits. It then equals no literal of a rule and keeps every remainder a rule can take, so no operand overflows. The generator checks that every rule's remainder divides 10^18.

## Plural rules

CLDR has 39 distinct cardinal and 28 ordinal rule sets for all its locales. Interpreted from bytecode a rule took about 47 ns; compiled to C it selects in about 4 ns. So the generator writes each distinct rule set as a C function over the operands (`src/generated/plural_rules.c`), and a locale's eight-byte unit names its cardinal and ordinal rule and its range table by index. The generated file's hash is part of the data version. UTS #35's semantics apply: `n` equals an integer only when `t` is 0, and `x != y` is `not (x = y)`.

## Number formatting

CLDR has number data for over a thousand locale files but few distinct values: 78 numeric numbering systems, a few hundred symbol sets and a handful of patterns. The data stores each distinct value once (`src/number_tables.h`): the numbering systems and their digits, the symbol sets, the patterns compiled by the generator into affixes, digit counts and group sizes (the library never reads a pattern string), and per locale key, only where its numbers differ from the next key's, the indexes of what it uses. A formatter bounds every index when it is made, so formatting checks nothing.

Every number, currency and unit formatter is built on one core (`number_core.c`, `number_format.c`). Options follow ECMA-402's `Intl.NumberFormat` exactly; fields a kind does not read must be zero, and conflicting options are refused. There is no default rounding for numbers: ECMA-402 rounds to three fraction digits by default because a `double` has no digits of its own, and a decimal does. Scientific notation writes the decimal pattern's affixes with the exponent symbol, as ICU's `NumberFormatter` does; the locales' scientific patterns are not stored. CLDR's `testData/decimal` (written by ICU4J's `NumberFormatter`) is the conformance check.

## Currencies

Symbols and names are inherited field by field, as UTS #35 resolves them, and filed per key only where they differ; a currency formatter walks the locale's chain once when it is made. The currency names (2.1 MB) are a file of their own because few programs show them.

The spacing between a symbol and digits follows root's `currencySpacing` as ICU4C applies it rather than UTS #35's letter rule alone: the `alphaNextToNumber` pattern is used when the symbol's character next to the digits is neither a symbol nor a space ("Fr. 1,234.50", "USD 1,234.50", "$1,234.50"); the letter rule alone would write "Fr.1,234.50". The generator stores per symbol whether its first and last characters are of that kind, from Maul Unicode's General_Category, which the library itself does not link for this.

## Units

Conversion is exact (`unit_convert.c`, `wide.c`). CLDR's factors are exact fractions over 19 constants (π among them as a fraction); a value in flight is ±a / b × 10^e with 2,048-bit integers and the exponent kept apart. The quotient is computed to 40 significant digits and rounded to odd, so any later rounding to 38 digits or fewer, in any mode, gives what rounding the exact value would, and the precision code needs no change. ICU4C converts in `double` and carries binary rounding; its results are an oracle only where they are exact.

Identifiers are parsed by UTS #35's grammar and normalized as ICU normalizes them. A unit CLDR names takes its own name; any other compound is built as ICU's `forArbitraryUnit` builds it, from per-language derivation rules. Unit names (3.6 MB with the grammatical cases and genders) are a file of their own. The generator models ICU's data production step per locale file, because ICU's data drops and restores some values that CLDR's resolution alone would give differently; with that model, ICU4C agrees on every named and compound unit amount tested but "generic", which ICU refuses.

Mixed units are split exactly (whole larger parts, the rest in the smallest, a part that rounds up carrying). The usage option routes each value to the unit the locale prefers for it, as ICU does, through the same preference tables `mlocGetUnitPreference` reads.

## Parsing

Number and currency parsing follow ICU4C's `DecimalFormat::parse`, the only implementation that reads localized numbers back (ICU4X and ECMA-402 have none): a list of matchers built from the formatter's pattern and symbols, run greedily, then validated. The list is an array on the stack and the text is read once, with no recursion and no allocation; affix patterns are bounded (32 matchers from 64 bytes) and a longer one is refused. Root's lenient separator classes and ICU's fixed sets, with the case folding ICU's comparisons use (Turkic I included), are one unit of no locale. Departures from ICU are listed in [conformance.md](conformance.md); the main one is that a byte that does not begin valid UTF-8 is no character, where ICU reads UTF-16.

## Spell-out

CLDR's rule-based formats (`common/rbnf`, 147 groups, about 22,000 rules) exist in ICU4C only, which parses the rule text whenever a formatter is made and formats by recursion on an `int64_t` or a `double`: past `int64_t` it writes digits, and fractions lose precision. Here the generator compiles the rules as ICU4C 79 reads them, byte for byte (`tools/mlocgen/spellout_compile.c`), into `spellout.mloc`, and the library runs them on the exact decimal: quotients and remainders by moving the point, fractions from the decimal's own digits. Results are ICU4C's wherever ICU holds the value exactly; recursion past 64 levels is an error.

Parsing words back follows ICU's strict parse, quirks included, but ICU's search is exponential on some inputs. The generator writes a lead record per rule set (the characters its rules can start with, through the sets their substitutions reach), so a search no rule can start is refused at once, and a 512-slot memo keeps costly searches; a fuzz target found Lithuanian texts that ran the search to its bound before both were added.
