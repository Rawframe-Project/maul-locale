// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Formats the currency oracle's values: reads
// "locale<TAB>code<TAB>display<TAB>value" lines (display symbol, narrow,
// code or name), prints "locale<TAB>code<TAB>text"; the names file is
// the first argument.
#include "maul-locale/currency.h"
#include "maul-locale/data.h"

#include <stdio.h>
#include <string.h>
int main(int argc, char** argv)
{
    static mlocData s[2];
    mlocOpenBuiltinData(&s[0]);
    static char names[4 << 20];
    FILE* f = fopen(argv[1], "rb");
    if (!f)
    {
        return 1;
    }
    size_t n = fread(names, 1, sizeof names, f);
    fclose(f);
    if (mlocOpenData(&s[1], names, n) != mloc_success)
    {
        return 2;
    }
    mlocDataSet d = {s, 2};
    static char line[4096];
    while (fgets(line, sizeof line, stdin))
    {
        size_t len = strlen(line);
        if (len && line[len - 1] == '\n')
        {
            line[--len] = 0;
        }
        char* x[4] = {line, 0, 0, 0};
        for (int i = 1; i < 4; i++)
        {
            x[i] = strchr(x[i - 1], '\t');
            if (!x[i])
            {
                break;
            }
            *x[i]++ = 0;
        }
        if (!x[3])
        {
            continue;
        }
        mlocLocale l;
        (void)mlocParseLocale(x[0], strlen(x[0]), &l);
        mlocCurrencyOptions o = {0};
        o.display = !strcmp(x[2], "narrow") ? mloc_currencyNarrowSymbol
                    : !strcmp(x[2], "code") ? mloc_currencyCode
                    : !strcmp(x[2], "name") ? mloc_currencyName
                                            : mloc_currencySymbol;
        mlocCurrencyFormatter cf;
        mlocDecimal v;
        char out[256];
        mlocOutput op = {.buffer = out, .capacity = sizeof out};
        if (mlocInitCurrencyFormatter(&cf, &d, &l, x[1], 3, &o) < 0 ||
            mlocParseDecimal(x[3], strlen(x[3]), &v).status < 0 ||
            mlocFormatCurrency(&cf, &v, &op) < 0)
        {
            continue;
        }
        printf("%s\t%s\t%.*s\n", x[0], x[1], (int)op.length, out);
    }
}
