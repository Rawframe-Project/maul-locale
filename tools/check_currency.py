#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Compares the library's currency fractions and regions with CLDR's JSON
# of the same release (currencyData of cldr-core; the JSON is an oracle,
# never an input). Every currency the JSON names, in its fractions or its
# regions, must have the digits and rounding increments the fractions
# give (the default for one they lack, a rounding of 0 being an
# increment of 1, cash values falling back to the standard ones); every
# region must list the JSON's currencies in its order, with their first
# and last days (UTS #35's date ranges, both days included) and their
# tender flags.
#
# usage: check_currency.py <conform_currency> <CLDR JSON full dir>

import datetime
import json
import os
import subprocess
import sys

EPOCH = datetime.date(1970, 1, 1).toordinal()


def day(text, open_end):
    if text is None:
        return open_end
    return str(datetime.date.fromisoformat(text).toordinal() - EPOCH)


def fraction(fractions, code):
    f = fractions.get(code, fractions["DEFAULT"])
    digits = int(f["_digits"])
    rounding = max(int(f["_rounding"]), 1)
    cash_digits = int(f.get("_cashDigits", digits))
    cash_rounding = max(int(f.get("_cashRounding", f["_rounding"])), 1)
    return "%d %d %d %d" % (digits, rounding, cash_digits, cash_rounding)


def region(entries):
    out = []
    for entry in entries:
        ((code, attributes),) = entry.items()
        out.append(",".join([
            code,
            day(attributes.get("_from"), "min"),
            day(attributes.get("_to"), "max"),
            "0" if attributes.get("_tender") == "false" else "1",
        ]))
    return " ".join(out)


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: check_currency.py <conform_currency> <CLDR JSON full dir>")
    tool, root = sys.argv[1:]
    path = os.path.join(root, "cldr-core", "supplemental", "currencyData.json")
    with open(path, encoding="utf-8") as f:
        data = json.load(f)["supplemental"]["currencyData"]
    fractions, regions = data["fractions"], data["region"]
    codes = {c for c in fractions if c != "DEFAULT"}
    for entries in regions.values():
        for entry in entries:
            codes |= set(entry)
    cases = {}
    for code in sorted(codes):
        cases["fraction " + code] = fraction(fractions, code)
    for name in sorted(regions):
        cases["region " + name] = region(regions[name])
    lines = "".join(k + "\n" for k in cases)
    result = subprocess.run([tool], input=lines, capture_output=True, text=True, check=True)
    got = dict(line.split("\t", 1) for line in result.stdout.splitlines())
    wrong = [k for k in cases if got.get(k) != cases[k]]
    for k in wrong[:20]:
        print("%s: expected %s, got %s" % (k, cases[k], got.get(k)))
    print("currency data: %d of %d cases agree" % (len(cases) - len(wrong), len(cases)))
    sys.exit(1 if wrong else 0)


if __name__ == "__main__":
    main()
