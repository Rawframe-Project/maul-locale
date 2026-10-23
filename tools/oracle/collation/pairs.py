# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes random pairs of short texts, as hex UTF-8 separated by a tab,
# for the collation options differential (icu4c_collation_options.cpp
# against maul-locale's conform_collation_options). The code points are
# ones whose root order ICU 74 (UCA 15.1) and CLDR 49 (UCA 18) share:
# Latin letters in both cases, precomposed and combining accents, a few
# punctuation marks, spaces, digits, a soft hyphen and U+FFFE; and runs
# of decimal digits for numeric ordering: zeros, long numbers, a few past
# a 254-digit segment, ASCII, Arabic-Indic, Devanagari and fullwidth
# digits mixed; Greek, Cyrillic, Han and kana letters and a few symbols
# and currency signs for reordering.
#   python3 -I pairs.py COUNT SEED > pairs
import random
import sys

POOL = ["a", "b", "c", "A", "B", "C", "é", "É", "à", "ç", "ô",
        "Ô", "̀", "́", "̂", "̣", "̧", "-", " ", ".", "'",
        "1", "2", "­", "￾", "e", "E", "o", "O",
        "α", "Β", "ά", "д", "Ж", "й", "一", "丁", "あ", "カ", "$", "+", "€"]

DIGITS = ["0123456789", "\u0660\u0661\u0662\u0663\u0664\u0665\u0666\u0667\u0668\u0669",
          "\u0966\u0967\u0968\u0969\u096A\u096B\u096C\u096D\u096E\u096F",
          "\uFF10\uFF11\uFF12\uFF13\uFF14\uFF15\uFF16\uFF17\uFF18\uFF19"]

count, seed = int(sys.argv[1]), int(sys.argv[2])
rng = random.Random(seed)


def number():
    r = rng.random()
    length = rng.randint(1, 3) if r < 0.5 else rng.randint(4, 24) if r < 0.97 else rng.randint(250, 520)
    script = rng.choice(DIGITS) if rng.random() < 0.2 else DIGITS[0]
    zeros = rng.random() < 0.3
    return "".join(script[0 if zeros and rng.random() < 0.6 else rng.randint(0, 9)]
                   for _ in range(length))


def text():
    return "".join(number() if rng.random() < 0.25 else rng.choice(POOL)
                   for _ in range(rng.randint(0, 6)))


for _ in range(count):
    a = text()
    # Half the pairs share most of their text, where the later levels decide.
    b = a[: rng.randint(0, len(a))] + text() if rng.random() < 0.5 else text()
    print(a.encode("utf-8").hex() + "\t" + b.encode("utf-8").hex())
