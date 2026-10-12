# mloc-0023. Units: identifiers parsed, conversion exact and rounded once

Status: Accepted

## Context

CLDR 49's `units.xml` converts 157 units onto 39 base units by factors
and two offsets written as expressions over 19 constants (`ft_to_m`,
`PI` as 411557987/131002976), all exact fractions; the largest
numerator or denominator has 127 bits. Units are named by identifiers
that build compound units from simple ones: SI and binary prefixes,
powers, products, "per", unit constants ("per-100-kilometer") and
mixed units ("foot-and-inch"), with 13 deprecated aliases. Beaufort
converts by a table UTS #35 does not give. ICU4C converts in `double`
(its factors read with a string-to-double converter, prefixes and
constants raised with `pow`), so its results carry binary rounding;
ICU4X 2.3 moved to exact ratios. Profile L14 asks for exact rationals.

## Decision

- **Data:** a unit of no locale (`src/unit_tables.h`, 6.9 KB): the
  simple units sorted by identifier, each with its base unit, flags
  (offset, Beaufort), its place in UTS #35's normalization order and
  its size in the base unit as a reduced fraction (numerator and
  denominator, then the offset's, little-endian integers of at most 64
  bytes); the prefixes; the aliases; the base units with their
  exponents of 14 dimensions (kilogram, meter, second, ampere, kelvin,
  candela, item, bit, revolution, pixel, em, part, night, year); the
  Beaufort table ICU uses (CLDR-17421). The generator evaluates CLDR's
  expressions exactly (`*` before `/`, UTS #35's rule).
- **Identifiers:** parsed as UTS #35's grammar: a simple unit is the
  longest run of tokens the table names, else a prefix on the first
  token and such a run after it (the table first, so "kilogram" is a
  unit and "kilometer" a prefixed one); powers before a unit; "per"
  once or more, the later ones multiplying; constants of up to eight
  characters ("1e6"), each multiplied into its side; `curr-xxx` units
  parsed but converting to nothing; private-use units refused. A
  deprecated identifier is replaced whole, or inside a compound where
  it names a simple unit. At most 8 single units, 4 mixed parts and
  255 bytes.
- **Conversion:** a value in flight is ±a / b × 10^e with a and b
  unsigned integers of 2,048 bits (`src/wide.c`, Knuth's division) and
  e kept apart, so scaling never folds a decimal's exponent into the
  integers; an offset (a temperature alone, never inside a compound,
  UTS #35's "discarding offsets") folds it first. Dimensions decide
  convertibility: equal, or opposite for UTS #35's reciprocals.
  Beaufort rounds a force half up into the table and takes its middle
  speed, and takes a speed to the force whose range holds it, as ICU.
  A unit whose size, or a value, passes the integers is refused with
  `mloc_errorCapacity`.
- **Rounding once:** the quotient is computed to 40 significant digits
  and rounded to odd (truncated, the last digit raised from 0 or 5
  when the value is inexact), so any later rounding to 38 significant
  digits or fewer, in any mode, gives what rounding the exact value
  would, and the precision code needs no change. `mlocConvertUnit`'s
  zero precision gives an exact value as it is and an inexact one
  rounded half even to 38 significant digits; a precision given is
  applied to the rounded-to-odd value. An inexact value whose 40
  digits pass the decimal's smallest exponent is refused.
- **Proof:** `tools/check_units.py` with `conform_units` in the cldr CI
  job: from `unitsTest.txt`'s own fractions, every unit to its base,
  to every unit of its quantity and to every unit of the reciprocal
  quantity, at five values, 21,173 conversions, each equal to the
  exact value as the default precision rounds it; the file's
  seven-digit column agrees with its fractions.

## Consequences

Conversion is exact and the same on every platform, at the cost of
multi-limb arithmetic: a conversion multiplies a few integers of a few
hundred bits and divides once. Units of absurd size (two `pow15`
daltons) are refused rather than rounded. The library holds no unit
names yet: formatting comes with its own data file (research 25).
