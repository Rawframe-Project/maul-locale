# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen

"""Writes random parse lines: random_cases.py LOCALES OUT SEED, strings of
digits, separators, signs, spaces, bidi marks, percent, per mille,
exponent and letters (or with "grouping" digits and separators alone),
for each locale, style and mode."""
import random
import sys

ALPHABET = (list("0123456789") * 4 + list(",.'’ -+%‰E∞~") + [" ", " ", "‎",
            "−", "٫", "٬", "١", "٢", "०", "５", "，", "。", "e", "a", "NaN", "(", ")",
            "؜", "٪", "−", "‰", "¤", "$", " "])
# With "grouping", strings of digits and separators alone.
GROUPING = list("0123456789") * 3 + list(",.' ’-") + ["\u00a0", "\u202f", "٬", "٫", "，", "."]
STYLES = ["decimal", "percent", "permille", "scientific", "plus"]


def main():
    rng = random.Random(int(sys.argv[3]))
    alphabet = GROUPING if len(sys.argv) > 4 and sys.argv[4] == "grouping" else ALPHABET
    locales = [l.strip() for l in open(sys.argv[1], encoding="utf-8") if l.strip()]
    lines = []
    for locale in locales:
        for style in STYLES:
            for _ in range(60):
                text = "".join(rng.choice(alphabet) for _ in range(rng.randint(1, 12)))
                for mode in ("lenient", "strict"):
                    lines.append(f"{locale}\t{style}\t{mode}\t{text}\n")
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.writelines(lines)
    print(len(lines), "lines")


main()
