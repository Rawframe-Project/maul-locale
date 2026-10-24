#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks that the data format version moves with the layouts it covers.
# tools/data-format.txt records MLOC_DATA_FORMAT_VERSION and the SHA-256
# of each header that lays out data the library reads (the patterns on
# its "layouts" line). A header added, removed or changed since fails the
# check until someone decides: a change of layout increases the format in
# include/maul-locale/base.h; then --write records the hashes again. The
# hashes cannot see a validator that accepts more or less;
# docs/internals/data.md#versions lists those changes.
#
# usage: tools/check_data_format.py [--write]

import glob
import hashlib
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
RECORD = os.path.join(ROOT, "tools", "data-format.txt")
BASE = os.path.join(ROOT, "include", "maul-locale", "base.h")


def library_format():
    with open(BASE, encoding="utf-8") as f:
        m = re.search(r"^#define MLOC_DATA_FORMAT_VERSION (0x[0-9A-Fa-f]+)u$", f.read(), re.M)
    if m is None:
        sys.exit("check_data_format: no MLOC_DATA_FORMAT_VERSION in base.h")
    return int(m.group(1), 16)


def read_record():
    comments, patterns, fmt, hashes = [], [], None, {}
    with open(RECORD, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("#"):
                comments.append(line)
                continue
            fields = line.split()
            if not fields:
                continue
            if fields[0] == "format" and len(fields) == 2:
                fmt = int(fields[1], 16)
            elif fields[0] == "layouts":
                patterns = fields[1:]
            elif fields[0] == "sha256" and len(fields) == 3:
                hashes[fields[2]] = fields[1]
            else:
                sys.exit("check_data_format: a line it does not read: " + line)
    if fmt is None or not patterns:
        sys.exit("check_data_format: the record names no format or no layouts")
    return comments, patterns, fmt, hashes


def current(patterns):
    paths = set()
    for pattern in patterns:
        found = glob.glob(os.path.join(ROOT, pattern))
        if not found:
            sys.exit("check_data_format: no file matches " + pattern)
        paths.update(os.path.relpath(p, ROOT).replace(os.sep, "/") for p in found)
    out = {}
    for path in sorted(paths):
        with open(os.path.join(ROOT, path), "rb") as f:
            out[path] = hashlib.sha256(f.read()).hexdigest()
    return out


def main():
    if sys.argv[1:] not in ([], ["--write"]):
        sys.exit("usage: tools/check_data_format.py [--write]")
    comments, patterns, fmt, hashes = read_record()
    now = current(patterns)
    own = library_format()
    if sys.argv[1:] == ["--write"]:
        with open(RECORD, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(comments) + "\n")
            f.write("format 0x%08X\n" % own)
            f.write("layouts " + " ".join(patterns) + "\n")
            for path, digest in now.items():
                f.write("sha256 %s %s\n" % (digest, path))
        print("data format 0x%08X: %d layout headers recorded" % (own, len(now)))
        return
    problems = []
    if fmt != own:
        problems.append("base.h's format 0x%08X is not the recorded 0x%08X" % (own, fmt))
    for path in sorted(set(now) | set(hashes)):
        if path not in hashes:
            problems.append(path + " is new")
        elif path not in now:
            problems.append(path + " is gone")
        elif now[path] != hashes[path]:
            problems.append(path + " changed")
    if problems:
        for p in problems:
            print("data format: " + p)
        print("If a layout or what a validator accepts changed, increase "
              "MLOC_DATA_FORMAT_VERSION in include/maul-locale/base.h; either way, record "
              "the layouts again with tools/check_data_format.py --write.")
        sys.exit(1)
    print("data format 0x%08X: %d layout headers as recorded" % (own, len(now)))


if __name__ == "__main__":
    main()
