#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks time zones by BCP 47 id and Windows name (record mloc-0041)
# against a reading of CLDR's bcp47/timezone.xml and
# supplemental/windowsZones.xml of its own: every id, in its own case and
# upper case, finds the zone of its identifier (a deprecated id its
# replacement's; "unk" none); every identifier an id lists gives that id
# and the Windows name its id is mapped to; every offset zone of whole
# hours from -12 to +14 gives its Etc/GMT id, other offsets none; and
# every Windows name finds, in each region it maps and in 001, a region
# it does not map and none, the first identifier of the mapping. An
# identifier CLDR lists that the tzdb source has no Zone or Link for is
# no zone at all.
#
# usage: check_zone_codes.py <conform_zone_codes> <CLDR common directory>
#                            <tzdb source directory>

import subprocess
import sys
import xml.etree.ElementTree as ET

FILES = ["africa", "antarctica", "asia", "australasia", "europe", "northamerica",
         "southamerica", "etcetera", "backward"]


def read_tzdb(directory):
    """Every Zone and Link name of the source, lower case."""
    names = set()
    for name in FILES:
        with open(f"{directory}/{name}", encoding="utf-8") as f:
            for line in f:
                fields = line.split("#", 1)[0].split()
                if fields[:1] == ["Zone"]:
                    names.add(fields[1].lower())
                elif fields[:1] == ["Link"]:
                    names.add(fields[2].lower())
    return names


def read_codes(common):
    """The ids: id -> (identifier or None, preferred id), and identifier -> id."""
    root = ET.parse(f"{common}/bcp47/timezone.xml").getroot()
    key = next(k for k in root.iter("key") if k.get("name") == "tz")
    codes, owner = {}, {}
    for t in key.iter("type"):
        name = t.get("name")
        if t.get("deprecated") == "true":
            codes[name] = (None, t.get("preferred"))
            continue
        aliases = t.get("alias").split()
        codes[name] = (t.get("iana") or aliases[0], name)
        for alias in aliases:
            owner[alias.lower()] = name
    return codes, owner


def read_windows(common):
    """The mappings: Windows name -> {region: identifiers}."""
    root = ET.parse(f"{common}/supplemental/windowsZones.xml").getroot()
    names = {}
    for m in root.iter("mapZone"):
        names.setdefault(m.get("other"), {})[m.get("territory")] = m.get("type").split()
    return names


def questions(codes, owner, windows, tzdb):
    """(question, expected answer) pairs."""
    named = {}
    for name, regions in windows.items():
        for ids in regions.values():
            for identifier in ids:
                named[owner[identifier.lower()]] = name
    for code, (identifier, preferred) in codes.items():
        want = "-" if code == "unk" else codes[preferred][0]
        yield f"b {code}", want
        yield f"b {code.upper()}", want
    yield "b nosuch", "-"
    for alias, code in owner.items():
        if code != "unk":
            yield f"z {alias}", f"{code}|{named.get(code, '-')}" if alias in tzdb else "?"
    for hours in range(-23, 24):
        offset = f"{'-' if hours < 0 else '+'}{abs(hours):02}:00"
        code = "utc" if hours == 0 else f"utc{'w' if hours < 0 else 'e'}{abs(hours):02}"
        want = f"{code}|{named.get(code, '-')}" if -12 <= hours <= 14 else "-|-"
        yield f"z {offset}", want
    yield "z +05:30", "-|-"
    for name, regions in windows.items():
        world = regions["001"][0]
        for region, ids in regions.items():
            yield f"w {region} {name}", ids[0]
            yield f"w {region.lower()} {name.upper()}", ids[0]
        yield f"w - {name}", world
        if "QO" not in regions:
            yield f"w QO {name}", world
    yield "w - No Such Standard Time", "-"


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: check_zone_codes.py <conform_zone_codes> <CLDR common directory> "
                 "<tzdb source directory>")
    codes, owner = read_codes(sys.argv[2])
    pairs = list(questions(codes, owner, read_windows(sys.argv[2]), read_tzdb(sys.argv[3])))
    asked = "".join(q + "\n" for q, _ in pairs)
    run = subprocess.run([sys.argv[1]], input=asked, capture_output=True, text=True, check=True)
    answers = run.stdout.splitlines()
    if len(answers) != len(pairs):
        sys.exit(f"{len(answers)} answers to {len(pairs)} questions")
    wrong = [(q, want, got) for (q, want), got in zip(pairs, answers) if want != got]
    for q, want, got in wrong[:20]:
        print(f"{q}: want {want}, got {got}")
    print(f"{len(pairs) - len(wrong)} of {len(pairs)} answers agree")
    sys.exit(1 if wrong else 0)


if __name__ == "__main__":
    main()
