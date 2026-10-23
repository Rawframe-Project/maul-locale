# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# crashsafe.py PROGRAM IN OUT: runs PROGRAM on IN's lines, restarting after
# a crash; a line that crashes it is written as "line\tCRASH".
import subprocess, sys
prog, src, dst = sys.argv[1:4]
lines = open(src).read().splitlines()
out = []
at = 0
while at < len(lines):
    p = subprocess.run([prog], input="\n".join(lines[at:]) + "\n", capture_output=True, text=True)
    got = p.stdout.splitlines()
    out += got
    at += len(got)
    if p.returncode != 0 and at < len(lines):
        out.append(lines[at] + "\tCRASH")
        at += 1
open(dst, "w").write("\n".join(out) + "\n")
