# mloc-0064. The root collation, read from FractionalUCA.txt as ICU's runtime holds it

Status: Accepted

## Context

Collation starts from CLDR's root: `common/uca/FractionalUCA.txt`, UCA
18 with CLDR's changes, the file ICU's own root builder reads. Its
weights are fractional byte strings. Some code points are not in it at
all:

- **Han:** the `[Unified_Ideograph]` ranges.
- **Unassigned code points.**
- **Code points with fixed weights:** U+FFFD and U+FFFF.

Two kinds of lines are not mappings:

- **FDD1 lines** give each script's first primary.
- **FDD0 lines** give special primaries.

Later slices allocate tailored weights in the gaps between the root's.
So the root's weights must be the same ones ICU's runtime gives, not
merely ones that sort the same.

## Decision

- **Elements are 64-bit, in ICU's layout:**
  - The primary is in bits 63..32, its 1 to 4 bytes left-aligned.
  - The secondary is in 31..16.
  - The tertiary, with its two case bits, is in 15..0.
  - The file's bytes are read exactly as ICU's `genuca.cpp` reads
    them: inner 00 and 01 bytes refused, at most 4 primary bytes and 2
    for each other level.
- **What the reader** (`tools/mlocgen/collation_root.c`) keeps:
  - each mapping, with its prefix (before `|`), its text and up to 32
    elements;
  - the Han ranges in the file's order;
  - the lead bytes whose primaries compress (`[top_byte ... COMPRESS]`);
  - the UCA version;
  - FDD1 lines as script starts and FDD0 lines as specials, apart from
    the mappings.
- **Weights the file does not give, as ICU makes them:**
  - **Han:**
    - Three-byte primaries a step of 2 apart, in the ranges' order.
    - They start at `(0xFE - leadBytes) << 24 | 0x20200`, with enough
      lead bytes for twice the Han code points plus 4.
    - Each next one is counted in 254 second and third byte values.
  - **Unassigned code points:** ICU's `unassignedPrimaryFromCodePoint`,
    lead byte FE.
  - **U+FFFD and U+FFFF:** left to the runtime's fixed primaries
    `0xFFFD0000` and `0xFFFF0000`. U+FFFE keeps its merge separator.
  - **An element written `[U+4E00, 10]`:** that code point's element,
    with its tertiary, or its secondary and tertiary, replaced.
- **The oracle is `allkeys_CLDR.txt` of the same release,** the same
  root in UCA's integer weights. ICU4X's data is an older CLDR and its
  elements are not public API. ICU4C here is UCA 15.1.
  - `conform_collation_root` reads both files.
  - For each allkeys mapping, ours has as many elements. An implicit
    pair `[.FBxx][.xxxx]` counts as one, and a prefix mapping counts
    with its prefix's own elements before it.
  - The map from allkeys' weights to ours is then checked to keep their
    order and be one-to-one:
    - the primaries;
    - the secondaries under each primary;
    - the tertiaries, without the case bits, under each primary and
      secondary.
  - These are checked per context because `FractionalUCA.txt` numbers
    each level within the one above it. The case bits are a level of
    their own unless case first or a case level asks.
  - **Set aside and counted:**
    - Where allkeys follows an element with a variant's secondary
      element (011F and above), as for a CJK radical or a Meroitic
      hieroglyph's variant, `FractionalUCA.txt` has one element with
      its own secondary. These pairs' primaries are still compared.
    - U+FFFD and U+FFFF are compared with the runtime's weights.

## Proof

- **`conform_collation_root` on CLDR 49, run here and in the cldr CI
  job:**
  - All 40,467 allkeys mappings hold the same order at every level.
    299 of them are folded pairs set aside, their primaries compared.
    None is missing, and none has another number of elements.
  - Swapping the primaries of `b` and `c` in a copy of the file makes
    it fail.
- **`test_gencollation`, on small files of the same form:**
  - weights of one to three bytes;
  - empty primaries;
  - case bits;
  - a contraction, and a prefix kept apart from its text;
  - elements naming a Han code point, with their tertiary, or secondary
    and tertiary, replaced;
  - Han's first primary, and its step;
  - FDD1 and FDD0 lines kept apart, U+FFFD and U+FFFF left out, U+FFFE
    kept;
  - ICU's unassigned primaries for U+0000, U+0378 and U+10FFFF;
  - eleven malformed files and a missing one refused with a message.
- **Read here:**
  - 60,692 mappings, 178 script starts, 22 specials and 16 Han ranges.
  - Han starts at `FA020200`.
  - Reading takes 33 ms.

## Consequences

- The root unit, a trie of compressed elements with its expansions and
  contexts (record mloc-0063's trie), is built from this reading in the
  next part of the slice.
- Tailorings will allocate weights between these. Han's step of 2 is
  what leaves room for pinyin and stroke orders.
