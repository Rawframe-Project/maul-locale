# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes cases.txt: the currency oracle's locales by the three list types,
# three widths and lists that test the joins and ICU's contextual changes.
import sys
here = sys.argv[1]
locales = open(f"{here}/../currency/locales.txt").read().split()
lists = ["a", "a|b", "a|b|c", "a|b|c|d", "a|b|c|d|e",
         "a|hielo", "a|hiedra", "a|hiato", "a|Isla", "a|hi", "a|HI", "a|ola", "a|Hola",
         "a|8", "a|11", "a|11 x", "a|110", "a|b|isla", "a|b|ocho", "isla|b",
         "א|ב", "א|b", "א|1", "א|ב|c", "א|ﬠ", "א|\U0001f600",
         "א|׈x", "{0} x|{0}"]
with open("cases.txt", "w") as f:
    for loc in locales + ["es_MX", "he_IL"]:
        for t in ["and", "or", "unit"]:
            for w in ["wide", "short", "narrow"]:
                for l in lists:
                    f.write(f"{loc} {t} {w} {l}\n")
