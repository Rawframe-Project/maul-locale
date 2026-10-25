# Maul Locale

Maul Locale is a C library of locale data and formatting from one CLDR release (49): locale identifiers and matching, plural rules, numbers, currencies, units, spell-out, calendars, time zones and Temporal arithmetic, date formatting, MessageFormat 2, grammatical suffixes, translation catalogs, collation and segmentation. It is part of the Maul family; the family rules are in [docs/internals/maul-conventions.md](docs/internals/maul-conventions.md), and [docs/internals/overview.md](docs/internals/overview.md) explains how the parts fit.

It never compromises on:

1. **Conformance.** Each part passes CLDR's conformance data and agrees with ICU4C (and ICU4X where it has the part) on the same inputs. Every difference is listed with its reason in the check that keeps it; there are no silent ones.
2. **Hostile input.** Data files are validated completely when opened; text is validated as it is read. No input makes it read out of bounds, loop without bound or allocate.
3. **Caller memory, no system.** Nothing allocates, no global state, no operating system call: no file, clock, environment or process locale is read. Data, instants, zones and the user's languages come from the caller.
4. **Same bits everywhere.** Integer arithmetic only (a `double` input becomes its shortest decimal digits; unit conversion is exact); identical output on every platform and compiler.
5. **Speed.** Each part is benchmarked against ICU4C and ICU4X on the same inputs, and the target is the faster of the two ([performance.md](docs/internals/performance.md)).

## Commands

```sh
tools/check.sh changed                          # before every push: rules, format, the suites a change reaches
tools/check.sh pr                               # what the pull request check runs, but the CLDR checks
tools/check.sh full                             # everything, the CLDR, tzdb and Unihan checks included
cmake --preset linux-clang-20-asan && cmake --build --preset linux-clang-20-asan
ctest --preset linux-clang-20-asan -R '^number$'  # one suite; each is test/test_<name>.c
./out/linux-clang-20-asan/test_number           # the same, printing each failed check
```

The presets name their compilers: the family's CMake refuses GCC 13 and Clang 18, which some machines default to. Run `pr`, `full` and fuzzing through the workspace's heavy-job wrapper on the shared machine. [checks.md](docs/operations/checks.md) explains the tiers and the CI jobs, [development.md](docs/operations/development.md#data-checks) the data checks, and [updating-data.md](docs/operations/updating-data.md) regenerating data. The speed bar ([performance.md](docs/internals/performance.md#the-bar)): each part within 10% of the faster of ICU4C and ICU4X on the same inputs, measured back to back on a quiet machine before a release, never in CI.

## Ways to hurt yourself

- **Killing by pattern.** Never `pkill -f`, `pgrep | kill` or `kill` a PID found by matching a name or path: other sessions build, test and fuzz on the same machine with the same binary names. Kill only a PID you started.
- **Endless fuzzing.** A libFuzzer target runs until it finds something. Always pass `-max_total_time`, and stop what you started.
- **Editing generated files.** `src/generated/` is written by `tools/mlocgen` and the `tools/gen_*.py` scripts; CI regenerates it and fails on a byte of difference. Change the generator or the pin, then regenerate.
- **Editing external files.** `tools/external/*/` holds third-party data exactly as published, with its SHA-256 in each README. Replace it as [updating-data.md](docs/operations/updating-data.md#external-data) says.
- **Editing shared family files.** These are synced from the canonical copies in the organization's `.github` repository and fail CI when they differ: `docs/internals/maul-conventions.md`; `.clang-format`, `.clang-tidy`, `.editorconfig`, `.gitattributes`, `.gitignore`; `cmake/CompilerFlags.cmake`, `cmake/Install.cmake`, `cmake/Library.cmake`, `cmake/ReadVersion.cmake`, `cmake/Testing.cmake`, `cmake/Config.cmake.in`, `cmake/pkg-config.pc.in`, `cmake/HeaderCheck/CMakeLists.txt`; `bench/CMakeLists.txt`; `test/test_harness.h`; and `tools/build_docs.py`, `check_docs.py`, `check_exports.py`, `check_guide.py`, `check_lengths.py`, `check_modules.py`, `check_source.py`, `check_test_hygiene.py`, `gen_api.py`, with `tools/test_hygiene/`. Change the canonical copy first.
- **Fetched sources in the tree.** CLDR, the tzdb and Unihan are fetched into a cache outside the repository; never commit them or point the generator at an unpinned copy.

## Invariants

- **Generated files:** `src/generated/builtin_data.c`, `src/generated/*.mloc` and the code tables (`plural_rules.c`, `calendar_years.c`, `calendar_eras.c`, `script_directions.c`, `list_genders.c`, `segment_overrides.c`, `data_version.h`) by `mlocgen generate`; `turkish_marks.h`, `finnish_types.h`, `hanja_finals.h` and `pow5_tables.h` by `tools/gen_*.py`; `test/mf2_suite.inc` by `tools/gen_mf2_suite.py`; `test/test262_*.inc` by `tools/oracle/test262/`. `test/*_icu.tsv` (ICU4C's date pattern answers) are committed golden data whose generators are lost; [oracles.md](docs/operations/oracles.md#icu-date-tables) says so and issue #4 tracks writing them again.
- **Data version:** a data file opens only with a library whose CLDR and Unicode versions, data format version, tzdb release and code tables' hash it carries ([data.md](docs/internals/data.md#versions)). `MLOC_DATA_FORMAT_VERSION` in `include/maul-locale/base.h` changes with a layout or with what a validator accepts, never with the library version; `tools/check_data_format.py` fails when a layout header changes until it is decided. A change of the format or of a code table means regenerating every data file.
- **Validators read their own unit only.** Units are validated one by one in table order; a check that spans units belongs where a formatter is made, never in a validator ([data.md](docs/internals/data.md#validation)).
- **Locales are canonicalized by the caller.** Every function but `mlocParseLocale` and `mlocCanonicalizeLocale` takes a canonicalized locale and replaces no aliases.
- **Fallback chains** come from `locale_fallback.c` alone; a part never truncates a locale id itself (`zh-Hant` falls back to root, never to `zh`).
- **Generator over runtime.** Pattern matching, rule compilation and tailoring are done by `mlocgen`; the library never parses a date pattern, skeleton, plural rule or collation rule at run time.
- **Library version:** `MLOC_VERSION_*` in `include/maul-locale/base.h`, which CMake reads. `MLOC_CLDR_VERSION_*` names the CLDR pin.
- **Public headers** are the common subset of C17 and C++17 and declare only the API; every public function has a `///` block with `@param`, `@return` and a thread safety paragraph (`tools/check_docs.py`). Opaque structs are `uint64_t` arrays whose size is ABI; a `static_assert` beside each internal struct keeps it within its public one.
- **Modules:** `tools/modules.txt` lists which module may include which; a new include needs a line there.
- **Errors and assertions:** untrusted input (data files, text, arguments) is refused with a status in every build, never by an assertion. `MLOC_ASSERT` (`src/invariant.h`) states internal invariants only; it traps without `NDEBUG` and compiles out of Release. Library code never calls `assert`, `abort` or anything that prints.
- **Workflows:** every action is pinned by commit SHA with its version in a comment, and each workflow and job declares the least `permissions` it needs. The build cells are the presets in `CMakePresets.json`, run by the organization's `cpp.yml`; a new cell is a new preset.

## Testing

Follow the [repository standard's Tests section](https://github.com/Rawframe-Project/.github/blob/main/docs/repository-standard.md#tests); in this repository:

- Conformance comes from CLDR's and the specifications' own files and from oracles, not from hand-written expectations. A behavior change is proven by the conformance check or oracle that covers it; [conformance.md](docs/internals/conformance.md) says which.
- Test through the public headers. A white-box suite (`WHITEBOX` in `CMakeLists.txt`) is for a data layout or a validator that no public call reaches; do not test internal state.
- A listed departure from an oracle needs its reason in the exceptions file; the checks fail on an unlisted difference and on a stale entry.
- Every C snippet of `docs/user/` appears unchanged in `test/test_guide.c` or `test/test_guide_catalog.c`; `tools/check_guide.py` enforces it.
- A fuzz finding becomes a test in its module's suite with the minimized input, and the input a seed when it reaches code random bytes rarely form.
- Suites that read data files get their paths as compile definitions in `CMakeLists.txt` and embed them under Emscripten; a test that calls `fopen` needs `_CRT_SECURE_NO_WARNINGS` on Windows.

## Documentation

Follow the [repository standard's Documentation section](https://github.com/Rawframe-Project/.github/blob/main/docs/repository-standard.md#documentation). A change in what a caller must do or can rely on updates the page of `docs/user/` for that part and, if it is in a snippet, the matching code in `test/test_guide*.c`. A new or removed departure from an oracle updates [conformance.md](docs/internals/conformance.md). The API reference is generated from the headers into the documentation site; never commit it.

## Pull requests

Titles are conventional commits. Scopes: `data`, `locale`, `names`, `decimal`, `plural`, `number`, `currency`, `unit`, `spellout`, `list`, `calendar`, `zone`, `ixdtf`, `datetime`, `relative`, `duration`, `message`, `grammar`, `catalog`, `collate`, `search`, `index`, `segment`, `generator`, `mloccat`, `oracle`, `bench`, `build`, `family`. Example: `fix(number): keep the minus sign of a compact zero`.

A change that breaks the API or ABI, gives existing inputs a different result, or refuses stored bytes is breaking, whatever its type: its title carries `!` and the last paragraph of its body a `BREAKING CHANGE:` footer saying what changed and what a program must do. Callers store formatted text, sort keys, index bytes, compiled messages, catalogs and data files, so a new CLDR or tzdb release, a fixed rule that changes output and a change of `MLOC_DATA_FORMAT_VERSION` or of any stored layout are all breaking.

A change to `src/generated/` names the generator change or the pin that caused it.
