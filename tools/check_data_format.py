#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks that each format version moves with the layouts it covers.
# tools/data-format.txt records MLOC_DATA_FORMAT_VERSION and the SHA-256
# of each header that lays out data the library reads (the patterns on
# its "layouts" line), and, for each asset format (docs/internals/
# data.md#asset-formats), its header, the constant there that holds its
# format version, that version and the header's SHA-256. A header added,
# removed or changed since fails the check until someone decides: a
# change of a data layout increases the format in
# include/maul-locale/base.h, a change of an asset layout its own version;
# then --write records them again. Each data layout header carries MARKER
# and each asset layout header ASSET_MARKER naming its constant, which
# that header or another sets; a header that lays out both (a hash both
# kinds store) carries both. A header of src/ that carries one must be
# recorded, so a new layout header cannot go unwatched. The hashes cannot see a validator that accepts more or
# less; docs/internals/data.md#versions lists those changes.
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
MARKER = "// A data layout: tools/check_data_format.py watches this header."
ASSET_MARKER = re.compile(
    r"^// An asset layout: tools/check_data_format\.py watches this header with (\w+)\.$", re.M
)


def library_format():
    with open(BASE, encoding="utf-8") as f:
        m = re.search(r"^#define MLOC_DATA_FORMAT_VERSION (0x[0-9A-Fa-f]+)u$", f.read(), re.M)
    if m is None:
        sys.exit("check_data_format: no MLOC_DATA_FORMAT_VERSION in base.h")
    return int(m.group(1), 16)


def read_record():
    comments, patterns, fmt, hashes, assets = [], [], None, {}, {}
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
            elif fields[0] == "asset" and len(fields) == 5:
                assets[fields[1]] = (fields[2], int(fields[3]), fields[4])
            else:
                sys.exit("check_data_format: a line it does not read: " + line)
    if fmt is None or not patterns:
        sys.exit("check_data_format: the record names no format or no layouts")
    return comments, patterns, fmt, hashes, assets


def text_of(path):
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        return f.read()


def digest(path):
    with open(os.path.join(ROOT, path), "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def headers():
    for header in sorted(glob.glob(os.path.join(ROOT, "src", "*.h"))):
        yield os.path.relpath(header, ROOT).replace(os.sep, "/")


def asset_constant(path):
    m = ASSET_MARKER.search(text_of(path))
    return m.group(1) if m else None


# The data layouts: the headers the patterns match, but one that lays out
# an asset alone, which a pattern such as src/*_tables.h may also match.
def current(patterns):
    paths = set()
    for pattern in patterns:
        found = glob.glob(os.path.join(ROOT, pattern))
        if not found:
            sys.exit("check_data_format: no file matches " + pattern)
        paths.update(os.path.relpath(p, ROOT).replace(os.sep, "/") for p in found)
    return {path: digest(path) for path in sorted(paths)
            if asset_constant(path) is None or MARKER in text_of(path).splitlines()}


# A version constant's value, from the header of src/ that sets it.
def version_of(constant):
    found = []
    for path in headers():
        m = re.search(r"\b%s = (\d+)\b" % re.escape(constant), text_of(path))
        if m is not None:
            found.append(int(m.group(1)))
    if len(found) != 1:
        sys.exit("check_data_format: %s is set %d times in src/" % (constant, len(found)))
    return found[0]


# The asset layouts: each header with the asset marker, its constant, the
# version it has and the header's hash.
def current_assets():
    out = {}
    for path in headers():
        constant = asset_constant(path)
        if constant is not None:
            out[path] = (constant, version_of(constant), digest(path))
    return out


def marker_problems(listed):
    problems = [path + " is listed but lacks the marker" for path in listed
                if MARKER not in text_of(path).splitlines()]
    for path in headers():
        if path not in listed and MARKER in text_of(path).splitlines():
            problems.append(path + " carries the marker but is not listed")
    return problems


def write(comments, patterns, own, now, assets):
    with open(RECORD, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(comments) + "\n")
        f.write("format 0x%08X\n" % own)
        f.write("layouts " + " ".join(patterns) + "\n")
        for path, sha in now.items():
            f.write("sha256 %s %s\n" % (sha, path))
        for path, (constant, version, sha) in assets.items():
            f.write("asset %s %s %d %s\n" % (path, constant, version, sha))
    print("data format 0x%08X: %d layout headers and %d asset layouts recorded"
          % (own, len(now), len(assets)))


def asset_problems(recorded, assets):
    problems = []
    for path in sorted(set(recorded) | set(assets)):
        if path not in recorded:
            problems.append(path + " is a new asset layout")
        elif path not in assets:
            problems.append(path + " is no longer an asset layout")
        else:
            constant, version, sha = assets[path]
            old_constant, old_version, old_sha = recorded[path]
            if sha != old_sha and version == old_version:
                problems.append("%s changed, and %s is still %d" % (path, constant, version))
            elif (constant, version, sha) != (old_constant, old_version, old_sha):
                problems.append("%s changed, %s now %d" % (path, constant, version))
    return problems


def main():
    if sys.argv[1:] not in ([], ["--write"]):
        sys.exit("usage: tools/check_data_format.py [--write]")
    comments, patterns, fmt, hashes, recorded_assets = read_record()
    now = current(patterns)
    assets = current_assets()
    own = library_format()
    unmarked = marker_problems(now)
    if unmarked:
        for p in unmarked:
            print("data format: " + p)
        print("Every header that lays out data carries the line '%s' and is listed in "
              "tools/data-format.txt." % MARKER)
        sys.exit(1)
    if sys.argv[1:] == ["--write"]:
        write(comments, patterns, own, now, assets)
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
    problems += asset_problems(recorded_assets, assets)
    if problems:
        for p in problems:
            print("data format: " + p)
        print("If a data layout or what a validator accepts changed, increase "
              "MLOC_DATA_FORMAT_VERSION in include/maul-locale/base.h; if an asset layout "
              "changed, increase its format version; either way, record the layouts again with "
              "tools/check_data_format.py --write.")
        sys.exit(1)
    print("data format 0x%08X: %d layout headers and %d asset layouts as recorded"
          % (own, len(now), len(assets)))


if __name__ == "__main__":
    main()
