# mloc-0062. Catalog speed: lookup against gettext, formatting against a bound message

Status: Accepted

## Context

The catalog design set three bars for its speed:

- **Lookup by key** against GNU gettext's lookup: the hash-table lookup
  every C program knows.
- **Lookup by index** against an array read.
- **Formatting** a catalog message through a chain against the same
  message bound and formatted directly. The catalog should add only its
  lookup.

## Decision

- **Benchmark rows:** the benchmark gains catalog rows (`bench/bench_catalog.c`):
  - `catalog-find-key`: a key found, each key in turn;
  - `catalog-message`: a message got by index;
  - `catalog-open`: a catalog opened and validated;
  - `catalog-bind`: a chain made and bound;
  - `catalog-format-index`, `catalog-format-key`: a message formatted
    through the chain, by index and by key;
  - `catalog-format-bound`: the same message bound alone and formatted.
- **The benchmark set:** 2,000 keys in en and fr. A quarter each are
  plain text, a placeholder, a plural selection and two numbers. It is
  written by `bench/catalog_set.cmake`, and the catalog tool compiles
  it at build time. A build without the tools has no catalog rows.
- **The gettext baseline:** the research notes' program
  `gettext_catalog_bench.c` times GNU gettext on the same set:
  - the set is exported by `mloccat export-po` and compiled by
    `msgfmt`;
  - a message is looked up as `pgettext` does, by key and English text.
- **Counted, not only timed:** each row is counted in instructions, by
  `perf stat` less a run of nothing. The machine these numbers come
  from is shared and was loaded (load average 28 to 97), so times
  are given only as a guide.

## Proof

Run here, GCC 14 Release, glibc 2.39's gettext. A count includes the
loop's own work (the next key's index, the call, the sink), which both
programs share.

| Row | Instructions per call | ns (loaded) |
|---|---:|---:|
| catalog-find-key, every key | 257 | 21 |
| gettext, every key | 1,055 | 193 |
| gettext, one key again (its cache) | 464 | 134 |
| gettext, a key it lacks | 1,887 | 303 |
| catalog-message (by index) | 77 | 4.6 |
| catalog-format-bound | 843 | 54 to 56 |
| catalog-format-index | 912 | 57 to 64 |
| catalog-format-key | 1,207 | 132 |
| catalog-open (2,000 keys, validated) | 3.2 million | 262,000 |
| catalog-bind (fr and en, 4,000 messages) | 11 million | 1.2 million |

- **Against gettext:** a lookup by key takes a quarter of gettext's
  instructions over the set. It takes about half of gettext's when
  gettext answers one key from its cache.
- **By index:** a message is the handle's check and two offsets.
- **Formatting through a chain** adds 69 instructions (8%) to the bound
  message's call. By key it adds the lookup, 295 instructions.
- **Opening and binding are once per program:**
  - Opening validates every key and message, about 1,600 instructions
    a key.
  - Binding is about 2,700 instructions a message.
  - Both are done before the first format call, and a program can
    measure both.

## Consequences

- The bars hold, and the rows stay in the benchmark so a regression
  shows. A timed comparison on a quiet machine can replace the guide
  column without changing the decision.
- Opening is the largest cost: 0.26 ms for 2,000 keys on this loaded
  machine. A program with many catalogs opens only those of its chain.
