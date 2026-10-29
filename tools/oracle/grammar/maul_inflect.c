// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The grammar oracle, the library's side: reads lines of kind (name
// or noun), word and request (gen, dat, ..., pl.dat, px3.dat, px1.acc,
// acc.pl.px2p) and writes the inflected form, or "!" and the result when
// it is refused.
// usage: maul_inflect SPELLOUT_FILE [LOCALE, tr by default] < requests
#include "maul-locale/grammar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char s_rules[8 << 20];

int main(int argc, char** argv)
{
    static mlocData d[2];
    mlocOpenBuiltinData(&d[0]);
    FILE* f = fopen(argv[1], "rb");
    size_t n = f == NULL ? 0 : fread(s_rules, 1, sizeof s_rules, f);
    if (f != NULL)
    {
        fclose(f);
    }
    if (argc < 2 || mlocOpenData(s_rules, n, &d[1]) != mloc_success)
    {
        return 2;
    }
    mlocDataSet set = {d, 2};
    const char* id = argc > 2 ? argv[2] : "tr";
    mlocLocale tr;
    mlocInflector inf;
    if (mlocParseLocale(id, strlen(id), &tr).status != mloc_success ||
        mlocInitInflector(&set, &tr, &inf) != mloc_success)
    {
        return 2;
    }
    char line[512];
    while (fgets(line, sizeof line, stdin) != NULL)
    {
        line[strcspn(line, "\n")] = 0;
        char* kind = strtok(line, "\t");
        char* word = strtok(NULL, "\t");
        char* req = strtok(NULL, "\t");
        if (kind == NULL || word == NULL || req == NULL)
        {
            continue;
        }
        mlocInflection r;
        memset(&r, 0, sizeof r);
        r.commonNoun = strcmp(kind, "noun") == 0;
        // The request: dot-separated parts, a case (gen, dat, acc, loc,
        // abl, ins), pl, and a possessor
        // pxN (singular) or pxNp (plural); px3.dat and px1.acc as before.
        r.grammaticalCase = mloc_caseNominative;
        for (char* part = strtok(req, "."); part != NULL; part = strtok(NULL, "."))
        {
            static const struct
            {
                const char* name;
                mlocGrammaticalCase c;
            } s_cases[] = {{"nom", mloc_caseNominative},   {"gen", mloc_caseGenitive},
                           {"dat", mloc_caseDative},       {"acc", mloc_caseAccusative},
                           {"loc", mloc_caseLocative},     {"abl", mloc_caseAblative},
                           {"ins", mloc_caseInstrumental}, {"cau", mloc_caseCausal},
                           {"tra", mloc_caseTranslative},  {"ter", mloc_caseTerminative},
                           {"ess", mloc_caseEssive},       {"ine", mloc_caseInessive},
                           {"sup", mloc_caseSuperessive},  {"ade", mloc_caseAdessive},
                           {"ill", mloc_caseIllative},     {"sub", mloc_caseSublative},
                           {"del", mloc_caseDelative},     {"ela", mloc_caseElative},
                           {"all", mloc_caseAllative},     {"par", mloc_casePartitive},
                           {"abe", mloc_caseAbessive},     {"com", mloc_caseComitative}};
            if (strcmp(part, "pl") == 0)
            {
                r.plural = true;
            }
            else if (strncmp(part, "px", 2) == 0)
            {
                r.possessorPerson = part[2] - '0';
                r.possessorPlural = part[3] == 'p';
            }
            else
            {
                for (size_t i = 0; i < sizeof s_cases / sizeof s_cases[0]; i++)
                {
                    if (strcmp(part, s_cases[i].name) == 0)
                    {
                        r.grammaticalCase = s_cases[i].c;
                    }
                }
            }
        }
        char out[512];
        mlocOutput o = {out, sizeof out, 0, NULL, NULL};
        mlocResult res = mlocInflector_InflectText(&inf, word, strlen(word), &r, &o);
        if (res == mloc_success)
        {
            printf("%.*s\n", (int)o.length, out);
        }
        else
        {
            printf("!%d\n", (int)res);
        }
    }
    return 0;
}
