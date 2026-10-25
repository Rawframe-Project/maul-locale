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
| `MAUL_LOCALE_TEST_PACKAGE` | The `package` test: installs the build and builds `samples/minimal` against it. |
| `MAUL_LOCALE_BUILD_TESTS`, `_BENCH`, `_TOOLS` | On when the project is built on its own; the tools are off in Emscripten builds. |

## Building and running one test

The CI cells are CMake presets (`CMakePresets.json`), each building into `out/<preset>`:

```sh
cmake --preset linux-clang-20-asan
cmake --build --preset linux-clang-20-asan
ctest --preset linux-clang-20-asan -R '^number$'
./out/linux-clang-20-asan/test_number
```

`tools/check.sh changed` runs the rule checks and the suites a change reaches; [checks.md](checks.md) has the tiers.

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
cmake -S . -B out/cldr -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-14
cmake --build out/cldr --target conformance
tools/check_cldr.sh out/cldr ~/cldr-cache ~/tzdb-cache/tzdb-2026e ~/tzdb-cache/tzdb-2025b
```

The `conformance` target builds `mlocgen` and every `conform_*` program; `tools/check_cldr.sh` runs each check in turn, and each check's script or program header gives its own usage. `tools/check.sh full` does all of this, and regenerates `src/generated/` first. The zone checks need `zic` and `zdump` and Python's `zoneinfo`. [Conformance](../internals/conformance.md) lists what each check proves.

## Fuzzing

```sh
cmake -S . -B out/fuzz -G Ninja -DCMAKE_C_COMPILER=clang-20 -DMAUL_LOCALE_FUZZ=ON \
    -DMAUL_LOCALE_BUILD_TESTS=OFF -DMAUL_LOCALE_BUILD_TOOLS=OFF
cmake --build out/fuzz
mkdir -p out/corpus/fuzz_message && cp test/seeds/fuzz_message/* out/corpus/fuzz_message/
./out/fuzz/fuzz_message -max_total_time=300 -max_len=512 out/corpus/fuzz_message
```

Always pass `-max_total_time`: a target runs until it finds something. A target writes a failing input as `crash-<hash>` in its working directory; run the target on that file to reproduce it. Every crash becomes a test in the suite of its module with the smallest input that still fails (`-minimize_crash=1`), and its input goes into `test/seeds/<target>/` when it reaches code that random bytes rarely form. The data targets and the collation targets read the generated files named in `CMakeLists.txt`.

## Debugging a failure

**A red CI cell.** A build cell that fails uploads its CTest logs as the artifact `logs-<preset>` (`logs-web` for the web job), kept for seven days: `LastTest.log` holds every suite's output. Reproduce it locally with the same preset, then run the one suite's binary, which prints each failed check with its file and line. A suite that takes arguments (a source directory, a scratch directory) has them in CTest's command line:

```sh
ctest --preset linux-clang-20-asan -R '^gendirection$' -N -V | grep 'Test command'
```

**A trap.** Neither the CI image nor the shared server has a debugger. A broken `MLOC_ASSERT` (`src/invariant.h`) executes a trap instruction, which stops a sanitizer build with "Illegal instruction" and no stack. Run it again with the sanitizer handling the signal, and the report gives the stack with file and line:

```sh
ASAN_OPTIONS=handle_sigill=1 ./out/linux-clang-20-asan/test_number
```

UBSan's own reports (`-fno-sanitize-recover=all` makes each one stop the run) and AddressSanitizer's already carry the stack. A Release build compiles `MLOC_ASSERT` out, so a trap seen only in Debug or a sanitizer build is still a real bug.

**A fuzz finding.** The `fuzz` job uploads the artifact `fuzz`, with each target's log in `fuzz-logs/` and its failing input in `fuzz-crashes/` as `crash-<hash>` (or `timeout-`, `oom-`). Build the fuzz tree as in [Fuzzing](#fuzzing), run the target on the input to reproduce it, and minimize it:

```sh
./out/fuzz/fuzz_message crash-<hash>
./out/fuzz/fuzz_message -minimize_crash=1 -max_total_time=60 -exact_artifact_path=minimal crash-<hash>
```

The minimal input becomes a test in its module's suite, and a seed when it reaches code random bytes rarely form.

**The generator.** Build `mlocgen` in Release, as the `generated` job does, for regeneration. To chase a crash or a wrong table, use the sanitizer build's `out/linux-clang-20-asan/mlocgen`, which runs the same code with every check on but far slower. Its smaller commands (`chains`, `plural-samples`, `zones`, `resolve`; run `mlocgen` alone for the list) read one part of CLDR or the tzdb without a full generate.

**A stale `src/generated/`.** The `generated` job regenerates into a clean `src/generated/` and prints `git status --porcelain` when anything differs:
- `M` for a changed file: the branch changed the generator, a pin or a code table without committing the regenerated output, or a file was edited by hand;
- `D` for a file the generator no longer writes;
- `??` for a new one it writes that is not committed.

Run the same step locally with the pins fetched as in [Data checks](#data-checks), and read the difference:

```sh
tools/regenerate.sh out/cldr/mlocgen ~/cldr-cache/cldr/common ~/tzdb-cache/tzdb-2026e
git status --porcelain -- src/generated
```

If the branch did not mean to change the output, the difference points at the generator change that caused it. If it did, commit the regenerated files with the change, and name the generator change or pin in the pull request.

## Benchmarks

`out/<build>/maul-locale_bench` times every part on fixed inputs. Build the base and the change in Release with the same compiler and run them back to back on a quiet machine; [performance.md](../internals/performance.md#measuring) explains how to measure on a busy one, and [oracles.md](oracles.md#benchmarks) how to run the ICU4C and ICU4X baselines. `MAUL_LOCALE_BUILD_TOOLS=OFF` builds the benchmark without the catalog cases, as the fuzz and web builds do; build it that way too when a change touches code behind a tools-only definition.
