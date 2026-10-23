# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Merges the rows of several t262cal.py runs into one file, refusing
# conflicting expectations.
#
# usage: python3 -I merge_rows.py <out.json> <rows.json>...

import json
import sys


def main():
    out = {"failures": [], "files": [], "rows": {}}
    for path in sys.argv[2:]:
        data = json.load(open(path))
        out["failures"] += data["failures"]
        out["files"] += data["files"]
        for k, r in data["rows"].items():
            row = out["rows"].setdefault(k, {"expect": {}, "files": []})
            for prop, v in r["expect"].items():
                if row["expect"].get(prop, v) != v:
                    out["failures"].append({"origin": k, "prop": prop,
                                            "conflict": [row["expect"][prop], v]})
                row["expect"][prop] = v
            row["files"] += [f for f in r["files"] if f not in row["files"]]
    with open(sys.argv[1], "w") as f:
        json.dump(out, f)


if __name__ == "__main__":
    main()
