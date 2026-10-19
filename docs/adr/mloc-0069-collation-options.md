# mloc-0069. Collation options and the public collator: shifted, maxVariable, the quaternary and identical levels, backwards, the case level, case first, numeric ordering, reordering, the script table

Status: Accepted

## Context

Slice (b) brings UCA's and CLDR's options to the general compare
(record mloc-0067), as ICU4C's `CollationCompare` and
`CollationSettings` apply them. This record holds them as they come, in
parts. The first part is the alternate setting, which needs the root's
group boundaries for maxVariable. Reordering needs the same table.

ICU builds that table, its `scriptStarts`, from FractionalUCA.txt's FDD1
lines (genuca.cpp and `CollationBaseDataBuilder::addScriptStart`):

- each line's sample code point names a script, or one of five groups
  (space, punctuation, symbol, currency, digit);
- each start is the top 16 bits of the line's primary, put back to its
  lead byte when it is that byte's lowest;
- scripts that share a range share a start (Hiragana and Katakana);
- the unassigned range's start is followed by the trailing range's,
  0xFF00.

`getLastPrimaryForGroup` is the next start, shifted up 16 bits, less one.
For punctuation, CLDR's default maxVariable, that is
`[variable top = 0B FF FF FF]` as FractionalUCA.txt states it.

## Decision

- **The script table is in the root unit** (record mloc-0065's layout,
  its header now 32 bytes). It holds the scripts and groups, each with
  its start's index, sorted by code for a binary search. It also holds
  ICU's starts and numeric order's lead byte (the FDD0 0034 line).
- **How the generator builds it:**
  - It reads the sample's script with Maul Unicode's `muniGetScript`.
    The five group samples and FDD0 are ICU's.
  - It adds ICU's aliases: Hrkt with Hira, Hans and Hant with Hani.
  - The two FDD0 reserved ranges either side of Latin are entries too,
    for reordering.
  - A sample of no script stops the build.
- **The validator checks:** starts rising from 0 to 0xFF00, scripts
  rising by code, and each script's start followed by another.
- **`CollationSettings`** holds the strength, primary to quaternary,
  shifted or not, and the variable top. `mlocCollationDefaults` gives
  the root's defaults: tertiary, non-ignorable, the end of punctuation.
- **Shifted, as ICU does it:**
  - **A variable element** is a primary above the merge separator, up
    to the variable top. It is passed over at the first three levels.
  - **The primary ignorables after a variable element,** until the next
    primary, are passed over at every level.
  - **At the quaternary level,** a variable element weighs its primary.
    Any other element weighs its tertiary's bits 7..6 above every
    primary. A text that ends first is the lesser.
  - With nothing buffered, each level applies the shifting again to the
    stream it reads.
- **Backwards secondaries** (French, `[backwards 2]`), as ICU's
  `CollationCompare`: the texts are cut into segments at the merge
  separator (U+FFFE, primary 02). In each pair of segments the
  secondaries are compared from the end. The longer segment's extra
  secondaries at its start are passed over, and the last differing pair
  decides; if none differs, the shorter segment is the lesser. Each
  segment is counted, then read again from a copy of the iterators, so
  nothing is buffered.
- **The case level** comes between the secondary and tertiary levels.
  Its weight is the tertiary's case bits (7..6), inverted for upper
  first. At the primary strength only elements with a primary weigh;
  otherwise any element with a tertiary does.
- **Case first** keeps the case bits in the tertiary (mask 0xFF3F, as
  `CollationSettings::getTertiaryMask` gives it when the case level is
  off). For upper first, a tertiary above 0x100 has its case bits
  inverted when the element has a primary, or 0x4000 added when not, as
  ICU does.
- **Numeric ordering** (CLDR's `kn`), as ICU4C's `appendNumericCEs`:
  - A decimal digit (General_Category Nd, by Maul Unicode's
    `muniGetDecimalDigitValue`) starts a run, unless its trie value is a
    prefix or contraction list. ICU marks the same code points with its
    digit tag; here the unit needs no digit values of its own.
  - Leading zeros are dropped, keeping one when all are zeros. The run
    is cut into segments of at most 254 digits, and each segment drops
    its own leading zeros.
  - A value below 74 + 40 × 254 + 16 × 254² (at most seven digits) is
    one element of one, two or three bytes after the numeric lead byte.
    Any other value is a length byte (128 plus the pairs), then the
    digit pairs (11 + 2 × pair, the last one less one), three bytes an
    element, with trailing zero pairs dropped.
  - A segment is buffered as digits, 254 bytes, and its elements
    (at most 43) in the iterator's element array.
- **Reordering** (CLDR's `kr`, `[reorder ...]`), as ICU4C's
  `CollationData::makeReorderRanges` and
  `CollationSettings::setReordering`, in `collation_reorder.c`:
  - The groups not named come first, in their order. Then the named
    codes, in order. Codes after Zzzz go to the end, the last named
    last. Then the remaining ranges in the root's order.
  - Latin first, with no group named, keeps Latin's bytes: the space
    before it is skipped, unless the order would then not fit.
  - The reserved ranges either side of Latin keep their place and are
    never named, as `getScriptIndex` does not reach them. A code the
    root has no range for is passed over.
  - Refused with `mloc_errorInvalid`: a duplicate, two codes of one
    range (Hira with Kana, Hani with Hans), and a second Zzzz. An order
    that does not fit the lead bytes is refused with
    `mloc_errorCapacity`.
  - `CollationReorder` holds a 256-byte lead byte table and the ranges
    from the first lead byte two ranges share. `CollationSettings`
    points at one, or null. Primaries are reordered at the primary
    level and as quaternary weights, as ICU's `CollationCompare`.
  - The validator now also holds the starts to at most 256, the
    tables' index.
  - **One deliberate difference from ICU.** ICU masks its front with
    0xFF00 while it places ranges. So an order that passes the last
    lead byte wraps to the first, and its overflow check misses it;
    ICU then takes the order, and unassigned code points and Han sort
    before Latin (ICU4C 79: U+0378 before "a"). Here the front keeps
    its high bits and an end below the first start is flagged, so such
    an order is refused with `mloc_errorCapacity`. An order beginning
    with Latin is first retried with Latin placed like any other
    script, as ICU does when its own check fails.
- **The unassigned range starts at FE00.** ICU's base data builder
  counts the unassigned implicit byte as compressible when it rounds a
  script start, so FE 04 (the first unassigned primary) is the byte's
  start. The generator rounded it as an uncompressible byte, to FE04,
  so the root split FE between Han and the unassigned range. Every
  reordering then spent a byte more than ICU's, and orders that ICU
  fits were refused. The script starts now equal ICU 79's on CLDR 49.
- **The public collator** (`include/maul-locale/collate.h`):
  - `mlocInitCollator(collatorOut, data, locale, options)` finds the
    root in the first source of the set that has it and makes a
    fixed-size value (768 bytes): the root's view, the settings and the
    reordering. A copy works: the settings point at the reordering only
    for the length of a compare.
  - The locale's keys set the options over the root's defaults: `ks`
    (level1 to level4, identic), `ka` (noignore, shifted), `kv`,
    `kf` (upper, lower, false), `kc`, `kn` and `kb` (true or false; a
    key alone is true, which canonical form leaves out) and `kr`. A
    value that is not one of a key's is passed over, as is a `kr` the
    root cannot order; ICU does the same with an unknown value.
  - `mlocCollatorOptions` overrides them; its zero value takes the
    locale's. Its reorder text is written as `kr`'s value, in any case
    (`others` for Zzzz); a text that is not a list of codes, or names
    one twice, is refused, and an empty text means none.
  - `kk` is not read: text is always compared in NFD (record mloc-0066),
    where ICU leaves text that is not FCD unreordered with kk off.
  - `co` waits for the tailorings (slice d), which also bring the
    locales' own defaults.
  - `mlocCompare` gives -1, 0 or 1; a byte that is not UTF-8 compares
    as U+FFFD.
  - The reordering keeps at most 64 ranges past the first shared lead
    byte. 40,000 random orders of up to 40 codes needed at most 24; an
    order needing more is refused with `mloc_errorCapacity`.
- **The identical level** compares the two texts' NFD code points.
  U+FFFE ranks below every other code point and the end of a text below
  that, as ICU's `compareNFDIter` does, so segments compare first.

## Proof

- **`conform_collation`, in the cldr CI job, at the quaternary strength
  on CLDR 49:**
  - The NON_IGNORABLE files, short and full, non-ignorable: 210,186
    lines each, none out of order.
  - The SHIFTED files, short and full, shifted: 210,186 lines each, none
    out of order, 27,966 equal to the line above.
  - Each file in the other mode puts thousands out of order (5,394 and
    3,287 for the short files), so each mode's work is seen.
- **`conform_collation --identical`, in the cldr CI job:** the four
  files in their modes at the identical strength: none out of order,
  4,357 equal to the line above (texts with the same NFD).
- **The options against ICU4C 74's sort keys**
  (`docs/locale/research/oracle/collation` in the docs repository):
  200,000 random pairs of Latin letters, accents, punctuation, U+00AD
  and U+FFFE, under 11 sets of options: every strength, shifted,
  backwards, the case level, lower and upper first. The sets agree on
  every pair, and on a second seed. ICU's collator has normalization
  on there: the root's default leaves text that is not FCD as it is,
  where UCA orders the NFD. The order is the keys': ICU 74's
  `ucol_strcollUTF8` disagrees with them when a common prefix ends in a
  shifted variable and U+00AD and a mark follows.
- **`test_gencollation`:**
  - a small root's scripts, starts and numeric lead byte;
  - seven refusals: a start not from 0, a last start not 0xFF00,
    starts not rising, a script naming start 0 or the last, scripts not
    rising, one script past the arrays.
- **`test_collation_data`,** on the generated root:
  - punctuation ends at 0B FF FF FF;
  - the groups in order;
  - Hrkt with Hira and Hans with Hani;
  - numeric order's lead byte 10.
- **`test_collation_compare`:**
  - the defaults;
  - punctuation and spaces ignored until the quaternary level, and
    ordered there;
  - an accent after a variable element ignored;
  - a currency sign not variable under punct.
  - backwards: cote < côte < coté < côté, and the segments U+FFFE parts
    compared one by one;
  - the case level at the primary strength;
  - lower and upper first;
  - the identical level, U+FFFE ranked below U+0001.
- **Numeric ordering against ICU4C 74,** the same differential with
  three numeric sets more (14 in all): the pairs also hold runs of
  digits (zeros, long numbers, 8,708 pairs past a 254-digit segment;
  ASCII, Arabic-Indic, Devanagari and fullwidth digits mixed). All 14
  sets agree on 200,000 pairs, on two seeds. Numeric order changes
  20,253 of the tertiary set's answers, so it is seen.
- **Reordering against ICU4C 74,** the differential with five
  reorder sets more (19 in all): Grek; Latn and Hani; digits and
  punctuation, numeric; Cyrl, Zzzz and Latn; and space, Hani, Zzzz and
  digits, shifted at the quaternary strength. The pairs add Greek,
  Cyrillic, Han and kana letters, symbols and currency signs. All 19
  sets agree on 200,000 pairs, on two seeds. Each reorder set changes
  between 1,438 and 57,078 answers of the same set without it.
- **Reordering against ICU4C 79 on CLDR 49**
  (`icu4c_collation_reorder.cpp` and `reorder_lists.py` in the docs
  repository, `conform_collation_reorder` here): random lists of one to
  forty codes, with Zzzz, Latin first, duplicates and codes of one
  range, each checked by its result and by the primary of a code point
  from each of the root's 165 sampled ranges. On two seeds of 20,000
  lists, the ICU arithmetic (with the 0xFF00 mask) gives ICU's result
  and order for every list. With the fix above, the only differences
  are ICU's wrapped orders: 1,954 of seed 9's lists, every one of them
  wrapped in ICU's arithmetic. One code point's primary differs between
  the two roots and is left out: U+2E80, a Han radical CLDR maps to
  U+4E36's implicit primary (FA 02 6E here, FA 05 3E in ICU 79), an
  open point for the tailoring slices.
- **`test_collation_compare`, reordering:** named scripts first; the
  groups not named before them; Zzzz sending the rest to the end; the
  refusals; Zzzz alone, no codes, and codes without a range changing
  nothing; an order past the lead bytes and one ICU wraps refused, one
  that fits keeping unassigned code points and Han after Latin.
- **`test_collation_data`:** the unassigned range starts at FE00.
- **`test_collate`:** the root's defaults; each key, a key alone, values
  passed over; options over the keys; reorder texts refused; a
  reordering too large; data without the root; null pointers and a
  collator the library did not make.
- **`test_collation_compare`, numeric:** values across one, two and
  three bytes and the pairs; leading zeros; digits of other scripts in
  one run; runs past a segment.
- **`fuzz_collation`,** in the fuzz job: any settings, up to three
  reorder codes, and two texts. Each text equals itself, and each order
  mirrors the other. Five minutes, about 465,000 inputs, found nothing;
  with reordering, five minutes more, about 212,000.
- **`conform_collation_options`** prints our order for the oracle's
  pairs, in its sets' order.

## Consequences

- Slice (b) is complete. `co`, the locales' own defaults and their
  rules come with the tailorings in slice (d); sort keys and the
  version in slice (c).
