# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes one of test/*_icu.tsv from its parts: the header (the table's
# comment lines), an ICU4C program's output (each case's columns, then
# ICU's answer) and the departures (each case's columns, then the
# library's own answer and its cause, where the library answers otherwise
# on purpose). A departure follows its case's line; one whose case the
# output lacks, or a case given twice, is an error.
#   python3 -I merge.py HEADER OUTPUT DEPARTURES CASE_COLUMNS > table
import sys


def main():
    header, output, departures, columns = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
    extra = {}
    with open(departures, encoding="utf-8") as f:
        for line in f.read().splitlines():
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            key = "\t".join(fields[:columns])
            if key in extra:
                sys.exit(f"merge.py: a departure given twice: {key}")
            extra[key] = "\t".join(fields[columns:])
    out = sys.stdout
    with open(header, encoding="utf-8") as f:
        out.write(f.read())
    used = set()
    with open(output, encoding="utf-8") as f:
        for line in f.read().splitlines():
            key = "\t".join(line.split("\t")[:columns])
            if key in extra:
                if key in used:
                    sys.exit(f"merge.py: a case given twice: {key}")
                used.add(key)
                line += "\t" + extra[key]
            out.write(line + "\n")
    unused = set(extra) - used
    if unused:
        sys.exit(f"merge.py: departures without their case: {sorted(unused)[:3]}")


main()
