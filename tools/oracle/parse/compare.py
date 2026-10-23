# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen

"""Compares the parse oracle's outputs: compare.py ICU OURS prints the
lines whose value or end differ, then counts."""
import sys
from decimal import Context, Decimal, InvalidOperation

CONTEXT = Context(prec=400, Emax=10**12, Emin=-10**12)


def value(text):
    if text in ("NaN", "Infinity", "-Infinity", "fail") or text.startswith("ERR"):
        return text
    if text == "-0":
        return "-0"
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
        if fa[:4] != fb[:4]:
            raise SystemExit("misaligned: " + a)
        va, vb = value(fa[4]), value(fb[4])
        ea = fa[5] if len(fa) > 5 else ""
        eb = fb[5] if len(fb) > 5 else ""
        if va == vb and ea == eb:
            same += 1
        else:
            diffs.append(f"{a}\t|\t{fb[4]}\t{eb}")
    for d in diffs:
        print(d)
    print(f"# {same} equal, {len(diffs)} differ", file=sys.stderr)


main()
