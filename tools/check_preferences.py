#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Compares the library's regional preferences with CLDR's JSON of the
# same release (timeData, weekData, measurementData of cldr-core; the
# JSON is an oracle, never an input). For every region the JSON names it
# asks for qaa-R (qaa has no likely subtags, so R is the region), for
# qaa with R as the -u-rg override, and for every language and region
# timeData lists (ca-ES); each answer must be R's values, a missing one
# being 001's.
#
# usage: check_preferences.py <conform_preferences> <CLDR JSON full dir>

import json
import os
import subprocess
import sys

FORMATS = ["K", "h", "H", "k", "hb", "hB"]
DAYS = ["mon", "tue", "wed", "thu", "fri", "sat", "sun"]
SYSTEMS = ["metric", "US", "UK"]
PAPERS = ["A4", "US-Letter"]


def load(root, name):
    path = os.path.join(root, "cldr-core", "supplemental", name + ".json")
    with open(path, encoding="utf-8") as f:
        return json.load(f)["supplemental"]


def plain(table):
    # Alternatives ("GB-alt-variant") are not the region's value.
    return {k: v for k, v in table.items() if "-alt-" not in k}


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: check_preferences.py <conform_preferences> <CLDR JSON full dir>")
    tool, root = sys.argv[1:]
    time = load(root, "timeData")["timeData"]
    week = load(root, "weekData")["weekData"]
    measure = load(root, "measurementData")["measurementData"]
    tables = {
        "min": plain(week["minDays"]),
        "first": plain(week["firstDay"]),
        "start": plain(week["weekendStart"]),
        "end": plain(week["weekendEnd"]),
        "system": plain(measure["measurementSystem"]),
        "paper": plain(measure["paperSize"]),
    }
    # A language and region is written "ca-ES" in the JSON.
    regions = {k for k in time if "-" not in k}
    for table in tables.values():
        regions |= set(table)

    def value(table, region):
        return table.get(region, table["001"])

    def expect(hours_key, region):
        hours = time.get(hours_key) or time.get(region) or time["001"]
        allowed = "".join(str(FORMATS.index(f)) for f in hours["_allowed"].split())
        return "\t".join([
            region,
            "%d %s" % (FORMATS.index(hours["_preferred"]), allowed),
            "%d %s %d %d" % (DAYS.index(value(tables["first"], region)) + 1,
                             value(tables["min"], region),
                             DAYS.index(value(tables["start"], region)) + 1,
                             DAYS.index(value(tables["end"], region)) + 1),
            "%d %d" % (SYSTEMS.index(value(tables["system"], region)),
                       PAPERS.index(value(tables["paper"], region))),
        ])

    cases = {}
    for region in sorted(regions):
        cases["qaa-" + region] = expect(None, region)
        if region.isalpha():
            cases["qaa-u-rg-%szzzz" % region.lower()] = expect(None, region)
    for key in sorted(k for k in time if "-" in k):
        language, region = key.split("-")
        cases[key] = expect(key, region)
    ids = "\n".join(cases) + "\n"
    run = subprocess.run([tool], input=ids, capture_output=True, text=True, check=True)
    differences = 0
    for line in run.stdout.splitlines():
        locale, got = line.split("\t", 1)
        if got != cases[locale]:
            differences += 1
            if differences <= 20:
                print("differs: %s: got %s, CLDR's JSON gives %s" % (locale, got, cases[locale]))
    print("preferences: %d of %d locales agree with CLDR's JSON"
          % (len(cases) - differences, len(cases)))
    sys.exit(1 if differences else 0)


main()
