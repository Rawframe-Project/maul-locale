# mloc-0020. Plural rules compiled to C, chosen per locale by data

Status: Accepted

## Context

CLDR 49 has 39 distinct cardinal rule sets and 28 ordinal ones
(`plurals.xml`, `ordinals.xml`) for all its locales, and 22 tables of
what a range's ends give (`pluralRanges.xml`). Rules read the operands
of record mloc-0019 with `=`, `!=`, ranges, lists and remainders. ICU4X
interprets a small tree per category (36 ns for an integer); the same
rule compiled to C selects in about 4 ns (research 18). Record L16 of
the design compiles algorithms over a closed set into the library and
lets locale data refer to them by index.

## Decision

- The generator compiles each distinct rule set into a C function over
  `mlocPluralOperands`, written to `src/generated/plural_rules.c`
  (cardinal sets first, then ordinal ones, numbered in that order). The
  file's hash enters the data version (record mloc-0014), so a data
  file opens only with the library whose rules its indexes name.
- Every locale id that the three files name gets a unit of kind
  `0x00030001` (part 3, plural), eight bytes: its cardinal rule,
  ordinal rule and range table, each resolved by truncation (`pt_PT`'s ordinals are `pt`'s,
  then root's) and `0xFF` where nothing resolves. The range tables are
  one unit of no locale, kind `0x00030002`, six by six categories each.
  A locale finds its unit along its chain in the plurals component
  (record mloc-0016), whose parents CLDR leaves empty.
- `mlocInitPluralRules` fills a 40-byte value with the rule's index and
  its range table, so selecting reads no data; it reports
  `mloc_fellBack`, a new positive outcome, when the unit is a parent's.
  `mlocSelectPlural` takes a decimal (infinity and NaN are other),
  `mlocSelectPluralInteger` an integer without making its decimal,
  `mlocSelectPluralOperands` takes operands with a compact exponent,
  `mlocSelectPluralRange` gives a range's category (the end's where
  CLDR has no table), and `mlocGetPluralCategories` the categories a
  locale uses.
- Rule semantics follow UTS #35 Part 3: `n` equals an integer only when
  `t` is 0; `x != y` is `not (x = y)`; `%` takes a remainder by a power
  of ten that divides 10^18, which the generator checks, so the wide
  operands of mloc-0019 never mislead a rule.
- Proof: every sample of both files (integers, decimals, ranges with
  `~`, compact values with `c`) selects its category, in the cldr CI
  job, through `mlocgen plural-samples`.

## Consequences

Selecting is a switch and a few comparisons; a locale's rules cost
eight bytes plus its id. Changing a rule means regenerating both the
code and the data, which the data version enforces.
