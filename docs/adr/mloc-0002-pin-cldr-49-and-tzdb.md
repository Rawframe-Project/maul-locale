# mloc-0002. Pin CLDR 49 and the time zone database

Status: Accepted

## Context

Output changes between CLDR releases: CLDR 42 replaced the space
before AM and PM with U+202F, and parsers and tests that expected a
plain space broke in Java 20, ICU 72 and browsers. Maul Unicode pins
Unicode 18.0.0, and a text stack should answer from one Unicode
version. CLDR 48 is built on Unicode 17; CLDR 49 is the first release
on Unicode 18. Time zone rules change several times a year,
independently of CLDR.

## Decision

Every table comes from one CLDR release, 49, and its version is
queryable (`mlocGetCldrVersion`). Time zone rules come from the IANA
time zone database release named by a separate pin (2026e now), moved
with each of its releases; its version is queryable too. Until CLDR 49
is final, design and tests read its last beta; no table is released
from a beta.

## Consequences

A program states exactly which data formats its text, and a new zone
rule ships without waiting for a CLDR release.
