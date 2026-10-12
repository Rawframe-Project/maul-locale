# mloc-0014. The data file layout

Status: Accepted

## Context

Record mloc-0005 fixes one data format for compiled-in and loaded data,
validated once and read in place. The layout must cost nothing to open
beyond that validation, allocate nothing, give the same bytes on every
platform, and refuse a file of another data version before reading it.

## Decision

A data file is little-endian and its first byte is 8-byte aligned. In
order:

1. **A 64-byte header:** the magic `MLOC`, the format version (1), the
   header size (64), the file size, the data version (the CLDR,
   Unicode and library versions, the time zone database release as
   eight ASCII bytes, and a 64-bit hash of the generated code tables
   and the pinned sources), the unit count, the locale count, and the
   offset and size of the string pool.
2. **The unit table:** 16 bytes per unit (kind, locale index or none,
   offset, size), sorted by kind and then locale, found by binary
   search.
3. **The locale table:** 8 bytes per locale (offset and length of its
   id in the pool), sorted by the id's bytes. Ids are CLDR's
   (`de_AT`, `sr_Latn`, `ca_ES_VALENCIA`), so variants need no packed
   form.
4. **The payloads:** each 8-byte aligned. A part's data for a locale is
   fully resolved by the generator, CLDR's inheritance applied, so a
   lookup finds one unit and never walks parents inside a file;
   identical payloads are stored once and shared by their units.
5. **The string pool,** UTF-8, ending the file.

A unit's kind names the part and the table, and each kind has a
validator in the part that reads it. Opening a file checks the header,
then the data version (`mloc_errorDataVersion` for another one, before
anything else is read), then every table, offset, length and order
(`mloc_errorData`), and runs the validator of every unit; an unknown
kind is refused. After that nothing is checked again.

`mlocDataSet` is a caller array of opened sources, searched in order.
A locale that no source has is the `locale` part's to replace through
its fallback, using the locale tables.

Compiled-in data is the same bytes as a static array, validated by the
generator and by the tests.

## Consequences

Every lookup is a binary search over a small table and a pointer into
caller memory. Resolved payloads cost more bytes than chains of
differences would; sharing identical payloads keeps that within the
size budgets of mloc-0013, as ICU4X's data shows for the same choice.
