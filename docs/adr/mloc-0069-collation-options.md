# mloc-0069. Collation options: shifted, maxVariable, the quaternary and identical levels, backwards, the case level, case first, the script table

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
- **`conform_collation_options`** prints our order for the oracle's
  pairs, in its sets' order.

## Consequences

- Still to come in slice (b): numeric order (its digit CE32s in the
  unit), reordering, and the public collator from a locale and its
  `-u-` keys. Each amends this record.
