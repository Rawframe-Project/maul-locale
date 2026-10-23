# Collation

The root collation, tailorings, the compare paths, sort keys, search and the alphabetic index. Runtime sources: `src/collat*.c`, `src/nfd_stream.c`, `src/search*.c`, `src/index*.c`; generator: `tools/mlocgen/collation_*.c`, `trie_gen.c`, `index_gen.c`. The order and keys are ICU4C's; the data is built here from CLDR, never taken from ICU.

## Contents

- [Data](#data)
- [Reading text](#reading-text)
- [Comparing](#comparing)
- [The fast Latin table](#the-fast-latin-table)
- [Tailorings](#tailorings)
- [Sort keys](#sort-keys)
- [Search and index](#search-and-index)
- [Kept differences](#kept-differences)

## Data

`collation.mloc` holds the root unit and a delta unit per tailoring (locale and type). The root is built from CLDR's `FractionalUCA.txt`, read as ICU's `genuca` reads it; Han and unassigned code points get the weights ICU computes for them. Elements are 64-bit in ICU's layout (primary in the high half). Lookups go through a code point trie of 32-bit values (`src/code_point_trie.h`) in ICU's shape but not its bytes: offsets are 32-bit, so no index needs ICU's 18-bit packing, and equal blocks are shared but not overlapped. A trie value is ICU's CE32: a simple element, or a tag for an expansion, contraction, prefix, offset or the implicit weight, ICU4X's subset of tags.

The unit leaves out every mapping whose first code point has a canonical decomposition, Hangul syllables and surrogates, as ICU4X's data does, because the runtime always reads text in NFD (below). The script table for reordering, the compressible lead bytes and the unsafe-backward ranges are sections of each unit.

## Reading text

`nfd_stream.c` gives a text's NFD as a stream of code points, from Maul Unicode's decomposition and combining classes, in a window of 64 code points: a starter and its marks, sorted. A longer run of marks is given in windows, in passes over the run's bytes, one per combining class present, so the stream is exactly NFD for every text with no allocation and no limit. Ill-formed bytes are U+FFFD, one per maximal ill-formed subpart. A quick path skips decomposition for text already in NFD. UTF-16 goes through the same stream with its own decoder, a lone surrogate as U+FFFD, so both forms of a text always collate alike.

Reading NFD always, instead of ICU's FCD shortcut with canonical closure in the data, keeps the data smaller and makes the tailoring builder simpler: a precomposed character never reaches a trie.

## Comparing

The general compare (`collation_compare.c`) goes level by level and buffers nothing: a level the one before leaves equal reads both texts again from the start, since most compares end at the first level. Shifted handling, backwards secondaries (French), case first and the case level, numeric ordering and reordering apply as ICU's `CollationCompare` applies them; with nothing buffered, each level applies the shifting again to the stream it reads.

Both compare paths start after the texts' identical prefix, backed up to a code point's start and over unsafe-backward code points (those a contraction or prefix could join with what precedes), so a shared prefix costs a byte compare. The general path's iterators get the prefix's last code points as history for prefix rules. The skip is not taken when shifted or with backwards secondaries, where the prefix can still change the order.

## The fast Latin table

ICU4C compares common Latin text through a table of mini elements, which is why it compares several times faster than ICU4X. The generator builds ICU4C's table and format, version 2, for the root and each tailoring, as ICU's builder does, but from the general compare's elements over NFD (é is e and U+0301, merged into one mini element). The fast compare reads it first and bails out to the general compare on anything it cannot answer. A tailoring whose table equals the root's says so instead of storing it. `fuzz_collate_fast` checks that the two paths agree on every input.

## Tailorings

The builder (`collation_builder.c`, `collation_weights.c`, `collation_rules.c`) follows ICU4C's `CollationBuilder` and `CollationWeights`: the rule parser reads rule strings as ICU's parser does, resets and relations insert nodes into lists over the root's elements, and the weight allocator gives the shortest weights between their neighbors within each level's byte limits. There is no canonical closure, since the runtime reads NFD; each tailored string is stored in NFD. A tailoring is a delta unit whose trie falls back to the root's, so the runtime looks a code point up in the tailoring, then the root.

Every collation type of every CLDR locale is built in CI and its answers compared with ICU4C 79's (`conform_collation_builder` with an expected hash), and every tailoring's unit is read back through the runtime iterator.

## Sort keys

Keys are ICU4C's bytes for every text and option (`collation_key.c`): compressed primaries, common-weight runs, the case level in nibbles, backwards secondaries reversed per segment, the identical level as BOCSU over the NFD. Nothing is buffered: each level reads the text again and writes into the caller's bytes, a backwards segment reversed in place. When the room is too small the length is still counted.

The collation version is an XXH64 of the root unit's bytes seeded with the key format, joined by the tailoring's hash, computed when the collator is made. Any change to the data the collator reads changes it; the options do not, as in ICU, since the caller keeps them with the keys.

## Search and index

Search (`search*.c`) follows ICU4C's `usearch`: processed elements as `UCollationPCE` makes them, ICU's match boundary rules (grapheme clusters, the mid-cluster rule, the identical check), forward and backward, as a stateless pair of calls where ICU keeps an iterator. Grapheme boundaries at any position come from Maul Unicode's grapheme iterator started at the nearest code point before the position that no rule reads back past.

The alphabetic index (`index*.c`) follows ICU4C's `AlphabeticIndex`. The generator resolves each locale's initial labels once (its index exemplars, else its exemplars with the synthesis and case mapping ICU applies) into a unit per locale key; the run time filters them over the collator and builds the buckets into caller memory. Labels follow CLDR's chain, where ICU4C falls back to the bundles it ships: ICU4C gives `az-Arab` Azerbaijani's Latin labels, which CLDR's chain never reaches from an Arabic-script locale.

## Kept differences

- **Han order:** CLDR's root and its conformance files order unified ideographs by code point; ICU4C's default data orders them by radical and stroke. The library follows CLDR, as ICU4X does, so keys holding Han differ from ICU4C's in those primaries; the `unihan` types order by radical and stroke.
- **Fast compare and keys:** in three cases ICU4C's fast compare and its keys disagree with each other (superscript digits under a digits-after reordering, U+FFFE's quaternary under shifted, a shifted identical prefix); the library follows the keys.
- **Lone surrogates** read as U+FFFD where ICU4C weighs them as themselves, and a position inside a surrogate pair counts as its first unit in search.
- **Private-use collation types** read as none, where ICU refuses them.
