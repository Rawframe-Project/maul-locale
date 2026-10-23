# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes genders.txt: "locale unit long - 2" for the locales whose
# language grammaticalFeatures.xml gives genders, every unit English's long
# width names and the compounds of cases.py, plus per-second and powers.
import os, re, sys, xml.etree.ElementTree as ET
common = sys.argv[1]
g = ET.parse(os.path.join(common, "supplemental/grammaticalFeatures.xml")).getroot()
langs = set()
for f in g.iter("grammaticalFeatures"):
    if any(c.tag == "grammaticalGender" and c.get("values") for c in f):
        langs.update(f.get("locales").split())
main = os.path.join(common, "main")
locales = sorted(l for l in langs if os.path.exists(os.path.join(main, l + ".xml")))
units = []
for ul in ET.parse(os.path.join(main, "en.xml")).getroot().iter("unitLength"):
    if ul.get("type") == "long":
        for u in ul.iter("unit"):
            t = u.get("type")
            units.append(t[t.index("-") + 1:])
src = open(os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), "cases.py")).read()
compounds = re.findall(r'"([a-z0-9-]+)"', src[src.index("compounds = ["):src.index("mixed = [")])
extra = ["per-second", "square-second", "cubic-kilogram", "newton-per-square-meter",
         "kilowatt-hour-per-100-kilometer", "byte-per-second", "liter-per-second-meter",
         "megabyte-per-square-hour", "hectoliter", "kilocalorie-per-gram"]
with open("genders.txt", "w") as out:
    for loc in locales:
        for u in units + compounds + extra:
            out.write(f"{loc} {u} long - 2\n")
print(len(locales), locales)
