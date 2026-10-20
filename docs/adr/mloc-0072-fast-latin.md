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
  something does is a list too. A character after a prefix bails out,
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
