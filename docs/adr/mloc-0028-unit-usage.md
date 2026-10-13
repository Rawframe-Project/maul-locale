# mloc-0028. The usage option: every output unit kept, the value choosing, as ICU routes it

Status: Accepted

## Context

ICU4C's number formatter takes a usage ("road", "person-height") with
an input unit and writes each value in the unit the locale prefers for
it (UsagePrefsHandler, UnitsRouter): "0.62 mi" for 1000 meters in the
United States, "5 ft, 11 in" for 1.8 meters of a person's height.
Record mloc-0023's mlocGetUnitPreference already chooses and converts
exactly for one value. A formatter must choose per value, so it needs
every unit the route may give, with their names, and it is a value with
no allocation.

## Decision

- **Route** (`src/unit_prefs.h`): the preferences code is split into
  the route of an input unit (the -u-mu unit, else the run of its
  quantity, usage and region with -u-ms, else its base unit; at most
  eight outputs, CLDR 49's longest run having seven) and the choice of
  one entry by the value; mlocGetUnitPreference and the formatter share
  both, and the threshold test (unit_mixed.c).
- **Formatter** (`src/unit_usage.c`): `mlocUnitOptions` gains `usage`
  and `usageLength`. The formatter keeps the input unit, each output's
  identifier, threshold and increment, and each output's patterns in
  slots of its pool (eight slots, 2,048 bytes, a mixed output taking a
  slot a part); the opaque value grows to 3,584 bytes. A value chooses
  the first output whose threshold it reaches (NaN the first, as the
  largest value would), is converted exactly, split across a mixed
  output, and rounded by the precision given, else the preference's
  increment, else to an integer of at least two significant digits.
- **Ranges:** ends that choose one output, not mixed, collapse as a
  unit's range ("30–70 ft", "~350 ft"); others are written whole and
  joined by the spaced range separator ("30 ft – 1.2 mi"), mixed ends
  included, which ICU aborts on.
- **Refused:** a usage not well-formed (mloc_errorSyntax); a mixed input,
  one that does not convert, or a notation other than standard
  (mloc_errorInvalid). A quantity without preferences writes its base
  unit, where ICU refuses ("3 kg⋅m").
- **Rounding mode:** the formatter's (ECMA-402's half-expand by
  default), where ICU's default is half-even; the oracle passes
  half-even.
- **Proof** (`docs/locale/research/oracle/units`): ICU4C 79.1rc on 107
  locales and two with -u- keywords by CLDR's 41 categories and usages
  (15 input units) by three widths by ten values: 134,070 lines, all
  equal but 1,685 where ICU's doubles drift (39.4999 seconds for 39.5,
  9.9499e-5 for 9.95e-5, plural operands of integers past 2^63) and the
  library's exact value is right. Ranges: 3,240 lines, 399 that crash
  ICU (long width, mixed outputs), the other 2,841 equal.

## Consequences

Usage works with one formatter and one call per value, every route's
names copied at init. Every unit formatter is 3,584 bytes, about twice
record mloc-0027's, the price of keeping it a value. A run longer than
eight units, or whose names pass the pool, is refused with
mloc_errorCapacity.
