# Documentation

## Using the library

- [Getting started](user/getting-started.md): requirements, building, installing, the model the API follows, results and output.
- [Data](user/data.md): the compiled-in data, the data files and which parts need them, versions, newer time zone rules.
- [Locales](user/locales.md): parsing and canonical forms, fallback, matching a user's languages, regional preferences, display names.
- [Numbers](user/numbers.md): decimals, plural rules, numbers, currencies, units, numbers in words, parsing.
- [Dates and times](user/dates-and-times.md): calendars, time zones, Temporal strings, formatting, relative time, durations.
- [Messages](user/messages.md): MessageFormat 2 and lists.
- [Grammar](user/grammar.md): case suffixes and particles on names and numbers.
- [Catalogs](user/catalogs.md): translation catalogs, the `mloccat` tool, chains.
- [Collation](user/collation.md): comparing, sort keys, search, alphabetic indexes.
- [API reference](https://rawframe-project.github.io/maul-locale/api.html), generated from the headers.

## How it works

- [Overview](internals/overview.md): the library profile, the parts and where data comes from.
- [Glossary](internals/glossary.md).
- [Data](internals/data.md), [Generator](internals/generator.md), [Locales](internals/locales.md), [Numbers](internals/numbers.md), [Dates and zones](internals/dates-and-zones.md), [Messages and catalogs](internals/messages-and-catalogs.md), [Grammar](internals/grammar.md), [Collation](internals/collation.md).
- [Conformance](internals/conformance.md): what proves each part, and the listed departures.
- [Performance](internals/performance.md): the speed bar and how speed is measured.
- [Maul conventions](internals/maul-conventions.md): the rules every Maul library follows.

## Maintaining it

- [Development](operations/development.md): toolchain, building and running tests, the rule checks, the data checks, fuzzing, benchmarks.
- [Updating data](operations/updating-data.md): the pins, regenerating `src/generated/`, new CLDR and tzdb releases, the grammar lexicons, external data.
- [Oracles](operations/oracles.md): the differential programs against ICU4C, ICU4X, Apertium and test262.
