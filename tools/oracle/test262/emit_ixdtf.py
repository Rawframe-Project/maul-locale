# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes the strings test262's Temporal tests expect a parser to refuse
# and the strings they expect each toString to write, as t262cal.py
# recorded them (through ixdtf262.py and str262.py), as maul-locale's
# test/test262_ixdtf.inc: one C string a row.
#
# usage: python3 -I emit_ixdtf.py <test262 commit> <out.inc> <rows.json>...

import json
import sys

ORDER = {"dt": 0, "offset": 1, "zone": 2, "calendar": 3, "duration": 4}
WRITERS = {"date": "wdate", "ym": "wym", "md": "wmd", "time": "wtime", "dt": "wdt", "inst": "winst",
           "zdt": "wzdt", "dur": "wdur", "offset": "woffset"}


def writer_row(o, r):
    if set(r["expect"]) != {"text"}:
        raise SystemExit("unexpected expectation %r for %r" % (r["expect"], o))
    values = ["-" if v is None else str(v) for v in o["v"]]
    options = [str(v) for v in o["o"]]
    text = r["expect"]["text"].encode("utf-8").hex()
    return " ".join([WRITERS[o["str"]]] + values + options + ["=", "x" + text])


def main():
    commit, out = sys.argv[1], sys.argv[2]
    rows = {}
    files = set()
    for path in sys.argv[3:]:
        data = json.load(open(path))
        if data["failures"]:
            raise SystemExit("%s: %d failures; resolve them first" % (path, len(data["failures"])))
        for k, r in data["rows"].items():
            o = json.loads(k)
            if "str" in o:
                files.update(r["files"])
                line = writer_row(o, r)
                rows[(10 + list(WRITERS).index(o["str"]), 0, line)] = line
                continue
            if "ixdtf" not in o:
                continue
            if r["expect"] != {"refused": 1}:
                raise SystemExit("unexpected expectation %r for %s" % (r["expect"], k))
            files.update(r["files"])
            text = o["text"].encode("utf-8", "surrogatepass").hex()
            parts = [o["ixdtf"]] + ([str(o["formats"])] if o["ixdtf"] == "dt" else []) + ["x" + text]
            rows[(ORDER[o["ixdtf"]], o.get("formats", 0), text)] = " ".join(parts) + " = refused"
    lines = [rows[k] for k in sorted(rows)]
    head = [
        "// SPDX-License-Identifier: BSD-3-Clause",
        "// Copyright (C) Ecma International; the test262 authors",
        "//",
        "// Generated from test262 at " + commit + " (github.com/tc39/test262); do not edit.",
        "// The license is in LICENSES/BSD-3-Clause-test262.txt.",
        "//",
        "// Every string test262's Temporal tests expect a parser to refuse, one row",
        "// each: the parser ('dt' with the goals as mlocTemporalFormat bits,",
        "// 'offset', 'zone', 'calendar' or 'duration'), x and the string's bytes in",
        "// hexadecimal, then '= refused'. Then every string they expect a toString to",
        "// write: the writer and its value ('wdate', 'wym' or 'wmd', a day in",
        "// mlocDate's count; 'wtime', hour, minute, second and nanosecond; 'wdt', a",
        "// day and a time; 'winst', seconds since 1970, nanoseconds and a zone or '-';",
        "// 'wzdt', a zone, seconds and nanoseconds; 'wdur', the ten fields; 'woffset',",
        "// nanoseconds), the options (calendar, precision with -1 auto and -2 minute,",
        "// show calendar, show time zone, hide offset; none for an offset), then '='",
        "// and x with the string's bytes in hexadecimal.",
        "//",
        "// %d rows, %d checks, from %d files:" % (len(lines), len(lines), len(files)),
    ]
    head += ["// " + f for f in sorted(files)]
    head.append("")
    head += ['"%s",' % l for l in lines]
    with open(out, "w") as f:
        f.write("\n".join(head) + "\n")


if __name__ == "__main__":
    main()
