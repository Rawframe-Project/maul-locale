# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen

"""Writes the currency parse oracle's lines.

currency_cases.py LOCALES FORMATTER NAMES OUT: for each locale of LOCALES,
twelve currencies, four displays (symbol, narrow, code, name) and three
values, the text FORMATTER (ours_cfmt, mlocCurrencyFormatter_Format) writes and its
variants, parsed with a formatter of the same currency (any 0 and 1) and
of another (any 1), in both modes: "locale code mode any text" lines.
"""
import subprocess
import sys

CURRENCIES = ["USD", "EUR", "JPY", "GBP", "CHF", "INR", "CVE", "RUB", "CNY", "BRL", "MXN", "TRY"]
DISPLAYS = ["symbol", "narrow", "code", "name"]
VALUES = ["12.5", "-1234.56", "1000000"]


def variants(text, code):
    out = {text, text + " x", " " + text, text.replace(" ", " "), text.replace(" ", ""),
           text.replace(" ", ""), text.replace(code, code.lower()), "-" + text,
           text.replace("−", "-"), text.replace(",", "").replace(".", "")}
    return sorted(v for v in out if "\t" not in v and "\n" not in v)


def main():
    locales = [l.strip() for l in open(sys.argv[1], encoding="utf-8") if l.strip()]
    requests = "".join(f"{l}\t{c}\t{d}\t{v}\n" for l in locales for c in CURRENCIES
                       for d in DISPLAYS for v in VALUES)
    formatted = subprocess.run([sys.argv[2], sys.argv[3]], input=requests, capture_output=True,
                               text=True, check=True).stdout
    lines = set()
    for row in formatted.splitlines():
        locale, code, text = row.split("\t")
        other = "EUR" if code == "USD" else "USD"
        for variant in variants(text, code):
            for mode in ("lenient", "strict"):
                lines.add(f"{locale}\t{code}\t{mode}\t0\t{variant}\n")
                lines.add(f"{locale}\t{code}\t{mode}\t1\t{variant}\n")
                lines.add(f"{locale}\t{other}\t{mode}\t1\t{variant}\n")
    with open(sys.argv[4], "w", encoding="utf-8") as f:
        f.writelines(sorted(lines))
    print(len(lines), "lines")


main()
