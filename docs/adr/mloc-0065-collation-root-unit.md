# mloc-0065. The root collation unit: CE32s in the trie, offsets, expansions, contexts

Status: Accepted

## Context

Record mloc-0064 reads CLDR's root into 64-bit elements. The runtime
needs, for any code point it meets, the elements it stands for in one
or two reads. Contractions and prefixes need to be found by what
follows or precedes the code point.

Storing a 64-bit element per code point is not an option: a long run of
code points would leave nothing to share in the trie. ICU and ICU4X
solve this with a compressed 32-bit element (CE32) per code point,
proved over a decade.

## Decision

- **The layout** is in `src/collation_tables.h`:
  - a header with the UCA version and the arrays' lengths;
  - the code point trie of CE32s (record mloc-0063);
  - the 64-bit elements;
  - the CE32 expansion words;
  - where each context list starts;
  - the context words.
- **A CE32 is ICU's:**
  - **Simple:** a low byte below 0xC0. It holds a two-byte primary,
    and one byte each of secondary and tertiary.
  - **Otherwise a tag:**
    - long primary (three bytes, common secondary and tertiary);
    - long secondary;
    - an expansion of CE32s or of 64-bit elements, up to 31 long;
    - prefix;
    - contraction;
    - offset;
    - the implicit word 0xFFFFFFFF.
  - We use ICU4X's subset of tags. Hangul syllables, U+0000 and
    surrogates have no tags of their own.
- **What the unit leaves out,** as ICU4X's data does:
  - every mapping whose first code point has a canonical
    decomposition;
  - Hangul syllables;
  - surrogates.
  The runtime decomposes text to NFD before it looks a code point up,
  so none of these is ever read. This is the canonical closure ICU4C
  keeps and ICU4X does not.
- **Offsets:**
  - **Han:** each `[Unified_Ideograph]` range is one offset element,
    holding its first primary, its first code point and a step of 2.
  - **Other runs:** from U+0180, runs of code points whose long
    primaries rise by one step (2 to 127) under one lead byte become
    offsets too. The criterion is ICU's genuca's: the run crosses three
    of the trie's 64-code-point block boundaries, or one or two with 4
    code points on either side.
  - At the runtime, a code point's primary is the range's first primary
    moved on by its distance times the step. This is ICU's
    `incThreeBytePrimaryByOffset`, shared as `mlocAddToPrimary`. A code
    point before its range reads as unassigned.
- **Context lists:**
  - A contraction's list holds, under its first code point, the code
    points after it. A prefix's list holds, under the code point that
    follows it, the prefix read backwards.
  - Each list has a default (the code point's own CE32) and entries in
    increasing order.
  - A prefix or contraction CE32 holds the list's number, not its
    place in the words; the list starts are an array of their own.
  - A context is at most 8 code points. A prefix together with a
    contraction on one code point is refused at build, since the root
    has none.
- **Validation at open** (`mlocReadCollationRoot`):
  - the trie;
  - the arrays inside the payload;
  - the list starts rising from 0 to the end of the contexts, and each
    list filled exactly by its entries, with valid CE32s; each list is
    checked once;
  - every trie value and expansion word: tags known, low bytes 0xC0
    and their tag, expansions and offsets inside their arrays, a
    context naming a list that exists;
  - expansions hold elements only.
  A unit that reads can be looked up without further checks. Opening
  costs the unit's size. With lists named by their place, each of the
  trie's values would check its list again: a crafted unit with many
  code points on one long list would take quadratic time to open.

## Proof

- **`conform_collation_root`, in the cldr CI job:**
  - It builds the unit from CLDR 49's root and reads it back.
  - Every one of the 58,604 mappings the unit keeps (single code
    points, contractions, prefixes) gives back the reader's elements
    exactly.
- **`test_gencollation`:**
  - simple, long primary and long secondary CE32s;
  - a decomposable code point, a Hangul syllable and an unassigned one
    as implicit;
  - Han through an offset, with nothing before its range;
  - a contraction and a prefix with their defaults;
  - a sequence with one element too wide for a CE32, as 64-bit
    elements;
  - refusals: a header longer than the payload, a payload cut short,
    a trie value with an unknown tag, an expansion past its array, a
    context naming no list, a nonzero reserved word, an empty list,
    list starts short of the contexts, and a list its entries overfill
    or underfill.
- **Size, CLDR 49:**
  - The unit is 192,688 bytes: a 149,684-byte trie, 1,068 64-bit
    elements, 4,927 expansion words and 3,061 context words, with the
    list starts and the script table (record mloc-0069).
  - Before the offset runs it was 310,576 bytes.

## Consequences

- **Size is open:** the root is larger than ICU4X's (research 10 gives
  143 KB for its root with metadata).
  - The trie dominates. Its supplementary data blocks are 64 values,
    where ICU's are 16.
  - Smaller supplementary blocks, or overlapping blocks, are the next
    lever. They are measured before speed work (slice g).
- **The file:** `mlocgen generate` writes the unit into
  `src/generated/collation.mloc`, a data file of its own that a program
  adds to its data set, as kind `kindCollationRoot` (0x000B0001), a
  unit of no locale. Opening the file runs this validator. It was
  194,952 bytes with the root alone; with the locales' tailorings
  (record mloc-0071) it is 2,341,666.
- **The script table and numeric order's lead byte** joined the unit
  with the options (record mloc-0069).
- **Numeric order needs no digit CE32s:** the iterator tells decimal
  digits by Maul Unicode (record mloc-0069).
- **The compressible lead bytes** joined the unit with the sort keys
  (record mloc-0070): 32 bytes after the script starts, a bit a lead
  byte, FractionalUCA's COMPRESS top bytes and the unassigned byte FE,
  as ICU's `compressibleBytes`. The reader checks they fit.
- **U+FDD1's contractions** joined the unit with the tailorings (record
  mloc-0071). ICU's root data maps each script start's FDD1 line as a
  contraction of U+FDD1, and FDD1 FDD0 to the first unassigned primary;
  U+FDD1 alone stays unassigned. Text holding U+FDD1 now orders as
  ICU4C's, and the tailoring builder's resets on these markers (emoji's
  `&[before 1]\uFDD1€`) read the same root as the runtime.

## Amendment (a), 2026-10-22: the head's hash

The header grows from 32 to 40 bytes: a 64-bit hash of the unit at 32,
before the trie, which the validator checks and a collator's version is
made of (record mloc-0070 amendment (a)).

