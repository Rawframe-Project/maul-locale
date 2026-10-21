# mloc-0066. The collation element iterator, and NFD read as a stream

Status: Accepted

## Context

Compare and sort keys need the collation elements of UTF-8 text, one at
a time. The root unit (record mloc-0065) leaves out every mapping whose
first code point decomposes, as ICU4X's data does. So the text must be
read as its NFD.

Maul Unicode's `muniNormalize` works on whole strings into a caller
buffer, and refuses more than 32 marks in a row. Compare allocates
nothing and must accept any text.

## Decision

- **NFD as a stream** (`src/nfd_stream.{h,c}`), its own module, for
  segmentation and transforms later:
  - It reads code points with `muniDecodeUtf8`: bad bytes give U+FFFD,
    one for each maximal ill-formed subpart.
  - It decomposes them with `muniDecomposePair`, applied until nothing
    decomposes, and orders marks by `muniGetCombiningClass`.
  - It fills a window of 64 code points: a starter and the marks after
    it, decomposed, the marks sorted stably by class.
  - A longer run of marks is given in windows, in order. The stream
    makes passes over the run's bytes, one for each class present,
    smallest first, decoding again each time. Each pass is linear in the
    run, and there are at most 254.
  - So the stream is exactly NFD for every text, with no allocation and
    no limit. Where windows end depends only on the NFD.
- **The iterator** (`src/collation_iterator.{h,c}`) gives 64-bit
  elements:
  - A trie value that is one element gives it. Expansions give theirs
    from the arrays. An offset's element is computed.
  - The implicit word gives ICU's unassigned primary, lead byte FE. It
    is now `mlocCollationUnassignedPrimary` in `collation_tables.h`, for
    the generator and the runtime alike.
  - U+FFFD and U+FFFF are written in the unit as simple CE32s with
    their fixed primaries (`0xFFFD0505`, `0xFFFF0505`). Bad bytes cost
    nothing extra.
  - Hangul reaches the trie as the jamo NFD gives.
  - **A prefix list** is matched against the code points before, longest
    entry first, from a history of the last eight.
  - **A contraction list** takes the longest entry the following code
    points spell, then follows UTS #10's discontiguous rule. A mark
    extends the match when the longer string is an entry, unless a
    skipped mark of an equal or higher class, or a starter, blocks it.
    The marks matched are taken out; the skipped ones follow in order.
  - The rule looks at most 72 code points ahead, a bound that depends
    only on the NFD (amended below: past it through a copy of the
    stream).
  - A completely ignorable element is given as 0; compare skips it.

## Proof

- **`test_nfd_stream`:**
  - 20,000 random texts against `muniNormalize`'s NFD. They mix
    decomposable letters, Hangul, jamo, marks of several classes, marks
    that decompose, starters whose decomposition ends with a mark, and
    bad bytes.
  - Runs of 62 to 1,000 marks, decomposing ones among them, against a
    reference sort.
  - Two canonically equal texts of 200 marks read the same.
  - A long run ending the text is read within it.
  - Every column of Maul Unicode's NormalizationTest.txt, when its
    sources were fetched.
- **`conform_collation_root`, in the cldr CI job, on CLDR 49:**
  - Every mapping is spelled as text and read through the iterator. A
    prefix mapping follows its prefix's own elements.
  - The 58,604 kept mappings give the file's elements through the
    unit's paths.
  - The 2,089 left-out mappings give them through NFD.
  - None differs once completely ignorable elements are set aside. NFD
    adds some, as U+1D165's after U+1D15E's notehead.
- **`test_collation_iterator`, on a small root:**
  - every element path;
  - bad bytes and the fixed code points;
  - Hangul;
  - a prefix;
  - contractions: contiguous, through NFD, discontiguous in either
    order, blocked by a mark of the same class, ended by a starter;
  - the lookahead's bound.

## Consequences

- **Speed is open** for slice (g). Each code point at or above U+00C0
  costs two calls into Maul Unicode. Contraction lists are searched in
  order, where ICU4X uses flags and tries.
- The general compare follows (slice a4). `test_collation_data` reads
  the generated file's root through the iterator.

## Amendment (a), 2026-10-21: no bound on the discontiguous rule

The 72-code-point bound made the general compare and the keys differ
from the fast Latin compare, which reads a precomposed letter as ICU4C
does (fuzz_collate_fast in CI, sv: U+00F4 then 72 U+0323 against "U":
the fast compare said after, the keys before, since the NFD's U+0302
lay past the bound and the contraction for ô was not matched). ICU4C's
skipped buffer has no bound. Now a contraction looks past the 72 code
points through a copy of the NFD stream; the code points it takes from
there are dropped by their count when the stream reaches them, and the
marks it skipped there are given from the match's end. At most 32 such
code points wait at once (only crafted text holds more marks between a
contraction's parts than that many contractions in a row); past that the
rule stops at the lookahead again, alike in every path. The test now
matches past 70, 71, 72, 80 and 1,000 marks and twice in one text, and
the search elements' oracle gives ICU4C's elements for 648 texts with
60 to 200 marks before a contraction's last part.
