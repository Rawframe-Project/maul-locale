# mloc-0072. The fast Latin table: ICU4C's, built for each unit over NFD

Status: Accepted

## Context

Most comparisons are of Latin text, and the general compare reads each
text through the iterator: a trie lookup, a CE32 decoded and a 64-bit
element a character. ICU4C compares such text through a small table
first (`CollationFastLatin`, format version 2): 16-bit "mini CEs" for
U+0000 to U+017F and U+2000 to U+203F, primaries packed into short and
long ranges, secondaries, case and tertiaries in the low bits, and
contraction lists for the characters that start one. The fast compare
bails out to the general one on any character the table cannot hold.
ICU4C builds the table with each tailoring
(`CollationFastLatinBuilder`). The library reads NFD (record
mloc-0066), while ICU reads raw text through its canonical closure, so
the table cannot be ICU's data copied.

## Decision

- **ICU4C's table and format,** version 2, built by the generator
  (`tools/mlocgen/collation_fast_table.c`) as ICU's builder builds it:
  the groups' top mini primaries, the unique elements sorted and
  encoded to mini CEs, one or two a character, expansions and
  contraction lists after the 448 character words, the retry with
  digits as long primaries when the letters overflow the short ones,
  and no table when they still do (en_US_POSIX, as in ICU).
- **The elements are the builder's lookup over NFD,** what the general
  compare gives the character's NFD: é is e and U+0301, which the
  table merges into one mini CE as ICU's closure gives é its own. A
  letter that is its own NFD and starts a contraction reads as a list
  (its default, then an entry for each fast character whose pair reads
  otherwise than the two apart, a bail-out entry where a longer
  mapping goes on), as in ICU. A precomposed letter after which
  nothing reads otherwise is the letter alone; one after which
  something does, a character outside the table included (amendment
  (c)), is a list too. A character after a prefix bails out,
  and so no entry names one: a longer prefix before it may change what
  it reads as.
- **A section after each unit's arrays:** a 32-bit count, then that
  many words, the unit ending with them. The root's unit and every
  tailoring's have one; a unit that ends after its arrays, or a count
  of 0, has no table (the general compare runs); a tailoring's count
  1 says that its table is the root's (most are, as ICU shares the
  base's). The search types have none, as ICU4C's data builder leaves
  them none: a search collator matches, it does not compare.
- **The validator** reads the head word, rising group tops and every
  character's expansion and contraction list inside the table, each
  list ending where the compare stops; the section must end the unit.
  No character's, expansion's or contraction result's mini CE may be
  the end of string mark (2): the compare would end that text early at
  the first level and its later levels, which trust the first to have
  checked every character, would read unchecked bytes (found by a
  review of the compare; ICU's builder never writes it).
- About 1.1 KB a table: 122 tables of their own and the root's, some
  140 KB of the collation data file.

## Proof

- `test_collation_fast_latin` on the data file: the root's table as
  long as ICU4C 79's with its head words, a, b, U+0000 and à as ICU
  encodes them, every type's table read (its own, the root's, or none
  for the search types and en_US_POSIX), and the reader refusing a
  wrong head, falling group tops, a contraction or expansion past the
  table, a list with no end, the end of string mark as a mini CE, a
  count other than the words there, a table short of its characters,
  and the root's table named by the root.
- `conform_collation_fast_builder` (the cldr CI job) builds the root's
  and every type's table, writes each after its unit and reads it back;
  CI checks their hash.
- In the docs repository's oracle, ICU4C 79 writes its table for each
  tag of `conform_collation_fast`'s output, and `fast_latin_compare.py`
  compares them by what the compare reads (a list holding only its
  default read as the plain character):
  - The root's table is ICU's byte for byte (480 words).
  - 142 of the 143 tags ICU's data can check read the same, the search
    types and en_US_POSIX without a table in both. 11 it cannot: it has
    no collation for aa, bal, cu, dz, sgs, ssy, vo and wae, no
    `digits-after` for cs and no `eor` for de.
  - vi's `trad` differs in four lists: ICU bails out on g and G before
    i and I, and on q and Q before u and U, where its closure has
    longer contractions (gì, qù); the library's NFD has none, and the
    mark after them bails out the compare anyway.

## Consequences

- To come: the identical prefix skipped before either compare, from a
  set of the characters a compare may not start at (ICU4C's unsafe
  backward set: every non-initial character of a contraction, and of
  four-character ones too, such as Hungarian's ddzs, which the table
  alone does not show), built by the generator for both compares
  (sub-slice (g)); the differential fuzz of the two compares (e3).
- Collation versions change: the root's unit holds the table.

## Amendment (a), 2026-10-20: the fast compare

- **`mlocCompare` tries the fast compare first,** ICU4C's
  `compareUTF8` over the collator's table (its tailoring's, or the
  root's when the tailoring names it or maps nothing), and runs the
  general one when it bails out: a character outside the table, a
  contraction whose list says so, a digit under numeric ordering, or
  differing secondaries backwards.
- **The options** are ICU4C's `getOptions`, made once with the
  collator: the variable top as the head's group top when shifted
  (none for a top that ends no group), case bits and upper-first
  tertiaries from case first unless the case level is on, digits
  bailing out when a reordering moves them apart, and no fast compare
  at the identical strength or for a reordering that moves the groups
  before Latin.
- **No primaries array:** a character's primary comes from its word,
  the same two comparisons the array saved ICU (slice (g) measures
  it). The collator stays 1,024 bytes.
- **Each level is one reader over both texts,** as the general compare
  reads them, rather than ICU's copy of the loop for each side and
  level.

### Proof

- `test_collation_fast_compare`: 2,000 random pairs of fast characters
  (with a combining acute, Cyrillic a, a middle dot and U+FFFE among
  them, half the pairs differing in one byte) for each of 35 tags:
  tailorings with contractions (cs, da, de phonebook, es traditional,
  hu, vi traditional, sv, pl, lt, cy), French backwards, en_US_POSIX
  and a search type without a table, and every option: shifted to each
  group, each strength, case first, the case level, backwards,
  numeric, reorderings and identical. `mlocCompare` orders each pair as
  its sort keys do (the keys never read the table). Over the root's
  table it answers more than a quarter of the pairs itself, as the
  general compare does. A compare with its tertiary order reversed
  fails all three checks.
- `conform_collation`: CollationTest's lines, non-ignorable and shifted,
  through the fast compare where it answers (1,480 and 1,538 pairs),
  all as the general compare orders them.

## Amendment (b), 2026-10-20: the differential fuzz

- **`fuzz_collate_fast`** (run a minute on every push, as every fuzz
  target): the first byte picks one of 32 tags (tailorings with
  contractions, a search type, en_US_POSIX, every option the fast
  compare reads, two reorderings, identical), the second cuts the rest
  into two texts, and each byte after them names a character: ASCII,
  letters and signs to U+015F, general punctuation, or one of ten the
  table bails out on. `mlocCompare` must order the texts as their sort
  keys do, the mirror the other way, and each text with itself as
  equal.
- **It found a divergence that ICU4C 79 has too:** under a reordering
  that puts digits after Latin, ICU's fast compare bails out on the
  ASCII digits alone, while U+00B2, U+00B3 and U+00B9 share their
  digits' primaries. ICU's `compareUTF8` orders U+00B3 before Z there,
  its sort keys after. The fast compare now bails out on every mini
  primary from 0's to 9's when digits are reordered, and follows the
  keys. A test pins the case.
- **And a second, in CI's first minute:** at the quaternary level of
  shifted text, ICU's fast compare keeps U+FFFE's merge weight (3) as
  its weight, the lowest, while the general compare and the sort keys
  rank the merge separator above every variable, as any element that
  is not variable. ICU 79 orders " ‒\uFFFE" after "\u00A0\uFFFE‒"
  through `compareUTF8` and before it by its keys (its identical
  prefix and lead byte checks hide most such pairs). The fast compare
  now gives U+FFFE the highest quaternary, and a test pins the pair.

## Amendment (c), 2026-10-22: a contraction read past a letter's marks

- **The fuzz target found a third divergence, the table's own:** under
  da, "Ą̈" (A with an ogonek, then a diaeresis) reads as ä, after z,
  because the contraction of a and a diaeresis reaches past the ogonek
  (combining class 202, below the diaeresis's 230). The table held Ą as
  the plain letter, so the compare decided by A against the other
  text's first letter before it read the diaeresis: "İ" before "Ą̈" by
  the keys, after by `mlocCompare`. ICU4C's closure gives such a
  letter a contraction whose list holds its default alone, so that its
  compare sees the next character and bails out on one outside the
  table (its builder's own comment: Danish's u and diaeresis).
- **The builder keeps a precomposed letter's list** when a contraction
  of its NFD's first code point, continued by that contraction's
  second code point outside the table, reads otherwise than the letter
  and the code point apart (the pair taken in NFD). 57 tables grow,
  1,630 words in all (3.2 KB of the data file).
- **The comparison with ICU4C's tables now tells a list holding its
  default alone from the plain character** (the earlier reading of the
  two as one hid this): every such list of ICU4C 79's is in the
  library's tables. The library keeps one more in 19 tables, where a
  contraction's mark sorts before the letter's own in NFD (Ć then a
  cedilla is c, cedilla, acute: ç in az), which ICU's closure leaves
  to the letter alone; there the compare bails out where ICU's
  decides. vi's `trad` keeps its four lists apart, as before.

### Proof

- `test_collation_fast_compare`: under da and fi, b before "Ą̈", v
  before "Ų̈" (y) and z before "ą̈", by the compare and the keys alike;
  each fails on the data built before.
- `fuzz_collate_fast`: CI's input passes, and 135,619 runs here found
  nothing.
- `conform_collation_fast_builder` (the cldr CI job) checks the tables'
  new hash, edb9afb552346149.

## Amendment (d), 2026-10-22: the identical prefix skipped

- **Both compares start after the texts' identical prefix** (research
  68, slice (g4)), as ICU4C's `compare` does: the bytes or units both
  begin with, backed up to a code point's start and, where a code point
  at the cut is unsafe-backward (a mark, a later code point of a
  contraction in the root's or the tailoring's unsafe ranges, a digit
  under numeric: `mlocCollationUnsafeBackward`, now shared with search),
  over the unsafe ones before it. The general compare gives its
  iterators the prefix's last eight NFD code points as history, so a
  prefix context before the cut still reads (the middle dot after l).
  Only texts that begin alike read the collator's general parts first.
- **No prefix is skipped when shifted or with backwards secondaries.**
  The fuzz target found both within minutes. Shifted, a variable
  element before the cut makes the ignorables after it read as 0: "["
  against "[" U+0005 U+0323 U+000C is equal by the keys, and ICU4C 79's
  compare, which skips the prefix there too, orders them apart (its
  keys agree with the library's). With backwards secondaries the
  prefix's are read last, and they decide where one text's others are
  the start of the other's (fr-CA). Texts of two encodings (the index's
  UTF-16 names against its UTF-8 boundaries) share no prefix either.
- On the bench, back to back against 4c08bc8 (two runs each, load 16 to
  32): the file names sharing 32 bytes 288 and 289 ns to 116 and 120,
  de 54 and 52 to 41 and 41, de UTF-16 56 and 54 to 46 and 47, zh 266
  and 287 to 190 and 198, ko 334 and 339 to 228 and 241; fr (backwards)
  and the index's bucket unchanged.

### Proof

- `test_collate`: a prefix context before the cut, a contraction (cs
  ch) and a number (a12a against a123) across it, and the two fuzz
  findings, each by compare, mirror and keys; leaving out the history,
  the unsafe backing up, the shifted rule or the backwards rule each
  fails a check.
- `fuzz_collate_fast` 536,870 runs and `fuzz_collation` 160,509 here
  after the fixes, nothing; the suite unchanged.
