# Oracles

How to run the differential oracles in `tools/oracle/`: programs that give the same inputs to the library and to ICU4C, ICU4X, Apertium or test262, and compare. They are run by hand when a pin moves (a new CLDR, ICU or tzdb release) or a part's behavior changes, not in CI, since CI has no ICU 79. What each proves is in [conformance.md](../internals/conformance.md); a difference found is either fixed, or listed with its reason in the check that keeps it.

## Contents

- [Building the oracles](#building-the-oracles)
- [Numbers, currencies, units and lists](#numbers-currencies-units-and-lists)
- [Parsing](#parsing)
- [Collation](#collation)
- [Calendars](#calendars)
- [test262 tables](#test262-tables)
- [Grammar](#grammar)
- [Segmentation](#segmentation)
- [Benchmarks](#benchmarks)
- [ICU date tables](#icu-date-tables)

## Building the oracles

Run everything from the repository root, after a Release build of the library in `out/<preset>` (the library-side programs read `src/generated/*.mloc` by that relative path).

- **ICU4C 79.1**, the release built on CLDR 49, from source (`https://github.com/unicode-org/icu` at the tag `tools/external/icu/README.md` names): `cd icu4c/source && ./configure --prefix=$ICU && make -j && make install`. Programs that read ICU internals also need the source tree, `$ICU_SRC=icu4c/source`. Build each `icu*.cpp` with `g++ -O2 -std=c++17 -I$ICU/include [-I$ICU_SRC/common -I$ICU_SRC/i18n] file.cpp -L$ICU/lib -licui18n -licuuc -licudata` and run with `LD_LIBRARY_PATH=$ICU/lib`; each file's header gives its own command where it differs.
- **ICU4X 2.3**: each `.rs` file is the `main.rs` of a cargo project whose dependencies its header names (`icu = "=2.3.0"`, `icu_calendar`, `icu_experimental`, `calendrical_calculations = "=0.2.4"`); build with `cargo build --release`.
- **The library's side**: each `ours_*.c` and `maul_*.c` with `gcc-14 -O2 -std=c23 -Iinclude [-Isrc] file.c out/<preset>/libmaul-locale.a out/<preset>/_deps/maul-unicode-build/libmaul-unicode.a`. Programs that read internal functions say so in their header and need `-Isrc`.

Every program reads its cases from standard input or a file, one case a line, and prints the case, a tab and its answer, so two outputs compare line by line (`diff`, or the area's `compare*.py`).

## Numbers, currencies, units and lists

The cases cover the 105 modern locales of `tools/oracle/currency/locales.txt`.

| Folder | Cases | Programs |
|---|---|---|
| `currency/` | "locale code display sign usage value" for amounts; "locale code display start end" for ranges; "locale code display short\|long value" for compact amounts. ICU rounds half up, the library's default. | `icu_cur.cpp`, `ours.c`; `icu_range.cpp`, `ours_range.c`; `icu_compact.cpp`, `ours_compact.c` |
| `units/` | `cases.py` writes `cases.txt`, `ranges.txt` and `names.txt` from `locales.txt` and `units.txt` (CLDR's unit types), `ccases.txt` from `cunits.txt` (compound units), `mcases.txt` from `mixed.txt`, `ucases.txt` and `urcases.txt` from `usage.txt`. | `icu_units.cpp`, `ours_units.c` (amounts and ranges with the unit fields); `icu_names.cpp`, `ours_names.c` (display names); `icu_usage.cpp`, `ours_usage.c` (the usage option). ICU aborts on some usage ranges; `crashsafe.py PROGRAM IN OUT` runs past a line that crashes it and marks it `CRASH`. |
| `unit-cases/` | `cases.py MAIN` writes the case lines of the locales with case patterns; `genders.py COMMON` the gender lines. | `icu_cases.cpp` (ICU's `unitDisplayCase` and `getNounClass`), `ours_cases.c UNITS` |
| `numeric/` | `cases.py DIR` writes numeric durations with their exact split. | `icu_numeric.cpp` (`MeasureFormat` numeric width), `ours_numeric.c` |
| `lists/` | `cases.py` writes 28 lists per locale and style, ICU's contextual cases among them. Give locales canonicalized ("he", not "iw"). | `icu_lists.cpp`, `ours_lists.c` (internal functions, `-Isrc`) |

Two classes of difference are expected: values where ICU's `double` arithmetic drifts and the library's exact value is right (unit conversion), and "generic", which ICU refuses.

## Parsing

`parse/` compares `mlocNumberFormatter_Parse` and `mlocCurrencyFormatter_Parse` with ICU's `DecimalFormat::parse` and `parseCurrency` on lines "locale style mode text" (and "locale code mode any text" for currencies):

1. `cases.py LOCALES ours_fmt OUT` formats twelve values in each style with the library (`ours_fmt.c`) and writes each text with its lenient variants; `currency_cases.py LOCALES ours_cfmt NAMES OUT` does the same for amounts of money. `random_cases.py` and `random_currency.py LOCALES OUT SEED` write random strings.
2. Run `icu_parse.cpp` and `ours_parse.c` (or `icu_currency.cpp` and `ours_currency.c`) on the same lines.
3. `compare.py ICU OURS` and `compare_currency.py ICU OURS` print the lines whose value, currency or end differ, then counts.

`icu_pattern.cpp` parses with an arbitrary pattern; it gave the crafted cases of `test_parse_patterns.c`. Expected differences are the listed departures: the scientific pattern of five Indic locales (ICU reads its brackets, the library writes the decimal pattern's affixes), exponents past 10^±999999, and long currency names that begin with their code.

## Collation

The conform programs of the library write their questions with `--queries`; the ICU4C programs answer them; the compare scripts weigh only the tags whose locale and type ICU's own loader took, since ICU's data leaves some CLDR locales out.

| Question | Library side | ICU4C side |
|---|---|---|
| Root elements | `conform_collation_root_elements --dump`, `--queries` (`root_elements_queries.py`) | `icu4c_collation_root_elements.cpp` |
| Tailored elements | `conform_collation_builder --queries`, `--answers` | `icu4c_collation_tailored.cpp` |
| Weight allocator | `test_collation_weights` (cases from `weights_cases.py`) | `icu4c_collation_weights.cpp` |
| Options | `conform_collation_options` (pairs from `pairs.py`) | `icu4c_collation_options.cpp` |
| Reordering | `conform_collation_reorder` (lists from `reorder_lists.py`) | `icu4c_collation_reorder.cpp` |
| Sort keys | `conform_collation_keys --keys`, compared by `keys_compare.py` | `icu4c_collation_keys.cpp` |
| Fast Latin tables | `conform_collation_fast`, compared by `fast_latin_compare.py` | `icu4c_collation_fast_latin.cpp` |
| Unsafe-backward sets | `conform_collation_fast --unsafe` | `icu4c_collation_unsafe.cpp` |
| Search elements | `conform_search_elements` (texts from `search_elements_texts.py`), `search_elements_compare.py` | `icu4c_search_elements.cpp`, `icu4c_search_elements_back.cpp` |
| Search | `conform_search` | `icu4c_search.cpp` |
| Index labels and buckets | `conform_index_labels`, `conform_index` | `icu4c_index_labels.cpp`, `icu4c_index.cpp` |

The expected hashes the CI data checks hold for these programs' outputs (`--expect`) come from the ICU4C side; when a pin moves, rerun both sides, read every difference, and update the hash only for differences explained.

## Calendars

`calendar/icu4x_calendar_fields.rs CALENDAR FIRST LAST` prints every day's fields as ICU4X computes them; `test_calendar_oracle` holds the FNV-1a hash of those lines for each calendar over 1600 to 2400 and checks the Chinese and Dangi ones, `conform_calendar_oracle` checks them all, and a mismatch is found day by day by running the program. `icu4c_calendar_months.cpp`, `icu4x_calendar_months.rs` and `reingold_calendar_months.rs` print the month starts of the Chinese and Dangi calendars by ICU4C, ICU4X and Reingold and Dershowitz's full astronomy, which is how the year tables and their overrides are checked against each other.

## test262 tables

`test/test262_*.inc` are tables of every assertion test262's Temporal tests make, recorded by running the tests' JavaScript on the library through a small interpreter:

1. Build the library as a shared library (`MAUL_LOCALE_BUILD_SHARED=ON`) and check out test262 at the commit the tables name.
2. `python3 -I tools/oracle/test262/t262cal.py out/<preset>/libmaul-locale.so <test262> rows.json <test files>` runs the Temporal tests of `test/built-ins/Temporal/` (and `test/intl402/Temporal/`) and records each assertion with what test262 expects; several runs merge with `merge_rows.py`, which refuses conflicting expectations.
3. Each `emit*.py` writes one table: `emit.py` the calendar table, `emit_zone.py`, `emit_time.py`, `emit_dt.py`, `emit_inst.py`, `emit_zdt.py`, `emit_dur.py` and `emit_ixdtf.py` the others (`python3 -I emit.py rows.json <test262 commit> test/test262_calendar.inc`).

A test the interpreter cannot run is reported, not recorded.

## Grammar

`grammar/` compares the inflector with Apertium's generators (GPL; run here, never shipped) and with Wiktionary's tables:

1. `build-apertium.sh LANG` builds a generator (tur, aze, kaz, kir, uzb, tuk, hun, fin) with hfst.
2. `tr_queries.py LEXC OUT_QUERIES OUT_REQUESTS` writes queries for the lexicon's names and nouns; `hfst-lookup` answers them; `maul_inflect SPELLOUT_FILE [LOCALE] < requests` gives the library's forms; `tr_compare.py` reports the lines where the library's form is not among Apertium's.
3. `wiktionary_queries.py` does the same from a kaikki.org extract of Wiktionary.

Apertium errs too (see [grammar.md](../internals/grammar.md#proof-and-its-limits)); a difference is a reason to read the spelling guide.

## Segmentation

`segment/` writes ICU4C's answers for the cases `test_segment` checks, `test/segment_icu.tsv`; the suite gives the same cases to the library, whole and in pieces, and fails on a difference `tools/segment-exceptions.txt` does not list and on a listed case that no longer differs. Write the table again when CLDR, ICU4C or the cases move:

```sh
python3 tools/oracle/segment/cases.py "$CLDR/common" > cases.txt
LD_LIBRARY_PATH=$ICU/lib ./icu4c_segment < cases.txt > test/segment_icu.tsv
```

`cases.py` puts each suppression of each locale in a sentence before a capital, with `-u-ss-standard` and without, and each partial's text through its first full stop alone; a break held back past the bytes a segmenter keeps; the class overrides; the line keywords over CJ, quotation marks, the wave dash, what the loose level adds and Latin text; Japanese words; and plain text in each locale and kind. `icu4c_segment.cpp` answers with UTF-8 byte offsets.

## Benchmarks

`bench/` holds the ICU4C (`icu4c_*.cpp`, `icu4c_bench.c`) and ICU4X (`icu4x_*.rs`) baselines of `bench/bench_main.c`'s cases, with the same inputs, and `gettext_catalog_bench.c` for catalog lookups against GNU gettext. Each prints nanoseconds per call, the best of five runs. `bench/count/compare.sh BUILD ICU4X_COUNT` counts instructions per call for the number part with `perf`, which a busy machine does not change ([performance.md](../internals/performance.md#measuring)).

## ICU date tables

`test/skeletons_icu.tsv`, `intervals_icu.tsv`, `date_ranges_icu.tsv`, `time_ranges_icu.tsv` and `datetime_ranges_icu.tsv` hold ICU4C 79.1's `DateTimePatternGenerator` and `DateIntervalFormat` answers; each file's header says what was asked. The programs that wrote them are lost: the files stay as committed golden data, and a new ICU release cannot regenerate them until the programs are written again from those headers ([#4](https://github.com/Rawframe-Project/maul-locale/issues/4)).
