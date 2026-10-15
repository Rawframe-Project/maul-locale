#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks the library's local times (record mloc-0041) against Python's
# zoneinfo reading zic's fat TZif files for the same tzdb source. For
# every zone, zdump -v lists the changes of offset from 1800 to 2500;
# around each one conform_local is asked about the local times at the
# edges of its gap or fold and in its middle, the start of those days,
# and the local time at the change and a second before. zoneinfo gives
# the possible instants (a local time's instant under each fold, kept
# when the instant's offset agrees), the disambiguations as ECMA-262
# writes them (in a gap, the time moved by the difference of the two
# folds' offsets), the start of a day and the local time at an instant.
#
# usage: check_local.py <conform_local> <tzdb source directory>

import datetime
import os
import subprocess
import sys
import tempfile
import zoneinfo

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_zones import FILES, MONTHS, seconds  # noqa: E402

FIRST, END = 1800, 2500
EPOCH = datetime.datetime(1970, 1, 1)
UTC_EPOCH = datetime.datetime(1970, 1, 1, tzinfo=datetime.timezone.utc)


def changes(path):
    # Each change of offset: its instant and the offsets before and after.
    out = subprocess.run(["zdump", "-v", "-c", f"{FIRST},{END}", path], capture_output=True,
                         text=True, check=True).stdout
    rows = []
    for line in out.splitlines():
        if " UT = " not in line or "NULL" in line:
            continue
        left = line.split(" UT = ")[0].split()
        h, m, s = map(int, left[-2].split(":"))
        t = seconds(int(left[-1]), MONTHS[left[-4]], int(left[-3]), h, m, s)
        rows.append((t, int(line.split("gmtoff=")[1])))
    pairs = [(b[0], a[1], b[1]) for a, b in zip(rows[::2], rows[1::2])]
    return [p for p in pairs if p[1] != p[2]]


class Zone:
    def __init__(self, path, name):
        with open(path, "rb") as f:
            self.tz = zoneinfo.ZoneInfo.from_file(f, key=name)

    def offset_at(self, t):
        utc = UTC_EPOCH + datetime.timedelta(seconds=t)
        return int(utc.astimezone(self.tz).utcoffset().total_seconds())

    def folds(self, local):
        d = EPOCH + datetime.timedelta(seconds=local)
        return [int(d.replace(fold=f, tzinfo=self.tz).utcoffset().total_seconds()) for f in (0, 1)]

    def possible(self, local):
        return sorted({local - o for o in self.folds(local) if self.offset_at(local - o) == o})

    def instant_for(self, local, mode):
        p = self.possible(local)
        if len(p) == 1:
            return p[0]
        if p:
            return None if mode == "reject" else p[-1] if mode == "later" else p[0]
        if mode == "reject":
            return None
        before, after = self.folds(local)
        shift = after - before
        if mode == "earlier":
            return self.possible(local - shift)[0]
        return self.possible(local + shift)[-1]

    def start_of_day(self, local):
        midnight = local - local % 86400
        p = self.possible(midnight)
        if p:
            return p[0]
        # The instant ending the gap: the first second with the later
        # offset, between the two folds' instants.
        before, after = self.folds(midnight)
        low, high = midnight - after, midnight - before
        while high - low > 1:
            middle = (low + high) // 2
            if self.offset_at(middle) == after:
                high = middle
            else:
                low = middle
        return high


def questions(zone, pairs):
    asked = []
    for t, before, after in pairs:
        for local in sorted({t + before - 1, t + before, t + after - 1, t + after,
                             t + (before + after) // 2}):
            asked.append(("at", local))
            asked.append(("day", local))
        asked.append(("local", t - 1))
        asked.append(("local", t))
    return asked


def expected(zone, kind, value):
    if kind == "at":
        p = zone.possible(value)
        modes = [zone.instant_for(value, m) for m in ("compatible", "earlier", "later", "reject")]
        fields = [str(len(p)), str(p[0] if p else 0), str(p[-1] if p else 0)]
        return " ".join(fields + ["-" if m is None else str(m) for m in modes])
    if kind == "day":
        return " " + str(zone.start_of_day(value))
    return str(value + zone.offset_at(value))


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: check_local.py <conform_local> <tzdb source directory>")
    conform, source = sys.argv[1], sys.argv[2]
    failures = 0
    total = 0
    with tempfile.TemporaryDirectory() as compiled:
        subprocess.run(["zic", "-b", "fat", "-d", compiled] + FILES, cwd=source, check=True)
        names = sorted(subprocess.run(["sh", "-c", "awk '$1 == \"Zone\" { print $2 }' " +
                                       " ".join(FILES)], cwd=source, capture_output=True,
                                      text=True, check=True).stdout.split())
        lines = []
        wants = []
        for name in names:
            path = os.path.join(compiled, name)
            zone = Zone(path, name)
            lines.append(f"zone {name}")
            for kind, value in questions(zone, changes(path)):
                lines.append(f"{kind} {value}")
                wants.append((name, kind, value, expected(zone, kind, value)))
        got = subprocess.run([conform], input="\n".join(lines) + "\n", capture_output=True,
                             text=True, check=True).stdout.splitlines()
        if len(got) != len(wants):
            sys.exit(f"conform_local answered {len(got)} of {len(wants)} questions")
        for (name, kind, value, want), have in zip(wants, got):
            total += 1
            if have != want:
                failures += 1
                if failures <= 20:
                    print(f"{name} {kind} {value}: zoneinfo {want!r}, library {have!r}")
    print(f"{total} questions over {len(names)} zones from {FIRST} to {END}")
    if failures:
        sys.exit(f"{failures} answers differ from zoneinfo")


if __name__ == "__main__":
    main()
