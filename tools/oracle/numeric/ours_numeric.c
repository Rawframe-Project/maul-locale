// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Lines "locale unit amount | ...": the library's numeric width.
#include "maul-locale/data.h"
#include "maul-locale/unit.h"

#include <stdio.h>
#include <string.h>
int main(void)
{
    static mlocData s[2];
    mlocOpenBuiltinData(&s[0]);
    FILE* f = fopen("src/generated/units.mloc", "rb");
    static char b[8 << 20];
    size_t n = fread(b, 1, sizeof b, f);
    fclose(f);
    if (mlocOpenData(&s[1], b, n) != mloc_success)
    {
        return 1;
    }
    mlocDataSet d = {s, 2};
    char line[512];
    while (fgets(line, sizeof line, stdin))
    {
        char* bar = strstr(line, " | ");
        if (bar)
        {
            *bar = 0;
        }
        char loc[64], unit[64], amount[64];
        if (sscanf(line, "%63s %63s %63s", loc, unit, amount) != 3)
        {
            continue;
        }
        mlocLocale l;
        mlocUnitOptions o = {.width = mloc_unitWidthNumeric};
        mlocUnitFormatter u;
        mlocDecimal x;
        char out[256];
        mlocOutput op = {.buffer = out, .capacity = sizeof out};
        if (mlocParseLocale(loc, strlen(loc), &l).status < 0 ||
            mlocInitUnitFormatter(&u, &d, &l, unit, strlen(unit), &o) < 0 ||
            mlocParseDecimal(amount, strlen(amount), &x).status < 0 ||
            mlocFormatUnit(&u, &x, &op) < 0)
        {
            printf("%s\tERR\n", line);
            continue;
        }
        printf("%s\t%.*s\n", line, (int)op.length, out);
    }
}
