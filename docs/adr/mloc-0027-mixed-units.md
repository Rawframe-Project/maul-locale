# mloc-0027. Mixed units: split exactly, joined by the unit list, as ICU writes them

Status: Accepted

## Context

A mixed unit ("foot-and-inch", "hour-and-minute-and-second") writes an
amount across its parts: "5 ft, 6 in". CLDR's preferences give five of
them, and UTS #35 lets any parts of one dimension mix. ICU4C's
MixedUnitLongNameHandler takes the amount in the largest part, splits
it with doubles (ComplexUnitsConverter), writes the larger parts with
the locale's default number format and their own plural category, the
smallest with the formatter, and joins them by the unit list of the
width. Record mloc-0023's preferences split mixed outputs exactly.

## Decision

- **Split** (`src/unit_mixed.c`, now shared by the preferences): the
  parts are taken largest first whatever their order, the amount in
  the largest; whole larger parts, the rest in the smallest, exactly;
  the smallest rounded by the formatter's precision (an inexact rest
  to 38 digits without one), a part that rounds up to the next
  carrying, and so on up. Parts of other dimensions, a temperature or
  Beaufort are refused.
- **Writing** (`src/unit_mixed_format.c`): each part with its own
  patterns (named or built, record mloc-0025) in the formatter's pool;
  the larger parts as the default number format writes them (grouped,
  no precision, one integer digit), the smallest with the options; the
  parts in the order given ("6 in, 5 ft" for "inch-and-foot"), joined
  by the unit list (record mloc-0026: long takes the wide one), the
  contextual changes reading the smallest part's pattern with "{0}"
  unfilled, as ICU gives it to its list formatter.
- **Where ICU is not followed:** the amount's sign goes on the first
  part with the formatter's sign display ("+5 ft, 6 in", "-0 ft, 6 in";
  ICU puts the sign display on the smallest, "5 ft, +6 in", and drops
  the sign of a zero first part, "0 ft, 6 in" for -0.5); NaN and the
  infinities go to the largest part with their sign (ICU writes NaN in
  the smallest and an overflowed integer before an infinity). Ranges of
  a mixed unit are refused, as ICU cannot format them; so are compact
  and scientific notation.
- **Formatter:** the opaque value grows to 1,664 bytes for four parts'
  patterns, the parsed identifier, the unit table and the list.
- **Proof** (`docs/locale/research/oracle/units`): ICU4C 79.1rc on 105
  locales by 14 mixed units (CLDR's five and nine more: three parts,
  an unsorted order, built parts) by three widths by ten amounts: all
  44,100 agree.

## Consequences

Mixed units format in every locale with one formatter; the split is
exact where ICU's doubles drift. The formatter is 320 bytes larger for
every unit. The usage option (6d-3) chooses mixed outputs through the
same path.
