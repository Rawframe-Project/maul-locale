// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "maul-locale/currency.h"

#include <stdio.h>
#include <string.h>
int main(void)
{
    static mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    static _Alignas(8) unsigned char names[4 << 20];
    FILE* file = fopen("src/generated/currency-names.mloc", "rb");
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
        char loc[64], code[8], display[16], a[64], b[64];
        if (sscanf(line, "%63s %7s %15s %63s %63s", loc, code, display, a, b) != 5)
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
        mlocCurrencyOptions o = {.display = strcmp(display, "narrow") == 0
                                                ? mloc_currencyNarrowSymbol
                                            : strcmp(display, "code") == 0 ? mloc_currencyCode
                                            : strcmp(display, "name") == 0 ? mloc_currencyName
                                                                           : mloc_currencySymbol};
        mlocCurrencyFormatter f;
        mlocDecimal x, y;
        char out[256];
        mlocOutput output = {.buffer = out, .capacity = sizeof(out)};
        if (mlocParseLocale(loc, strlen(loc), &locale).status != mloc_success ||
            mlocInitCurrencyFormatter(&data, &locale, code, strlen(code), &o, &f) < 0 ||
            mlocParseDecimal(a, strlen(a), &x).status != mloc_success ||
            mlocParseDecimal(b, strlen(b), &y).status != mloc_success ||
            mlocCurrencyFormatter_FormatRange(&f, &x, &y, &output) != mloc_success)
        {
            printf("%s\tERR\n", line);
            continue;
        }
        printf("%s\t%.*s\n", line, (int)output.length, out);
    }
    return 0;
}
