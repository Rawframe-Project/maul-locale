#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks mlocgen's semantic skeletons (record mloc-0042, UTS #35 "Mapping
# to Standard Skeletons") against CLDR's own test data: every case of
# common/testData/datetime/datetime.json that names a semantic skeleton
# must map to its classicalSkeleton, and the matcher must give a pattern;
# so must the check's own questions below.
#
# usage: check_datetime.py <mlocgen> <CLDR common directory>

import json
import os
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


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: check_datetime.py <mlocgen> <CLDR common directory>")
    path = os.path.join(sys.argv[2], "testData", "datetime", "datetime.json")
    with open(path, encoding="utf-8") as f:
        cases = [c for c in json.load(f) if "semanticSkeleton" in c]
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
    sys.exit(1 if wrong else 0)


if __name__ == "__main__":
    main()
