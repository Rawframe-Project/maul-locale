#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks that every internal function returning bool carries
# MLOC_NODISCARD: each one declared in a header under src/, and each
# static one in a source file under src/, its subdirectories (the
# generated files among them) included. A bool answers or refuses, and a
# caller that drops it goes on past a refusal or with an out value left
# unset; a call that means to drop it says so with (void). The check
# fails closed: a declaration it cannot read, one indented or with bool
# on a line of its own, is an error naming its place, so it never passes
# what it did not check. Run with --self-test to check the matching on
# its own examples.

import pathlib
import re
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent

SPECIFIERS = r"((?:(?:MLOC_\w+|static|inline|extern)\s+)*)"
# A function declaration or definition starting at the line's first
# column: its specifiers and macros, bool, the name and its parenthesis.
DECLARATION = re.compile(r"^" + SPECIFIERS + r"bool\s+(\w+)\s*\(")
# The same, indented: not read, so refused.
INDENTED = re.compile(r"^\s+" + SPECIFIERS + r"bool\s+(\w+)\s*\(")
# A return type of bool on a line of its own, the name on the next: not
# read, so refused.
ALONE = re.compile(r"^\s*" + SPECIFIERS + r"bool\s*$")


def Problems(text, header):
    """The (line, message) of each bool function the text declares
    without MLOC_NODISCARD (every one in a header, the static ones
    elsewhere), and of each declaration in a shape the check does not
    read."""
    found = []
    for number, line in enumerate(text.splitlines(), 1):
        if INDENTED.match(line):
            found.append((number, "an indented bool declaration, which this check does not read: "
                                  "start it at the line's first column"))
            continue
        if ALONE.match(line):
            found.append((number, "bool on a line of its own, which this check does not read: "
                                  "keep the return type on the name's line"))
            continue
        match = DECLARATION.match(line)
        if match is None:
            continue
        specifiers = match.group(1).split()
        if not header and "static" not in specifiers:
            continue
        if "MLOC_NODISCARD" not in specifiers:
            found.append((number, f"{match.group(2)} returns bool without MLOC_NODISCARD"))
    return found


def Scan(root):
    """Every problem in the C files under root/src, as printable lines."""
    lines = []
    for path in sorted((root / "src").rglob("*.[ch]")):
        text = path.read_text(encoding="utf-8")
        for number, message in Problems(text, path.suffix == ".h"):
            lines.append(f"{path.relative_to(root)}:{number}: {message}")
    return lines


def SelfTest():
    cases = [
        ("bool mlocA(int x);", True, 1),
        ("MLOC_NODISCARD bool mlocA(int x);", True, 0),
        ("static inline bool mlocB(void)", True, 1),
        ("MLOC_NODISCARD static inline bool mlocB(void)", True, 0),
        ("static bool Step(Parser* p)", False, 1),
        ("MLOC_NOINLINE static bool Step(Parser* p)", False, 1),
        ("MLOC_NODISCARD MLOC_NOINLINE static bool Step(Parser* p)", False, 0),
        ("bool mlocA(int x)", False, 0),
        ("static bool s_ready = false;", False, 0),
        ("    bool done;", True, 0),
        ("    bool ok = mlocA(1);", False, 0),
        ("typedef bool (*Visit)(void* context);", True, 0),
        # The shapes the check cannot read fail, marked or not.
        ("    MLOC_NODISCARD bool mlocA(int x);", True, 1),
        ("    static bool Step(Parser* p)", False, 1),
        ("bool\nmlocA(int x);", True, 1),
        ("MLOC_NODISCARD static bool\nStep(Parser* p)", False, 1),
    ]
    failed = 0
    for text, header, expected in cases:
        got = len(Problems(text, header))
        if got != expected:
            print(f"check_nodiscard self-test: {text!r} gave {got} problems, expected {expected}")
            failed += 1
    # A header in a subdirectory of src/ is read too.
    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        (root / "src" / "nested").mkdir(parents=True)
        (root / "src" / "nested" / "deep.h").write_text("bool mlocDeep(void);\n", encoding="utf-8")
        found = Scan(root)
        if len(found) != 1 or "src/nested/deep.h:1:" not in found[0]:
            print(f"check_nodiscard self-test: a header under src/nested gave {found}")
            failed += 1
    return failed == 0


def main():
    if "--self-test" in sys.argv[1:]:
        return 0 if SelfTest() else 1
    problems = Scan(ROOT)
    for line in problems:
        print(line)
    if problems:
        print(f"nodiscard: {len(problems)} bool declarations unmarked or unread")
        return 1
    print("nodiscard: every internal bool function carries MLOC_NODISCARD")
    return 0


if __name__ == "__main__":
    sys.exit(main())
