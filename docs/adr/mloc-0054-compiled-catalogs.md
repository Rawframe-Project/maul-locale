# mloc-0054. Compiled catalogs: a perfect hash and one key order per set

Status: Accepted

## Context

A program looks messages up by key, often every frame. A program may
also resolve a key once and keep its index. A partly translated locale
falls back message by message to another catalog. All of it must work
with no allocation, from a file read in place (records mloc-0005 and
mloc-0014). The keys are known when the catalog is built, so a minimal
perfect hash fits: a hash, a table read and one comparison, at about
half a byte a key.

## Decision

- **A catalog is a data file** of the library's container, holding one
  locale and two units of it (`src/catalog_tables.h`):
  - **Keys** (`kindCatalogKeys`): the count, buckets, places, seed and
    set id, then:
    - 16-bit pilots;
    - the remap of places past the count;
    - the key offsets;
    - the keys' NFC UTF-8.
  - **Messages** (`kindCatalogMessages`): the count, flags (the set's
    source), the set id, the offsets, and the compiled messages
    (`mlocCompileMessage`'s form). An absent message is an empty span.
- **The hash:**
  - XXH64, written from its specification. The reference
    implementation's values are a test.
  - The perfect hash is PTHash's design (Pibiri and Trani, SIGIR 2021):
    - Keys go into buckets of about four, by the hash's high half.
    - Buckets are placed largest first, each with the first 16-bit
      pilot whose places for its keys are all free. A place is the whole
      hash mixed with the pilot's mix, then reduced by multiplication.
    - The table is 2% larger than the keys. Places past the count are
      remapped to the free ones below it, so indexes are dense.
    - The seed changes when a bucket finds no pilot.
  - At a million keys the largest pilot was 8,219 and the build took
    1.5 s.
- **One key order per set:** the tool builds the table from the
  source's keys. Every catalog of the set stores the same keys, table
  and order, and a set id hashed from the keys in order. So a key's
  index is the same in every catalog of the set.
- **Opening validates once** (`mlocOpenData`, then `mlocOpenCatalog`):
  - offsets rising and in bounds;
  - remap entries below the count;
  - every stored key NUL-free and found at its own index;
  - every message readable as a compiled message;
  - the two units' counts and set ids equal, and the set id recomputed.

  After that a lookup needs no check but its one comparison.
- **The calls:**
  - `mlocOpenCatalog(catalog, data)`.
  - `mlocGetCatalogInfo`: set id, key count, message count, source
    flag, locale.
  - `mlocFindCatalogKey(catalog, key, length, index)`: an unknown key
    is not found, never mapped to another message.
  - `mlocGetCatalogMessage(catalog, index, message, size)`: not found
    for an absent translation, invalid past the count.

  The hash and table readers are inline in `src/catalog_hash.h` and
  `src/catalog_tables.h`, so the host tool shares them without the
  library's internals.
- **The tool:** `mloccat compile DIRECTORY SOURCE [TRANSLATION...]`:
  - It reads and checks each resource (mloc-0053). The check now also
    refuses a key that is not in NFC.
  - It leaves out obsolete entries.
  - It reports a translation's keys that the source lacks, and leaves
    them out.
  - A key marked `@translate no` keeps the source's message in every
    locale.
  - It writes `LOCALE.mloc` per resource, with the locale's canonical
    id, and reports each catalog's counts.

## Proof

- `test_catalog`:
  - XXH64 against the reference values at the lengths that take each
    of its paths.
  - 200,000 keys get distinct indexes below their count; no keys gives
    one bucket and no places.
  - A source and a translation compiled and opened: counts, the source
    flag, one set id, one index per key across the set, messages equal
    to their compiled text, `@translate no`, an untranslated key, an
    index past the count, keys the set lacks (a prefix, a longer key,
    the empty key, the orphan).
  - Every byte after the header changed in turn: most such catalogs are
    refused. The rest changed only a message's literal text or padding.
- `fuzz_catalog`, with the compiled catalogs as seeds: a catalog that
  opens finds every stored key at its own index, and gives every
  message from inside its own bytes. 16 million runs were clean.

## Consequences

- A catalog is compiled for one library version, as compiled messages
  are (L16), and recompiled with the library.
- Chains, binding and the stated results of failures come next
  (catalog slice (c)).
