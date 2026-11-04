#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks that every internal function returning bool carries
# MLOC_NODISCARD: each one declared in a header of src/, and each static
# one in a source file of src/. A bool answers or refuses, and a caller
# that drops it goes on past a refusal or with an out value left unset;
# a call that means to drop it says so with (void). The generated files
# are left to their generators. Run with --self-test to check the
# matching on its own examples.

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# A function declaration or definition starting at the line's first
# column: its specifiers and macros, bool, the name and its parenthesis.
DECLARATION = re.compile(r"^((?:(?:MLOC_\w+|static|inline|extern)\s+)*)bool\s+(\w+)\s*\(")


def Missing(text, header):
    """The (line, name) of each bool function the text declares without
    MLOC_NODISCARD: every one in a header, the static ones elsewhere."""
    found = []
    for number, line in enumerate(text.splitlines(), 1):
        match = DECLARATION.match(line)
        if match is None:
            continue
        specifiers = match.group(1).split()
        if not header and "static" not in specifiers:
            continue
        if "MLOC_NODISCARD" not in specifiers:
            found.append((number, match.group(2)))
    return found


def SelfTest():
    cases = [
        ("bool mlocA(int x);", True, ["mlocA"]),
        ("MLOC_NODISCARD bool mlocA(int x);", True, []),
        ("static inline bool mlocB(void)", True, ["mlocB"]),
        ("MLOC_NODISCARD static inline bool mlocB(void)", True, []),
        ("static bool Step(Parser* p)", False, ["Step"]),
        ("MLOC_NOINLINE static bool Step(Parser* p)", False, ["Step"]),
        ("MLOC_NODISCARD MLOC_NOINLINE static bool Step(Parser* p)", False, []),
        ("bool mlocA(int x)", False, []),
        ("static bool s_ready = false;", False, []),
        ("    bool done;", True, []),
        ("typedef bool (*Visit)(void* context);", True, []),
    ]
    failed = 0
    for text, header, expected in cases:
        names = [name for _, name in Missing(text, header)]
        if names != expected:
            print(f"check_nodiscard self-test: {text!r} gave {names}, expected {expected}")
            failed += 1
    return failed == 0


def main():
    if "--self-test" in sys.argv[1:]:
        return 0 if SelfTest() else 1
    missing = []
    for path in sorted((ROOT / "src").glob("*.[ch]")):
        text = path.read_text(encoding="utf-8")
        for number, name in Missing(text, path.suffix == ".h"):
            missing.append(f"{path.relative_to(ROOT)}:{number}: {name} returns bool without MLOC_NODISCARD")
    for line in missing:
        print(line)
    if missing:
        print(f"nodiscard: {len(missing)} bool functions lack MLOC_NODISCARD")
        return 1
    print("nodiscard: every internal bool function carries MLOC_NODISCARD")
    return 0


if __name__ == "__main__":
    sys.exit(main())
