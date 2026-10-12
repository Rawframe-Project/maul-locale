# mloc-0013. What proves each part

Status: Accepted

## Context

CLDR publishes conformance data for most of the library's scope:
likely subtags, canonical forms, display names, decimals, dates and
skeletons, units, spell-out, person names, transforms, the root
collation and MessageFormat 2. Where it does not, ICU4C and ICU4X give
independent answers.

## Decision

A part is done when it passes all of CLDR's conformance data for it,
its differential tests against ICU4C and ICU4X (oracles built for the
tests only, never linked), the sanitizers, fuzzing of every parser and
of the data file validator, a mutation sweep, its size budget and its
benchmark (mloc-0004). Parts are built in this order: generator core
and `data`; `locale`; `decimal` and `plural`; `number` and `spellout`;
`list` and `names`; `calendar`, `zone` and `datetime`; `message`,
`grammar` and `catalog`; `collate`; `segment`, `transform` and
`person`.

## Consequences

Each part rests on proved parts, and "done" has the same meaning
everywhere.
