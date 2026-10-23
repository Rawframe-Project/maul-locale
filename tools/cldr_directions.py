#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Every locale of CLDR's common/main with the character order it inherits
# (its own file's, then its parent's by parentLocales or by truncation,
# then root's), one "id<TAB>ltr|rtl" line each (a vertical order,
# Mongolian's, being left to right in a horizontal line), for
# conform_direction to check mlocGetLocaleDirection against.
#
# usage: cldr_directions.py <CLDR common>

import os
import re
import sys
import xml.etree.ElementTree as ET


def main():
    common = sys.argv[1]
    parents = {}
    supplemental = ET.parse(os.path.join(common, "supplemental", "supplementalData.xml"))
    for group in supplemental.getroot().iter("parentLocales"):
        if group.get("component") is None:
            for p in group.iter("parentLocale"):
                for locale in p.get("locales").split():
                    parents[locale] = p.get("parent")
    order = {}
    pattern = re.compile(r"<characterOrder>([a-z-]+)</characterOrder>")
    for name in os.listdir(os.path.join(common, "main")):
        if name.endswith(".xml"):
            with open(os.path.join(common, "main", name), encoding="utf-8") as f:
                match = pattern.search(f.read())
            order[name[:-4]] = match.group(1) if match else None

    def inherited(locale):
        while not order.get(locale):
            if locale == "root":
                sys.exit("cldr_directions: root has no characterOrder")
            if locale in parents:
                locale = parents[locale]
            elif "_" in locale:
                locale = locale.rsplit("_", 1)[0]
            else:
                locale = "root"
        return order[locale]

    for locale in sorted(order):
        if locale == "root":
            continue
        value = inherited(locale)
        # Mongolian's top-to-bottom is vertical: set in a horizontal line
        # its letters (bidi class L) run left to right.
        if value not in ("left-to-right", "right-to-left", "top-to-bottom"):
            sys.exit(f"cldr_directions: {locale}: characterOrder {value}")
        print(locale.replace("_", "-") + "\t" + ("rtl" if value == "right-to-left" else "ltr"))


main()
