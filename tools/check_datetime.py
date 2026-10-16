#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks mlocgen's semantic skeletons (record mloc-0042, UTS #35 "Mapping
# to Standard Skeletons") against CLDR's own test data: every case of
# common/testData/datetime/datetime.json that names a semantic skeleton
# must map to its classicalSkeleton, and the matcher must give a pattern;
# so must the check's own questions below. Given the formatter's tool and
# data files, every date case (field sets without a time, and the
# standard date formats alone) and every time case (the time field set,
# and the standard time formats alone) must also give the file's text.
#
# usage: check_datetime.py <mlocgen> <CLDR common directory>
#            [<conform_datetime> <dates.mloc> <spellout.mloc>]

import json
import os
import re
import subprocess
import sys

LENGTHS = {"long": 0, "medium": 1, "short": 2}
YEARS = {None: 0, "full": 1, "with_era": 2}
HOURS = {None: 0, "H11": 1, "H12": 2, "H23": 3, "H24": 4, "Clock12": 5, "Clock24": 6}
ZONES = {None: 0, "specific": 0, "generic": 1, "location": 2, "offset": 3}
# Questions the test data does not ask, with their answers by UTS #35's
# rules ("-" for a field set it does not define): other field sets,
# precisions and hour cycles, and the year styles of a two-digit year.
OWN = [
    ("en gregorian YM 1 0 0 0 0", "yMMM"),
    ("en gregorian YD 1 0 0 0 0", "-"),
    ("en gregorian YMT 1 0 0 2 0", "-"),
    ("en gregorian ME 1 0 0 0 0", "-"),
    ("en gregorian MDEX 1 0 0 0 0", "-"),
    ("en gregorian TZ 1 0 0 1 0", "jmz"),
    ("en gregorian T 0 0 0 0 0", "j"),
    ("en gregorian T 0 0 3 1 0", "Hm"),
    ("en gregorian T 2 0 1 5 0", "hmsSSS"),
    ("en gregorian DT 2 0 0 0 0", "dj"),
    ("en gregorian E 2 0 0 0 0", "EEEEE"),
    ("en gregorian Y 2 0 0 0 0", "yy"),
    ("en gregorian Y 2 1 0 0 0", "y"),
    ("en gregorian Y 2 2 0 0 0", "Gy"),
]

# A lone zone may leave the length out (medium); the test data's time is
# to the second.
PRECISION = 2


def question(case):
    locale = case["locale"].replace("-", "_")
    return " ".join([locale, case["calendar"], case["semanticSkeleton"],
                     str(LENGTHS[case.get("semanticSkeletonLength", "medium")]),
                     str(YEARS[case.get("yearStyle")]), str(HOURS[case.get("hourCycle")]),
                     str(PRECISION), str(ZONES[case.get("zoneStyle")])])


# The date cases' field sets (mlocDateFieldSet), year styles and
# standard styles, and the calendars' BCP 47 types.
FIELD_SETS = {"YMDE": 1, "M": 7}
STYLES = {"full": 1, "long": 2, "medium": 3, "short": 4}
BCP47 = {"gregorian": "gregory", "buddhist": "buddhist", "japanese": "japanese",
         "islamic-civil": "islamic-civil"}


def date_line(case):
    locale = "%s-u-ca-%s" % (case["locale"], BCP47[case["calendar"]])
    day = case["input"][:10]
    if "semanticSkeleton" in case:
        return "\t".join([locale, str(FIELD_SETS[case["semanticSkeleton"]]),
                          str(LENGTHS[case.get("semanticSkeletonLength", "medium")]),
                          str(YEARS[case.get("yearStyle")]), "0", day])
    return "\t".join([locale, "0", "0", "0", str(STYLES[case["dateLength"]]), day])


def check_dates(cases, tool, dates, spellout):
    """The formatted text of every date case; the count of differences."""
    chosen = [c for c in cases if c.get("semanticSkeleton") in FIELD_SETS or
              ("dateLength" in c and "timeLength" not in c)]
    asked = "".join(date_line(c) + "\n" for c in chosen)
    run = subprocess.run([tool, dates, spellout], input=asked, capture_output=True, text=True,
                         check=True)
    got = run.stdout.split("\n")[:len(chosen)]
    wrong = [(date_line(c), c["expected"], g) for c, g in zip(chosen, got) if g != c["expected"]]
    for line, want, have in wrong[:20]:
        print(f"{line}: want {want}, got {have}")
    print(f"{len(chosen) - len(wrong)} of {len(chosen)} dates agree")
    return len(wrong)


# Time cases whose text CLDR's own data and ICU4C 79 both give otherwise:
# th's abbreviated am and pm are "↑↑↑" in CLDR 49, root's "AM" and "PM"
# (as the JSON resolves them and ICU4C writes them), where the file
# expects th's wide names.
KNOWN_TIMES = {("th-TH", "AM"), ("th-TH", "PM")}


def known(case, got):
    return (case["locale"], got[-2:]) in KNOWN_TIMES and "\u0e40\u0e17\u0e35\u0e48\u0e22\u0e07" in case["expected"]


def time_line(case):
    locale = "%s-u-ca-%s" % (case["locale"], BCP47[case["calendar"]])
    found = re.search(r"T(\d\d):(\d\d)(?::(\d\d))?", case["input"])
    time = "%s:%s:%s.0" % (found.group(1), found.group(2), found.group(3) or "00")
    if "semanticSkeleton" in case:
        return " ".join(["T", locale, str(HOURS[case.get("hourCycle")]), "0", "0", time])
    return " ".join(["T", locale, "0", "0", str(STYLES[case["timeLength"]]), time])


def check_times(cases, tool, dates, spellout):
    """The formatted text of every time case; the count of differences."""
    chosen = [c for c in cases if c.get("semanticSkeleton") == "T" or
              ("timeLength" in c and "dateLength" not in c)]
    asked = "".join(time_line(c) + "\n" for c in chosen)
    run = subprocess.run([tool, dates, spellout], input=asked, capture_output=True, text=True,
                         check=True)
    got = run.stdout.split("\n")[:len(chosen)]
    wrong = [(time_line(c), c["expected"], g) for c, g in zip(chosen, got)
             if g != c["expected"] and not known(c, g)]
    listed = sum(1 for c, g in zip(chosen, got) if g != c["expected"] and known(c, g))
    for line, want, have in wrong[:20]:
        print(f"{line}: want {want}, got {have}")
    print(f"{len(chosen) - len(wrong) - listed} of {len(chosen)} times agree, "
          f"{listed} listed as known")
    return len(wrong)


def main():
    if len(sys.argv) not in (3, 6):
        sys.exit("usage: check_datetime.py <mlocgen> <CLDR common directory> "
                 "[<conform_datetime> <dates.mloc> <spellout.mloc>]")
    path = os.path.join(sys.argv[2], "testData", "datetime", "datetime.json")
    with open(path, encoding="utf-8") as f:
        every = json.load(f)
    cases = [c for c in every if "semanticSkeleton" in c]
    questions = [question(c) for c in cases] + [q for q, _ in OWN]
    wants = [c["classicalSkeleton"] for c in cases] + [w for _, w in OWN]
    asked = "".join(q + "\n" for q in questions)
    run = subprocess.run([sys.argv[1], "semantic", sys.argv[2]], input=asked,
                         capture_output=True, text=True, check=True)
    answers = run.stdout.split("\n")[:len(questions)]
    if len(answers) != len(questions):
        sys.exit(f"{len(answers)} answers to {len(questions)} questions")
    wrong = []
    for asked_line, want, answer in zip(questions, wants, answers):
        classical, _, pattern = answer.partition("\t")
        if classical != want or (want != "-" and not pattern):
            wrong.append((asked_line, want, answer))
    for asked_line, want, got in wrong[:20]:
        print(f"{asked_line}: want {want}, got {got}")
    print(f"{len(questions) - len(wrong)} of {len(questions)} semantic skeletons agree "
          f"({len(OWN)} of them the check's own)")
    dates = check_dates(every, *sys.argv[3:]) if len(sys.argv) == 6 else 0
    times = check_times(every, *sys.argv[3:]) if len(sys.argv) == 6 else 0
    sys.exit(1 if wrong or dates or times else 0)


if __name__ == "__main__":
    main()
