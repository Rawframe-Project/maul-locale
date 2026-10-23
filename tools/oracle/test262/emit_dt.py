# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes the PlainDateTime rows t262cal.py recorded (through
# datetime262.py) as maul-locale's test/test262_datetime.inc: one C
# string a row.
#
# usage: python3 -I emit_dt.py <rows.json> <test262 commit> <out.inc>

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rowkind  # noqa: E402

KEYS = [("year", "y"), ("month", "m"), ("monthCode", "mc"), ("day", "d"), ("era", "era"),
        ("eraYear", "ey"), ("hour", "h"), ("minute", "mi"), ("second", "s"),
        ("millisecond", "ms"), ("microsecond", "us"), ("nanosecond", "ns"),
        ("years", "dy"), ("months", "dm"), ("weeks", "dw"), ("days", "dd"), ("hours", "dh"),
        ("minutes", "dmi"), ("seconds", "ds"), ("milliseconds", "dms"),
        ("microseconds", "dus"), ("nanoseconds", "dns")]
ORDER = {"make": 0, "add": 1, "round": 2, "until": 3, "since": 4}


def field(v):
    return "-" if v is None or v == "" else str(v)


def origin_parts(o):
    t, cal = o["dt"], o["calendar"]
    if t == "make":
        return ["make", cal] + [str(v) for v in o["iso"] + o["t"]]
    if t == "add":
        return ["add", cal] + [str(v) for v in o["at"] + o["d"]] + [o["overflow"][0]]
    if t == "round":
        return ["round", cal] + [str(v) for v in o["at"] + o["r"]]
    return [t, cal] + [str(v) for v in o["one"] + o["two"] + o["r"]]


def expectations(cal, e):
    if "refused" in e:
        if len(e) != 1:
            raise SystemExit("refused and fields together")
        return ["refused"]
    out = []
    for name, short in KEYS:
        if name not in e:
            continue
        if cal == "iso8601" and name in ("era", "eraYear"):
            continue
        out.append("%s=%s" % (short, field(e[name])))
    known = {name for name, _ in KEYS}
    if any(name not in known for name in e):
        raise SystemExit("unknown expectation in %r" % (e,))
    return out


def main():
    data = json.load(open(sys.argv[1]))
    if data["failures"]:
        raise SystemExit("%d failures; resolve them first" % len(data["failures"]))
    commit, out = sys.argv[2], sys.argv[3]
    rows = []
    files = set()
    for k, r in data["rows"].items():
        o = json.loads(k)
        if rowkind.kind(o) != "dt":
            continue
        parts = expectations(o["calendar"], r["expect"])
        if not parts:
            continue
        files.update(r["files"])
        line = origin_parts(o) + ["="] + parts
        if any(" " in p or '"' in p or "\\" in p for p in line):
            raise SystemExit("unexpected character in " + k)
        rows.append((ORDER[o["dt"]], k, line))
    rows.sort()
    lines = [" ".join(p) for _, _, p in rows]
    checks = sum(len(l.split(" = ", 1)[1].split()) for l in lines)
    head = [
        "// SPDX-License-Identifier: BSD-3-Clause",
        "// Copyright (C) Ecma International; the test262 authors",
        "//",
        "// Generated from test262 at " + commit + " (github.com/tc39/test262); do not edit.",
        "// The license is in LICENSES/BSD-3-Clause-test262.txt.",
        "//",
        "// Every assertion test262's Temporal.PlainDateTime tests make about a date-time",
        "// or a difference of two, one row each, after the calendar: 'make', an ISO",
        "// date (year, month, day) and time fields (hour to nanosecond); 'add', a",
        "// date-time (mlocDate's day, hour, minute, second, nanoseconds past the",
        "// second), a duration (years, months, weeks, days, seconds, nanoseconds)",
        "// and the overflow (c or r); 'round', a date-time, the time unit (0 day to",
        "// 6 nanosecond), the increment and the mode; 'until' or 'since', two",
        "// date-times, the largest and smallest units (0 year to 9 nanosecond), the",
        "// increment and the mode as test262 gives it (since negates it). Then '='",
        "// and 'refused', or the fields checked: y, m, mc, d, era, ey of the date ('-'",
        "// for none); h, mi, s, ms, us, ns of the time; dy, dm, dw, dd, dh, dmi, ds,",
        "// dms, dus, dns of a duration (since's as until gives them; each as the",
        "// float64 Temporal keeps).",
        "//",
        "// %d rows, %d checks, from %d files:" % (len(lines), checks, len(files)),
    ]
    head += ["// " + f for f in sorted(files)]
    head.append("")
    head += ['"%s",' % l for l in lines]
    with open(out, "w") as f:
        f.write("\n".join(head) + "\n")


if __name__ == "__main__":
    main()
