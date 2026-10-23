# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes random lists of reorder codes for the reordering differential
# (icu4c_collation_reorder.cpp against conform_collation_reorder): one to
# forty of the given script codes and the five groups, sometimes Zzzz
# inside, sometimes Latin first; duplicates and codes of one range come
# by chance and must be refused alike.
#   python3 -I reorder_lists.py SCRIPTS COUNT SEED > lists
# SCRIPTS is a file of four-letter codes, one a line.
import random
import sys

names = [n for n in open(sys.argv[1]).read().split() if len(n) == 4 and n not in ("Hans", "Hant", "Hrkt")]
names += ["space", "punct", "symbol", "currency", "digit"]
count, seed = int(sys.argv[2]), int(sys.argv[3])
rng = random.Random(seed)
for _ in range(count):
    codes = [rng.choice(names) for _ in range(rng.choice([1, 1, 2, 2, 3, 4, 5, 6, 8, 12, 20, 40]))]
    if rng.random() < 0.15:
        codes.insert(rng.randint(0, len(codes)), "Zzzz")
    if rng.random() < 0.1:
        codes = ["Latn"] + codes
    print(" ".join(codes))
