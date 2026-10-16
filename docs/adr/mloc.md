# Maul Locale records

The design records that apply to Maul Locale only. The family records
are listed in [README.md](README.md).

| Number | Title | Status |
|---|---|---|
| [mloc-0001](mloc-0001-library-profile.md) | Library profile | Accepted |
| [mloc-0002](mloc-0002-pin-cldr-49-and-tzdb.md) | Pin CLDR 49 and the time zone database | Accepted |
| [mloc-0003](mloc-0003-scope-and-parts.md) | The whole scope, in eighteen parts | Accepted |
| [mloc-0004](mloc-0004-speed-bar.md) | No part slower than the fastest implementation | Accepted |
| [mloc-0005](mloc-0005-one-data-format.md) | One data format, rules as code, exact data versions | Accepted |
| [mloc-0006](mloc-0006-generator-from-cldr-xml.md) | A generator from CLDR's XML with its own compilers | Accepted |
| [mloc-0007](mloc-0007-api-shape.md) | The shape of the API | Accepted |
| [mloc-0008](mloc-0008-dates-as-values.md) | Dates and times as values with arithmetic | Accepted |
| [mloc-0009](mloc-0009-grammatical-functions.md) | Grammatical functions for messages | Accepted |
| [mloc-0010](mloc-0010-catalog-formats.md) | Catalogs: a source format, a compiled form, exchange files | Accepted |
| [mloc-0011](mloc-0011-word-breaking-data.md) | Word breaking: dictionaries by default, models by choice | Accepted |
| [mloc-0012](mloc-0012-system-languages.md) | The system's languages come from Maul Window | Accepted |
| [mloc-0013](mloc-0013-proof-per-part.md) | What proves each part | Accepted |
| [mloc-0014](mloc-0014-data-file-layout.md) | The data file layout | Accepted |
| [mloc-0015](mloc-0015-locale-value-and-output.md) | The locale value, strict parsing and the output struct | Accepted |
| [mloc-0016](mloc-0016-locale-fallback.md) | Locale keys and fallback per component | Accepted |
| [mloc-0017](mloc-0017-language-matching.md) | Language matching as ICU's LocaleMatcher does it | Accepted |
| [mloc-0018](mloc-0018-regional-preferences.md) | Regional preferences from one region, with the keys on top | Accepted |
| [mloc-0019](mloc-0019-decimal-value.md) | The decimal value and the plural operands | Accepted |
| [mloc-0020](mloc-0020-plural-rules.md) | Plural rules compiled to C, chosen per locale by data | Accepted |
| [mloc-0021](mloc-0021-number-formatting.md) | Number formatting from distinct tables, without default rounding | Accepted |
| [mloc-0022](mloc-0022-currency-formatting.md) | Currency formatting: symbols field by field, spacing as ICU | Accepted |
| [mloc-0023](mloc-0023-unit-conversion.md) | Units: identifiers parsed, conversion exact and rounded once, preferences | Accepted |
| [mloc-0024](mloc-0024-unit-formatting.md) | Unit formatting: names in a data file of their own, read as ICU reads them | Accepted |
| [mloc-0025](mloc-0025-compound-unit-names.md) | Compound unit names: built as ICU builds them, from the data ICU reads | Accepted |
| [mloc-0026](mloc-0026-list-patterns.md) | List patterns: compiled in by style, joined as ICU joins them | Accepted |
| [mloc-0027](mloc-0027-mixed-units.md) | Mixed units: split exactly, joined by the unit list, as ICU writes them | Accepted |
| [mloc-0028](mloc-0028-unit-usage.md) | The usage option: every output unit kept, the value choosing, as ICU routes it | Accepted |
| [mloc-0029](mloc-0029-numeric-and-coordinates.md) | Numeric durations and coordinates: CLDR's patterns, written as ICU writes them | Accepted |
| [mloc-0030](mloc-0030-grammatical-cases.md) | Grammatical cases: ICU's reading of CLDR's production data, kept in the names file | Accepted |
| [mloc-0031](mloc-0031-unit-genders.md) | Unit genders: ICU's noun class, derived as ICU derives it | Accepted |
| [mloc-0032](mloc-0032-number-parsing.md) | Number parsing: ICU's matchers, read once without allocation | Accepted |
| [mloc-0033](mloc-0033-currency-parsing.md) | Currency parsing: ICU's currency matcher, the chain's names without allocation | Accepted |
| [mloc-0034](mloc-0034-spellout.md) | Spell-out: ICU's rules compiled, run on the exact decimal | Accepted |
| [mloc-0035](mloc-0035-list-formatter.md) | The list formatter: patterns kept in the formatter, items joined in place | Accepted |
| [mloc-0036](mloc-0036-names-data.md) | The names data: CLDR's display names by code, filed per key in a file of their own | Accepted |
| [mloc-0037](mloc-0037-single-code-names.md) | Single-code display names: ICU4C's lookups, BCP 47's codes | Accepted |
| [mloc-0038](mloc-0038-locale-names.md) | Whole-locale names: UTS #35's algorithm as CLDR's tools run it | Accepted |
| [mloc-0039](mloc-0039-subdivision-names.md) | Subdivision names: a small file of their own, read through the names lookup | Accepted |
| [mloc-0040](mloc-0040-calendars.md) | Calendars: Temporal's values, computed in code, astronomical years as tables | Accepted |
| [mloc-0041](mloc-0041-time-zones.md) | Time zones: the library's own tzdb compiler, Temporal's exact time | Accepted |
| [mloc-0042](mloc-0042-datetime.md) | Dates and times formatted: semantic skeletons, patterns compiled by the generator | Accepted |
| [mloc-0043](mloc-0043-relative-time.md) | Relative time: CLDR's fields filed per key, a decimal and a unit in | Accepted |
| [mloc-0044](mloc-0044-durations.md) | Durations: ECMA-402's algorithm on Temporal's fields, exact | Accepted |
