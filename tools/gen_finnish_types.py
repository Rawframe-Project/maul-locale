#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes src/generated/finnish_types.h, each Finnish nominal's inflection
# type and consonant gradation, from Kotus's word list of modern Finnish
# in tools/external/kotus (CC BY 3.0; LICENSES/CC-BY-3.0-kotus.txt).
# Only nominals (types 1 to 51) are
# read, and only what the ending does not decide is kept:
#   - rules: a word's last one to five letters and the type and
#     gradation most words ending so have, kept where it differs from the
#     rule of the ending one letter shorter and at least three words end
#     so (the empty ending is the default);
#   - words: those the rules, or a shorter word of the table ending
#     them, give another type or gradation.
# A word with several types takes its first. Each table is a blob of
# records, sorted by their bytes: the key's length, its bytes, the type
# and the gradation letter (0 for none); an offset per record.
#
# usage: gen_finnish_types.py [--check | --output-path]
#   The list's SHA-256 is checked against the directory's README.md.
#   --check  fail when the file differs from what would be written
#   --output-path  print the file it writes, relative to the repository's
#                  root (tools/regenerate.sh keeps that file)

import collections
import hashlib
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PATH = os.path.join(ROOT, "src", "generated", "finnish_types.h")
SOURCE = os.path.join(ROOT, "tools", "external", "kotus")
LONGEST = 5
SUPPORT = 3


def pinned():
    """Each file of the source directory and its SHA-256, from the
    ```text block of its README.md."""
    readme = open(os.path.join(SOURCE, "README.md"), encoding="utf-8").read()
    block = re.search(r"```text\n(.*?)```", readme, re.S).group(1)
    return [line.split()[::-1] for line in block.splitlines() if line.strip()]


def read():
    rows = []
    for name, digest in pinned():
        data = open(os.path.join(SOURCE, name), "rb").read()
        if hashlib.sha256(data).hexdigest() != digest:
            sys.exit("gen_finnish_types: %s is not the pinned file" % name)
        text = data.decode("utf-8")
        for word, rest in re.findall(r"<st><s>([^<]+)</s>(.*?)</st>", text):
            types = re.findall(r"<tn>(\d+)</tn>", rest)
            grades = re.findall(r"<av>([A-M])</av>", rest)
            if not types or int(types[0]) > 51 or not word.isalpha() or word != word.lower():
                continue
            rows.append((word, (int(types[0]), grades[0] if grades else "")))
    return rows


def learn(rows):
    counts = collections.defaultdict(collections.Counter)
    for word, kind in rows:
        for k in range(0, LONGEST + 1):
            counts[word[-k:] if k else ""][kind] += 1
    rules = {}
    for ending in sorted(counts, key=lambda e: (len(e), e)):
        if ending and sum(counts[ending].values()) < SUPPORT:
            continue
        shorter = ending[1:]
        while shorter not in rules and shorter:
            shorter = shorter[1:]
        best = counts[ending].most_common(1)[0][0]
        if not ending or best != rules[shorter]:
            rules[ending] = best
    return rules


def predict(rules, word):
    for k in range(min(LONGEST, len(word)), -1, -1):
        ending = word[-k:] if k else ""
        if ending in rules:
            return rules[ending]
    return None


def lookup(rules, table, word, tail):
    """The kind grammar_fi.c gives a common noun: the word, a compound's
    last word of three letters or more after two or more, the rules; as a
    compound's last word (tail), from its second letter too."""
    starts = [0] + list(range(1 if tail else 2, len(word) - 2))
    for start in starts:
        if word[start:] in table:
            return table[word[start:]]
    return predict(rules, word)


def table(rules, rows):
    """The words the rules or a shorter word of the table give another
    kind, alone or as a compound's last word, until every word's lookup
    is its own."""
    words = {w: k for w, k in rows if predict(rules, w) != k}
    while True:
        wrong = {w: k for w, k in rows if lookup(rules, words, w, True) != k}
        if not wrong:
            return list(words.items())
        words.update(wrong)


def blob(name, entries):
    data = bytearray()
    offsets = []
    starts = [0] * 257
    for key, (kind, grade) in sorted(entries, key=lambda e: e[0].encode("utf-8")):
        raw = key.encode("utf-8")
        for b in range(raw[0] + 1, 257):
            starts[b] = len(offsets) + 1
        offsets.append(len(data))
        data += bytes([len(raw)]) + raw + bytes([kind, ord(grade) if grade else 0])
    lines = ["static const uint8_t s_%s[%d] = {" % (name, len(data))]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02X" % b for b in data[i:i + 16]) + ",")
    lines.append("};")
    lines.append("static const uint16_t s_%sOffsets[%d] = {" % (name, len(offsets)))
    for i in range(0, len(offsets), 12):
        lines.append("    " + ", ".join(str(o) for o in offsets[i:i + 12]) + ",")
    lines.append("};")
    lines.append("// The first entry whose first byte is b or more, at [b].")
    lines.append("static const uint16_t s_%sStarts[257] = {" % name)
    for i in range(0, 257, 12):
        lines.append("    " + ", ".join(str(o) for o in starts[i:i + 12]) + ",")
    lines.append("};")
    assert len(data) < 65536
    return "\n".join(lines)


def main():
    if sys.argv[1:] == ["--output-path"]:
        print(os.path.relpath(PATH, ROOT).replace(os.sep, "/"))
        return
    if any(a != "--check" for a in sys.argv[1:]):
        sys.exit("usage: gen_finnish_types.py [--check | --output-path]")
    rows = read()
    first = {}
    for word, kind in rows:
        first.setdefault(word, kind)
    rows = list(first.items())
    rules = learn(rows)
    words = table(rules, rows)
    default = rules.pop("")
    text = "\n".join([
        "// SPDX-License-Identifier: MIT AND CC-BY-3.0",
        "// Copyright (c) 2026 Sirac Ozmen",
        "// Copyright 2006 Kotimaisten kielten tutkimuskeskus (Kotus's word list)",
        "//",
        "// Generated by tools/gen_finnish_types.py; do not edit. Each Finnish",
        "// nominal's inflection type and gradation the endings do not tell,",
        "// from Kotus's word list of modern Finnish, version 1 (CC BY 3.0,",
        "// LICENSES/CC-BY-3.0-kotus.txt).",
        "// clang-format off",
        "",
        "#ifndef MLOC_GENERATED_FINNISH_TYPES_H",
        "#define MLOC_GENERATED_FINNISH_TYPES_H",
        "",
        "#include <stdint.h>",
        "",
        "// %d nominals: %d endings, %d words." % (len(rows), len(rules), len(words)),
        "enum",
        "{",
        "    finnishDefaultType = %d," % default[0],
        "    finnishDefaultGrade = %d," % (ord(default[1]) if default[1] else 0),
        "    finnishLongestEnding = %d," % LONGEST,
        "};",
        "",
        blob("finnishEndings", list(rules.items())),
        "",
        blob("finnishWords", words),
        "",
        "#endif",
        "",
    ])
    if "--check" in sys.argv[1:]:
        if open(PATH, encoding="utf-8").read() != text:
            sys.exit("gen_finnish_types: %s is stale" % PATH)
        return
    with open(PATH, "w", encoding="utf-8") as f:
        f.write(text)


if __name__ == "__main__":
    main()
