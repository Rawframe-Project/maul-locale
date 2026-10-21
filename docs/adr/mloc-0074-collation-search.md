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
- **No allocation:** the searcher is a fixed-size value; the text's
  elements pass through a fixed ring within each call.

## Sub-slices

(f2b1) the unsafe-backward ranges; (f2b2) processed elements and the
forward find; (f2b3) the backward find.

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

## Consequences

- The identical-prefix skip of slice (g) backs up over the same
  ranges.
