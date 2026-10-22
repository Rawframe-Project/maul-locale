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
there are dropped by their count when the stream reaches them (the
marks it skipped there already come from the segment's end, a segment
past the lookahead being longer than the NFD window). At most 32 such
code points wait at once (only crafted text holds more marks between a
contraction's parts than that many contractions in a row); past that the
rule stops at the lookahead again, alike in every path. The test now
matches past 70, 71, 72, 80 and 1,000 marks and twice in one text, and
the search elements' oracle gives ICU4C's elements for 648 texts with
60 to 200 marks before a contraction's last part.

Scanning to the end of a run of marks for every mark with a contraction
list cost the square of the run (the commit's security review: 5,000
U+0F71 took 72 s under the sanitizers, 15,000 more than 300 s). The
iterator now remembers the last scan that reached a starter or the end
and the highest class it left in the stream; a later scan inside it that
has skipped a mark of that class or higher stops, since the marks there
are in canonical order and all blocked. The results are unchanged and
the same texts take 0.15 s and 0.31 s; `test_collation_iterator` runs
20,000 marks. Runs whose marks really match far apart (U+0F71 then
U+0F72, each pair a contraction) still scan as ICU4C does.

## Amendment (b), 2026-10-22: a quick path through the NFD stream

Research 68 found the stream's segment work (each code point
decomposed, its class read, the next code point decoded and decomposed
to see whether it is a mark, the sources recorded and placed) to be a
third of a Chinese compare and most of a German key, and clearing the
whole stream when it starts an eighth more.

- **A segment of one code point that is its own NFD with class 0**
  (below U+00C0, or a code point with class 0 and no canonical
  decomposition), **or of a Hangul syllable**, with the text's end or a
  code point that is not a mark after it, goes into the window without
  the segment work: the code point, or the syllable's jamo computed
  arithmetically, the first placed at the source's start and the rest
  at its end, as the full path places them. Anything else takes the
  full path, unchanged.
- **Starting a stream** sets only the fields a fill reads before it
  writes them.
- On the bench, back to back against the commit before (two runs each,
  load 25 to 38): compare zh 599 and 762 ns to 327 and 271, ko 1,143 and
  1,007 to 378 and 356, German keys 1,051 and 1,280 to 557 and 609, the
  index's bucket 1,629 and 1,434 to 874 and 1,250, the first search
  match 1,565 and 1,275 to 779 and 782; compares the fast Latin table
  answers are unchanged.

### Proof

The stream gives the same code points, classes and places: the suite
(the conformance files, the fuzz targets' seeds), the 1,246,793 keys of
the key differential and the 18,286 searches of the search differential
against ICU4C (both offsets, both directions) are the same, and so are
the 1,294 indexes of the index differential.
