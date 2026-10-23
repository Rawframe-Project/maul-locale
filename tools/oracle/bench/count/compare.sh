#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Instructions a call, the library's and ICU4X's, for the number part's
# cases, counted with perf so that a busy machine does not change them:
# each program runs a case 0 and 20000 times, and the difference is
# divided. Run from the repository root.
#
# usage: compare.sh BUILD ICU4X_COUNT
#   BUILD        a Release build tree of the library
#   ICU4X_COUNT  icu4x_number_count.rs built with cargo --release
# CASES (a space-separated list) narrows the cases.
set -euo pipefail
build=${1:?usage: compare.sh BUILD ICU4X_COUNT}
icu4x=${2:?usage: compare.sh BUILD ICU4X_COUNT}
ours=$(mktemp)
trap 'rm -f "$ours"' EXIT
gcc-14 -O2 -std=c23 -Iinclude "$(dirname "$0")"/maul_number_count.c "$build"/libmaul-locale.a \
    "$build"/_deps/maul-unicode-build/libmaul-unicode.a -o "$ours" -lm
per() {
    a=$(perf stat -x, -e instructions:u "$@" 0 2>&1 >/dev/null | cut -d, -f1)
    b=$(perf stat -x, -e instructions:u "$@" 20000 2>&1 >/dev/null | cut -d, -f1)
    echo $(((b - a) / 20000))
}
for c in ${CASES:-number-init number-format percent-format unit-init unit-format}; do
    o=$(per "$ours" "$c")
    x=$(per "$icu4x" "$c")
    printf "%-16s ours %6d  icu4x %6d  %s\n" "$c" "$o" "$x" "$(awk "BEGIN{printf \"%.2f\", $o/$x}")"
done
