# mloc-0071. Tailorings: the builder and its weight allocator

Status: Accepted

## Context

CLDR 49 has 136 collation files. Each holds one or more types:
`standard` in 114, `search` in 21, and also `traditional`,
`phonebook`, `eor`, `unihan`, `emoji`, and zh's `pinyin`, `stroke` and
`zhuyin`. Each type is a rule string (record mloc-0068) over the root
(records mloc-0064, mloc-0065). ICU4C turns rules into a tailoring with
its `CollationBuilder`:

1. root elements to find the weights after or before a root weight;
2. a node list per root primary for the items inserted at each
   strength;
3. `CollationWeights` to allocate new weights in the root's gaps;
4. a data builder that maps the tailored strings, with the root behind
   it for everything else;
5. a canonical closure.

ICU4C 79's release build on CLDR 49 orders by the same data, so it is
the oracle for every type.

## Decision

- **The builder follows ICU4C's `CollationBuilder`**, in the generator:
  - root elements built from FractionalUCA.txt, never shipped;
  - node lists, and weights from the weight allocator;
  - extensions, prefixes, and contractions that carry the root's
    contractions with the same first character;
  - case bits, `[suppressContractions]`, and the rules' settings as
    the tailoring's defaults; `[optimize]` read and dropped.
- **No canonical closure.** The runtime always reads NFD (record
  mloc-0066), so a precomposed character never reaches a trie. Each
  tailored string is stored in NFD, where its canonical equivalents
  meet it. ICU's closure exists for its FCD shortcut, which this
  library does not take.
- **A tailoring is a delta unit over the root**, found by locale and
  type along the locale's chain. Its trie gives a fallback value for
  "the root's". The runtime looks a code point up in the tailoring
  first, then in the root.
- **The weight allocator** (`tools/mlocgen/collation_weights.c`) is
  ICU's `CollationWeights`:
  - It finds the ranges of weights between two limits: a middle range
    at the level's middle length, and lower and upper ranges at each
    longer length, merged when no middle range is left.
  - It takes the shortest ranges that hold n weights, lengthening
    ranges by a byte at a time up to four bytes.
  - Byte limits by level:
    - primaries: a lead byte from 03 to FF; the second byte from 04 to
      FE under a compressible lead byte, else from 02 to FF;
    - secondaries: bytes from 02 to FF;
    - tertiaries: bytes from 02 to 3F, the case bits left clear.
  - It refuses a gap that cannot hold n weights; it never wraps. As in
    ICU, it also refuses limits out of order, equal, or one extending
    the other.

- **The root elements** (`tools/mlocgen/collation_root_elements.c`)
  are ICU's table, built as its `CollationBaseDataBuilder` builds it
  and read as its `CollationRootElements` reads it:
  - every distinct root element without its case bits, in order, from
    the mappings, the script start and special lines, and ICU's own
    primaries (the first unassigned and trailing ones, U+FFFD's and
    U+FFFF's); U+FFFE, the merge separator, has none;
  - a primary unit, then a delta unit per set of secondary and
    tertiary weights other than the common one; a run of primaries a
    fixed step apart is its first and last; Han's primaries are runs
    around the Han characters with weights of their own;
  - the queries the builder asks: the primary, secondary and tertiary
    before and after a root weight, the last element before a primary
    and the first at or after it.
  - It refuses an element the table cannot hold, as ICU does: a
    four-byte primary, a secondary or tertiary of 01 00 or less, a Han
    primary under weights below the common ones; and a tertiary with
    the top bits of its second byte, which ICU only asserts.
  - **Han's section differs from ICU4C's** by the root's Han order
    (record mloc-0070): ICU4C orders Han by radical and stroke, the
    root here by code point, so the Han characters with weights of
    their own (the radicals' variants) split Han's runs elsewhere.
    The same delta units occur in both.

## Proof

- **`test_collation_weights`:**
  - one-byte primaries in a middle range;
  - two-byte ones where a lower and an upper range overlap;
  - 300 weights in a gap of one, lengthened by a byte and then by
    another, the 254th and 300th as computed by hand;
  - a compressible lead byte's second byte kept from 04 to FE;
  - tertiaries in an upper range and capped at 3F;
  - secondaries in a middle range;
  - each refusal.
- **`test_collation_root_elements`:** a small root's table unit by
  unit (ignorables, runs of two- and three-byte primaries, the case
  bits dropped, a common unit after weights below it, Han's runs, the
  root's own primaries); every query on it; runs across a byte's end
  written by hand; each refusal.
- **`conform_collation_root_elements`** (the cldr CI job): CLDR 49's
  table equals ICU4C 79's outside Han's section, 6,110 units with the
  indexes. In the docs repository's oracle, its 409,032 queries over
  every root weight outside Han, and about 2,000 primaries that are no root
  primary, give ICU4C's answers.

## Consequences

- To come in slice (d):
  - the nodes and the weights for resets and relations;
  - the unit, its validator and the runtime's lookup with `co`;
  - extensions, contexts, case bits and settings;
  - every CLDR type, with the differential against ICU4C 79.

  Each amends this record.
