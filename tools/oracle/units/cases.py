# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes the oracle's lines: cases.txt (amounts), ranges.txt and
# names.txt, from the currency oracle's locales and units.txt (CLDR 49's
# unit types without their category, one a line); ccases.txt (amounts of
# the compound units of cunits.txt, most of them unnamed in CLDR);
# mcases.txt (amounts of the mixed units of mixed.txt); ucases.txt and
# urcases.txt (amounts and ranges for each usage of usage.txt).
import sys

here = sys.argv[1] if len(sys.argv) > 1 else "."
locales = open(f"{here}/../currency/locales.txt").read().split()
units = open(f"{here}/units.txt").read().split()
values = ["0", "1", "2", "3", "5", "11", "21", "100", "1.5", "0.5", "1234.567", "-1"]
pairs = [("1", "2"), ("3", "5"), ("0", "1"), ("2", "2"), ("1.5", "3"), ("-5", "5")]
widths = ["short", "narrow", "long"]
with open("cases.txt", "w") as f, open("ranges.txt", "w") as r, open("names.txt", "w") as n:
    for loc in locales:
        for unit in units:
            for width in widths:
                n.write(f"{loc} {unit} {width}\n")
                for v in values:
                    f.write(f"{loc} {unit} {width} {v}\n")
                for a, b in pairs:
                    r.write(f"{loc} {unit} {width} {a} {b}\n")
compounds = open(f"{here}/cunits.txt").read().split()
with open("ccases.txt", "w") as c:
    for loc in locales:
        for unit in compounds:
            for width in widths:
                for v in ["0", "1", "2", "5", "1.5"]:
                    c.write(f"{loc} {unit} {width} {v}\n")
mixed = open(f"{here}/mixed.txt").read().split()
mvalues = ["0", "1", "1.5", "2.25", "5.5", "10.75", "-3.5", "1234.5", "0.125", "100"]
with open("mcases.txt", "w") as m:
    for loc in locales:
        for unit in mixed:
            for width in widths:
                for v in mvalues:
                    m.write(f"{loc} {unit} {width} {v}\n")
usages = [row.split() for row in open(f"{here}/usage.txt")]
uvalues = ["0", "0.25", "1", "3.7", "12", "99.5", "250", "1234.5", "100000", "-2.5"]
ulocales = locales + ["en_US", "pt_BR", "en-US-u-ms-metric", "en-GB-u-mu-fahrenhe"]
with open("ucases.txt", "w") as u:
    for loc in ulocales:
        for row in usages:
            for usage in row[2:]:
                for width in widths:
                    for v in uvalues:
                        u.write(f"{loc} {row[0]} {usage} {width} {v}\n")
rlocales = ["en_US", "en_GB", "en_CA", "de", "fr", "ja", "ar", "es", "ru", "hi", "pt_BR", "zh"]
rrows = [("meter", "road"), ("meter", "default"), ("meter", "person-height"), ("kilogram", "person"),
         ("celsius", "weather"), ("liter", "fluid"), ("meter-per-second", "wind"),
         ("second", "media"), ("square-meter", "land"), ("liter-per-100-kilometer", "vehicle-fuel")]
pairs = [("10", "20"), ("10", "2000"), ("2000", "3000"), ("100", "100"), ("100", "101"),
         ("-5", "5"), ("0", "1"), ("1.7", "1.8"), ("50", "5000")]
with open("urcases.txt", "w") as r:
    for loc in rlocales:
        for unit, usage in rrows:
            for width in widths:
                for a, b in pairs:
                    r.write(f"{loc} {unit} {usage} {width} {a} {b}\n")
