#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# The checks, in three tiers. Each tier includes the one before it.
#
#   tools/check.sh changed   the files changed against origin/main: the
#                            source rules, formatting, the documentation
#                            checks, and the linux-clang-20-asan preset
#                            (assertions on) running the test labels the
#                            changes reach. Under two minutes on a built
#                            tree; run it before every push.
#   tools/check.sh pr        what the pull request check runs on this
#                            machine but the CLDR checks: every rule check,
#                            the tables generated from data in the
#                            repository, clang-tidy, the linux-gcc-14 and
#                            linux-clang-20-asan presets, and a short
#                            fuzzing pass.
#   tools/check.sh full      also the other Linux presets (shared with the
#                            package test, Clang Release), longer fuzzing,
#                            the generated sources regenerated and every
#                            check against the pinned CLDR and tzdb
#                            releases, and the Hanja finals against Unihan.
#                            The releases are fetched into out/cache once.
#
# The builds are the CI cells' CMake presets (CMakePresets.json), in
# out/<preset>; the other trees go to out/check-*. Needs gcc-14, clang-20,
# clang-tidy-20, ninja, python3 and the clang-format version CI pins, and
# for full, zic and zdump: the organization's CI image has them all. The
# organization's documentation check runs when its .github checkout is at
# ORG_DEFAULTS (default: ../../.github from the repository).
set -euo pipefail

tier=${1:-changed}
case "$tier" in
changed | pr | full) ;;
*)
    echo "usage: tools/check.sh [changed|pr|full]" >&2
    exit 2
    ;;
esac

cd "$(dirname "$0")/.."
root=$PWD
out=$root/out
mkdir -p "$out"
org=${ORG_DEFAULTS:-$root/../../.github}
jobs=${JOBS:-8}

step() { printf '\n== %s\n' "$*"; }

# A tree outside the presets (clang-tidy, fuzzing, the CLDR checks).
configure() { # name, then cmake arguments
    local name=check-$1
    shift
    cmake -S "$root" -B "$out/$name" -G Ninja -DMAUL_LOCALE_WERROR=ON -DMAUL_LOCALE_BUILD_BENCH=OFF "$@" \
        >"$out/$name.log"
}

# A CI cell: its configure, build and test presets. More ctest arguments
# (a label filter) follow the preset's name.
preset() {
    local name=$1
    shift
    step "preset $name"
    cmake --preset "$name" >"$out/$name.log"
    cmake --build --preset "$name" -j "$jobs" >>"$out/$name.log"
    ctest --preset "$name" -j "$jobs" "$@"
}

# The ctest labels a changed path reaches, as a regular expression over
# the labels (each suite's label is its name); "all" when it reaches every
# suite: shared code, the data, the build.
labels_for() {
    local stem
    case "$1" in
    docs/user/*) echo "guide|guide_catalog" ;;
    *.md | LICENSE | LICENSES/* | .github/* | tools/check_*.py | tools/check_cldr.sh | \
        tools/build_docs.py | tools/gen_api.py | tools/oracle/* | tools/external/* | \
        tools/*.txt | test/fuzz_* | test/seeds/* | bench/* | samples/*) ;;
    test/test_*.c) stem=${1#test/test_} && echo "${stem%.c}" ;;
    tools/mloccat/*) echo "catalog.*|guide_catalog|accept|report|pseudo|po|xliff|resource" ;;
    tools/mlocgen/*) echo "gen.*|collation_.*|search_.*|trie" ;;
    src/base.c | src/data* | src/kinds.h | src/bytes.h | src/wide.h | src/binary64.* | src/generated/* | \
        include/maul-locale/base.h | include/maul-locale/data.h) echo all ;;
    src/* | include/maul-locale/*)
        stem=$(basename "$1")
        stem=${stem%.*}
        stem=${stem%%_*}
        case "$stem" in
        collation | collator | collate | nfd | search | index) stem="collat|search|index" ;;
        datetime | date | time | instant | zoned | rounding) stem="date|time|instant|zone|datetime|calendar" ;;
        parse) stem="number|currency|parse" ;;
        script | code) stem=locale ;;
        esac
        # The guide's snippets reach every part; the message functions
        # reach most.
        echo "($stem).*|guide|message.*"
        ;;
    *) echo all ;;
    esac
}

source_checks() {
    step "source, module, length, documentation and snippet rules"
    python3 tools/check_source.py
    python3 tools/check_modules.py
    python3 tools/check_lengths.py
    python3 tools/check_docs.py
    python3 tools/check_guide.py
    if [ -f "$org/scripts/docs_check.py" ]; then
        python3 "$org/scripts/docs_check.py" --exclude tools/external
    else
        echo "skipped the organization's documentation check: no checkout at $org"
    fi
}

all_c_files() { git ls-files '*.[ch]' '*.cpp' | grep -v '^src/generated/'; }

if [ "$tier" = changed ]; then
    base=$(git merge-base HEAD origin/main 2>/dev/null || echo HEAD)
    mapfile -t changed < <({
        git diff --name-only "$base"
        git ls-files --others --exclude-standard
    } | sort -u)
    if [ ${#changed[@]} -eq 0 ]; then
        echo "nothing changed against origin/main"
        exit 0
    fi
    source_checks
    mapfile -t c_files < <(printf '%s\n' "${changed[@]}" | grep -E '\.([ch]|cpp)$' | grep -v '^src/generated/' || true)
    present=()
    for f in "${c_files[@]+"${c_files[@]}"}"; do [ -f "$f" ] && present+=("$f"); done
    if [ ${#present[@]} -gt 0 ]; then
        step "clang-format on ${#present[@]} changed files"
        clang-format --dry-run -Werror "${present[@]}"
    fi
    labels=()
    for f in "${changed[@]}"; do
        l=$(labels_for "$f")
        [ -n "$l" ] && labels+=("$l")
    done
    if [ ${#labels[@]} -eq 0 ]; then
        echo "no test reached by the changes"
        exit 0
    fi
    if printf '%s\n' "${labels[@]}" | grep -qx all; then
        preset linux-clang-20-asan
    else
        regex=$(printf '%s\n' "${labels[@]}" | sort -u | paste -sd '|')
        echo "tests labeled ^($regex)\$"
        preset linux-clang-20-asan -L "^($regex)\$"
    fi
    exit 0
fi

source_checks

step "clang-format on every file"
mapfile -t files < <(all_c_files)
[ ${#files[@]} -gt 500 ] || {
    echo "too few files to format: ${#files[@]}" >&2
    exit 1
}
clang-format --dry-run -Werror "${files[@]}"

step "tables generated from data in the repository"
python3 tools/gen_pow5_tables.py --check
python3 tools/gen_turkish_marks.py --check
python3 tools/gen_finnish_types.py --check

step "clang-tidy"
configure tidy -DCMAKE_C_COMPILER=clang-20 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMAUL_LOCALE_BUILD_TESTS=OFF
files=(src/*.c)
[ -f "${files[0]}" ] || {
    echo "no source file for clang-tidy" >&2
    exit 1
}
printf '%s\n' "${files[@]}" | xargs -P "$jobs" -n 1 clang-tidy-20 -p "$out/check-tidy" --quiet --warnings-as-errors='*' 2>/dev/null

preset linux-gcc-14
preset linux-clang-20-asan

fuzz_seconds=10
[ "$tier" = full ] && fuzz_seconds=60
step "fuzzing, $fuzz_seconds seconds per target"
configure fuzz -DCMAKE_C_COMPILER=clang-20 -DMAUL_LOCALE_FUZZ=ON -DMAUL_LOCALE_BUILD_TESTS=OFF \
    -DMAUL_LOCALE_BUILD_TOOLS=OFF
cmake --build "$out/check-fuzz" -j "$jobs" >>"$out/check-fuzz.log"
targets=("$out"/check-fuzz/fuzz_*)
[ -x "${targets[0]}" ] || {
    echo "no fuzz target was built" >&2
    exit 1
}
for target in "${targets[@]}"; do
    name=$(basename "$target")
    corpus=$out/corpus/$name
    mkdir -p "$corpus"
    [ -d "test/seeds/$name" ] && cp test/seeds/"$name"/* "$corpus"/
    "$target" -max_total_time=$fuzz_seconds -max_len=512 -artifact_prefix="$out/" "$corpus" \
        >"$out/$name.log" 2>&1 ||
        {
            tail -30 "$out/$name.log"
            exit 1
        }
    echo "$name: no finding"
done

[ "$tier" = full ] || exit 0

for name in linux-shared linux-clang-20; do
    preset "$name"
done

step "the pinned CLDR and tzdb releases"
cache=$out/cache
bash tools/fetch_cldr.sh "$cache/cldr" >/dev/null
bash tools/fetch_tzdb.sh "$cache/tzdb" >/dev/null
bash tools/fetch_tzdb.sh "$cache/tzdb" older >/dev/null
tzdb=$cache/tzdb/tzdb-$(awk '$1 == "release" { print $2 }' tools/tzdb-release.txt)
older=$cache/tzdb/tzdb-$(awk '$1 == "release" { print $2 }' tools/tzdb-older.txt)
configure cldr -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-14
cmake --build "$out/check-cldr" -j "$jobs" --target conformance >>"$out/check-cldr.log"

step "the generated sources regenerated"
"$out/check-cldr/mlocgen" generate "$cache/cldr/cldr/common" . "$tzdb"
git diff --exit-code --stat src/generated

tools/check_cldr.sh "$out/check-cldr" "$cache/cldr" "$tzdb" "$older"

step "the Hanja finals against the pinned Unihan"
version=$(awk '$1 == "version" { print $2 }' tools/unihan-release.txt)
mkdir -p "$cache/unihan"
[ -f "$cache/unihan/Unihan.zip" ] ||
    curl -sSfL -o "$cache/unihan/Unihan.zip" "https://www.unicode.org/Public/$version/ucd/Unihan.zip"
python3 tools/gen_hanja_finals.py "$cache/unihan" --check

step "all checks passed"
