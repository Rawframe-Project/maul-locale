#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Compares Maul's forms with apertium-tur's: a line agrees when Maul's
# form is among the generator's (it gives several for some requests).
# usage: tr_compare.py QUERIES ANSWERS REQUESTS MAUL OUT_DIFFS
import collections, sys
queries, answers, requests, maul, diffs = sys.argv[1:6]
forms = collections.defaultdict(set)
with open(answers, encoding='utf-8') as f:
    for line in f:
        parts = line.rstrip('\n').split('\t')
        if len(parts) >= 2 and not parts[1].endswith('+?'):
            forms[parts[0]].add(parts[1])
q = open(queries, encoding='utf-8').read().split('\n')
r = open(requests, encoding='utf-8').read().split('\n')
m = open(maul, encoding='utf-8').read().split('\n')
count = collections.Counter()
with open(diffs, 'w', encoding='utf-8') as out:
    for query, req, ours in zip(q, r, m):
        if not query:
            continue
        kind, word, name = req.split('\t')
        theirs = forms.get(query)
        if not theirs:
            count[(kind, name, 'no oracle form')] += 1
            continue
        key = (kind, name, 'agree' if ours in theirs else 'differ')
        count[key] += 1
        if ours not in theirs:
            out.write(f'{kind}\t{name}\t{word}\t{ours}\t{"|".join(sorted(theirs))}\n')
for k in sorted(count):
    print(*k, count[k], sep='\t')
