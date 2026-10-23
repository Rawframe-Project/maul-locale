#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks the compiled zones against zic and zdump on the same tzdb source:
# zic compiles the default build's files into fat TZif files (transitions
# listed through 2037, then the TZ string), and zdump -v gives each zone's
# transitions from 1800 to 2500 and in the last years of Temporal's range.
# conform_zones prints the library's. Every change of offset must match in
# time and offset, and every change of the daylight flag too, except in
# the zones whose source has negative savings: there the library keeps
# tzdb's rearguard daylight periods, where zdump shows the main form's.
# Given a zones file (mlocgen zonesfile), the library reads its zones from
# it, and its release must be the source's.
#
# usage: check_zones.py <conform_zones> <tzdb source directory> [zones file]

import os
import subprocess
import sys
import tempfile

FILES = ["africa", "antarctica", "asia", "australasia", "europe", "northamerica",
         "southamerica", "etcetera", "backward"]
REARGUARD = {"Africa/Casablanca", "Africa/El_Aaiun", "Africa/Windhoek", "Europe/Dublin",
             "Europe/Prague"}
SPANS = [(1800, 2500), (275700, 275760)]
MONTHS = {m: i for i, m in enumerate(["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
                                      "Sep", "Oct", "Nov", "Dec"], 1)}


def seconds(year, month, day, h, m, s):
    # Days from 1970-01-01 on the proleptic Gregorian calendar, for years
    # past Python's datetime.
    year -= month <= 2
    era = year // 400
    yoe = year - era * 400
    doy = (153 * ((month + 9) % 12) + 2) // 5 + day - 1
    days = era * 146097 + yoe * 365 + yoe // 4 - yoe // 100 + doy - 719468
    return days * 86400 + h * 3600 + m * 60 + s


def library(conform, first, end, extra):
    zones = {}
    rows = None
    out = subprocess.run([conform, str(first), str(end)] + extra, capture_output=True, text=True,
                         check=True).stdout
    for line in out.splitlines():
        f = line.split()
        if f[0] == "release":
            zones[None] = f[1]
        elif f[0] == "zone":
            rows = zones[f[1]] = [(None, int(f[2]) + int(f[3]), f[3] != "0")]
        else:
            rows.append((int(f[1]), int(f[2]) + int(f[3]), f[3] != "0"))
    return zones


def zdump(path, first, end):
    out = subprocess.run(["zdump", "-v", "-c", f"{first},{end}", path], capture_output=True,
                         text=True, check=True).stdout
    rows = []
    for line in out.splitlines():
        if " UT = " not in line or "NULL" in line:
            continue
        left = line.split(" UT = ")[0].split()
        h, m, s = map(int, left[-2].split(":"))
        t = seconds(int(left[-1]), MONTHS[left[-4]], int(left[-3]), h, m, s)
        dst = line.split("isdst=")[1].split()[0] == "1"
        rows.append((t, int(line.split("gmtoff=")[1]), dst))
    # zdump -v prints each transition as its last second before and its
    # first second.
    return [(b[0], b[1], b[2], a[1], a[2]) for a, b in zip(rows[::2], rows[1::2])]


def changes(rows, flags):
    out = []
    for (_, o0, d0), (t1, o1, d1) in zip(rows, rows[1:]):
        if o1 != o0 or (flags and d1 != d0):
            out.append((t1, o1, d1 if flags else None))
    return out


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit("usage: check_zones.py <conform_zones> <tzdb source directory> [zones file]")
    conform, source = sys.argv[1], sys.argv[2]
    extra = sys.argv[3:]
    with open(os.path.join(source, "version")) as f:
        release = f.read().strip()
    failures = 0
    with tempfile.TemporaryDirectory() as compiled:
        subprocess.run(["zic", "-b", "fat", "-d", compiled] + FILES, cwd=source, check=True)
        for first, end in SPANS:
            zones = library(conform, first, end, extra)
            if zones.pop(None) != release:
                sys.exit(f"the library's zones are not tzdb {release}")
            for name, rows in sorted(zones.items()):
                flags = name not in REARGUARD
                want = [(t, o, d if flags else None)
                        for t, o, d, po, pd in zdump(os.path.join(compiled, name), first, end)
                        if o != po or (flags and d != pd)]
                got = changes(rows, flags)
                if want != got:
                    failures += 1
                    pair = next(((a, b) for a, b in zip(want, got) if a != b), None)
                    print(f"{name} {first}-{end}: zdump {len(want)} changes, library {len(got)};"
                          f" first difference {pair}")
            print(f"{len(zones)} zones from {first} to {end}")
    if failures:
        sys.exit(f"{failures} zones differ from zdump")


if __name__ == "__main__":
    main()
