#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks the library's unit preferences against CLDR's
# unitPreferencesTest.txt and unitLocalePreferencesTest.txt (the files
# are oracles, never inputs). Each case gives an input unit, usage,
# region or locale and value, and the unit chosen with its amounts as
# exact fractions (a mixed unit's larger parts as integers). The library
# must choose the same unit, the same integers, and the last amount as
# mlocConvertUnit's default precision writes the exact value (exact, or
# 38 significant digits half even). An input that is no finite decimal
# is given to 40 digits, and its last amount compared at 30.
#
# usage: check_unit_prefs.py <conform_unit_prefs> <testData/units directory>

import decimal
import fractions
import os
import subprocess
import sys

F = fractions.Fraction


def rational(text):
    text = text.replace(",", "").replace(" ", "")
    numerator, _, denominator = text.partition("/")
    return F(decimal.Decimal(numerator)) / (F(decimal.Decimal(denominator)) if denominator else 1)


def terminates(value):
    d = value.denominator
    for p in (2, 5):
        while d % p == 0:
            d //= p
    return d == 1


def plain(d):
    return format(d.normalize(decimal.Context(prec=80)), "f")


def digits(value, places):
    context = decimal.Context(prec=places, rounding=decimal.ROUND_HALF_EVEN)
    return context.divide(decimal.Decimal(value.numerator), decimal.Decimal(value.denominator))


def expected(value):
    if value == 0:
        return "0"
    if terminates(value):
        exact = digits(value, 80)
        if len(exact.normalize(decimal.Context(prec=80)).as_tuple().digits) <= 40:
            return plain(exact)
    return plain(digits(value, 38))


def cases(directory):
    # (locale, unit, usage, input value, output unit, [integers], last, exact input)
    out = []
    for line in open(os.path.join(directory, "unitPreferencesTest.txt"), encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        f = [x.strip() for x in line.split(";")]
        _, usage, region, given, _, unit = f[:6]
        rest = f[6:]
        units = [rest[-1]]
        last = rational(rest[-3])
        integers = []
        pairs = rest[:-3]
        for i in range(0, len(pairs), 2):
            integers.append(int(pairs[i].replace(",", "")))
            units.insert(len(units) - 1, pairs[i + 1])
        out.append(("und-" + region, unit, usage, rational(given), "-and-".join(units),
                    integers, last))
    for line in open(os.path.join(directory, "unitLocalePreferencesTest.txt"), encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        f = [x.strip() for x in line.split("#")[0].split(";")]
        unit, given, usage, locale, output, value = f[:6]
        out.append((locale, unit, usage, rational(given), output, [], rational(value)))
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: check_unit_prefs.py <conform_unit_prefs> <testData/units directory>")
    tool, directory = sys.argv[1:]
    all_cases = cases(directory)
    lines = []
    for locale, unit, usage, given, *_ in all_cases:
        text = plain(digits(given, 80)) if terminates(given) else plain(digits(given, 40))
        lines.append("%s\t%s\t%s\t%s" % (locale, unit, usage, text))
    out = subprocess.run([tool], input="".join(l + "\n" for l in lines), capture_output=True,
                         text=True, check=True).stdout.splitlines()
    wrong = []
    for line, got, case in zip(lines, out, all_cases):
        _, _, _, given, unit, integers, last = case
        fields = got.split("\t")
        amounts = fields[5].split(" ") if len(fields) == 6 else []
        ok = len(fields) == 6 and fields[4] == unit and len(amounts) == len(integers) + 1
        ok = ok and all(a == str(i) for a, i in zip(amounts, integers))
        if ok and terminates(given):
            ok = amounts[-1] == expected(last)
        elif ok:
            ok = plain(digits(F(decimal.Decimal(amounts[-1])), 30)) == plain(digits(last, 30))
        if not ok:
            wrong.append("%s: expected %s %s %s, got %s" % (
                line.replace("\t", " "), unit, integers, expected(last), "\t".join(fields[4:])))
    for w in wrong[:20]:
        print(w)
    print("unit preferences: %d of %d cases agree" % (len(all_cases) - len(wrong), len(all_cases)))
    sys.exit(1 if wrong else 0)


if __name__ == "__main__":
    main()
