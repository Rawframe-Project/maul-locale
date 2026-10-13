# mloc-0024. Unit formatting: names in a data file of their own, read as ICU reads them

Status: Accepted

## Context

CLDR 49 names 238 units in three widths (long, short, narrow), each with
a display name, a pattern per plural category ("{0} meters") and a
per-unit pattern ("{0} per meter"), and the long width with a
grammatical gender; compound patterns build names CLDR does not list:
"per", "times", powers (some by gender) and 32 SI and binary prefixes.
Root aliases the long and narrow widths to short and a few units to
others ("dot" to "pixel"), and many values are inheritance markers.
Over the 1,000 locale files the names are about 3 MB; most programs
format a few units in a few locales. ICU4C formats them from its data,
which CLDR's production step derives: a marker becomes the value it
stands for. Its sinks fill each plural category from the first locale
giving it, then leave a missing one to other.

## Decision

- **Data:** a data file of its own, `src/generated/units.mloc` (3.0 MB),
  which a program adds to its data set as it adds the currency names
  (record mloc-0022). A unit of no locale lists the 282 identifiers it
  knows, sorted, each with its kind (unit, prefix, power, "per" and
  "times"); per locale key, a sparse unit per width (the shape of
  `currency_tables.h`) with eight string fields, and the long width's
  extras (the gender, and each pattern of a unit or prefix lowercased by
  the locale's case rules through Maul Unicode, for the compound names
  ICU lowercases; empty where nothing changes). A name unit's codes
  index its own file's identifiers, so a file a program adds over
  another stands alone. The layout is in `src/unit_tables.h`.
- **Values** (`tools/mlocgen/unit_names_gen.c`): CLDR's resolution, with
  ICU's reading on top: a plural category no locale along the chain has
  in the width, or in short through root's alias, as a value or a marker
  takes other's in the width ("1 kilôgam" in Vietnamese, whose new
  "one" no locale gives, where CLDR's own count fallback would follow
  the alias to short first); a "-person" duration takes the plain one's
  (ICU-20400); a power of a gender is ICU's per category: the gender's,
  neuter's or the plain one's at the first locale with any. A key files
  a field where the library's lookup (the first key filing it, a plural
  category taking other's, a power of a gender the plain power's) would
  give another; locales are taken parent first, and one under its
  parent's key files nothing and is checked to resolve alike. Only the
  categories of the locale's rules and other are filed.
- **Formatting** (`mlocInitUnitFormatter`, `mlocFormatUnit`,
  `mlocFormatUnitRange`): a number formatter's core within the unit's
  pattern for the number's category, the patterns copied into the
  formatter (`{0}` marked, kept once each in a pool of 1,024 bytes) so a
  pattern from any key along the chain lives in one value. A pattern
  without `{0}` writes no number (Arabic's dual names), as ICU. The
  name is a part of its own, `mloc_numberPartUnit`, its ends' spaces,
  bidi controls and variation selectors literal, as ICU trims its unit
  and compact fields (the same trim now serves compact parts). Percent
  and per mille, short or narrow and not compact, take the locale's
  percent pattern unscaled, as ICU. A range writes the unit once, in the
  category the plural ranges give, other where they give none (ICU's
  `StandardPluralRanges`; currency names around a range now do the
  same). `mlocGetUnitDisplayName` writes the display name, the
  identifier with `mloc_fellBack` where no key names it.
- **Scope of this slice:** the units CLDR names. Names built from
  compound patterns (the identifiers' prefixes, powers, "per" and
  "times", which the data already carries), mixed units, the usage
  option and the grammatical cases come after.
- **Proof** (`docs/locale/research/oracle/units` in the docs repository):
  ICU4C 79.1rc against the library on 105 locales by the 238 units by
  three widths: 899,640 amounts (text and unit parts), 449,820 ranges and
  74,970 display names. All agree except "generic", which ICU refuses,
  and 27 amounts and 13 ranges of three units (de-CH's long jigger,
  pt-PT's narrow bit, fr-CA's narrow meter per second): their markers
  inherit through a parent's marker and root's alias, where CLDR's
  resolver, which the generator keeps (record mloc-0006), looks the
  alias up from the locale asked for and the production data ICU reads
  from the parent, the same 21 values in which CLDR's JSON departs from
  the resolver.

## Consequences

A program formats units by adding one file; the compiled-in data does
not grow. Patterns are copied, so a formatter is 1,344 bytes. The
generator reads every locale's units twice (about 20 s more). The
lowercased extras ship before the compound names that read them.
