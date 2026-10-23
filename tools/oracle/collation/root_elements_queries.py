# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen

"""Queries for the root elements' differential.

Reads a root elements table (one hex unit per line, as
icu4c_collation_root_elements writes ICU4C's) and FractionalUCA.txt (its
compressible lead bytes), and writes, for every root element outside
Han's section, the queries conform_collation_root_elements --queries
and the oracle answer: the primary before and after each primary, the
secondary and tertiary before and after each of its weights, the
element before and at each primary; then those around 2,000 primaries
that are no root primary (reordering bounds).

usage: python3 -I root_elements_queries.py TABLE FractionalUCA.txt
"""

import random
import re
import sys

DELTA = 0x80
STEP = 0x7F
COMMON = 0x05000500
FIRST_HAN, LAST_HAN = 0xFA020200, 0xFD2B2200


def compressible_bytes(path):
    out = {0xFE}
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"\[top_byte\s+([0-9A-F]{2})\s[^\]]*COMPRESS", line)
            if m:
                out.add(int(m.group(1), 16))
    return out


def add(p, compressible, step):
    """ICU's incTwo/ThreeBytePrimaryByOffset."""
    low, count = (4, 251) if compressible else (2, 254)
    if p & 0xFFFF == 0:
        off = step + ((p >> 16) & 0xFF) - low
        return ((p & 0xFF000000) + ((off // count) << 24)) | ((off % count + low) << 16)
    off = step + ((p >> 8) & 0xFF) - 2
    third = off % 254 + 2
    off //= 254
    off += ((p >> 16) & 0xFF) - low
    second = off % count + low
    off //= count
    return ((p & 0xFF000000) + (off << 24)) | (second << 16) | (third << 8)


def decode(units, comp):
    """[(primary, [secTer, ...])] in order, the primary 0 first."""
    fpi = units[2]
    out = [(0, [u & ~DELTA for u in units[5:fpi]])]
    prev = None
    for u in units[fpi:-1]:
        if u & DELTA:
            out[-1][1].append(u & ~DELTA)
            continue
        p, step = u & ~STEP, u & STEP
        if step:
            q = prev
            while True:
                q = add(q, (q >> 24) in comp, step)
                out.append((q, [COMMON]))
                if q == p:
                    break
        else:
            out.append((p, [COMMON]))
        prev = p
    for p, st in out[1:]:
        if st[0] > COMMON or len(st) > 1 and st[1] <= COMMON:
            st.remove(COMMON)  # an explicit common unit, or none implied
    return out


def main():
    units = [int(line, 16) for line in open(sys.argv[1])]
    comp = compressible_bytes(sys.argv[2])
    rows = decode(units, comp)
    rnd = random.Random(1)
    q = []
    primaries = [p for p, _ in rows[1:]]
    first = primaries[0]
    for p, sts in rows:
        if FIRST_HAN <= p <= LAST_HAN:
            continue
        c = int((p >> 24) in comp)
        if p != 0:
            if p != first:
                q += ["pb %x %d" % (p, c), "lb %x" % p]
            q += ["pa %x %d" % (p, c), "fa %x" % p]
        for st in sorted(set(sts)):
            s, t = st >> 16, st & 0xFFFF
            if p == 0 and s == 0:
                q += ["tb 0 0 %x" % t, "ta 0 0 %x" % t]
                continue
            q += ["sb %x %x" % (p, s), "sa %x %x" % (p, s)]
            q += ["tb %x %x %x" % (p, s, t), "ta %x %x %x" % (p, s, t)]
    lo, hi = first, 0xFE000000
    for _ in range(2000):
        p = rnd.randrange(lo, hi) & 0xFFFFFF00
        if not FIRST_HAN <= p <= LAST_HAN:
            q += ["lb %x" % p, "fa %x" % p]
    print("\n".join(q))


main()
