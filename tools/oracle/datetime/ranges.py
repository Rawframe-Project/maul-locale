# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Asks ICU4C for the ranges of test/date_ranges_icu.tsv ("date") or
# test/datetime_ranges_icu.tsv ("datetime"): reads the cases, makes each
# one's skeleton, has icu4c_intervals.cpp answer them and writes each case
# with ICU's text and spans, for merge.py.
#
# A date option's skeleton is the classical one mlocgen semantic gives
# for its fields, length and year style; a standard date style's is its
# pattern's fields. A date-time adds the time's skeleton as the interval
# generator names it (tools/mlocgen/interval_gen.c, TimeSkeleton): h or H
# by the clock, then nothing, m or ms by the precision, then the zone
# form's letters; a date in a zone (D) adds the zone's letters alone.
#   python3 -I ranges.py date|datetime CASES MLOCGEN CLDR_COMMON ICU_INTERVALS > output
import datetime
import subprocess
import sys

FIELDS = ["YMD", "YMDE", "MD", "MDE", "D", "DE", "E", "M", "Y", "YM"]
ZONES = ["", "zzzz", "z", "vvvv", "v", "VVVV", "VVV", "OOOO", "O"]
PRECISIONS = ["ms", "", "m"]
DATE_LETTERS = set("GyYuUrQqMLlwWEcedDFg")
CALENDARS = {"gregory": "gregorian", "ethioaa": "ethiopic-amete-alem"}


def time_skeleton(hour_cycle, precision, zone):
    return ("H" if hour_cycle == 6 else "h") + PRECISIONS[precision] + ZONES[zone]


def classical(mlocgen, common, queries):
    """The classical skeletons mlocgen semantic gives for the queries."""
    text = "".join(q + "\n" for q in queries)
    out = subprocess.run([mlocgen, "semantic", common], input=text, capture_output=True,
                         text=True, check=True).stdout.splitlines()
    if len(out) != len(queries):
        sys.exit("ranges.py: mlocgen semantic answered another number of lines")
    answers = {}
    for query, line in zip(queries, out):
        if line in ("-", "?"):
            sys.exit(f"ranges.py: no skeleton for {query}")
        # The date's letters alone; the time's and the zone's are added after.
        answers[query] = "".join(c for c in line.split("\t")[0] if c in DATE_LETTERS)
    return answers


def locale_and_calendar(tag):
    """A tag's CLDR locale id and calendar ("zh-Hant-u-ca-roc": zh_Hant, roc)."""
    base, _, calendar = tag.partition("-u-ca-")
    return base.replace("-", "_"), CALENDARS.get(calendar or "gregory", calendar)


def noon(iso):
    day = datetime.date.fromisoformat(iso)
    return int(datetime.datetime(day.year, day.month, day.day, 12,
                                 tzinfo=datetime.timezone.utc).timestamp())


def main():
    mode, cases_path, mlocgen, common, icu = sys.argv[1:6]
    cases = [l for l in open(cases_path, encoding="utf-8").read().splitlines() if l]
    rows = []
    for case in cases:
        f = case.split("\t")
        if mode == "date":
            fields, length, year, style = (int(x) for x in f[1].split())
            locale, calendar = locale_and_calendar(f[0])
            query = None if style else f"{locale} {calendar} {FIELDS[fields]} {length} {year} 0 0 0"
            rows.append((f[0], "UTC", noon(f[2]), noon(f[3]), query, f"date:{style - 1}", ""))
        else:
            fields, length, year, hour_cycle, precision, zone = (int(x) for x in f[2].split())
            locale = f[1].replace("-", "_")
            # The date as semantic gives it beside a time or a zone (a weekday
            # alone at the short length is EEE there, EEEEE alone).
            with_time = "Z" if f[0] == "D" else "T"
            query = f"{locale} gregorian {FIELDS[fields]}{with_time} {length} {year} 0 0 0"
            rest = ZONES[zone] if f[0] == "D" else time_skeleton(hour_cycle, precision, zone)
            rows.append((f[1], f[3], f[4], f[5], query, None, rest))
    queries = sorted({r[4] for r in rows if r[4] is not None})
    skeletons = classical(mlocgen, common, queries)
    lines = []
    for locale, zone, start, end, query, standard, rest in rows:
        skeleton = skeletons[query] + rest if query is not None else standard
        lines.append(f"{locale}\t{zone}\t{start}\t{end}\t{skeleton}\n")
    answers = subprocess.run([icu], input="".join(lines), capture_output=True, text=True,
                             check=True).stdout.splitlines()
    if len(answers) != len(cases):
        sys.exit("ranges.py: icu4c_intervals answered another number of lines")
    for case, answer in zip(cases, answers):
        sys.stdout.write(case + "\t" + answer + "\n")


main()
