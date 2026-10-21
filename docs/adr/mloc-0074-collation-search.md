# mloc-0074. Collation search: ICU4C's usearch over processed elements

Status: Accepted

## Context

Slice (f) gives collation string search (L32; research 67). ICU4C's
`usearch` matches a pattern's processed elements against the text's,
each element carrying the source range it came from, with match
boundaries on grapheme clusters. ICU4X has no search. A search that
starts inside a text must first back up to a position where the
collation iterator gives what it would give from the text's start:
ICU's `CollationElementIterator::setOffset` backs up over its
unsafe-backward set.

## Decision

- **ICU4C's semantics:** the element comparison types (exact, and the
  two wildcard types), forward and backward matching with ICU's match
  boundary rules (grapheme clusters, the midcluster rule, the identical
  check), over UTF-8 and UTF-16 alike.
- **A stateless pair of calls:** a find from a position forward, as
  `usearch_search` from that offset, and one backward, as
  `usearch_searchBackwards`; overlapping or not is where the caller
  starts the next find (after the match's start or at its end), as
  `usearch_next` does with its overlap attribute.
- **Processed elements as ICU4C makes them:** each 64-bit element
  split into ICU's two 32-bit halves, the strength's levels kept and
  variable elements shifted as `UCollationPCE::processCE` does, so the
  matches are ICU's to the element.
- **Unsafe-backward ranges in the data:** each unit (the root and each
  tailoring) holds the code points after the first of each of its
  contractions (`src/collation_unsafe.h`), a section after the fast
  Latin table. A position is unsafe before a mark (lead combining class
  not 0), before a code point whose NFD starts with a code point in the
  root's or the tailoring's ranges (ICU's canonical closure adds the
  precomposed forms, which the library reads as their NFD), and before
  a digit under numeric ordering.
- **Grapheme boundaries at any position** (`src/search_boundary.c`):
  Maul Unicode's grapheme iterator, run from the nearest code point
  before the position whose break is Other, Control, CR or LF, not
  Extended_Pictographic and without an Indic_Conjunct_Break: no rule
  reads back past such a code point, so the boundaries after it are the
  whole text's. UTF-16 goes to the iterator as UTF-8 in pieces, a lone
  surrogate as U+FFFD (ICU's break iterator reads it as a control; the
  library reads both forms alike).
- **No allocation:** the searcher is a fixed-size value; the text's
  elements pass through a fixed ring within each call.

- **Source ranges as ICU4C's FCD iterator gives them** with
  normalization on (the library always reads the NFD): text in
  canonical order keeps each code point's range, a decomposed one's
  first element spanning it and the rest empty at its end; the part ICU
  normalizes (from the start of the part where a mark is out of order,
  or a Tibetan composite vowel, to the segment's end) has one range. A
  discontiguous contraction spans to its last code point's end and the
  marks it skipped are empty there. A segment of more than 64 code
  points reads as normalized whole (ICU keeps the ranges of one in
  order); only crafted text has one.

## Sub-slices

(f2b1) the unsafe-backward ranges; (f2b2a) processed elements and
their ranges; (f2b2b1) grapheme boundaries at any position; (f2b2b2)
the searcher and the forward find; (f2b3) the backward find.

## Proof

- (f2b1) `conform_collation_fast --unsafe` against ICU4C 79's
  `unsafeBackwardSet` for all 154 tags, marks and surrogates set apart
  (an oracle in the docs repository reads ICU's set): every code point ICU holds
  that the library does not is a precomposed form whose NFD starts in
  the library's ranges or with a mark, except U+16D93 and U+16D9D,
  which ICU's UCA 17 root has as contraction suffixes and CLDR 49's
  UCA 18 root does not; the library holds none ICU does not, but for
  bal, bal-Latn, cu, dz and wae, whose tailorings ICU 79 does not ship.
  `test_collation_unsafe_table`: a small root and a tailoring (a
  contraction, a prefix's contraction, a prefix left out, a mark among
  the code points), and the readers' refusals.

- (f2b2a) `conform_search_elements` against ICU4C 79's `UCollationPCE`
  (normalization on), every tag under seven option sets (strengths,
  shifted, quaternary, numeric, identical), random texts of up to eight
  code points from a pool of letters, marks of many classes,
  precomposed letters, Hangul, digits, Thai and Lao, Tibetan composite
  vowels, controls, noncharacters, emoji and regional indicators:
  215,600 texts, every element and range alike. Han and U+16D68 are
  left out of the pool: Han's implicit primaries differ between ICU's
  UCA 17 root and CLDR 49's UCA 18 root, and ICU reads U+16D68 (whose
  NFD is U+16D67 twice) as itself where the library reads its NFD,
  which a contraction before it then takes. The nine tags ICU 79 does
  not ship (aa, bal, bal-Latn, cu, dz, sgs, ssy, vo, wae) and de's
  provisional eor are set apart. `test_search_elements`: the halves,
  each strength, shifted (a continuation half compared alone with the
  variable top, as ICU does), the ranges.
- (f2b2b1) `test_search_boundary`: 20,000 random texts of the code
  points the rules read back through (regional indicators, ZWJ
  sequences, conjuncts and their linkers, Hangul, prepends, spacing
  marks, CR LF, controls) and ill-formed bytes; at every code point
  boundary, whether it is a boundary and the next one agree with the
  iterator over the whole text, in UTF-8 and in UTF-16; regional
  indicators paired from far back; a cluster longer than a piece.
- The differential found that numeric ordering passed over digits the
  emoji type gives contractions (the keycaps): ICU4C's builder gives
  every mapped digit its digit tag, which its iterator reads before any
  list, so every digit is numeric under kn. The library now does the
  same (`test_collate`).

## Consequences

- The identical-prefix skip of slice (g) backs up over the same
  ranges.
