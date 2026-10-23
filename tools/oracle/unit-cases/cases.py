# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes cases.txt: "locale unit width case amount" lines for the locales
# whose files (or their parents') have case patterns, every unit en's long
# width names, compounds and mixed units, each case those files give and
# none ("-"), three amounts; a short-width line per unit as a control.
import os, sys, xml.etree.ElementTree as ET
main = sys.argv[1]
def cases_of(f):
    p = os.path.join(main, f + ".xml")
    if not os.path.exists(p):
        return set()
    out = set()
    for e in ET.parse(p).getroot().iter():
        c = e.get("case")
        if c and e.tag in ("unitPattern", "compoundUnitPattern1"):
            out.add(c)
    return out
locales = sorted(x[:-4] for x in os.listdir(main) if x.endswith(".xml") and cases_of(x[:-4]))
units = []
for ul in ET.parse(os.path.join(main, "en.xml")).getroot().iter("unitLength"):
    if ul.get("type") == "long":
        for u in ul.iter("unit"):
            t = u.get("type")
            units.append(t[t.index("-") + 1:])
compounds = ["square-meter", "cubic-meter", "kilometer-per-hour", "meter-per-second",
             "newton-meter", "kilowatt-hour", "square-kilometer", "cubic-decimeter",
             "kilonewton", "megawatt-hour", "square-millimeter", "cubic-kilometer",
             "meter-per-square-second", "joule-per-kilogram", "watt-per-square-meter",
             "decimeter", "hectometer", "micrometer", "kilosecond", "gram-per-cubic-centimeter",
             "milligram-per-deciliter", "square-foot", "cubic-inch", "kilogram-meter",
             "megajoule-per-square-meter", "liter-per-kilometer", "kilometer-per-liter",
             "square-centimeter-per-second", "pound-force-foot", "mile-per-gallon"]
mixed = ["hour-and-minute", "foot-and-inch", "meter-and-centimeter"]
amounts = ["1", "2", "5.5"]
with open("cases.txt", "w") as out:
    for loc in locales:
        chain = [loc]
        while "_" in chain[-1]:
            chain.append(chain[-1][:chain[-1].rindex("_")])
        cs = sorted(set().union(*(cases_of(c) for c in chain)))
        for u in units + compounds + mixed:
            out.write(f"{loc} {u} short {cs[0]} 2\n")
            for c in ["-"] + cs:
                for a in amounts:
                    out.write(f"{loc} {u} long {c} {a}\n")
