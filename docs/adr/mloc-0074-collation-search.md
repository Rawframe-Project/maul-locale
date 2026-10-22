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
- **The forward find is usearch_search** (`src/search.c`), its loop and
  bounds as ICU4C 79 writes them, over a ring of the text's elements
  sized as ICU's CEIBuffer (the pattern's elements, 32, and with a
  wildcard comparison 3 a UTF-16 unit of the pattern, 8 for a possible
  leading jamo; at most 512). Where ICU would read past its buffer (a
  null element it dereferences) the find returns `mloc_errorCapacity`;
  where ICU loops for ever (a wildcard comparison skipping the
  pattern's elements against the text's end) the find stops: no match
  starts at the end.
- **A start inside the text** is ICU's setOffset: back over unsafe code
  points, forward to the last element boundary at or before the
  position; the iterator starts there with the eight code points before
  it as its prefix history, as ICU's iterator reads them back.
- **Refused as ICU refuses:** a numeric collator (U_UNSUPPORTED_ERROR)
  and a pattern none of whose 32-bit halves ICU's getCE keeps (only
  ignorables, or only variable ones when shifted below the quaternary
  strength).
- **No allocation:** the searcher is a fixed-size value; the text's
  elements pass through a fixed ring within each call.
- **Backward as ICU4C finds it** (`mlocFindPrevious`): its
  usearch_searchBackwards over UCollationPCE::previousProcessed. The
  text's raw halves walking back are gathered to one with a primary
  that is not a continuation, that chunk processed forward and handed
  out last first, the shifted state carried on to the chunk before it
  (so a run without a primary at the text's start is processed as if
  the chunk after it came first, as ICU does). The library reads them
  from pieces read forward, each starting at a safe code point (back
  over the unsafe ranges), so the collation iterator stays one; a piece
  or a chunk holds at most 128 halves, past which the find answers
  mloc_errorCapacity. The walk starts after the cluster at the
  position, at the last boundary between elements at or before its end;
  a match's last element starts before the position, and one with
  nothing after it in the walk ends at the position at the latest.
- **No empty and no endless match:** where a wildcard comparison
  skips a pattern of marks against the text's end (forward) or start
  (backward), ICU4C loops for ever or returns an empty match; the
  library finds none. A match whose elements outgrow the ring is
  refused where ICU4C, its buffer's places reused, returns a wrong one.

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
- (f2b2b2) `conform_search` against ICU4C 79's `usearch_search`
  (normalization on; the oracle runs each search in a child process
  with a time limit, as ICU crashes or hangs on some): every tag under
  seven option sets, patterns of one to three and texts of up to ten
  code points from the elements' pool, half of the texts holding the
  pattern, a random start in 30%, each comparison: 5,040 searches, all
  alike but the 5 where ICU crashed or never ended (the library finds
  none there); the UTF-8 find agrees with the UTF-16 one on every line.
  ICU refuses an empty text, the library finds nothing in it.
  `test_search`: strengths, a precomposed letter, an accent's cluster,
  Czech ch never split and a start inside it, the comparisons, UTF-16
  positions and a start inside a pair, the refusals.
- (f2b3a) `conform_search_elements --back` against ICU4C 79's
  `previousProcessed` from the text's end (oracle
  `icu4c_search_elements_back.cpp`), on the elements' 215,600 texts:
  all of the 213,742 whose forward elements agree also agree backward
  but 22 under numeric ordering (ICU's backward walk splits a digit run
  its forward one keeps whole), which search refuses; ICU's backward
  elements differ from its forward ones reversed in 1,426 texts, each
  reproduced. `test_search_elements`: last first, a walk from inside the
  text, UTF-16, the shifted state carried back, a piece and a chunk too
  long.
- (f2b3b) `conform_search` with a backward field against ICU4C 79's
  `usearch_searchBackwards`: the forward find's 15,490 inputs walked
  back from the text's end and from their own position, 18,286
  searches. All agree but 1,131: 1,080 empty texts (ICU refuses them),
  23 where ICU crashed or never ended, 18 where it returns an empty
  match at 0 for a pattern of one mark under any-base comparison, and
  10 whose window outgrows the ring, where ICU returns 0 to 5 for a
  120-letter pattern; the library finds none in the first three and
  refuses the last. `test_search`: from several positions, a walk from
  inside a cluster, a contraction, UTF-16 across a pair, the refusals.
- The differential found that numeric ordering passed over digits the
  emoji type gives contractions (the keycaps): ICU4C's builder gives
  every mapped digit its digit tag, which its iterator reads before any
  list, so every digit is numeric under kn. The library now does the
  same (`test_collate`).

## Consequences

- The identical-prefix skip of slice (g) backs up over the same
  ranges.
