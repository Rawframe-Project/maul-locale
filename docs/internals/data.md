# Data

The library reads its locale data in one format, whether compiled in or loaded by the program, validates it once and then reads it in place. The layout is defined in [`src/data_format.h`](../../src/data_format.h) and [`src/kinds.h`](../../src/kinds.h); opening and lookups are in [`src/data.c`](../../src/data.c).

## Why one format

A program needs some locales compiled in and others downloaded later. With one format, one code path reads both, and the compiled-in data (`src/generated/builtin_data.c`) is simply a data file's bytes as an 8-byte aligned array. The generator validates what it writes, the tests open the compiled-in data through the same validator, and nothing distinguishes the two at run time.

## Layout

A data file is little-endian and 8-byte aligned:

1. **Header, 64 bytes:** the magic `MLOC`, the format version, the header and file sizes, the data version, the unit and locale counts, and the string pool's place.
2. **Unit table:** 16 bytes per unit (kind, locale index or none, offset, size), sorted by kind then locale, found by binary search.
3. **Locale table:** 8 bytes per locale, the offset and length of its CLDR id (`de_AT`, `ca_ES_VALENCIA`) in the pool, sorted by the id's bytes.
4. **Payloads,** each 8-byte aligned. Identical payloads are stored once and shared by their units.
5. **String pool,** UTF-8, ending the file.

A part's data for a locale is filed under the locale's key ([locales.md](locales.md#keys-and-chains)) only where it differs from what the next key along the chain gives. A lookup walks the chain and, at each key, the sources of the set in order, so a downloaded file placed first answers for the locales it has and the compiled-in data for the rest. Most locales inherit most of their data, which keeps files small; it also means that which locale held the data says nothing about whether a locale is supported, which is language matching's question.

Large values that recur across locales (number symbol sets, compiled patterns, compact tables, currency pattern sets) are stored once in a unit of no locale, and per-locale units name them by index.

## Validation

`mlocOpenData` checks, in this order: the header and size; the data version, refused with `mloc_errorDataVersion` before anything else is read; every table, offset, length and order (`mloc_errorData`); then each unit through its kind's validator. An unknown kind is refused. After a file opens, no lookup checks anything again: a validated unit is safe to read with plain loads. A part that lays a unit out when a formatter or collator is made (collation's `mlocViewCollationRoot`, say) checks only that its sections fit; the checks of their contents run in the validator alone, which every way to an opened source passes through.

Two rules keep this sound:

- **A validator reads its own unit only.** Units are validated one at a time in table order, so a validator that looked up another unit could read one whose bounds are not checked yet, and a crafted file would then read out of bounds. A check that spans units (an index into a distinct set, say) is made when a formatter is created, against the source that holds the locale's unit, never in a validator.
- **Indexes are bounded where they are read.** A formatter stores only indexes that it bounded when it was made, so formatting needs no checks.

`fuzz_data` and the other `fuzz_*` targets feed crafted files to the validators; every crash they found became a test. `fuzz_collation_data` fuzzes what that trust rests on: it patches a collation unit, seals its hash again, and when the validator passes it, makes and uses the locale's collators.

## Versions

The data version is the CLDR and Unicode versions, the data format version, the tzdb release, and a 64-bit hash of the generated code tables and the pinned sources. The code tables are the parts of the data compiled into the library as C (plural rule functions, calendar year tables, script directions, list genders, segmentation class overrides); a data file refers to them by index, so a file is only meaningful with the library whose tables it was generated with. The hash makes a mismatch impossible to miss, and the library opens only files of its own data version. A zones file is the one exception: every field but the tzdb release must match.

The data format version (`MLOC_DATA_FORMAT_VERSION`) is not the library version: it changes only when the format does, so a release that keeps the format keeps every data file and catalog valid, and a release needs no new data. Compiled messages carry it too, since catalogs store them. Its bit 31 is set: the field once held a packed library version, which never sets it, so no file of 0.1.0 or before matches any format. These are format changes, which increase it and regenerate every file:

- **A layout:** a field of a kind added, moved, resized or given another meaning; a kind added, removed or renumbered; the header; a compiled message's layout. `tools/check_data_format.py` fails when a header that lays these out changes (`tools/data-format.txt` lists them), until the format is decided and the hashes recorded again with `--write`. Each such header carries the line `// A data layout: tools/check_data_format.py watches this header.`, and the check fails on a listed header without it and on a header of `src/` with it that is not listed.
- **What the reader takes from the same bytes:** a value, flag or string encoding read another way, even where the layout header is unchanged.
- **What a validator accepts, for files a generator of the format writes:** a check that refuses such a file, or one that admits a value or length the reader then relies on, which an older library would refuse. The hashes cannot see these. Tightening a validator against bytes no generator writes, as hardening against hostile input does, is not a format change.

A generator fix that writes other content in the same format is not one either: a file it writes opens with every library of the format, and so does the older file.

`mlocReadDataVersion` reads the version from the header alone, so a program can choose which file to fetch.

## Adding a kind

A new table is a new kind in `src/kinds.h`, a validator in the part that reads it (registered in `src/data_kinds.c`), the generator's writer, and a layout header beside the part (`src/<part>_tables.h`, which carries the data layout line and which `tools/data-format.txt` lists). A kind's number never changes meaning within a data version; a change of layout changes the code tables' hash and with it the data version. Whether the kind goes into the compiled-in data or a file of its own is decided by size and by how many programs need it: the large name and pattern tables go into files, so a program that shows no dates does not ship them.
