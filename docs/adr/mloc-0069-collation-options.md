# mloc-0069. Collation options: shifted, maxVariable, the quaternary level, the script table

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

## Proof

- **`conform_collation`, in the cldr CI job, at the quaternary strength
  on CLDR 49:**
  - The NON_IGNORABLE files, short and full, non-ignorable: 210,186
    lines each, none out of order.
  - The SHIFTED files, short and full, shifted: 210,186 lines each, none
    out of order, 27,966 equal to the line above.
  - Each file in the other mode puts thousands out of order (5,394 and
    3,287 for the short files), so each mode's work is seen.
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

## Consequences

- Still to come in slice (b): the identical level, backwards
  secondaries, case first and the case level, numeric order (its digit
  CE32s in the unit), reordering, and the public collator from a locale
  and its `-u-` keys. Each amends this record.
