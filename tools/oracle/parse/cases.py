# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen

"""Writes the parse oracle's lines.

cases.py LOCALES FORMATTER OUT: for each locale of LOCALES and each style
(decimal, percent, permille, scientific, plus), the values below formatted
by FORMATTER (ours_fmt, the library's mlocFormatNumber), then each text and
its lenient variants, in both modes, as "locale<TAB>style<TAB>mode<TAB>text"
lines in OUT.
"""
import subprocess
import sys

VALUES = ["0", "1", "-1", "12.5", "-1234.567", "1234567.891", "0.001234",
          "100", "1000000", "-0.5", "987654321987654321", "12345.6789"]
STYLES = ["decimal", "percent", "permille", "scientific", "plus"]
SWAPS = {
    ",": ["，", "﹐", "٫", "、", "."], ".": ["．", "﹒", "。", ","],
    " ": [" ", " ", "'"], " ": [" ", " "],
    "٫": [",", "."], "٬": [",", "'"], "’": ["'", " "],
    " ": [" "],
}
DIGITS = ["٠١٢٣٤٥٦٧٨٩", "०१२३४५६७८९", "０１２３４５６７８９"]


def variants(text):
    out = {text, text + " apples", text + "abc", " " + text, "‎" + text,
           text + " ", text.replace("−", "-"), text.replace("-", "−"),
           "+" + text, "-" + text, "--" + text, text.replace("E", "e"),
           text.replace("%", ""), text + "%", text.replace("%", "٪"),
           text.replace("‰", ""), text + ",", text + ".", "(" + text + ")"}
    for old, news in SWAPS.items():
        for new in news:
            if old in text:
                out.add(text.replace(old, new))
                out.add(text.replace(old, new, 1))
    for sep in [",", ".", " ", " ", "’", "'", " ", "٬"]:
        if sep in text:
            out.add(text.replace(sep, ""))
    digits = [i for i, ch in enumerate(text) if ch.isdigit()]
    if len(digits) > 1:
        at = digits[0] + 1
        for sep in [",", ".", " "]:
            out.add(text[:at] + sep + text[at:])
    for table in DIGITS:
        out.add("".join(table[int(ch)] if "0" <= ch <= "9" else ch for ch in text))
    return sorted(out)


def main():
    locales = [l.strip() for l in open(sys.argv[1], encoding="utf-8") if l.strip()]
    requests = "".join(f"{l}\t{s}\t{v}\n" for l in locales for s in STYLES for v in VALUES)
    formatted = subprocess.run([sys.argv[2]], input=requests, capture_output=True,
                               text=True, check=True).stdout
    lines = []
    for row in formatted.splitlines():
        locale, style, text = row.split("\t")
        for variant in variants(text):
            if "\t" in variant or "\n" in variant:
                continue
            for mode in ("lenient", "strict"):
                lines.append(f"{locale}\t{style}\t{mode}\t{variant}\n")
    with open(sys.argv[3], "w", encoding="utf-8") as f:
        f.writelines(lines)
    print(len(lines), "lines")


main()
