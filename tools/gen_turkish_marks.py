#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes src/generated/turkish_marks.h, the Turkish words whose suffixes
# the inflector's rules alone would get wrong (record mloc-0046), from
# Zemberek-NLP's dictionaries at the commit tools/zemberek-release.txt
# pins (Apache License 2.0; LICENSES/Apache-2.0-zemberek.txt):
#   - a word that keeps its final stop where Zemberek's rule softens it
#     (NoVoicing: ahlak, tank), and one that softens it where the rule
#     does not (Voicing: kalp); the rule softens a final ç, k, p or t in
#     a word of two syllables or more without inverse harmony, and a
#     final nk or og in any word;
#   - inverse harmony (rol, saat, katedral), doubling (hak: hakkı), the
#     last vowel dropped (burun: burnu), and a compound that ends in a
#     third person's possessive (zeytinyağı), with the bytes of that
#     possessive.
# Verbs are left out; a word with several entries takes its first one
# without an index. Words are keyed in Turkish lowercase without the
# circumflex.
#
# The table is front-coded: each entry is the bytes it shares with the
# one before, its own length and bytes, its marks, and for a compound
# the possessive's length; every 16th shares nothing, and their offsets
# are searched first.
#
# usage: gen_turkish_marks.py DIR [--check]
#   DIR      holds master-dictionary.dict and proper.dict at the pinned
#            commit (their SHA-256 is checked)
#   --check  fail when the file differs from what would be written

import hashlib
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PATH = os.path.join(ROOT, "src", "generated", "turkish_marks.h")
PIN = os.path.join(ROOT, "tools", "zemberek-release.txt")
BLOCK = 16
VOWELS = set("aeıioöuü")
STOPS = ("p", "ç", "t", "k")
MARKS = {"keeps": 1, "softens": 2, "InverseHarmony": 4, "Doubling": 8, "LastVowelDrop": 16,
         "CompoundP3sg": 32}
VERB_ATTRS = {"Aorist_A", "Aorist_I", "Reciprocal", "Reflexive", "NonReciprocal"}


def pins():
    out = {}
    with open(PIN, encoding="utf-8") as f:
        for line in f:
            parts = line.split()
            if parts and not line.startswith("#"):
                out.setdefault(parts[0], []).append(parts[1:])
    return out


def key(word):
    w = word.replace("I", "ı").replace("İ", "i").lower()
    return w.replace("â", "a").replace("î", "i").replace("û", "u")


def read(directory):
    pinned = pins()
    entries = {}
    for name, digest in pinned["file"]:
        path = os.path.join(directory, name)
        data = open(path, "rb").read()
        if hashlib.sha256(data).hexdigest() != digest:
            sys.exit(f"{path}: SHA-256 differs from {PIN}")
        for line in data.decode("utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            m = re.match(r"^(\S+)(?:\s*\[(.*)\])?", line)
            word, meta = m.group(1), m.group(2) or ""
            attrs = set()
            found = re.search(r"A:([^;\]]*)", meta)
            if found:
                attrs = {a.strip() for a in found.group(1).split(",")}
            roots = re.search(r"Roots:([^;\]]*)", meta)
            if "P:Verb" in meta.replace(" ", "") or attrs & VERB_ATTRS:
                continue
            k = key(word)
            if not all(c in VOWELS or c.isalpha() for c in k) or not k.isalpha():
                continue
            if k in entries and "Index:" in meta:
                continue
            entries.setdefault(k, (attrs, roots.group(1).strip() if roots else None))
    return entries


def softens_by_rule(word, inverse):
    # Zemberek's inference, which its marks are written against: a
    # final ç, k, p or t softens in a word of two syllables or more
    # without inverse harmony; one in nk or og softens in any word.
    syllables = sum(1 for c in word if c in VOWELS)
    return ((word.endswith(STOPS) and syllables >= 2 and not inverse)
            or word.endswith("nk") or word.endswith("og"))


def marks_of(word, attrs, roots):
    marks = 0
    if softens_by_rule(word, "InverseHarmony" in attrs):
        if "NoVoicing" in attrs:
            marks |= MARKS["keeps"]
    elif "Voicing" in attrs and word.endswith(STOPS + ("g",)):
        marks |= MARKS["softens"]
    for name in ("InverseHarmony", "Doubling", "LastVowelDrop"):
        if name in attrs:
            marks |= MARKS[name]
    strip = 0
    if "CompoundP3sg" in attrs and roots:
        root = key(roots.replace("-", ""))
        # Only a compound whose roots stand whole before its possessive.
        if word.startswith(root) and 0 < len(word) - len(root) <= 3:
            marks |= MARKS["CompoundP3sg"]
            strip = len(word.encode()) - len(root.encode())
    return marks, strip


def table(entries):
    rows = []
    for word in sorted(entries, key=lambda w: w.encode()):
        attrs, roots = entries[word]
        marks, strip = marks_of(word, attrs, roots)
        if marks:
            rows.append((word.encode(), marks, strip))
    data = bytearray()
    blocks = []
    previous = b""
    for i, (word, marks, strip) in enumerate(rows):
        shared = 0
        if i % BLOCK == 0:
            blocks.append(len(data))
        else:
            while shared < min(len(word), len(previous)) and word[shared] == previous[shared]:
                shared += 1
        rest = word[shared:]
        data += bytes([shared, len(rest)]) + rest + bytes([marks])
        if marks & MARKS["CompoundP3sg"]:
            data.append(strip)
        previous = word
    return rows, data, blocks


def render(rows, data, blocks):
    commit = pins()["commit"][0][0]
    lines = [
        "// SPDX-License-Identifier: MIT AND Apache-2.0",
        "// Copyright (c) 2026 Sirac Ozmen",
        "// Copyright 2018 Ahmet A. Akın, Mehmet D. Akın (Zemberek-NLP's dictionaries)",
        "//",
        "// Generated by tools/gen_turkish_marks.py; do not edit. The Turkish words",
        "// whose suffixes the inflector's rules alone would get wrong, from",
        "// Zemberek-NLP's dictionaries at commit",
        f"// {commit}",
        "// (Apache License 2.0, LICENSES/Apache-2.0-zemberek.txt).",
        "// clang-format off",
        "",
        "#ifndef MLOC_TURKISH_MARKS_H",
        "#define MLOC_TURKISH_MARKS_H",
        "",
        "#include <stdint.h>",
        "",
        "enum",
        "{",
        f"    turkishMarkWords = {len(rows)},",
        f"    turkishMarkBlocks = {len(blocks)},",
        f"    turkishMarkBlock = {BLOCK},",
        f"    turkishMarkBytes = {len(data)},",
        "};",
        "",
        "static const uint16_t s_turkishMarkBlocks[turkishMarkBlocks] = {",
    ]
    for i in range(0, len(blocks), 12):
        lines.append("    " + ", ".join(str(b) for b in blocks[i:i + 12]) + ",")
    lines += ["};", "", "static const uint8_t s_turkishMarks[turkishMarkBytes] = {"]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
    lines += ["};", "", "#endif", ""]
    return "\n".join(lines)


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: gen_turkish_marks.py DIR [--check]")
    rows, data, blocks = table(read(sys.argv[1]))
    if len(data) >= 65536:
        sys.exit("the table passes 64 KiB; the block offsets are 16 bits")
    text = render(rows, data, blocks)
    if "--check" in sys.argv[2:]:
        if open(PATH, encoding="utf-8").read() != text:
            sys.exit(f"{PATH} is stale; run tools/gen_turkish_marks.py")
        print(f"turkish marks: {len(rows)} words, {len(data)} bytes, current")
        return
    with open(PATH, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"turkish marks: {len(rows)} words, {len(data)} bytes")


if __name__ == "__main__":
    main()
