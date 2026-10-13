# mloc-0025. Compound unit names: built as ICU builds them, from the data ICU reads

Status: Accepted

## Context

Record mloc-0024 formats the 238 units CLDR names. Any other core unit
("kilometer-per-second", "square-millimeter", "gram-per-1000-meter")
takes a name built from the locale's compound patterns: "per",
"times", powers (some by gender), SI and binary prefixes, and the
plural category each part takes, which CLDR's grammaticalFeatures.xml
derives per language. ICU4C builds such names in
`LongNameHandler::forArbitraryUnit` after normalizing the identifier
(`MeasureUnit`'s order and spelling). Its result hangs on which plural
categories its data has: it takes a unit's placeholder from the first
category it has and builds a category only where a part has it. Its
data is CLDR's production data, which `GenerateProductionData` derives
from the development data the generator reads (record mloc-0006): it
drops every inheritance marker and every value a locale's parents below
root give at the same path, then restores, with the value CLDR resolves
at that locale, a dropped path of the file whose logical group (a
unit's plural categories, and a long unit's cases) keeps a value.

## Decision

- **Identifiers** (`src/unit_compound.c`): parsed as record mloc-0023
  parses them, then normalized as ICU: the numerator first, then by
  quantity, then by simple unit, a larger prefix first; written with
  ICU's spelling ("square-", "cubic-", "pow4-", "per-", a constant past
  1,000 as "1e6"). Kilogram is kilo and gram. A mixed unit, a currency,
  a constant in the numerator, or a power past fifteen is refused.
- **Names:** a unit CLDR names, exactly or once normalized, takes its
  own; any other is built as `forArbitraryUnit` builds it: each part in
  the category the derivations give, a prefix and a power around the
  core unit (lowercased in long names by the extras of record
  mloc-0024), the product joined by "times", the quotient by the
  denominator's per-unit pattern or by "per" around its "one"
  pattern, the constant before the first category present. The
  library's parts stay within one built pattern of 256 bytes.
- **Derivations:** a unit of their own in units.mloc
  (`kindUnitDerivations`), one record of 16 bytes per language, sorted,
  root among them under its name: for "per", "times", "power" and
  "prefix", two values, each a category, the compound's or none. A
  language without a record takes root's.
- **Values** (`tools/mlocgen/unit_names_gen.c`), refining record
  mloc-0024: each field is the one ICU's data gives. The generator
  models the production step per locale file (a leaf kept, or a dropped
  leaf restored beside a kept one of its group) and reads the chain as
  ICU's sinks read it (the width, then short through root's alias, then
  the unit root aliases), the value of the first locale having the
  field: its leaf, or CLDR's resolution at that locale for a restored
  one. A plural category ICU has whose text is other's is filed as an
  empty string: it marks the category present, which a built name
  needs, and costs no text. An empty display name or per-unit pattern
  is none.
- **Proof** (`docs/locale/research/oracle/units` in the docs
  repository): ICU4C 79.1rc against the library on 105 locales by three
  widths. 203 compound units by five amounts (319,725 lines), the 238
  named units (899,640 amounts, 449,820 ranges, 74,970 display names):
  all agree except "generic", which ICU refuses. The 27 amounts and 13
  ranges record mloc-0024 left apart (de-CH's long jigger, pt-PT's
  narrow bit, fr-CA's narrow meter per second) now agree: they were the
  production step's restored values.

## Consequences

Any core unit formats in any locale with the names CLDR's patterns
give, as ICU gives them. units.mloc is 3.1 MB (about 0.1 MB more than
the named units alone, the empty markers costing a few bytes each).
The API is unchanged; `mlocInitUnitFormatter` takes any core unit, and
the formatter stays 1,344 bytes. The generator models a CLDR tool
beside its resolver; a change in `GenerateProductionData`'s options
would show in the oracle. Mixed units, the usage option and the
grammatical cases come after.
