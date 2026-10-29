// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Our side of the parse oracle: reads "locale<TAB>style<TAB>mode<TAB>text"
// lines and prints the line, a tab, the value (digits"e"exponent, NaN,
// Infinity, -Infinity, with "-" for negative) and the end, or "fail" and
// the offset, or the error.
#include "maul-locale/data.h"
#include "maul-locale/number.h"

#include <stdio.h>
#include <string.h>
int main(void)
{
    static mlocData s[1];
    mlocOpenBuiltinData(&s[0]);
    mlocDataSet d = {s, 1};
    static char line[1 << 16];
    while (fgets(line, sizeof line, stdin))
    {
        size_t n = strlen(line);
        if (n && line[n - 1] == '\n')
        {
            line[--n] = 0;
        }
        char* f[4] = {line, 0, 0, 0};
        for (int i = 1; i < 4; i++)
        {
            f[i] = strchr(f[i - 1], '\t');
            if (!f[i])
            {
                break;
            }
            *f[i]++ = 0;
        }
        if (!f[3])
        {
            continue;
        }
        mlocLocale l;
        mlocTextResult lr = mlocParseLocale(f[0], strlen(f[0]), &l);
        (void)lr;
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
        mlocResult r = mlocInitNumberFormatter(&d, &l, &o, &nf);
        printf("%s\t%s\t%s\t%s\t", f[0], f[1], f[2], f[3]);
        if (r < 0)
        {
            printf("ERR %d\n", r);
            continue;
        }
        mlocDecimal x;
        size_t end = 0;
        r = mlocNumberFormatter_Parse(&nf, f[3], strlen(f[3]),
                                      strcmp(f[2], "strict") ? mloc_parseLenient : mloc_parseStrict,
                                      &x, &end);
        if (r == mloc_errorSyntax)
        {
            printf("fail\t%zu\n", end);
            continue;
        }
        if (r < 0)
        {
            printf("ERR %d\n", r);
            continue;
        }
        if (x.kind == mloc_decimalNaN)
        {
            printf("NaN");
        }
        else if (x.kind == mloc_decimalInfinity)
        {
            printf("%sInfinity", x.negative ? "-" : "");
        }
        else
        {
            printf("%s", x.negative ? "-" : "");
            if (x.count == 0)
            {
                printf("0");
            }
            for (int i = 0; i < x.count; i++)
            {
                putchar('0' + x.digits[i]);
            }
            printf("e%d", x.exponent);
        }
        printf("\t%zu\n", end);
    }
}
