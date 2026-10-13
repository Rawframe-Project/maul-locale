# mloc-0032. Number parsing: ICU's matchers, read once without allocation

Status: Accepted

## Context

A program that shows numbers in a locale also reads them back from a
form field: "1.234,5" in German, "12,34,567" in Hindi, Arabic digits
with their own separators, a percent sign. ICU4C is the only library
that does this (DecimalFormat::parse); ICU4X and ECMA-402 do not. ICU
builds a list of matchers from the formatter's pattern and symbols
(ignorables, signs, percent and per mille, NaN, infinity, the digits
with their separators, the exponent, the pattern's affixes), runs it
greedily, then post-processes the result. Its lenient mode reads the
separators by equivalence classes from root's parseLenients; its strict
mode checks grouping sizes and the full affixes.

## Decision

- **API**: `mlocParseMode` (lenient 0, strict 1) and
  `mlocParseNumber(formatter, text, length, mode, decimalOut, endOut)`
  on a number formatter of standard, scientific or engineering
  notation (compact notation is refused: the value rounded away cannot
  come back). The result is an exact `mlocDecimal` and the byte offset
  where the number ended ("12 apples" ends at 2); its fraction digits
  are the ones typed, moved with the exponent and the scale ("50%" is
  0.50). Nothing read is `mloc_errorSyntax` with the offset reading
  stopped at; digits past the decimal's 40, or an exponent past its
  range, are `mloc_errorCapacity` (ICU saturates to infinity or zero).
- **Rules** (`src/number_parse.c`, `src/parse_match.c`), ICU 79's
  NumberParserImpl as DecimalFormat::parse builds it: the affix
  matchers (made only when an affix has a literal ignorables do not
  cover, or in strict mode; paired, and in lenient mode unpaired;
  sorted by prefix then suffix length), in lenient mode the percent or
  per mille sign the pattern has and the plus, minus and approximately
  signs, NaN, infinity, the ignorables, the decimal matcher (ICU's
  separator sets, its group validation and back-off, its lead set), the
  exponent; then the number validator, in strict mode the affix
  validator, and the scale of a percent or per mille style. The greedy
  run starts the list again after any matcher reads text. The per mille
  style reads the percent placeholder as the per mille sign; a
  formatter that shows the plus sign makes ICU's plus-sign affixes. The
  list is an array on the stack, the text is read once, with no
  recursion and no allocation; leading zeros are not held and trailing
  zeros are counted, so only significant digits meet the capacity. An
  affix pattern is a series of at most 32 matchers from at most 64
  bytes (16 literals in lenient mode, where an ignorables matcher goes
  between two, 32 in strict mode); a longer one is
  `mloc_errorCapacity`. CLDR's decimal and percent patterns have no
  literal the ignorables do not cover, so in lenient mode they make no
  affix matchers at all.
- **Data**: the parse sets, a unit of no locale in the numbers data
  (`src/parse_tables.h`, 0x40017): root's parseLenients of the general
  and number scopes, which the generator compiles from CLDR as ICU
  reads them (root's alone), and ICU's fixed sets (the ignorables, the
  bidi controls, the other grouping separators, infinity, the
  approximately signs, the decimal digits of Unicode as runs), and the
  simple case folding ICU's comparisons use (u_foldCase with the
  Turkic mappings for I, as its StringSegment asks; BMP code points
  alone where it compares UTF-16 units), from Maul Unicode as 222 runs:
  "НЕ ЧИСЛО" reads as Russian's NaN, "1,5е3" as Ukrainian's exponent.
- **Departures**: a byte that does not begin valid UTF-8 is no
  character. ICU's createScientificInstance reads the locale's
  scientific pattern, which in five locales has brackets ("[#E0]");
  the library's scientific notation writes the decimal pattern's
  affixes (record mloc-0021) and reads what it writes.
- **Proof**: the parse oracle (`docs/locale/research/oracle/parse`)
  over the 105 locales of the earlier oracles, five styles and both
  modes: 314,824 lines made from formatted values and their lenient
  variants, and 504,000 random lines (digits, separators, signs,
  spaces, bidi marks, symbols and letters; and digits with separators
  alone): value and end equal to ICU4C 79's but the departures above
  (the bracket patterns' strict lines, and exponents past the
  decimal's range); and the NaN and exponent symbols of every locale
  ICU has in five casings, all equal. The tests hold 108 of those
  cases; 138 on crafted patterns with literals (a pack whose patterns
  are "#,##0 pcs", explicit negative subpatterns, parentheses, a
  trailing minus), each as ICU's DecimalFormat gives it with the same
  pattern; the round trip of formatted values in 40 locales (the same
  decimal back, fraction digits too); the bounds and the refusals.

## Consequences

Parsing costs the formatter nothing until it is called and needs one
small unit of data (4 KB). Currency parsing (the formatter's
currency and any currency) and the fuzz target follow in their own
steps.
