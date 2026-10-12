# mloc-0006. A generator from CLDR's XML with its own compilers

Status: Accepted

## Context

ICU4X reads CLDR's JSON for most data and takes its collation tables,
normalization and dictionaries from data ICU's own tools export.
CLDR's XML is the source both derive from; its JSON omits collation.

## Decision

The generator, written in C, reads CLDR's XML release and depends on
no ICU data. It resolves CLDR's inheritance itself (parent locales per
component, aliases, the explicit inheritance marker, lateral
inheritance of counts, cases, genders and alternates, draft and
coverage filtering), builds the root collation from CLDR's
`FractionalUCA.txt`, and compiles every tailoring's rules with its own
compiler. Dictionaries, models and time zone rules are read from their
own sources.

ICU and CLDR's JSON are test oracles only: resolved values are compared
with the JSON of the same release, and collation order with ICU4C's.

## Consequences

The library has one upstream for its locale data, and the collation
compiler, the hardest piece of the generator, is ours to prove.
