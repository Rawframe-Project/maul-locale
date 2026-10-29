# Conformance

What proves each part, and where the library departs from its oracles on purpose. A part is complete when it passes all of CLDR's conformance data for it and agrees with ICU4C (and ICU4X where it covers the part) on the same inputs, every difference listed with its reason. The checks run in CI against the pinned releases ([development](../operations/development.md#data-checks)); the oracle programs that wrote the committed tables are in `tools/oracle/` ([oracles](../operations/oracles.md)).

## Contents

- [Evidence per part](#evidence-per-part)
- [Listed departures](#listed-departures)
- [Committed oracle data](#committed-oracle-data)

## Evidence per part

| Part | Evidence |
|---|---|
| CLDR resolution | Every path of every locale against CLDR's JSON of the same release (`tools/check_resolution.py`). |
| Locales | CLDR's `likelySubtags.txt`, `localeCanonicalization.txt`; CLDR's parent chains (`mlocgen chains`, `conform_parents`); ICU's `localeMatcherTest.txt` (`conform_match`); regional preferences against the JSON (`check_preferences.py`); directions against every locale's character order. |
| Display names | CLDR's `localeDisplayName.txt` (`conform_locale_names`); ICU4C's `LocaleDisplayNames` by oracle. |
| Plurals | Every sample of CLDR's plural and ordinal rules (`mlocgen plural-samples`, `conform_plurals`). |
| Numbers | CLDR's `testData/decimal` (`conform_numbers`); ICU4C's formatting of currencies, units, lists and compact amounts by oracle; ICU4C's parse on generated and random texts. |
| Units | CLDR's `unitsTest.txt` and unit preference tests (`check_units.py`, `check_unit_prefs.py`); currencies by region and date against the JSON (`check_currency.py`). |
| Spell-out | CLDR's `testData/rbnf` (`conform_spellout`); ICU4C over every public rule set of 119 locales. |
| Calendars | test262's Temporal calendar tests (`test_calendar_test262`); ICU4X's fields for each day from 1600 to 2400 (`conform_calendar_oracle`; `test_calendar_oracle` checks the Chinese and Dangi days). |
| Zones | zic and zdump for every zone from 1800 to 2500, for the pinned and an older release (`check_zones.py`); Python's `zoneinfo` for local times (`check_local.py`); CLDR's BCP 47 and Windows mappings (`check_zone_codes.py`). |
| Temporal arithmetic and strings | test262's Temporal tests as tables (the `*_test262` suites); `check_zoned.py` and `check_ixdtf.py` against second readings of ECMA-262. |
| Date formatting | CLDR's datetime test data (`check_datetime.py`); ICU4C's `DateTimePatternGenerator` and `DateIntervalFormat` (`check_skeletons.py`, `check_intervals.py`, the range suites). |
| Messages | The MessageFormat 2 test suite in CLDR's `testData/messageFormat` (`test_message`). |
| Grammar | The spelling guides' examples; Apertium's generators and Wiktionary's tables by oracle. |
| Collation | `allkeys_CLDR.txt` and `CollationTest_CLDR_*` (`conform_collation_root`, `conform_collation`); ICU4C 79's root elements, every tailoring's answers, fast Latin tables, sort keys, search and index labels (the `conform_collation_*`, `conform_search*` and `conform_index*` programs). |
| Segmentation | ICU4C 79.1's `BreakIterator` on every suppression of every locale, the class overrides, the line keywords and plain text (`test_segment` over `test/segment_icu.tsv`); the same breaks for text in pieces as whole. |
| Data files | Every validator under fuzzing (`fuzz_data` and the part targets). |

## Listed departures

Each list is read by its check, which fails on an unlisted difference and on a listed one that no longer differs, so the lists stay exact:

| File | What it lists |
|---|---|
| `tools/resolution-exceptions.txt` | Paths where CLDR's JSON restarts an alias from the parent and CLDR's own resolver, which the generator follows, does not. |
| `tools/number-exceptions.txt` | Lines of CLDR's decimal test data written from older data than the pinned release. |
| `tools/match-exceptions.txt` | Matcher test lines where UTS #35's legacy aliases canonicalize further than ICU. |
| `tools/locale-name-exceptions.txt` | Locale display name lines where CLDR's tools and the test file part. |
| `tools/skeleton-exceptions.txt`, `tools/interval-exceptions.txt` | Patterns where the generator follows CLDR's inheritance and ICU4C's data build does not. |
| `tools/segment-exceptions.txt` | Segmentation cases where ICU4C uses its own line rules or dictionary, not CLDR's segments files ([segmentation.md](../user/segmentation.md#differences-from-icu4c)). |

Departures without a file of their own, each stated where its part is documented:

- **Spell-out:** a plural block's operand is the exact quotient where ICU casts it to `int32_t`; values past `int64_t` are spelled out, where ICU writes digits; a locale without rules takes root's, not the process's default locale. Parsing searches a span of text once by each rule set and keeps its result at every depth, where ICU searches it again deeper and can stop at its recursion limit there: a long Lithuanian number's search, which ICU's would take minutes for, reads it back, and no text the formatters write meets the step bound.
- **Number parsing:** input is UTF-8, so a byte that does not begin a valid sequence is no character.
- **Currencies:** with the symbol hidden, the amount takes the locale's `alt="noCurrency"` currency pattern, which has no symbol and no space beside it (de `1.234,50`), as `mloc_currencyHidden` documents; ICU4C instead deletes `¤` from the standard pattern and keeps the no-break space that stood beside it. Cash rounding keeps the currency's cash digits, which default to its digits when CLDR gives none: DKK's 0.50 cash increment gives `1.234,50 kr.`, as Danish cash prices are written, where ICU4C writes only the digits of the increment, `1.234,5 kr.`.
- **Collation:** see [collation.md](collation.md#kept-differences).
- **Temporal arithmetic:** a zoned difference rounded or totaled across a jump of the zone's clocks by a whole day (`Pacific/Apia` skipping 30 December 2011, `America/Sitka` living 18 October 1867 twice) is refused, as temporal_rs refuses it. ECMA-262 asserts there what does not hold and gives no answer yet ([tc39/proposal-temporal#3310](https://github.com/tc39/proposal-temporal/issues/3310)).
- **Calendars:** `islamic` reads as Umm al-Qura, as in ICU4X, where ICU4C estimates it astronomically; Chinese years from the astronomical model where ICU4C's reduced model differs from the observatories.

## Committed oracle data

Tests that compare with an oracle the CI image does not have read tables generated from it and committed:

| File | Generated from |
|---|---|
| `test/test262_*.inc` | test262's Temporal tests, by `tools/oracle/test262/`. |
| `test/mf2_suite.inc` | CLDR's MessageFormat 2 test suite, by `tools/gen_mf2_suite.py`. |
| `test/skeletons_icu.tsv`, `intervals_icu.tsv`, `*_ranges_icu.tsv` | ICU4C 79.1's `DateTimePatternGenerator` and `DateIntervalFormat`. |
| `test/segment_icu.tsv` | ICU4C 79.1's `BreakIterator`, by `tools/oracle/segment/`. |
| `test/cldr/` | A small CLDR tree of crafted locales for the resolver's tests. |
| `test/collation/crafted.xml` | Crafted tailoring rules for the builder's checks. |
