# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes the PlainTime rows t262cal.py recorded (through time262.py) as
# maul-locale's test/test262_time.inc: one C string a row.
#
# usage: python3 -I emit_time.py <rows.json> <test262 commit> <out.inc>

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rowkind  # noqa: E402

TIME_KEYS = [("hour", "h"), ("minute", "mi"), ("second", "s"), ("millisecond", "ms"),
             ("microsecond", "us"), ("nanosecond", "ns")]
DURATION_KEYS = [("hours", "dh"), ("minutes", "dmi"), ("seconds", "ds"), ("milliseconds", "dms"),
                 ("microseconds", "dus"), ("nanoseconds", "dns")]
ORDER = {"make": 0, "add": 1, "round": 2, "until": 3, "since": 4}


def origin_parts(o):
    t = o["time"]
    if t == "make":
        return ["make", "r" if o["overflow"] == "reject" else "c"] + [str(v) for v in o["in"]]
    if t == "add":
        return ["add"] + [str(v) for v in o["t"] + o["d"]]
    if t == "round":
        return ["round"] + [str(v) for v in o["t"] + o["r"]]
    return [t] + [str(v) for v in o["one"] + o["two"] + o["r"]]


def expectations(e):
    if "refused" in e:
        if len(e) != 1:
            raise SystemExit("refused and fields together")
        return ["refused"]
    out = ["%s=%d" % (short, e[name]) for name, short in TIME_KEYS + DURATION_KEYS if name in e]
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
        if rowkind.kind(o) != "time":
            continue
        files.update(r["files"])
        rows.append((ORDER[o["time"]], k, origin_parts(o) + ["="] + expectations(r["expect"])))
    rows.sort()
    lines = [" ".join(p) for _, _, p in rows]
    checks = sum(len(l.split("=", 1)[1].split()) for l in lines)
    head = [
        "// SPDX-License-Identifier: BSD-3-Clause",
        "// Copyright (C) Ecma International; the test262 authors",
        "//",
        "// Generated from test262 at " + commit + " (github.com/tc39/test262); do not edit.",
        "// The license is in LICENSES/BSD-3-Clause-test262.txt.",
        "//",
        "// Every assertion test262's Temporal.PlainTime tests make about a time of day",
        "// or a difference of two, one row each: 'make', the overflow (c constrain,",
        "// r reject) and the fields in (hour, minute, second, millisecond,",
        "// microsecond, nanosecond); 'add', a time (hour, minute, second, nanoseconds",
        "// past the second) and a time duration (seconds, nanoseconds); 'round', a",
        "// time, the unit (0 day to 6 nanosecond), the increment and the mode; 'until'",
        "// or 'since', two times, the largest and smallest units, the increment and",
        "// the mode as test262 gives it (since negates it). Then '=' and 'refused', or",
        "// the fields checked: h, mi, s, ms, us, ns of a time; dh, dmi, ds, dms, dus,",
        "// dns of a duration (since's as until gives them, before negation).",
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
