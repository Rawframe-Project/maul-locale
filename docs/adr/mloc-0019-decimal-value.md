# mloc-0019. The decimal value and the plural operands

Status: Accepted

## Context

A number's plural category depends on its visible digits: in English
`1` is one and `1.0` is other, and CLDR's rules read the operands `i`,
`v`, `w`, `f`, `t` and `c` of the digits shown, not of a binary value.
Formatting a `double` to a number of decimals answers for the binary
value, not the decimal the user meant (`1.005`). ICU4X formats a
decimal type of unbounded length on the heap; its 2.3 release fixed an
overflow in its decimal exponent and another in its plural operands.
This library allocates nothing (record mloc-0007's values are
fixed-size and copyable).

## Decision

- `mlocDecimal` is a 52-byte value: up to 40 significant digits (the
  first and last nonzero, one per byte), the power of ten of the last
  one, the count of fraction digits shown, the sign, and its kind
  (finite, infinity or NaN). Trailing zeros are not stored as digits:
  `1.50` is the digits `15`, exponent -1, two fraction digits shown, so
  padding costs no capacity. Every byte is defined, so two decimals
  compare equal with `memcmp` exactly when they have the same value
  and show the same digits; `-0` keeps its sign, as ECMA-402 formats
  it.
- It is made from `int64_t` and `uint64_t`, and parsed from text:
  an optional sign, digits with an optional fraction (at least one
  digit), and an optional exponent `e` or `E`. The exponent moves the
  point, so `1.50e1` shows `15.0`. More than 40 significant digits, or
  a value beyond 10^±999,999, is refused with `mloc_errorCapacity`,
  never rounded silently; anything else is `mloc_errorSyntax` at the
  offending byte.
- `mlocMakeShortestDecimal` gives a `double`'s shortest round-trip
  digits (of the fewest that read back, the nearest, ties to even) by
  Ulf Adams's Ryu, in integer arithmetic only, so every platform gives
  the same digits. Its powers of five, 125 bits each (9,872 bytes),
  are written by `tools/gen_pow5_tables.py`, which CI runs with
  `--check`. It agreed with Python's `repr` on 31 million doubles
  (every exponent with four mantissas, powers of ten and their
  neighbours, integers, thousandths and random bit patterns); the
  tests read 200,000 random ones back with `strtod`.
- `mlocRoundDecimal` rounds to a multiple of an increment (1, 2, 5 or
  25, which with the position give all of ECMA-402's increments) times
  a power of ten, in ECMA-402's nine modes, half-expand the default.
  The result shows the fewer of its fraction digits and the position's
  (`1.999` at hundredths is `2.00`, `1.5` stays `1.5`), a value that
  rounds to zero keeps its sign, and a result past 40 significant
  digits or 10^999999 is refused. `mlocShowFractionDigits` pads or
  trims the zeros shown without touching the digits. Python's
  `decimal` module agreed on 1.3 million random cases (the two it
  could write and this cannot hold refused with a reason).
- `mlocFormatDecimal` writes it back as plain text (`-1234.50`,
  `Infinity`, `NaN`), which parses to the same value.
- The plural operands are computed here, for the `plural` part and the
  number formatter: `i`, `f` and `t` as integers, `v` and `w` as
  counts, and the compact exponent `e` (`c`), which the caller sets.
  An integer of more than 18 digits is stored as 10^18 plus its low 18
  digits: it then equals no literal of a rule and keeps every
  remainder by a power of ten up to 10^18, the only operations rules
  make on it, so no operand overflows.

## Consequences

A value from text keeps the digits the user typed; formatting
options that show more fraction digits change `v` without touching the
digits. A text of 41 significant digits, which ECMA-402 accepts and
rounds, is refused here with a reason.
