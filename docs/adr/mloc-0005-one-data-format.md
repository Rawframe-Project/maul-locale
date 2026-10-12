# mloc-0005. One data format, rules as code, exact data versions

Status: Accepted

## Context

A program needs some locales compiled in and others downloaded later.
A plural rule compiled to C selects a category in about 4 ns; the same
rule interpreted from bytecode took 47 to 49 ns, and ICU4X's
interpreter 36 ns. CLDR 49 has 39 distinct cardinal and 28 ordinal
rule sets in all.

## Decision

The generator writes one data format. Compiled-in data is a data
file's bytes as a static array, validated when generated; a loaded
file is validated once on open and then read in place. One code path
reads both.

Algorithms over a closed set (the plural rule sets, the calendars, the
engines for collation, transforms, spell-out and messages, the
grammatical functions) are compiled into the library, and locale data
refers to them by index. A data file carries its data version (CLDR,
time zone database, Unicode, generator and code tables), and the
library opens only files of its own data version.

## Consequences

Language packs are regenerated with each release, and a pack can never
change output silently under a library it was not made for.
