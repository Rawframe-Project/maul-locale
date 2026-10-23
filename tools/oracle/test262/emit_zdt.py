# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes the ZonedDateTime arithmetic rows t262cal.py recorded (through
# zdt262.py) as maul-locale's test/test262_zoned.inc: one C string a row.
#
# usage: python3 -I emit_zdt.py <rows.json> <test262 commit> <out.inc>

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rowkind  # noqa: E402


def text(s):
    # A token as C string text, as emit_zone.py writes it: printable ASCII
    # but space, quote and backslash as itself, every other byte as a
    # three-digit octal escape.
    if " " in s or not s:
        raise SystemExit("an empty token or a space in " + s)
    out = []
    for b in s.encode("utf-8"):
        c = chr(b)
        out.append(c if 0x21 <= b <= 0x7E and c not in '"\\' else "\\%03o" % b)
    return "".join(out)

DURATION_KEYS = [("years", "dy"), ("months", "dm"), ("weeks", "dw"), ("days", "dd"),
                 ("hours", "dh"), ("minutes", "dmi"), ("seconds", "ds"), ("milliseconds", "dms"),
                 ("microseconds", "dus"), ("nanoseconds", "dns")]
ORDER = {"offset": 0, "start": 1, "add": 2, "round": 3, "until": 4, "since": 5}


def origin_parts(o):
    t, zone = o["zdt"], text(o["id"])
    if t == "offset":
        return ["offset", zone] + [str(v) for v in o["dt"]] + [
            str(o["offset"]), o["option"], o["dis"], str(o["minutes"])]
    if t == "start":
        return ["start", zone, str(o["day"])]
    if t == "add":
        return ["add", zone, o["calendar"]] + [str(v) for v in o["at"] + o["d"]] + [
            o["overflow"][0]]
    if t == "round":
        return ["round", zone] + [str(v) for v in o["at"] + o["r"]]
    return [t, zone, o["calendar"]] + [str(v) for v in o["one"] + o["two"] + o["r"]]


def expectations(e):
    if "refused" in e:
        if len(e) != 1:
            raise SystemExit("refused and fields together")
        return ["refused"]
    out = ["ns=" + e["ns"]] if "ns" in e else []
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
        if rowkind.kind(o) != "zdt":
            continue
        files.update(r["files"])
        rows.append((ORDER[o["zdt"]], k, origin_parts(o) + ["="] + expectations(r["expect"])))
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
        "// Every assertion test262's Temporal.ZonedDateTime tests make about the",
        "// arithmetic of zoned date-times, one row each; an instant is its seconds",
        "// since 1970 and the nanoseconds past them (floored), a day mlocDate's",
        "// count, a zone its identifier: 'offset', a zone, a local date-time (day,",
        "// hour, minute, second, nanosecond), an offset in nanoseconds, the offset",
        "// option, the disambiguation and whether minutes match; 'start', a zone and",
        "// a day; 'add', a zone, a calendar, an instant, a duration (years, months,",
        "// weeks, days, seconds, nanoseconds) and the overflow's initial; 'round', a",
        "// zone, an instant, the unit (0 day to 6 nanosecond), the increment and the",
        "// mode; 'until' or 'since', a zone, a calendar, two instants, the largest and",
        "// smallest units (0 year to 9 nanosecond), the increment and the mode as",
        "// test262 gives it (since negates it). Then '=' and 'refused', or what is",
        "// checked: ns, the result's nanoseconds since 1970; dy, dm, dw, dd, dh, dmi,",
        "// ds, dms, dus, dns of a duration (since's as until gives them; the time",
        "// balanced to the largest unit or hours, each as the float64 Temporal keeps).",
        "// Bytes beyond printable ASCII are octal escapes.",
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
