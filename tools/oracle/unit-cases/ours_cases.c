// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's side of icu_cases.cpp: the same lines; the text, then a
// tab and mlocGetUnitGender's gender ("undefined" for none).
#include "maul-locale/unit.h"

#include <stdio.h>
#include <string.h>
static const char* const s_cases[] = {"nominative",
                                      "abessive",
                                      "ablative",
                                      "accusative",
                                      "adessive",
                                      "allative",
                                      "causal",
                                      "comitative",
                                      "dative",
                                      "delative",
                                      "elative",
                                      "ergative",
                                      "essive",
                                      "genitive",
                                      "illative",
                                      "inessive",
                                      "instrumental",
                                      "locative",
                                      "locativecopulative",
                                      "oblique",
                                      "partitive",
                                      "prepositional",
                                      "sociative",
                                      "sublative",
                                      "superessive",
                                      "terminative",
                                      "translative",
                                      "vocative"};
int main(int argc, char** argv)
{
    static mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    static _Alignas(8) unsigned char names[8 << 20];
    FILE* file = fopen(argc > 1 ? argv[1] : "units.mloc", "rb");
    size_t size = file ? fread(names, 1, sizeof(names), file) : 0;
    if (file)
    {
        fclose(file);
    }
    if (mlocOpenData(names, size, &sources[1]) != mloc_success)
    {
        fprintf(stderr, "names\n");
        return 1;
    }
    mlocDataSet data = {sources, 2};
    char line[512];
    while (fgets(line, sizeof(line), stdin) != NULL)
    {
        line[strcspn(line, "\n")] = '\0';
        char loc[64], unit[80], width[16], gcase[32], a[64];
        if (sscanf(line, "%63s %79s %15s %31s %63s", loc, unit, width, gcase, a) != 5)
        {
            continue;
        }
        for (char* c = loc; *c; c++)
        {
            if (*c == '_')
            {
                *c = '-';
            }
        }
        int k = 0;
        while (k < 28 && strcmp(gcase, s_cases[k]) != 0)
        {
            k++;
        }
        mlocUnitOptions o = {
            .width = strcmp(width, "long") == 0     ? mloc_unitWidthLong
                     : strcmp(width, "narrow") == 0 ? mloc_unitWidthNarrow
                                                    : mloc_unitWidthShort,
            .grammaticalCase = (mlocGrammaticalCase)(k < 28 ? k : 0),
        };
        mlocLocale locale;
        mlocUnitFormatter f;
        mlocDecimal x;
        char out[512];
        mlocOutput output = {.buffer = out, .capacity = sizeof(out)};
        mlocResult r = mloc_errorInvalid;
        if (mlocParseLocale(loc, strlen(loc), &locale).status == mloc_success &&
            mlocInitUnitFormatter(&data, &locale, unit, strlen(unit), &o, &f) >= 0 &&
            mlocParseDecimal(a, strlen(a), &x).status == mloc_success)
        {
            r = mlocUnitFormatter_Format(&f, &x, &output);
        }
        static const char* const s_genders[] = {"undefined", "animate",   "common", "feminine",
                                                "inanimate", "masculine", "neuter", "personal"};
        mlocGender g = mloc_genderNone;
        bool gendered = mlocGetUnitGender(&data, &locale, unit, strlen(unit), &g) == mloc_success;
        if (r != mloc_success)
        {
            printf("%s\tERR\t\n", line);
            continue;
        }
        printf("%s\t%.*s\t%s\n", line, (int)output.length, out, gendered ? s_genders[g] : "ERR");
    }
    return 0;
}
