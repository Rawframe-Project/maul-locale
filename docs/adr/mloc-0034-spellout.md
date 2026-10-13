# mloc-0034. Spell-out: ICU's rules compiled, run on the exact decimal

Status: Accepted

## Context

CLDR 49 writes numbers in words, ordinal suffixes ("21st") and the
algorithmic numbering systems (Roman, Hebrew, Armenian, Chinese and
the others) as rule-based number formats in ICU's syntax: 101 files
in `common/rbnf`, 147 groups of rules (98 spell-out, 48 ordinal
suffix, root's numbering systems), 1,676 rule sets, 21,862 rules, 779
KB of text. A group is inherited whole along the locale's chain
(`es_419` has its own ordinal suffixes and takes `es`'s words).
`common/testData/rbnf` holds 52,913 expected results.

ICU4C is the only implementation (ICU4X 2.3 and ECMA-402 have none).
It parses the rule text whenever a formatter is made and formats by
recursion into UTF-16 strings, on an `int64_t` for integers and a
`double` for everything else: `12345678901234567.5` is written
"... five hundred sixty-eight", the half lost and the integer rounded,
and values past `int64_t` are not spelled out at all but written as
digits, an ordinal suffix dropped.

## Decision

- **Compiled by the generator** (`tools/mlocgen/spellout_compile.c`):
  each group's rules are read as ICU4C 79 reads them (its whitespace
  stripping, descriptors, implicit bases, bracketed rules expanded into
  their two rules, substitution tokens starting at any `<`, `>` or `=`,
  the special rules, fraction rule sets, plural blocks), and refused
  where ICU refuses them. The compiled rules of every group, written
  back as ICU's `getRules` writes them, are byte for byte ICU4C 79's.
  One rule ICU takes is refused: a fraction rule set read before a
  rule names it, which ICU parses as a normal set.
- **Data:** a file of its own, `src/generated/spellout.mloc` (1.0 MB),
  which a program adds to its data set as it adds the unit names
  (record mloc-0024). A unit per locale key and group whose file has
  the group, so the lookup along the chain inherits a group whole; a
  unit of no locale lists the 19 algorithmic numbering systems with the
  locale, group and rule set each names. A group's unit holds its rule
  sets, its rules (base, divisor, kind, ICU's rollback flag), each
  rule's pieces in the order they are written (text, substitution,
  plural block), the plural blocks' forms and the decimal patterns
  compiled; identical texts and plural blocks are stored once. The
  layout is in `src/spellout_tables.h`; a file is validated when it
  opens: every offset, index and target in range, the text well-formed
  UTF-8, each set's bases in order.
- **Formatting** runs the rules on the exact `mlocDecimal`: quotients
  and remainders by moving the point (a divisor not a power of ten
  divides a value below its set's next base), fractions from the
  decimal's own digits, a fraction rule set's choice computed exactly.
  The results are ICU4C's wherever ICU holds the value exactly (every
  integer of `int64_t`, every fraction exact in binary); the others
  are listed departures. The rules apply to every value; recursion past
  64 levels is an error. The value is spelled out, not the digits
  shown; nothing is rounded.
- **Rule sets** by name (public ones only, ICU's default for none),
  numbering systems by CLDR id, capitalization contexts as ICU applies
  them; a locale whose rules are root's alone is formatted by them and
  reported with `mloc_fellBack`.
- **Parsing** as ICU's strict parse: every public rule set, the
  longest match, the value exact, the byte offset where it ended. No
  lenient parsing, which needs a collator and which CLDR 49's data does
  not use.

## Consequences

Spell-out costs a megabyte for a program that loads it and nothing for
one that does not. Long fractions and values past `int64_t` are
written exactly where ICU loses digits; a program comparing the two
sees those differences only. Making a formatter reads the compiled
rules in place instead of parsing text.
