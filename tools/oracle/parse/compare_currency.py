# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen

"""Compares the currency parse oracle's outputs: compare_currency.py ICU
OURS prints the lines whose value, currency or end differ, then counts."""
import sys
from decimal import Context, InvalidOperation

CONTEXT = Context(prec=400, Emax=10**12, Emin=-10**12)


def value(text):
    if text == "fail" or text.startswith("ERR"):
        return text
    try:
        d = CONTEXT.create_decimal(text.replace("E", "e"))
    except InvalidOperation:
        return "?" + text
    if d == 0:
        return "-0" if d.is_signed() else "0"
    return d.normalize(CONTEXT)


def main():
    icu = open(sys.argv[1], encoding="utf-8").read().splitlines()
    ours = open(sys.argv[2], encoding="utf-8").read().splitlines()
    same = 0
    diffs = []
    for a, b in zip(icu, ours):
        fa, fb = a.split("\t"), b.split("\t")
        if fa[:5] != fb[:5]:
            raise SystemExit("misaligned: " + a)
        if value(fa[5]) == value(fb[5]) and fa[6:] == fb[6:]:
            same += 1
        else:
            diffs.append(f"{a}\t|\t" + "\t".join(fb[5:]))
    for d in diffs:
        print(d)
    print(f"# {same} equal, {len(diffs)} differ", file=sys.stderr)


main()
