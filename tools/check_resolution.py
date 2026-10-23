#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Compares the generator's CLDR resolution with CLDR's JSON of the same
# release, which CLDR's own tools resolve (the JSON is an oracle, never an
# input). For every locale the JSON has, it asks mlocgen for number
# symbols, Gregorian month and day names in every context and width (the
# paths root aliases the most), Buddhist months (aliased to Gregorian),
# unit names and patterns (lateral count fallback, and narrow aliased to
# short), and language names.
#
# usage: check_resolution.py <mlocgen> <CLDR common dir> <CLDR JSON full dir>
# It shows five differences per area; ALL=1 shows every one.

import json
import os
import subprocess
import sys

SYMBOLS = ["decimal", "group", "list", "percentSign", "plusSign", "minusSign",
           "approximatelySign", "exponential", "superscriptingExponent", "perMille",
           "infinity", "nan", "timeSeparator"]
DAYS = ["sun", "mon", "tue", "wed", "thu", "fri", "sat"]
UNITS = ["mass-gram", "duration-hour", "length-kilometer", "speed-kilometer-per-hour"]
LANGUAGES = ["de", "en", "tr", "ja", "zh_Hant"]


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def main_of(data, locale):
    return data["main"][locale]


def symbol_queries(root, locale, xml_id):
    path = os.path.join(root, "cldr-numbers-full", "main", locale, "numbers.json")
    if not os.path.exists(path):
        return
    symbols = main_of(load(path), locale)["numbers"].get("symbols-numberSystem-latn", {})
    for name in SYMBOLS:
        if name in symbols:
            chain = f'//ldml/numbers/symbols[@numberSystem="latn"]/{name}'
            yield ("symbols", xml_id, chain, symbols[name])


def calendar_queries(root, locale, xml_id, calendar, package, kinds):
    path = os.path.join(root, package, "main", locale, f"ca-{calendar}.json")
    if not os.path.exists(path):
        return
    data = main_of(load(path), locale)["dates"]["calendars"][calendar]
    for kind, element, keys in kinds:
        for context, widths in data[kind].items():
            for width, names in widths.items():
                for key in keys:
                    if key not in names:
                        continue
                    chain = (f'//ldml/dates/calendars/calendar[@type="{calendar}"]/{kind}/'
                             f'{element}Context[@type="{context}"]/{element}Width'
                             f'[@type="{width}"]/{element}[@type="{key}"]')
                    yield (f"{calendar} {kind}", xml_id, chain, names[key])


def unit_queries(root, locale, xml_id):
    path = os.path.join(root, "cldr-units-full", "main", locale, "units.json")
    if not os.path.exists(path):
        return
    units = main_of(load(path), locale)["units"]
    for length in ["long", "short", "narrow"]:
        for unit in UNITS:
            entry = units.get(length, {}).get(unit)
            if entry is None:
                continue
            base = f'//ldml/units/unitLength[@type="{length}"]/unit[@type="{unit}"]'
            for key, value in entry.items():
                if key == "displayName":
                    yield ("units", xml_id, base + "/displayName", value)
                elif key.startswith("unitPattern-count-"):
                    count = key[len("unitPattern-count-"):]
                    yield ("units", xml_id, base + f'/unitPattern[@count="{count}"]', value)


def language_queries(root, locale, xml_id):
    path = os.path.join(root, "cldr-localenames-full", "main", locale, "languages.json")
    if not os.path.exists(path):
        return
    names = main_of(load(path), locale)["localeDisplayNames"]["languages"]
    for language in LANGUAGES:
        key = language.replace("_", "-")
        if key in names:
            chain = f'//ldml/localeDisplayNames/languages/language[@type="{language}"]'
            yield ("languages", xml_id, chain, names[key])


def xml_ids(common):
    """Locale ids as the XML files name them, by their lowercase form:
    the JSON writes variants in lowercase (el-polyton), the files in
    uppercase (el_POLYTON)."""
    names = os.listdir(os.path.join(common, "main"))
    return {n[:-4].lower(): n[:-4] for n in names if n.endswith(".xml")}


def queries(root, common):
    ids = xml_ids(common)
    locales = sorted(os.listdir(os.path.join(root, "cldr-numbers-full", "main")))
    gregorian = [("months", "month", [str(i) for i in range(1, 14)]), ("days", "day", DAYS)]
    for locale in locales:
        xml_id = "root" if locale == "und" else ids.get(locale.replace("-", "_").lower())
        if xml_id is None:
            print(f"the JSON's locale {locale} has no XML file")
            sys.exit(1)
        yield from symbol_queries(root, locale, xml_id)
        yield from calendar_queries(root, locale, xml_id, "gregorian", "cldr-dates-full",
                                    gregorian)
        yield from calendar_queries(root, locale, xml_id, "buddhist", "cldr-cal-buddhist-full",
                                    [("months", "month", [str(i) for i in range(1, 13)])])
        yield from unit_queries(root, locale, xml_id)
        yield from language_queries(root, locale, xml_id)


def exceptions():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "resolution-exceptions.txt")
    known = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.strip() and not line.startswith("#"):
                locale, prefix, _ = line.rstrip("\n").split("\t")
                known.append((locale, prefix))
    return known


def main():
    if len(sys.argv) != 4:
        print(__doc__ or "usage: check_resolution.py <mlocgen> <common dir> <JSON dir>")
        return 2
    mlocgen, common, json_root = sys.argv[1:]
    cases = list(queries(json_root, common))
    lines = "".join(f"{locale}\t{chain}\n" for _, locale, chain, _ in cases)
    result = subprocess.run([mlocgen, "resolve", common], input=lines.encode("utf-8"),
                            capture_output=True, check=False)
    if result.returncode != 0:
        print(result.stderr.decode("utf-8", "replace"))
        return 1
    answers = result.stdout.decode("utf-8").split("\n")[:-1]
    if len(answers) != len(cases):
        print(f"mlocgen answered {len(answers)} of {len(cases)} queries")
        return 1
    known = exceptions()
    used = set()
    totals = {}
    failures = {}
    excused = 0
    for (area, locale, chain, expected), answer in zip(cases, answers):
        totals[area] = totals.get(area, 0) + 1
        got = answer.split("\t")[0]
        if got == expected:
            continue
        match = next((k for k in known if k[0] == locale and chain.startswith(k[1])), None)
        if match is not None:
            used.add(match)
            excused += 1
            continue
        failures.setdefault(area, []).append((locale, chain, expected, got))
    stale = [k for k in known if k not in used]
    for locale, prefix in stale:
        print(f"resolution-exceptions.txt: {locale} {prefix} agrees now; remove the line")
    for area in sorted(totals):
        bad = failures.get(area, [])
        print(f"{area}: {totals[area] - len(bad)} of {totals[area]} agree")
        for locale, chain, expected, got in bad if os.environ.get("ALL") else bad[:5]:
            print(f"  {locale} {chain}\n    JSON {expected!r}, mlocgen {got!r}")
    count = sum(len(v) for v in failures.values())
    print(f"resolution: {len(cases) - count - excused} of {len(cases)} values agree with "
          f"CLDR's JSON, {excused} differ as tools/resolution-exceptions.txt explains")
    return 1 if count or stale else 0


if __name__ == "__main__":
    sys.exit(main())
