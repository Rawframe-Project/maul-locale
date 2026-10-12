#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks the library's unit conversion against CLDR's unitsTest.txt (the
# file is an oracle, never an input). Each line gives a unit, its base
# unit and the conversion as a fraction ("5/9 * x + 2,298.35/9"); from
# them every unit converts to its base unit, to every other unit of its
# quantity, and to every unit of the reciprocal quantity
# ("consumption-inverse"), exactly. The library must give each value
# exactly when it has at most 40 significant digits, else rounded half
# even to 38, as mlocConvertUnit's default precision says. The file's
# own rounded column is checked against the exact values too.
#
# usage: check_units.py <conform_units> <unitsTest.txt>

import decimal
import fractions
import re
import subprocess
import sys

F = fractions.Fraction
VALUES = ["1000", "1", "0.001", "-12.5", "123456789.0123456789"]
BEAUFORT = [0, 3, 16, 34, 55, 80, 108, 139, 172, 208, 245, 285, 327, 369, 414, 461, 511,
            558, 614]


def rational(text):
    text = text.replace(",", "").replace(" ", "")
    numerator, _, denominator = text.partition("/")
    return F(decimal.Decimal(numerator)) / (F(decimal.Decimal(denominator)) if denominator else 1)


def parse(conversion):
    # "a * x" or "a * x + b" with a and b fractions.
    if conversion.startswith("special:beaufort"):
        return None
    m = re.fullmatch(r"(.*)\*x(?:\+(.*))?", conversion.replace(" ", ""))
    return rational(m.group(1)), rational(m.group(2)) if m.group(2) else F(0)


def from_beaufort(x):
    force = min(int(abs(x) + F(1, 2)), len(BEAUFORT) - 2)
    return F(BEAUFORT[force] + BEAUFORT[force + 1], 20)


def to_beaufort(v):
    force = 0
    for i in range(1, len(BEAUFORT) - 1):
        if abs(v) * 10 >= BEAUFORT[i]:
            force = i
    return F(force)


def expected(value):
    # The library's default precision: exact when the value has at most 40
    # significant digits, else 38 rounded half even.
    if value == 0:
        return "0"
    d = value.denominator
    for p in (2, 5):
        while d % p == 0:
            d //= p
    context = decimal.Context(prec=60, rounding=decimal.ROUND_HALF_EVEN)
    if d == 1:
        exact = context.divide(decimal.Decimal(value.numerator), decimal.Decimal(value.denominator))
        if len(exact.normalize(context).as_tuple().digits) <= 40:
            return plain(exact)
    context = decimal.Context(prec=38, rounding=decimal.ROUND_HALF_EVEN)
    return plain(context.divide(decimal.Decimal(value.numerator),
                                decimal.Decimal(value.denominator)))


def plain(d):
    text = format(d.normalize(decimal.Context(prec=80)), "f")
    return text


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: check_units.py <conform_units> <unitsTest.txt>")
    tool, path = sys.argv[1:]
    units = {}
    for line in open(path, encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        quantity, source, base, conversion, result = [f.strip() for f in line.split(";")]
        units[source] = (quantity, base, parse(conversion), result)
    cases = {}
    rounded = []
    for source, (quantity, base, conversion, result) in units.items():
        x = F(1000)
        v = from_beaufort(x) if conversion is None else conversion[0] * x + conversion[1]
        cases["1000\t%s\t%s" % (source, base)] = expected(v)
        # The file's rounded column: seven significant digits, or the
        # digits it shows when it shows fewer.
        shown = decimal.Decimal(result.replace(",", ""))
        magnitude = decimal.Decimal(v.numerator).__abs__() / decimal.Decimal(v.denominator)
        unit = max(F(10) ** shown.as_tuple().exponent, F(10) ** (magnitude.adjusted() - 6))
        if v != 0 and abs(F(shown) - v) > unit / 2:
            rounded.append(source)
    for a, (qa, base_a, ca, _) in units.items():
        for b, (qb, base_b, cb, _) in units.items():
            reciprocal = qa + "-inverse" == qb or qb + "-inverse" == qa
            if base_a != base_b and not reciprocal:
                continue
            if reciprocal and (ca is None or cb is None):
                continue
            for text in VALUES:
                x = F(decimal.Decimal(text))
                v = from_beaufort(x) if ca is None else ca[0] * x + ca[1]
                if reciprocal:
                    if v == 0:
                        continue
                    v = 1 / v
                v = to_beaufort(v) if cb is None else (v - cb[1]) / cb[0]
                cases["%s\t%s\t%s" % (text, a, b)] = expected(v)
    lines = "".join(k + "\n" for k in cases)
    out = subprocess.run([tool], input=lines, capture_output=True, text=True, check=True).stdout
    got = dict(line.rsplit("\t", 1) for line in out.splitlines())
    wrong = [k for k in cases if got.get(k) != cases[k]]
    for k in wrong[:20]:
        print("%s: expected %s, got %s" % (k.replace("\t", " "), cases[k], got.get(k)))
    for source in rounded:
        print("%s: the file's rounded column disagrees with its fraction" % source)
    print("units: %d of %d conversions agree" % (len(cases) - len(wrong), len(cases)))
    sys.exit(1 if wrong or rounded else 0)


if __name__ == "__main__":
    main()
