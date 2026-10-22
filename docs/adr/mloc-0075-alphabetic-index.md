# mloc-0075. Alphabetic index: ICU4C's AlphabeticIndex, its labels in the data

Status: Accepted

## Context

Slice (f) gives an alphabetic index beside search (L32; research 67):
the labels of a sorted list's sections (A, B, C... in English; あ, か,
さ... in Japanese) and the section each name falls into, under the
locale's collation. ICU4C's `AlphabeticIndex` builds the labels from
the locale's index exemplars, or synthesizes them from its standard
exemplars, then filters them through a primary-strength clone of the
collator and adds underflow, inflow and overflow buckets. ICU4X has no
index.

## Decision

- **ICU4C's algorithm,** in two parts: the locale's initial labels in
  the data (f3a), the filter and the buckets at run time over the
  collator (f3b).
- **The initial labels as ICU4C takes them from the locale**
  (`addIndexExemplars`): the index exemplars along the main chain, else
  the standard exemplars found the same way with a to z added when the
  set holds one of them or is empty, Hangul syllables cut to the 14
  initials, Ethiopic to the bases of its eights the set holds, and each
  item upper-cased by the language (Turkic rules for tr and az,
  Lithuanian for lt). Index exemplars are used as written.
- **Resolved by the generator, a unit per locale key** (kind
  `kindCollationIndex`) in the collation data file: the set's labels as
  UTF-8 in code point order, each with a byte of length after a 32-bit
  count (`src/collation_index.h`). A locale whose set matches that of
  the nearest locale with another key along its chain files nothing, as
  every per-locale part files; the run time takes the first unit along
  the locale's main chain. The synthesis and its case mapping happen
  once, in the generator.
- **CLDR's sets read strictly** (`tools/mlocgen/set_pattern.c`, which
  the parse sets now share): characters, `\uXXXX` and `\UXXXXXXXX`,
  other escaped characters, ranges and strings in braces. A property,
  a nested set, an operator or another escape stops the generator, and
  so does a synthesis needing Greek or Armenian case rules, which Maul
  Unicode lacks and no CLDR 49 locale needs (every el and hy locale has
  index exemplars).
- **CLDR's chain, not ICU4C's bundles:** a locale ICU4C does not ship
  falls back in ICU4C to the bundle it has (by truncation, or to root),
  so az-Arab gets az's Latin labels and en-Dsrt A to Z; the library has
  every CLDR locale and follows CLDR's parents (az-Arab's is root, its
  own Arabic set).
- **Read at run time, not stored:** the script boundaries (the root's
  U+FDD1 contractions whose sample is a letter or unassigned) and the zh
  types' labels (their U+FDD0 contractions, which ICU4C takes before the
  locale's set) are in the root and tailoring units already; (f3b) reads
  them there, as ICU4C does.

## Proof

- (f3a) `conform_index_labels` against ICU4C 79
  (`icu4c_index_labels.cpp`, `addIndexExemplars`'s own calls) for all
  1,148 locales of CLDR 49's main: all 907 whose own bundle ICU4C ships
  agree; of the 241 it does not, 42 agree anyway and 199 differ, each
  where ICU4C reads another locale's bundle. (The 21 zh and yue locales,
  whose collators have U+FDD0 contractions, take those labels instead
  in ICU4C, which (f3b) does too.) `test_collation_index` on the CLDR
  fixture: an index set as written (a string, an escape, a range), a
  standard set made into labels (ß to SS in full, the 14 Hangul
  initials, the Ethiopic bases, no a to z), an unconfirmed index set
  passed over, locales equal to their next keyed one filing nothing; the
  pattern reader's escapes, ranges and strings; the validator refusing
  nine crafted units. The generated data is otherwise unchanged (every
  other generated file byte for byte), so the parse sets read as before
  through the shared reader.

## Consequences

- The collation data file grows by 39.5 KB (2,428,978 to 2,468,477
  bytes).
- The generator reads common/main a second time for the labels (329 s
  to 368 s for a full generation).
- (f3b) builds the index value over a collator and these labels.
