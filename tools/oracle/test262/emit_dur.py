# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes the Duration rows t262cal.py recorded (through dur262.py) as
# maul-locale's test/test262_duration.inc: one C string a row.
#
# usage: python3 -I emit_dur.py <rows.json> <test262 commit> <out.inc>

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rowkind  # noqa: E402

DURATION_KEYS = [("years", "dy"), ("months", "dm"), ("weeks", "dw"), ("days", "dd"),
                 ("hours", "dh"), ("minutes", "dmi"), ("seconds", "ds"), ("milliseconds", "dms"),
                 ("microseconds", "dus"), ("nanoseconds", "dns")]
ORDER = {"round": 0, "total": 1, "add": 2, "compare": 3}


def text(s):
    # A token as C string text, as emit_zone.py writes it.
    if " " in s or not s:
        raise SystemExit("an empty token or a space in " + s)
    out = []
    for b in s.encode("utf-8"):
        c = chr(b)
        out.append(c if 0x21 <= b <= 0x7E and c not in '"\\' else "\\%03o" % b)
    return "".join(out)


def rel_parts(rel):
    if rel[0] == "none":
        return ["none"]
    if rel[0] == "plain":
        return ["plain", rel[1], str(rel[2])]
    return ["zoned", text(rel[1]), rel[2]] + [str(v) for v in rel[3]]


def origin_parts(o):
    t = o["dur"]
    if t == "add":
        return ["add"] + [str(v) for v in o["one"] + o["two"]] + [str(o["largest"])]
    if t == "compare":
        return ["compare"] + rel_parts(o["rel"]) + [str(v) for v in o["one"] + o["two"]]
    parts = [t] + rel_parts(o["rel"]) + [str(v) for v in o["d"]]
    if t == "total":
        return parts + [str(o["unit"])]
    return parts + [str(v) for v in o["r"]]


def expectations(e):
    if "refused" in e:
        if len(e) != 1:
            raise SystemExit("refused and fields together")
        return ["refused"]
    out = []
    if "total" in e:
        out.append("total=" + e["total"])
    if "c" in e:
        out.append("c=%d" % e["c"])
    out += ["%s=%d" % (short, e[name]) for name, short in DURATION_KEYS if name in e]
    if len(out) != len(e):
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
        if rowkind.kind(o) != "dur":
            continue
        files.update(r["files"])
        rows.append((ORDER[o["dur"]], k, origin_parts(o) + ["="] + expectations(r["expect"])))
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
        "// Every assertion test262's Temporal.Duration tests make about rounding,",
        "// totals, sums and comparisons of durations, one row each. A duration is its",
        "// internal record: years, months, weeks, days, then a time duration as",
        "// seconds and nanoseconds (the days folded into it, as 24 hours, without a",
        "// start or from a plain one). A start is 'none'; 'plain', a calendar and a",
        "// day in mlocDate's count (midnight); or 'zoned', a zone, a calendar and an",
        "// instant (seconds since 1970 and nanoseconds, floored). 'round', a start, a",
        "// duration, the largest and smallest units (0 year to 9 nanosecond), the",
        "// increment and the mode; 'total', a start, a duration and the unit; 'add',",
        "// two time durations and the largest unit; 'compare', a start and two",
        "// durations. Then '=' and 'refused', or what is checked: total, the float64",
        "// total; c, the comparison; dy, dm, dw, dd, dh, dmi, ds, dms, dus, dns of a",
        "// duration (each as the float64 Temporal keeps). Bytes beyond printable",
        "// ASCII are octal escapes.",
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
