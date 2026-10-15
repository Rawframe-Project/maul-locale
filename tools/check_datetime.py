#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks mlocgen's semantic skeletons (record mloc-0042, UTS #35 "Mapping
# to Standard Skeletons") against CLDR's own test data: every case of
# common/testData/datetime/datetime.json that names a semantic skeleton
# must map to its classicalSkeleton, and the matcher must give a pattern.
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
    asked = "".join(question(c) + "\n" for c in cases)
    run = subprocess.run([sys.argv[1], "semantic", sys.argv[2]], input=asked,
                         capture_output=True, text=True, check=True)
    answers = run.stdout.split("\n")[:len(cases)]
    if len(answers) != len(cases):
        sys.exit(f"{len(answers)} answers to {len(cases)} cases")
    wrong = []
    for case, answer in zip(cases, answers):
        classical, _, pattern = answer.partition("\t")
        if classical != case["classicalSkeleton"] or not pattern:
            wrong.append((question(case), case["classicalSkeleton"], answer))
    for asked_line, want, got in wrong[:20]:
        print(f"{asked_line}: want {want}, got {got}")
    print(f"{len(cases) - len(wrong)} of {len(cases)} semantic skeletons agree")
    sys.exit(1 if wrong else 0)


if __name__ == "__main__":
    main()
