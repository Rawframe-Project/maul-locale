# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen

"""Writes random currency parse lines: random_currency.py LOCALES OUT SEED,
strings of digits, separators, signs, spaces, bidi marks, currency signs,
codes and names, with a formatter of three currencies, any 0 and 1, both
modes."""
import random
import sys

TOKENS = (list("0123456789") * 3 + list(",. -()") + [" ", "‎", "−", "$", "€", "£",
          "¥", "US$", "USD", "usd", "EUR", "Eur", "euro", "euros", "dollar", "dollars",
          "US dollars", "CHF", "Fr.", "₹", "Rs", "kr", "R$", "JP¥", "円", "€ ", " €", "zł", "₽"])


def main():
    rng = random.Random(int(sys.argv[3]))
    locales = [l.strip() for l in open(sys.argv[1], encoding="utf-8") if l.strip()]
    lines = []
    for locale in locales:
        for code in ("USD", "EUR", "JPY"):
            for _ in range(20):
                text = "".join(rng.choice(TOKENS) for _ in range(rng.randint(1, 8)))
                for mode in ("lenient", "strict"):
                    for any_ in ("0", "1"):
                        lines.append(f"{locale}\t{code}\t{mode}\t{any_}\t{text}\n")
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.writelines(lines)
    print(len(lines), "lines")


main()
