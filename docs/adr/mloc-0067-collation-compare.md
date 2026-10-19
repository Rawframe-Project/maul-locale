# mloc-0067. The general compare, level by level, with nothing buffered

Status: Accepted

## Context

The element iterator (record mloc-0066) gives a text's elements one at
a time. Compare must order two texts by UCA's levels, as ICU4C's
`CollationCompare` does, within the fixed-size collator (L32): no
allocation, and any text length.

ICU4C and ICU4X keep each text's elements in a buffer while the first
level is compared, so later levels read them back. Their buffers grow
on the heap past a few elements.

## Decision

- **`mlocCollationCompare`** (`src/collation_compare.{h,c}`) takes a
  root unit, a strength (primary, secondary or tertiary) and two UTF-8
  texts. It returns -1, 0 or 1.
- **One level at a time:**
  - At each level, the elements whose weight there is zero are passed
    over.
  - A text that ends first reads ICU's NO_CE: primary 1, secondary and
    tertiary 0x0100. These are below every weight, so that text is the
    lesser.
  - Primaries are compared whole and secondaries forwards. Tertiaries
    are compared without the case bits (mask `0x3F3F`), as ICU does
    when neither case first nor a case level asks for them.
- **Nothing is buffered.** A level the one before leaves equal reads
  both texts again from the start. Most compares end at the first
  level, so the second and third passes are rare. Whether a small
  buffer pays is measured in slice (g).
- **The alternate setting is non-ignorable,** CLDR's root default. The
  options come in slice (b): shifted with the variable top (CLDR's
  `[variable top = 0B FF FF FF]` for punctuation), quaternary and
  identical strengths, case first, case level, backwards secondaries,
  numeric order and reordering. The SHIFTED conformance files come with
  them.

## Proof

- **`conform_collation`, in the cldr CI job:**
  - It reads the root from `src/generated/collation.mloc`.
  - It runs `CollationTest_CLDR_NON_IGNORABLE_SHORT.txt` and
    `CollationTest_CLDR_NON_IGNORABLE.txt` on CLDR 49.
  - All 210,186 lines of each compare no greater than the next at the
    tertiary strength, 27,421 of them equal to the line above.
  - 30 lines hold a surrogate, which UTF-8 cannot carry, and are passed
    over, as ICU4X's conformance test does.
  - The same lines in reverse order put 182,734 out of order.
- **`test_collation_compare`, on the generated root:**
  - each level alone, and the strength where comparing stops;
  - forward secondaries (côte after coté);
  - a text that ends first;
  - canonically equal texts, the marks in either order;
  - U+0000 and a soft hyphen completely ignorable;
  - Latin, then Han in order, then unassigned, then U+FFFD;
  - a bad byte as U+FFFD;
  - every answer the mirror of its swap.

## Consequences

- **Speed is open.** The conformance file's 210,186 compares take about
  0.1 s here, about 0.5 µs each, with no identical prefix skipped and
  no fast Latin path. Slice (e) brings the fast Latin path and slice (g)
  the speed work, measured against ICU4C's compare.
- The public collator, opened from a locale and its `-u-` keys, comes
  with the options in slice (b).
