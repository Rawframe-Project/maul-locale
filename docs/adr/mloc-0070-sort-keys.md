# mloc-0070. Sort keys: ICU4C's bytes, written level by level

Status: Accepted

## Context

Slice (c) gives collation its sort keys: bytes a program stores and
compares with `memcmp` in place of comparing the texts, as a database
index or a sorted file does. ICU4C writes them in
`CollationKeys::writeSortKeyUpToQuaternary` and
`RuleBasedCollator::writeIdenticalLevel`. Its keys are the de facto
format: a program that stores keys from one library and compares them
with another's expects the same bytes. ICU buffers the secondary, case,
tertiary and quaternary levels while it reads the elements once, then
appends them after the primaries.

## Decision

- **ICU4C's bytes,** for every text and option:
  - The primaries, a compressible lead byte written once for a run:
    03 or FF before a new lead byte, as the run went down or up.
  - Each further level after a 01 byte: the secondaries, the case
    level, the tertiaries and the quaternaries.
  - In each level, a run of common weights is written as one byte
    whose distance from the low or the high end counts it, with
    CollationKeys' constants for each level and mode.
  - Backwards secondaries reversed within each segment (U+FFFE ends
    one, written as 02).
  - The case level in nibbles, two a byte.
  - The upper-first tertiaries as ICU transforms them.
  - Shifted variables' primaries in the quaternaries, 1B before a lead
    byte from 1B up.
  - The identical level as BOCSU over the NFD code points, U+FFFE as
    02 with the base reset.
  - A final 00. The identical strength writes the quaternary level too,
    empty unless shifted, as ICU does.
- **Nothing is buffered:** each level reads the text again through its
  own iterator and writes into the caller's bytes. A backwards segment
  is reversed where it was written. When the caller's room is too
  small, the length is still counted and returned, and nothing is
  written past the room; the bytes written then are not a key.
- **The compressible lead bytes** are in the root unit (record
  mloc-0065, 32 bytes): the key writer cannot derive them.
- **Han ideographs keep CLDR's order, not ICU4C's default.** CLDR's
  root and its conformance files order unified ideographs by code point
  within the implicit ranges. ICU4C's default root data (`unihan`)
  orders them by radical and stroke, from FractionalUCA's `[radical]`
  lines: U+4E01 is FA 02 06 there, FA 02 04 here, with U+2A6D9 between
  U+4E00 and U+4E01. ICU4X's default is CLDR's order too. Keys holding
  Han therefore differ from ICU4C's in those primaries; the `unihan`
  types order Han by radical and stroke (record mloc-0071).

- **The public API** (`collate.h`):
  - `mlocGetSortKey(collator, text, length, key, capacity, lengthOut)`
    writes the key and its length, or gives `mloc_errorCapacity` with
    the length needed.
  - `mlocGetCollationVersion(collator, versionOut)` gives a 64-bit
    version: XXH64 (the catalog's, record mloc-0054) of the root unit's
    bytes, seeded with the key format (1, raised whenever the writer's
    bytes change). It is computed when the collator is made. Any change
    to the data the collator reads changes it, as does a new key
    format; the options do not, as in ICU, since the caller keeps them
    with the keys. A locale's tailoring joins the hash: XXH64 of its
    bytes seeded with the root's version (record mloc-0071).
  - A collator is read only after its stamp, a first field read alone,
    so bytes the library did not make are refused without reading
    their other fields.

## Proof

- **Byte for byte against ICU4C 79 on CLDR 49**
  (`conform_collation_options --keys` against
  `icu4c_collation_options.cpp --keys` in the docs repository): the
  first texts of 200,000 random pairs without Han (179,686), under all
  19 option sets (every strength, shifted, backwards, the case level,
  lower and upper first, numeric, five reorderings), every key equal.
- **Against the compare** (`conform_collation`, in the cldr CI job): on
  CLDR 49's four conformance files, in their modes, at the quaternary
  and identical strengths, each line's key orders it against the line
  above as the compare does, ties included; none out of order.
- **`test_collation_key`:** keys ICU4C 79 writes for chosen texts at
  each level and mode, and the caller's room (no room, too little,
  nothing written past it).
- **`test_collate`:** the public keys order texts as `mlocCompare` does
  under each key and level; the length when a key does not fit and
  nothing written past the room; an empty text's key; the version the
  same for any locale and options over the same data and for a copy;
  the refusals.
- **`fuzz_collation`:** for any settings and two texts, the keys order
  them as the compare does, and a short buffer gives the same length.
  Five minutes, about 108,000 inputs, found nothing.

## Consequences

- Slice (c) is complete; search, UTF-16 and index characters come in
  slice (f).
- A key's length grows with the text and the strength; the caller
  sizes the buffer from the returned length.
