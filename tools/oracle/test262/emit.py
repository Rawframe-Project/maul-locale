# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes the rows t262cal.py recorded as maul-locale's
# test/test262_calendar.inc: one C string a row.
#
# usage: python3 -I emit.py <rows.json> <test262 commit> <out.inc>

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rowkind  # noqa: E402

KEYS = [("year", "y"), ("month", "m"), ("monthCode", "mc"), ("day", "d"), ("era", "era"),
        ("eraYear", "ey"), ("inLeapYear", "leap"), ("monthsInYear", "miy"),
        ("daysInMonth", "dim"), ("daysInYear", "diy"), ("dayOfYear", "doy"),
        ("years", "dy"), ("months", "dm"), ("weeks", "dw"), ("days", "dd"),
        ("referenceYear", "ry"), ("referenceDay", "rd")]
UNIT_NAMES = ["year", "month", "week", "day"]


def field(v):
    return "-" if v is None or v == "" else str(v)


def overflow(o):
    return "r" if o["overflow"] == "reject" else "c"


def given(f):
    # '-' for a field not given, '~' for one given as the empty string.
    return ["~" if f.get(k) == "" else field(f.get(k))
            for k in ("era", "eraYear", "year", "monthCode", "month", "day")]


def base_origin(o):
    if "monthday" in o:
        return ["monthday", overflow(o)] + given(o["monthday"])
    if "iso" in o:
        y, m, d = o["iso"]
        return ["r", "iso", str(y), str(m), str(d)]
    return [overflow(o), field(o["era"]), field(o["eraYear"]), field(o["year"]),
            field(o["monthCode"]), str(o["month"]), str(o["day"])]


def canonical(o):
    return json.dumps(o, sort_keys=True)


def expectations(cal, e):
    if "refused" in e:
        if len(e) != 1:
            raise SystemExit("refused and fields together")
        return ["refused"]
    parts = []
    for name, short in KEYS:
        if name not in e:
            continue
        if cal == "iso8601" and name in ("era", "eraYear"):
            continue
        parts.append("%s=%s" % (short, field(e[name])))
    return parts


def lines_of(rows):
    # Every date a row's date was made from is a row too, before it, so a
    # row names the row it adds to by its index.
    nodes = {}
    depths = {}

    # Returns a node's key, walking each node once and its depth with it.
    def walk(o):
        key = canonical(o)
        if key in nodes:
            return key
        parents = [o["of"]] if "of" in o else o["until"] if "until" in o else []
        depths[key] = 1 + max(depths[walk(p)] for p in parents) if parents else 0
        nodes[key] = o
        return key

    for key in rows:
        if rowkind.kind(json.loads(key)) == "calendar":
            walk(json.loads(key))
    order = sorted(nodes, key=lambda k: (depths[k], k))
    index = {k: i for i, k in enumerate(order)}
    out = []
    for k in order:
        o = nodes[k]
        cal = o["calendar"]
        if "until" in o:
            smallest, increment, mode = o.get("round", [3, 1, "trunc"])
            parts = [cal, "@%d" % index[canonical(o["until"][0])], "until",
                     "@%d" % index[canonical(o["until"][1])], UNIT_NAMES[o["unit"]],
                     UNIT_NAMES[smallest], str(increment), mode]
        elif "with" in o:
            parts = [cal, "@%d" % index[canonical(o["of"])], "with"] + given(o["with"]) + [overflow(o)]
        elif "of" in o:
            parts = [cal, "@%d" % index[json.dumps(o["of"], sort_keys=True)], "add"] + \
                [str(x) for x in o["add"]] + [overflow(o)]
        else:
            parts = [cal] + base_origin(o)
        parts.append("=")
        parts += expectations(cal, rows[k]["expect"]) if k in rows else []
        for p in parts:
            if " " in p or '"' in p or "\\" in p:
                raise SystemExit("unexpected character in " + k)
        out.append(" ".join(parts))
    return out


def main():
    data = json.load(open(sys.argv[1]))
    if data["failures"]:
        raise SystemExit("%d failures; resolve them first" % len(data["failures"]))
    commit, out = sys.argv[2], sys.argv[3]
    lines = lines_of(data["rows"])
    files = sorted({f for r in data["rows"].values() for f in r["files"]})
    text = [
        "// SPDX-License-Identifier: BSD-3-Clause",
        "// Copyright (C) Ecma International; the test262 authors",
        "//",
        "// Generated from test262 at " + commit + " (github.com/tc39/test262); do not edit.",
        "// The license is in LICENSES/BSD-3-Clause-test262.txt.",
        "//",
        "// Every assertion test262's Temporal calendar tests make about a date made from",
        "// fields, one row a date: the calendar, the overflow (c constrain, r reject),",
        "// the fields in (era, era year, year, month code, month, day; '-' for none, month",
        "// 0 for none) or 'iso' and an ISO date; or '@' and the index of an earlier row",
        "// whose date a duration is added to: 'add', its years, months, weeks and days",
        "// and its overflow; or '@', an index, 'until', '@' and another index, the",
        "// largest and smallest units, the increment and the rounding mode: the",
        "// rounded duration between those rows' dates; or '@', an index, 'with', the",
        "// fields given (era, era year, year, month code, month, day; '-' for not given,",
        "// '~' for the empty string)",
        "// and the overflow: that row's date with those fields changed; or 'monthday',",
        "// the overflow and the fields given: a month and day's reference date (ry its",
        "// year in the ISO calendar, rd a date's day of the month there). Then '=' and what",
        "// test262 expects of the row's date or duration (nothing for a date only made",
        "// on the way):",
        "// 'refused', or the fields it checks (y year, m month, mc month code, d day,",
        "// era, ey era year, leap in a leap year, miy months in the year, dim days in",
        "// the month, diy days in the year, doy day of the year; dy, dm, dw and dd a",
        "// duration's years, months, weeks and days; '-' for none).",
        "// Calendar iso8601 is the Gregorian calendar without eras.",
        "//",
        "// %d rows, %d checks, from %d files:" % (len(lines), sum(len(l.split("=", 1)[1].split()) for l in lines), len(files)),
    ]
    text += ["// " + f for f in files]
    text.append("")
    text += ['"%s",' % l for l in lines]
    with open(out, "w") as f:
        f.write("\n".join(text) + "\n")


if __name__ == "__main__":
    main()
