// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Our side of the currency parse oracle: reads
// "locale<TAB>code<TAB>mode<TAB>any<TAB>text" lines with the builtin data
// and the names file given as the first argument; prints the line, the
// value (digits"e"exponent), the currency read with any (else "-") and
// the end, or "fail" and the offset, or the error.
#include "maul-locale/currency.h"
#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char** argv)
{
    static mlocData s[2];
    mlocOpenBuiltinData(&s[0]);
    size_t count = 1;
    static char names[4 << 20];
    if (argc > 1)
    {
        FILE* f = fopen(argv[1], "rb");
        if (!f)
        {
            return 1;
        }
        size_t n = fread(names, 1, sizeof names, f);
        fclose(f);
        if (mlocOpenData(names, n, &s[1]) != mloc_success)
        {
            return 2;
        }
        count = 2;
    }
    mlocDataSet d = {s, count};
    static char line[1 << 16];
    while (fgets(line, sizeof line, stdin))
    {
        size_t n = strlen(line);
        if (n && line[n - 1] == '\n')
        {
            line[--n] = 0;
        }
        char* f[5] = {line, 0, 0, 0, 0};
        for (int i = 1; i < 5; i++)
        {
            f[i] = strchr(f[i - 1], '\t');
            if (!f[i])
            {
                break;
            }
            *f[i]++ = 0;
        }
        if (!f[4])
        {
            continue;
        }
        mlocLocale l;
        (void)mlocParseLocale(f[0], strlen(f[0]), &l);
        mlocCurrencyFormatter cf;
        printf("%s\t%s\t%s\t%s\t%s\t", f[0], f[1], f[2], f[3], f[4]);
        mlocResult r = mlocInitCurrencyFormatter(&d, &l, f[1], 3, nullptr, &cf);
        if (r < 0)
        {
            printf("ERR %d\n", r);
            continue;
        }
        mlocDecimal x;
        char code[3];
        size_t end = 0;
        int any = f[3][0] == '1';
        r = mlocCurrencyFormatter_Parse(
            &cf, f[4], strlen(f[4]), strcmp(f[2], "strict") ? mloc_parseLenient : mloc_parseStrict,
            (uint8_t)any, &x, code, &end);
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
        printf("%s", x.negative ? "-" : "");
        if (x.count == 0)
        {
            printf("0");
        }
        for (int i = 0; i < x.count; i++)
        {
            putchar('0' + x.digits[i]);
        }
        printf("e%d\t%.*s\t%zu\n", x.exponent, any ? 3 : 1, any ? code : "-", end);
    }
}
