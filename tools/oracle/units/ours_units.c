// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's side of icu_units.cpp: the same lines, the same output.
#include "maul-locale/unit.h"

#include <stdio.h>
#include <string.h>
typedef struct Parts
{
    const char* text;
    char fields[512];
    size_t used;
} Parts;
static void Part(void* context, uint32_t kind, size_t start, size_t length)
{
    Parts* p = context;
    if ((kind & 0xFF) != mloc_numberPartUnit)
    {
        return;
    }
    p->used += (size_t)snprintf(p->fields + p->used, sizeof(p->fields) - p->used, "%s%.*s",
                                p->used ? "|" : "", (int)length, p->text + start);
}
int main(void)
{
    static mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    static _Alignas(8) unsigned char names[8 << 20];
    FILE* file = fopen("src/generated/units.mloc", "rb");
    size_t size = file ? fread(names, 1, sizeof(names), file) : 0;
    if (file)
    {
        fclose(file);
    }
    if (mlocOpenData(&sources[1], names, size) != mloc_success)
    {
        fprintf(stderr, "names\n");
        return 1;
    }
    mlocDataSet data = {sources, 2};
    char line[512];
    while (fgets(line, sizeof(line), stdin) != NULL)
    {
        line[strcspn(line, "\n")] = '\0';
        char loc[64], unit[80], width[16], a[64], b[64] = "";
        int n = sscanf(line, "%63s %79s %15s %63s %63s", loc, unit, width, a, b);
        if (n < 4)
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
        mlocLocale locale;
        mlocUnitOptions o = {.width = strcmp(width, "long") == 0     ? mloc_unitWidthLong
                                      : strcmp(width, "narrow") == 0 ? mloc_unitWidthNarrow
                                                                     : mloc_unitWidthShort};
        mlocUnitFormatter f;
        mlocDecimal x, y;
        char out[512];
        Parts parts = {.text = out};
        mlocOutput output = {.buffer = out, .capacity = sizeof(out)};
        mlocResult r = mloc_errorInvalid;
        if (mlocParseLocale(loc, strlen(loc), &locale).status == mloc_success &&
            mlocInitUnitFormatter(&f, &data, &locale, unit, strlen(unit), &o) >= 0 &&
            mlocParseDecimal(a, strlen(a), &x).status == mloc_success)
        {
            if (n == 4)
            {
                r = mlocFormatUnit(&f, &x, &output);
                output.part = Part;
                output.context = &parts;
                if (r == mloc_success)
                {
                    r = mlocFormatUnit(&f, &x, &output);
                }
            }
            else if (mlocParseDecimal(b, strlen(b), &y).status == mloc_success)
            {
                r = mlocFormatUnitRange(&f, &x, &y, &output);
                output.part = Part;
                output.context = &parts;
                if (r == mloc_success)
                {
                    r = mlocFormatUnitRange(&f, &x, &y, &output);
                }
            }
        }
        if (r != mloc_success)
        {
            printf("%s\tERR\n", line);
            continue;
        }
        printf("%s\t%.*s\t%s\n", line, (int)output.length, out, parts.fields);
    }
    return 0;
}
