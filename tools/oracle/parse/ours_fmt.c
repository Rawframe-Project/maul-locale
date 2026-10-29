// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Formats the oracle's values: reads "locale<TAB>style<TAB>value" lines,
// prints "locale<TAB>style<TAB>text".
#include "maul-locale/data.h"
#include "maul-locale/number.h"

#include <stdio.h>
#include <string.h>
int main(void)
{
    static mlocData s[1];
    mlocOpenBuiltinData(&s[0]);
    mlocDataSet d = {s, 1};
    static char line[4096];
    while (fgets(line, sizeof line, stdin))
    {
        size_t n = strlen(line);
        if (n && line[n - 1] == '\n')
        {
            line[--n] = 0;
        }
        char* f[3] = {line, 0, 0};
        for (int i = 1; i < 3; i++)
        {
            f[i] = strchr(f[i - 1], '\t');
            if (!f[i])
            {
                break;
            }
            *f[i]++ = 0;
        }
        if (!f[2])
        {
            continue;
        }
        mlocLocale l;
        (void)mlocParseLocale(f[0], strlen(f[0]), &l);
        mlocNumberOptions o = {0};
        if (!strcmp(f[1], "percent"))
        {
            o.style = mloc_numberPercent;
        }
        else if (!strcmp(f[1], "permille"))
        {
            o.style = mloc_numberPerMille;
        }
        else if (!strcmp(f[1], "scientific"))
        {
            o.notation = mloc_notationScientific;
        }
        else if (!strcmp(f[1], "plus"))
        {
            o.signDisplay = mloc_signAlways;
        }
        mlocNumberFormatter nf;
        mlocDecimal x;
        char out[256];
        mlocOutput op = {.buffer = out, .capacity = sizeof out};
        if (mlocInitNumberFormatter(&d, &l, &o, &nf) < 0 ||
            mlocParseDecimal(f[2], strlen(f[2]), &x).status < 0 ||
            mlocNumberFormatter_Format(&nf, &x, &op) < 0)
        {
            continue;
        }
        printf("%s\t%s\t%.*s\n", f[0], f[1], (int)op.length, out);
    }
}
