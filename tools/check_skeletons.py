#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks mlocgen's skeleton matching (UTS #35 "Matching Skeletons")
# against ICU4C 79's DateTimePatternGenerator: every question of
# test/skeletons_icu.tsv must get ICU's pattern, or the one
# tools/skeleton-exceptions.txt gives where mlocgen follows CLDR's
# inheritance and ICU's data build does not. An exception that no longer
# differs from ICU is reported too, so the list stays exact.
#
# usage: check_skeletons.py <mlocgen> <CLDR common directory> <oracle> <exceptions>

import subprocess
import sys


def read_table(path):
    """(locale, calendar, skeleton) -> pattern, comments skipped, in order."""
    rows = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 4:
                sys.exit(f"{path}: not four fields: {line.rstrip()}")
            rows[tuple(fields[:3])] = fields[3]
    return rows


def main():
    if len(sys.argv) != 5:
        sys.exit("usage: check_skeletons.py <mlocgen> <CLDR common directory> <oracle> "
                 "<exceptions>")
    oracle = read_table(sys.argv[3])
    exceptions = read_table(sys.argv[4])
    questions = list(oracle)
    asked = "".join("\t".join(q) + "\n" for q in questions)
    run = subprocess.run([sys.argv[1], "skeletons", sys.argv[2]], input=asked,
                         capture_output=True, text=True, check=True)
    answers = run.stdout.split("\n")[:len(questions)]
    if len(answers) != len(questions):
        sys.exit(f"{len(answers)} answers to {len(questions)} questions")
    wrong = []
    stale = []
    for question, got in zip(questions, answers):
        icu = oracle[question]
        want = exceptions.get(question, icu)
        if got != want:
            wrong.append((question, want, got))
        elif question in exceptions and got == icu:
            stale.append(question)
    for question, want, got in wrong[:20]:
        print(f"{' '.join(question)}: want {want}, got {got}")
    for question in stale[:20]:
        print(f"{' '.join(question)}: listed as an exception but equal to ICU's")
    agree = len(questions) - len(wrong)
    print(f"{agree} of {len(questions)} patterns agree ({len(exceptions)} by the exceptions)")
    sys.exit(1 if wrong or stale else 0)


if __name__ == "__main__":
    main()
