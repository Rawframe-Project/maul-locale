# mloc-0073. Collation over UTF-16: one iterator, a lone surrogate as U+FFFD

Status: Accepted

## Context

Slice (f) gives collation UTF-16 beside UTF-8 (L32; research 67).
ICU4C compares UTF-16 natively and UTF-8 through `compareUTF8`, each
with its own iterator and fast Latin reader. Programs that hold
UTF-16 (Windows, Java and JavaScript strings) would otherwise convert
every text before comparing it.

## Decision

- **`mlocCompareUtf16` and `mlocGetSortKeyUtf16`** take `uint16_t`
  units, as Maul Unicode spells UTF-16, and give what `mlocCompare` and
  `mlocGetSortKey` give the text's UTF-8 form: the same order and the
  same key bytes.
- **A lone surrogate reads as U+FFFD**, as an ill-formed UTF-8 sequence
  does, so both forms of a text always collate alike. ICU4C weighs a
  lone surrogate as itself (an implicit weight); a well-formed text has
  none, so the difference shows only on ill-formed input.
- **One iterator:** the NFD stream takes a text in either form and
  decodes the next code point by it; the general compare, the sort key
  writer and the iterator take that text and are otherwise unchanged.
  The UTF-8 entry points are inline wrappers.
- **The fast compare** reads either form through one reader per form
  (ICU's `compareUTF16`: a unit up to U+017F indexes the table, U+2000
  to U+203F, U+FFFE and U+FFFF as in UTF-8, anything else bails out),
  its levels shared; the later levels use the same readers as the first
  rather than ICU's unchecked lookups.

## Proof

- `conform_collation`: every CollationTest line's UTF-16 form orders
  against the line above as its UTF-8 form does, through the general
  compare and the fast one, and gives the same key bytes; non-ignorable,
  shifted and identical, 0 out of order (the 30 lines with a surrogate
  are passed over in both forms).
- `test_collate`: eleven texts (supplementary characters, the fast
  compare's range, a lone surrogate against U+FFFD) under four
  collators, every pair and every key alike in both forms; the
  refusals.
- `fuzz_collate_fast`: each input's UTF-16 form compares and keys as
  its UTF-8 form.

## Consequences

- Search and index characters (sub-slices (f2) and (f3)) take both
  forms through the same text.
