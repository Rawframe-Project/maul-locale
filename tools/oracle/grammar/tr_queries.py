#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# The grammar oracle's queries: the names and nouns of an Apertium Turkic
# lexicon (tur, kaz, kir), each with the requests below, as hfst-lookup queries and as
# lines for maul_inflect (kind, word, request).
# usage: tr_queries.py LEXC OUT_QUERIES OUT_REQUESTS
import re, sys
lexc, queries, requests = sys.argv[1:4]
CLASSES = {'NP-ANT-M': ('name', '<np><ant><m>'), 'NP-ANT-F': ('name', '<np><ant><f>'),
           'NP-TOP': ('name', '<np><top>'), 'NP-COG-MF': ('name', '<np><cog><mf>'),
           'N1': ('noun', '<n>'), 'N-INFL': ('noun', '<n>')}
REQ = [('gen', '<gen>'), ('dat', '<dat>'), ('acc', '<acc>'), ('loc', '<loc>'), ('abl', '<abl>'),
       ('ins', '<ins>'), ('pl.dat', '<pl><dat>'), ('px3.dat', '<px3sp><dat>'),
       ('px1.acc', '<px1sg><acc>')]
entry = re.compile(r'^([^!:;%\s]+):\S+\s+([A-Z0-9-]+)\s*;')
seen = set()
with open(lexc, encoding='utf-8') as f, open(queries, 'w', encoding='utf-8') as q, \
        open(requests, 'w', encoding='utf-8') as r:
    for line in f:
        m = entry.match(line)
        if not m or m.group(2) not in CLASSES:
            continue
        word = m.group(1)
        kind, tags = CLASSES[m.group(2)]
        if (word, kind) in seen or not word.isalpha():
            continue
        seen.add((word, kind))
        for name, t in REQ:
            q.write(f'{word}{tags}{t}\n')
            r.write(f'{kind}\t{word}\t{name}\n')
print(len(seen), 'words')
