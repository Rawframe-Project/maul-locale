#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes test/mf2_suite.inc from the MessageFormat 2 test suite in CLDR's
# common/testData/messageFormat: one row per case, with its file, locale,
# bidi isolation, message, expected text and errors, and arguments, for
# test_message to run.
#
# usage: gen_mf2_suite.py <cldr> <commit> > test/mf2_suite.inc

import glob
import json
import os
import sys

HEADER = """// SPDX-License-Identifier: Unicode-3.0
// Copyright (C) Unicode, Inc. and others
//
// Generated from CLDR at {commit} (common/testData/messageFormat) by
// tools/gen_mf2_suite.py; do not edit. The license is in
// LICENSES/Unicode-3.0.txt.
//
// Every case of the MessageFormat 2 test suite, one row each: the file,
// the locale, whether bidi isolation is on, the message and its length,
// the expected text (null when the case gives none), the expected errors'
// types joined by commas, and the arguments, each a name, a type (s
// string, i integer, d double, b boolean, t date-time string) and a value
// joined by U+001F, the arguments joined by U+001E. Bytes beyond
// printable ASCII are octal escapes.
//
// {count} rows from {files} files.
"""


def c_string(text):
    out = []
    for byte in text.encode("utf-8"):
        char = chr(byte)
        if char in '"\\':
            out.append("\\" + char)
        elif 0x20 <= byte < 0x7F and char != "?":
            out.append(char)
        else:
            out.append("\\%03o" % byte)
    return '"' + "".join(out) + '"'


def argument(param):
    value = param["value"]
    if param.get("type") == "datetime":
        kind = "t"
    elif isinstance(value, bool):
        kind, value = "b", "true" if value else "false"
    elif isinstance(value, int):
        kind = "i"
    elif isinstance(value, float):
        kind = "d"
    else:
        kind = "s"
    return "\x1f".join([param["name"], kind, str(value)])


def rows(root):
    files = sorted(glob.glob(os.path.join(root, "tests", "**", "*.json"), recursive=True))
    for path in files:
        with open(path, encoding="utf-8") as stream:
            suite = json.load(stream)
        defaults = suite.get("defaultTestProperties", {})
        name = os.path.relpath(path, os.path.join(root, "tests"))
        for case in suite["tests"]:
            def get(key, default=None):
                return case.get(key, defaults.get(key, default))
            errors = get("expErrors", [])
            if isinstance(errors, list):
                errors = ",".join(error["type"] for error in errors)
            else:
                errors = "any" if errors else ""
            expected = case.get("exp")
            yield name, [
                c_string(name),
                c_string(get("locale", "en-US")),
                "true" if get("bidiIsolation", "none") == "default" else "false",
                c_string(case["src"]),
                str(len(case["src"].encode("utf-8"))),
                "NULL" if expected is None else c_string(expected),
                c_string(errors),
                c_string("\x1e".join(argument(p) for p in get("params", []))),
            ]
    return files


def main():
    root = os.path.join(sys.argv[1], "common", "testData", "messageFormat")
    table = list(rows(root))
    names = sorted({name for name, _ in table})
    sys.stdout.write(HEADER.format(commit=sys.argv[2], count=len(table), files=len(names)))
    sys.stdout.write("\n// clang-format off\n")
    for _, row in table:
        sys.stdout.write("{" + ", ".join(row) + "},\n")
    sys.stdout.write("// clang-format on\n")


if __name__ == "__main__":
    main()
