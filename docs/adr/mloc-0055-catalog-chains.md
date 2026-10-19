# mloc-0055. Catalog chains: messages bound once, every failure stated

Status: Accepted

## Context

A program shows a locale's messages through its fallback locales: a
`fr-CA` translation, then `fr`, then the source `en`. Fallback is per
message, since a translation is often partial. The catalogs of one set
share key indexes (record mloc-0054), so one index serves every step.

A compiled message is formatted only after it is bound to a formatter
of its locale (record mloc-0045). Binding finds functions and makes
number and date formatters once, so it is done once per message, not
per call. Its size varies. It was measured from 119 bytes for plain
text to 2,119 bytes for a message with a currency and a date, and
nothing bounds it.

A program must never show an empty string or crash for a missing
translation, so every failure needs a defined text.

## Decision

- **A chain** (`mlocInitCatalogChain`) is one to eight catalogs of one
  set in fallback order, each with its locale's formatter, and a cache
  of the caller's memory. Catalogs of different sets are refused.
- **The cache** holds:
  - a head (the bytes used);
  - an eight-byte slot per key index of the set: which position's
    message is bound and why, or the reason the key stands;
  - an arena the bound messages are written into.

  Nothing is allocated (record mloc-0014).
- **Binding is lazy:** the first format of a key binds its message.
  - `mlocBindCatalogChain` binds them all now.
  - `mlocMeasureCatalogChain` gives the size that holds them all.

  A message is never bound on the stack, since bound messages have no
  size bound.
- **Threads:** a chain that still binds writes its cache, so it is used
  by one thread at a time. Once `mlocBindCatalogChain` succeeds, every
  slot is bound and format calls only read, so any thread may format.
- **The stated results** (`mlocCatalogResult`: a reason and the chain
  position written):
  - the first catalog's message (`mloc_catalogFound`);
  - a later catalog's when the earlier ones lack it
    (`mloc_catalogFallback`);
  - the source's when the chain's message does not bind
    (`mloc_catalogSource`);
  - the key when:
    - the set has no such key (`mloc_catalogMissingKey`);
    - no catalog of the chain has the message (`mloc_catalogAbsent`);
    - neither the message nor the source's binds
      (`mloc_catalogFailed`);
    - the message does not fit in the cache's room
      (`mloc_catalogCacheFull`).

  A key written is one fallback part.
- **A message with fallbacks** (`mloc_withFallbacks`) writes its own
  text and faults, as `mlocFormatMessage` does. It does not fall to
  another locale: the faults name a translator's mistake that text in
  the source's language would hide. Fluent's bundles do the same.
- **Strict mode's `mloc_errorFormat`**, `mloc_errorCapacity` and
  `mloc_errorInvalid` are returned as they are. The caller asked for
  the error, or must fix the call.
- **The calls:**
  - `mlocFormatCatalogMessage(chain, index, ...)` by an index resolved
    once.
  - `mlocFormatCatalogKey(chain, key, length, ...)` by key.
- **The container's locale ids:** the tool writes a catalog's locale in
  the container's form (`fr_CA`, as CLDR's files name locales). The
  container refuses a hyphen, so a regional catalog compiled before
  this would not open. `mlocOpenCatalog` now refuses a catalog whose
  locale is not a locale id, so its info never fails afterwards; the
  fuzzer found one.

## Proof

- `test_catalog_chain` compiles an `en` source, a partial `fr` and a
  one-message `fr-CA`, and formats through the chain `fr-CA`, `fr`,
  `en`, checking each reason:
  - a message found;
  - two fallbacks, one of them a plural;
  - a translation of 33 selectors that does not bind, falling to the
    source;
  - one that fails in the source too;
  - a fault kept with its fallback text;
  - a missing key.

  The cases are run:
  - bound lazily at an odd address, then again from the cache;
  - bound at once in the measured size;
  - in half that room (not all fits, the bound ones still serve);
  - with room for the slots alone (every message its key, "cache
    full");
  - without the source in the chain (absent and failed).

  Further checks:
  - a key too long for the buffer, measured, and reported as a part;
  - an index past the keys;
  - catalogs of different sets;
  - counts out of range;
  - null pointers.
- `fuzz_catalog` builds a chain of each catalog that opens. The chain
  binds in its measured cache, and every key formats with a stated
  result and a reason other than "cache full". The seeds now include
  the regional catalog and the one whose messages do not bind.

## Consequences

- The cache costs eight bytes per key plus the bound messages. A
  program that wants no key text for want of room measures it once.
- A chain is built per locale the program shows. Several chains may
  share catalogs and formatters, but not a cache.
