# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes cases.txt: "locale unit amount | h m s", the amount of the
# unit's largest part split exactly (fields a unit lacks are "-").
import sys
from fractions import Fraction
from decimal import Decimal
here = sys.argv[1]
locales = open(f"{here}/../currency/locales.txt").read().split()
units = {"hour-and-minute": "hm", "hour-and-minute-and-second": "hms",
         "minute-and-second": "ms", "hour-and-second": "hs"}
amounts = ["0", "1", "1.5", "1.5083", "25.25", "1234.5", "0.0025", "100.75", "-1.5", "3.0125"]
def text(f):
    d = Decimal(f.numerator) / Decimal(f.denominator)
    s = format(d.normalize(), "f")
    return s
with open("cases.txt", "w") as out:
    for loc in locales:
        for unit, fields in units.items():
            for a in amounts:
                x = Fraction(a)
                sign = -1 if x < 0 else 1
                x = abs(x)
                vals = {"h": "-", "m": "-", "s": "-"}
                big = fields[0]
                scale = {"hm": [60], "hms": [60, 60], "ms": [60], "hs": [3600]}[fields]
                parts = list(fields)
                rest = x
                for i, p in enumerate(parts[:-1]):
                    whole = rest.numerator // rest.denominator
                    vals[p] = str(whole * (sign if i == 0 else 1))
                    rest = (rest - whole) * scale[i]
                vals[parts[-1]] = text(rest)
                out.write(f"{loc} {unit} {a} | {vals['h']} {vals['m']} {vals['s']}\n")
