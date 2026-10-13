# mloc-0031. Unit genders: ICU's noun class, derived as ICU derives it

Status: Accepted

## Context

A message that agrees with a formatted amount needs the unit's
grammatical gender ("der Meter", "die Sekunde"). ICU4C gives it as
FormattedNumber::getNounClass: a unit CLDR names has its long name's
gender, else LongNameHandler derives one from the parts with CLDR's
gender derivations (grammaticalFeatures.xml). Record mloc-0024 already
files each locale's long name genders in units.mloc's extras, and record
mloc-0030 added the derivations' gender values.

## Decision

- **API**: `mlocGender` (none 0, then CLDR's seven in alphabetical
  order) and `mlocGetUnitGender(data, locale, unit, unitLength, out)`,
  the gender of an identifier whatever the width.
- **Rules** (`src/unit_gender.c`), ICU's: a unit CLDR names takes its
  long name's gender along the chain; one without, in a language where
  meter has a gender, the calculated one (calculateGenderForUnit: the
  per rule picks a side, the times rule a unit, a fixed power or prefix
  rule wins, else the simple unit's; ICU reads the prefix rule only
  beside a power, kept). A built name takes its sides' through
  processPatternTimes (each unit's gender through its prefix and power
  rules, then the times rule; a unit without a gender skipped) and the
  per rule. A mixed unit, a unit without a numerator ("per-second") and
  a language without genders give none.
- **Proof**: the case oracle's noun classes (121,485 lines) and a
  gender oracle over the 35 locales whose language has genders, every
  unit English names and 40 compounds (9,590 lines,
  `docs/locale/research/oracle/cases`): all equal, but the short and
  narrow widths' percent and per mille, which ICU writes with the
  percent pattern and gives no noun class (the library's gender is the
  unit's, whatever the width).

## Consequences

Genders need no data beyond what the names file has. A program asks
for the gender of the unit it formats; with a usage the output unit
depends on the value (mlocGetUnitPreference gives it).
