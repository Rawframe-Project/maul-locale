# mloc-0030. Grammatical cases: ICU's reading of CLDR's production data, kept in the names file

Status: Accepted

## Context

CLDR 49 gives the long width's unit patterns in grammatical cases
(36,276 `unitPattern` elements with `case`, 16 cases in 34 locale
files) and power2/power3 by case and gender; grammaticalFeatures.xml
names each language's cases and genders and how a compound unit
passes its case to its components. ICU4C's number formatter writes them
with `unitDisplayCase` ("2 Metern", German's dative). Record mloc-0024
put the names in `units.mloc`; a first design gave the cases a file of
their own.

## Decision

- **Data, in units.mloc** (`src/unit_tables.h`): two kinds per locale
  key in the sparse shape, the case patterns and their lowercased forms
  (ICU lowercases a core under a prefix or power), a row per unit or
  power and case past the nominative coded by the identifier's index
  plus the identifier count times the case's index less one, six
  fields zero to other, each filed where ICU's result differs from what
  the key's chain gives (an empty field takes the plain lookup). The
  derivations unit gains each structure's cases and the compound's
  gender (28-byte records). The cases stay in the names file: their
  codes are its identifiers, so a file of their own would carry a copy
  and could pair with names of another build; they add 0.46 MB
  (units.mloc 3.63 MB).
- **ICU's data as the generator models it** (`tools/mlocgen/
  unit_names_model.c`): a case pattern is the first locale along the
  chain whose production data has it, else the plain pattern, a
  category with neither taking other's. Production data restores a
  dropped leaf (an inheritance marker) of a logical group with a kept
  member: in a locale grammaticalFeatures.xml lists, the plural
  categories by its cases for the units (and genders for powers);
  elsewhere the same case's categories (LogicalGrouping's COUNT_CASE,
  COUNT_CASE_GENDER, addCaseOnly). A restored value is CLDR's
  resolution there: the chain, then laterally the gender to neuter and
  none, the case to none, the count to other. Powers are read as ICU's
  InflectedPluralSink: per locale the gender's, neuter's, then the
  plain one's, each in the case, the nominative, then none.
- **Compounds** (`src/unit_compound.c`): the case goes through
  case/per (numerator and denominator), case/times (units before the
  last, the last), a power's pattern in the unit's case and the core in
  what case/power value0 gives; prefixes have none; "compound" passes
  the case on, a language without a rule takes the nominative, as ICU.
- **API**: `mlocGrammaticalCase` (the nominative 0, then the other 27
  of CLDR in alphabetical order) as `mlocUnitOptions.grammaticalCase`,
  read by the long width only, for simple, compound, mixed and usage
  outputs; a case the locale lacks writes the nominative; names without
  the case kinds refuse a case other than the nominative
  (mloc_errorNotFound).
- **Proof** (`docs/locale/research/oracle/cases`): ICU4C 79.1rc's
  unitDisplayCase on the 34 locales with case patterns, every unit
  English names and 30 compounds and 3 mixed units, by each case the
  locale's chain gives and none, three amounts, plus a short line each:
  121,485 lines, all equal but "generic", which ICU refuses. The
  earlier unit oracles are unchanged.

## Consequences

Cases cost 0.46 MB of the names file and no formatter room: patterns
are loaded in the case at init. The generator's model of production
data now follows LogicalGrouping per locale, which the oracle confirmed
on de-CH (a locale grammaticalFeatures.xml does not list) and on the
Bosnian Serbian locales.
