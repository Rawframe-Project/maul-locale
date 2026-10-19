# mloc-0063. A code point trie of 32-bit values

Status: Accepted

## Context

Collation looks up a value for nearly every code point it reads: a
collation element, or a tag that leads to more. The lookup sits in the
innermost loop of every comparison. Segmentation and transforms will
need the same shape of table.

The library had no such table:

- Its units find entries by binary search or by hash.
- Maul Unicode's rank indexes over bit maps suit sparse properties,
  not a value for every code point.

ICU's fast code point trie answers this:

- a BMP code point in two reads, a supplementary one in three;
- one value for the long tail of unassigned planes.

Its byte layout, with 18-bit index tricks and overlapping blocks, is
more than this needs.

## Decision

- **The layout** (`src/code_point_trie.h`) keeps ICU's shape, but not
  its bytes:
  - **BMP:** an index of 1,024 offsets, one per 64 code points.
  - **Supplementary:** an index of 16-bit numbers, one per 4,096 code
    points, naming index blocks of 64 offsets.
  - **Data:** blocks of 64 values, each stored once however many
    places share it.
  - **High start:** every code point from it, a multiple of 4,096,
    takes one value.
  - Offsets are 32-bit, so no index needs ICU's 18-bit packing.
- **The reader is inline,** so the generator's tests read exactly what
  the library will. `mlocReadCodePointTrie` checks that every offset
  leaves a whole block inside the data, and every section names an
  index block that exists. A trie that passes is safe to look up
  without further checks.
- **The builder** (`tools/mlocgen/trie_gen.c`) takes a value for each
  of the 1,114,112 code points:
  - It finds the high start.
  - It shares equal data blocks and equal index blocks through a hash
    of their words.
  - It does not overlap blocks; ICU's compaction is not attempted
    until a measured size asks for it.

## Proof

- `test_trie`: for five maps, the trie read back gives every code
  point's value. The maps are:
  - one value;
  - single code points at the BMP's ends, the first supplementary one
    and U+10FFFF;
  - a range of distinct values ending inside a section;
  - a different value for every code point;
  - repeated blocks, shown stored once: two data blocks and one index
    block for 32 sections.
- **Refusals:**
  - a span too short;
  - a high start out of range or not a multiple of 4,096;
  - a data length below one block or above its bound;
  - a block count above its bound;
  - a BMP offset, an index block's offset or a section's block number
    past what exists.
- **Measured here:**
  - A map with Han and the BMP's first 13,312 code points distinct
    builds a 57,616-byte trie.
  - A BMP lookup takes about 1 ns.

## Consequences

- A collation unit holds a trie of CE32s. Its size beside ICU4X's root
  trie is measured when the root is built.
- Overlapping blocks can be added to the builder later without
  changing the reader.
