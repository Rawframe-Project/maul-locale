# Development

How to build, test and change Maul Locale as a maintainer. Building and using it as a user is in [getting-started.md](../user/getting-started.md).

## Toolchain

- CMake 3.25 or newer and Ninja.
- GCC 14 and Clang 20 (`clang-20`, `clang-tidy-20`), with the sanitizer and libFuzzer runtimes (`libclang-rt-20-dev`). The family's CMake refuses GCC 13 and Clang 18, which some systems still default to; name the compiler (`-DCMAKE_C_COMPILER=gcc-14`).
- `clang-format` at the version CI pins: `pip install clang-format==22.1.5`.
- Python 3 for the checks, the lexicon generators and the oracle checks; `pip install markdown==3.6` for the documentation site.
- Network access the first time: the build fetches the pinned Maul Unicode release unless an installed package or `FETCHCONTENT_SOURCE_DIR_MAUL-UNICODE` gives one, and the data checks fetch CLDR and the tzdb.

## Build options

| Option | Effect |
|---|---|
| `MAUL_LOCALE_WERROR` | Warnings as errors, as CI builds. |
| `MAUL_LOCALE_SANITIZE` | AddressSanitizer and UBSan. |
| `MAUL_LOCALE_COVERAGE` | gcov instrumentation. |
| `MAUL_LOCALE_FUZZ` | The libFuzzer targets, `fuzz_<area>` at the top of the build tree (Clang only). |
| `MAUL_LOCALE_BUILD_SHARED` | A shared library; the white-box suites, which reach internal symbols, are then not built. |
| `MAUL_LOCALE_BUILD_TESTS`, `_BENCH`, `_TOOLS` | On when the project is built on its own; the tools are off in Emscripten builds. |

## Building and running one test

```sh
cmake -S . -B out/dev -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc-14 -DMAUL_LOCALE_WERROR=ON
cmake --build out/dev
ctest --test-dir out/dev -R '^number$'
./out/dev/test_number
```

Every suite is `test/test_<name>.c`, one executable and one CTest test. A suite that reads a data file gets its path as a compile definition (`MLOC_UNITS_FILE` and the like) in `CMakeLists.txt`; under Emscripten the files are embedded. A suite prints one line per failed check with its file and line.

`test/test_guide.c` and `test/test_guide_catalog.c` run the C snippets of `docs/user/` as they are written there; `python3 tools/check_guide.py` fails when a snippet and its copy differ. Change both together.

## Rules the tools check

```sh
python3 tools/check_source.py    # SPDX lines, comment style, banned calls, history markers
python3 tools/check_modules.py   # the module graph in tools/modules.txt
python3 tools/check_lengths.py   # file and function lengths
python3 tools/check_docs.py      # every public function documented
python3 tools/check_guide.py     # documentation snippets run by the tests
clang-format --dry-run -Werror $(git ls-files '*.[ch]')
```

These and the shared family files they live in are synced from the organization's `.github` repository; a change to one is made there first.

## The generator

`mlocgen` writes `src/generated/` from CLDR and the tzdb. [Updating data](updating-data.md) has the commands; never edit `src/generated/` by hand.

## Data checks

CI checks the generator and the library against the pinned CLDR and tzdb releases and against ICU's matcher test. The same steps run locally with the sources fetched into a cache outside the repository:

```sh
tools/fetch_cldr.sh ~/cldr-cache
tools/fetch_tzdb.sh ~/tzdb-cache
tools/fetch_tzdb.sh ~/tzdb-cache older
cmake --build out/dev --target mlocgen conform_numbers   # and the other conform_* programs
```

Then each check is one command: `tools/check_*.py` with the program and the data it compares (each script's header gives its usage), the `conform_*` programs on CLDR's test files, and `mlocgen generate` followed by `git diff --exit-code src/generated`. The zone checks need `zic` and `zdump` from the tzdb's own build and Python's `zoneinfo`. [Conformance](../internals/conformance.md) lists what each check proves.

## Fuzzing

```sh
cmake -S . -B out/fuzz -G Ninja -DCMAKE_C_COMPILER=clang-20 -DMAUL_LOCALE_FUZZ=ON \
    -DMAUL_LOCALE_BUILD_TESTS=OFF -DMAUL_LOCALE_BUILD_TOOLS=OFF
cmake --build out/fuzz
mkdir -p out/corpus/fuzz_message && cp test/seeds/fuzz_message/* out/corpus/fuzz_message/
./out/fuzz/fuzz_message -max_total_time=300 -max_len=512 out/corpus/fuzz_message
```

Always pass `-max_total_time`: a target runs until it finds something. A target writes a failing input as `crash-<hash>` in its working directory; run the target on that file to reproduce it. Every crash becomes a test in the suite of its module with the smallest input that still fails (`-minimize_crash=1`), and its input goes into `test/seeds/<target>/` when it reaches code that random bytes rarely form. The data targets and the collation targets read the generated files named in `CMakeLists.txt`.

## Benchmarks

`out/<build>/maul-locale_bench` times every part on fixed inputs. Build the base and the change in Release with the same compiler and run them back to back on a quiet machine; [performance.md](../internals/performance.md#measuring) explains how to measure on a busy one, and [oracles.md](oracles.md#benchmarks) how to run the ICU4C and ICU4X baselines. `MAUL_LOCALE_BUILD_TOOLS=OFF` builds the benchmark without the catalog cases, as the fuzz and web builds do; build it that way too when a change touches code behind a tools-only definition.
