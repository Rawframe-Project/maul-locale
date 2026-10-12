# mloc-0003. The whole scope, in eighteen parts

Status: Accepted

## Context

Libraries that grow by stages accumulate the compromises of their
first stage. The scope of a complete client library is known:
ECMA-402's services, MessageFormat 2, and what ICU adds that clients
use. ICU4C's parts cannot be left out of a binary because they are
interconnected.

## Decision

The library covers, before its first release: locale identifiers,
fallback and matching; plural rules; numbers, currencies, units,
ranges, spell-out and parsing of numbers; calendars, time zones, date
arithmetic and formatting, intervals, relative time and durations;
lists; display names; person names; collation; dictionary word
breaking; transliteration; MessageFormat 2 with grammatical functions;
and translation catalogs. It leaves out charset conversion, regular
expressions, IDNA and spoof checking.

It is divided into eighteen parts, each with its own sources, build
switch and data: `data`, `locale`, `decimal`, `plural`, `number`,
`spellout`, `calendar`, `zone`, `datetime`, `list`, `names`,
`collate`, `segment`, `transform`, `person`, `message`, `grammar`,
`catalog`. `tools/modules.txt` holds their dependencies. Maul Unicode
supplies normalization, case, properties, UTF-8 validation and bidi
classes; none of its tables is duplicated.

## Consequences

A program links and ships only the parts and locales it uses; the size
of a part is a reason to leave it out of a program, never out of the
library.
