#!/bin/bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Regenerates src/generated/ with mlocgen into a clean directory. mlocgen
# writes its files and removes none, so its old ones go first: a file it no
# longer writes then shows as deleted instead of staying behind. The tables
# the scripts below write stay, each script giving its own file
# (--output-path); mlocgen does not remake them. A table whose script is
# not listed is removed, and the CI's `generated` job fails on it.
#
# usage: tools/regenerate.sh <mlocgen> <CLDR common directory> <tzdb directory>
#   then `git status --porcelain -- src/generated` is empty when the
#   committed files are current.
set -euo pipefail
[ $# -eq 3 ] || { echo "usage: regenerate.sh <mlocgen> <CLDR common directory> <tzdb directory>" >&2; exit 2; }
root=$(cd "$(dirname "$0")/.." && pwd)
tables=(gen_pow5_tables.py gen_turkish_marks.py gen_finnish_types.py gen_hanja_finals.py)
keep=()
for script in "${tables[@]}"; do
    path=$(python3 -I "$root/tools/$script" --output-path)
    case "$path" in
    src/generated/*) [ -f "$root/$path" ] || { echo "regenerate.sh: $script names $path, which does not exist" >&2; exit 1; } ;;
    *) echo "regenerate.sh: $script writes $path, outside src/generated" >&2; exit 1 ;;
    esac
    keep+=("$path")
done
for file in "$root"/src/generated/*; do
    name=src/generated/$(basename "$file")
    printf '%s\n' "${keep[@]}" | grep -qxF "$name" || rm -- "$file"
done
"$1" generate "$2" "$root" "$3"
