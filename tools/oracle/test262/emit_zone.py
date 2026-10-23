# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes the zone rows t262cal.py recorded as maul-locale's
# test/test262_zone.inc: one C string a row. The tests that loop over
# Intl.supportedValuesOf("timeZone") are left out; the replay test runs
# their loops over the library's list itself.
#
# usage: python3 -I emit_zone.py <rows.json> <test262 commit> <out.inc>

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rowkind  # noqa: E402

LIST_FILES = {
    "test/intl402/Temporal/ZonedDateTime/prototype/equals/canonical-not-equal.js",
    "test/intl402/Temporal/ZonedDateTime/supported-values-of.js",
    "test/intl402/Temporal/ZonedDateTime/prototype/getTimeZoneTransition/transition-at-instant-boundaries.js",
}
ORDER = {"find": 0, "id": 1, "equal": 2, "offset": 3, "next": 4, "previous": 5}


def text(s):
    # A token as C string text: printable ASCII but space, quote and
    # backslash as itself, every other byte as a three-digit octal escape.
    if " " in s:
        raise SystemExit("a space in " + s)
    out = []
    for b in s.encode("utf-8"):
        c = chr(b)
        out.append(c if 0x21 <= b <= 0x7E and c not in '"\\' else "\\%03o" % b)
    return "".join(out)


def line(o, e):
    z = o["zone"]
    if z == "find":
        if e != {"refused": 1}:
            raise SystemExit("find row without refused")
        return ["refused", text(o["id"])]
    if z == "id":
        return ["id", text(o["id"]), "=", text(e["id"])]
    if z == "equal":
        return ["equal", text(o["one"]), text(o["two"]), "=", str(e["equal"])]
    if z == "offset":
        return ["offset", text(o["id"]), o["ns"], "=", str(e["offset"])]
    at = e["at"]
    return [z, text(o["id"]), o["ns"], "=", "none" if at is None else at]


def main():
    data = json.load(open(sys.argv[1]))
    if data["failures"]:
        raise SystemExit("%d failures; resolve them first" % len(data["failures"]))
    commit, out = sys.argv[2], sys.argv[3]
    rows = []
    files = set()
    for k, r in data["rows"].items():
        kept = [f for f in r["files"] if f not in LIST_FILES]
        if not kept:
            continue
        o = json.loads(k)
        if rowkind.kind(o) != "zone":
            continue
        if len(r["expect"]) != 1:
            raise SystemExit("row with %d expectations: %s" % (len(r["expect"]), k))
        files.update(kept)
        rows.append((ORDER[o["zone"]], k, line(o, r["expect"])))
    rows.sort()
    lines = [" ".join(parts) for _, _, parts in rows]
    head = [
        "// SPDX-License-Identifier: BSD-3-Clause",
        "// Copyright (C) Ecma International; the test262 authors",
        "//",
        "// Generated from test262 at " + commit + " (github.com/tc39/test262); do not edit.",
        "// The license is in LICENSES/BSD-3-Clause-test262.txt.",
        "//",
        "// Every assertion test262's Temporal.ZonedDateTime tests make about a time",
        "// zone, one row each: 'refused' and an identifier no zone has; 'id', an",
        "// identifier, '=' and the identifier the zone keeps; 'equal', two",
        "// identifiers, '=' and whether their zones are equal (1) or not (0);",
        "// 'offset', an identifier, an instant in nanoseconds since 1970, '=' and",
        "// the zone's offset then in nanoseconds; 'next' or 'previous', an",
        "// identifier, an instant, '=' and the zone's transition after or before it",
        "// in nanoseconds, or 'none'. Bytes beyond printable ASCII are octal escapes.",
        "//",
        "// %d rows from %d files:" % (len(lines), len(files)),
    ]
    head += ["// " + f for f in sorted(files)]
    head.append("")
    head += ['"%s",' % l for l in lines]
    with open(out, "w") as f:
        f.write("\n".join(head) + "\n")


if __name__ == "__main__":
    main()
