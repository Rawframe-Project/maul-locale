# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Random limits of each level for the weight allocator differential
# (icu4c_collation_weights.cpp): valid weights of 1 to 4 bytes, near
# neighbours for half the primaries, n from 1 to 70,000.
#   python3 -I weights_cases.py COUNT SEED
import random, sys
rng = random.Random(int(sys.argv[2]))
def byte(lo, hi): return rng.randint(lo, hi)
def weight(level, comp):
    if level == 1:
        n = rng.choice([1, 2, 2, 3, 3, 4])
        b = [byte(3, 0xFF)]
        if n >= 2: b.append(byte(4, 0xFE) if comp else byte(2, 0xFF))
        while len(b) < n: b.append(byte(2, 0xFF))
        b += [0] * (4 - n)
    else:
        top = 0xFF if level == 2 else 0x3F
        n = rng.choice([1, 2])
        b = [0, 0, byte(2, top)] + ([byte(2, top)] if n == 2 else [0])
    return b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]
for _ in range(int(sys.argv[1])):
    level = rng.choice([1, 1, 2, 3]); comp = rng.randint(0, 1) if level == 1 else 0
    a, b = weight(level, comp), weight(level, comp)
    if rng.random() < 0.5 and level == 1:  # near neighbours, where the merge matters
        b = (a & 0xFFFF0000) + (rng.randint(1, 3) << 16) if rng.random() < 0.5 else a + (rng.randint(1, 3) << 8)
    lo, hi = min(a, b), max(a, b)
    n = rng.choice([1, 2, 3, 5, 10, 50, 300, 1000, 70000])
    print(level, comp, "%08x" % lo, "%08x" % hi, n)
