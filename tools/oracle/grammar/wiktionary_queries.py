# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen

"""The grammar oracle's cases from Wiktionary's inflection tables, as kaikki.org
extracts them (wiktextract JSONL; CC BY-SA, read here, never shipped).

  wiktionary_queries.py requests LANG DUMP OUT   writes OUT.req (kind, word,
      request per line, for maul_inflect) and OUT.ans (the table's forms,
      | between alternatives)
  wiktionary_queries.py compare OUT MAUL DIFFS   counts agreement per kind and
      request, writes the differences

Run with python3 -I; the dump is untrusted data and only parsed as JSON.
"""

import collections
import json
import sys

CASES = {
    "hu": {
        "nominative": "nom", "accusative": "acc", "dative": "dat",
        "instrumental": "ins", "causal-final": "cau", "translative": "tra",
        "terminative": "ter", "essive-formal": "ess", "inessive": "ine",
        "superessive": "sup", "adessive": "ade", "illative": "ill",
        "sublative": "sub", "delative": "del", "elative": "ela",
        "ablative": "abl", "allative": "all",
    },
    "fi": {
        "nominative": "nom", "genitive": "gen", "partitive": "par",
        "inessive": "ine", "elative": "ela", "illative": "ill",
        "adessive": "ade", "ablative": "abl", "allative": "all",
        "essive": "ess", "translative": "tra", "abessive": "abe",
        "instructive": "ins", "comitative": "com",
    },
}
PERSONS = {"first-person": 1, "second-person": 2, "third-person": 3}
SKIP = {"error-unrecognized-form", "table-tags", "inflection-template", "class",
        "romanization", "obsolete", "archaic", "dialectal", "rare"}


def request(lang, tags):
    """The request a form's tags name, or None."""
    tags = set(tags)
    if tags & SKIP:
        return None
    cases = [CASES[lang][t] for t in tags if t in CASES[lang]]
    if "possessive" in tags:
        person = [PERSONS[t] for t in tags if t in PERSONS]
        if len(person) != 1:
            return None
        if lang == "hu":
            many = "possessed-many" in tags
            owners = "plural" in tags
            parts = ["nom"] + (["pl"] if many else []) + ["px%d%s" % (person[0], "p" if owners else "")]
            return ".".join(parts)
        return None
    if len(cases) != 1:
        return None
    number = "pl" if "plural" in tags else "" if "singular" in tags else None
    if number is None:
        return None
    return cases[0] + ("." + number if number else "")


def requests(lang, dump, out):
    kinds = {"noun": "noun", "name": "name"}
    with open(dump, encoding="utf-8") as f, open(out + ".req", "w", encoding="utf-8") as req, \
            open(out + ".ans", "w", encoding="utf-8") as ans:
        seen = set()
        for line in f:
            try:
                entry = json.loads(line)
            except ValueError:
                continue
            kind = kinds.get(entry.get("pos"))
            word = entry.get("word", "")
            if kind is None or " " in word or (kind, word) in seen:
                continue
            forms = collections.defaultdict(list)
            for form in entry.get("forms", []):
                r = request(lang, form.get("tags", []))
                text = form.get("form", "")
                if r is None or not text or text == "-" or " " in text:
                    continue
                if text not in forms[r]:
                    forms[r].append(text)
            if len(forms) < 10:
                continue
            seen.add((kind, word))
            for r, texts in forms.items():
                if r == "nom":
                    continue
                req.write("%s\t%s\t%s\n" % (kind, word, r))
                ans.write("|".join(texts) + "\n")


def compare(out, maul, diffs):
    with open(out + ".req", encoding="utf-8") as f:
        reqs = [l.rstrip("\n").split("\t") for l in f]
    with open(out + ".ans", encoding="utf-8") as f:
        answers = [l.rstrip("\n").split("|") for l in f]
    with open(maul, encoding="utf-8") as f:
        mine = [l.rstrip("\n") for l in f]
    counts = collections.defaultdict(collections.Counter)
    with open(diffs, "w", encoding="utf-8") as d:
        for (kind, word, r), forms, got in zip(reqs, answers, mine):
            if got.startswith("!"):
                counts[(kind, r)]["refused"] += 1
            elif got in forms:
                counts[(kind, r)]["agree"] += 1
            else:
                counts[(kind, r)]["differ"] += 1
                d.write("%s\t%s\t%s\t%s\t%s\n" % (kind, r, word, got, "|".join(forms)))
    for key in sorted(counts):
        c = counts[key]
        total = sum(c.values())
        print("%s %-14s %6d  agree %5.1f%%  differ %5d  refused %5d" % (
            key[0], key[1], total, 100.0 * c["agree"] / total, c["differ"], c["refused"]))


if __name__ == "__main__":
    if sys.argv[1] == "requests":
        requests(sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        compare(sys.argv[2], sys.argv[3], sys.argv[4])
