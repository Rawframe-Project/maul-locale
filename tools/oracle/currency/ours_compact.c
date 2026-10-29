// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "maul-locale/currency.h"

#include <stdio.h>
#include <string.h>
int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    char line[512];
    while (fgets(line, sizeof(line), stdin) != NULL)
    {
        line[strcspn(line, "\n")] = '\0';
        char loc[64], code[8], display[16], len[8], value[64];
        if (sscanf(line, "%63s %7s %15s %7s %63s", loc, code, display, len, value) != 5)
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
        mlocCurrencyOptions o = {.number.notation = strcmp(len, "long") == 0
                                                        ? mloc_notationCompactLong
                                                        : mloc_notationCompactShort};
        const char* names[] = {"symbol", "narrow", "code", "formal", "variant", "hidden"};
        for (int i = 0; i < 6; i++)
        {
            if (strcmp(display, names[i]) == 0)
            {
                o.display = (mlocCurrencyDisplay)i;
            }
        }
        mlocCurrencyFormatter f;
        mlocDecimal d;
        char out[256];
        mlocOutput output = {.buffer = out, .capacity = sizeof(out)};
        if (mlocParseLocale(loc, strlen(loc), &locale).status != mloc_success ||
            mlocInitCurrencyFormatter(&data, &locale, code, strlen(code), &o, &f) < 0 ||
            mlocParseDecimal(value, strlen(value), &d).status != mloc_success ||
            mlocCurrencyFormatter_Format(&f, &d, &output) != mloc_success)
        {
            printf("%s\tERR\n", line);
            continue;
        }
        printf("%s\t%.*s\n", line, (int)output.length, out);
    }
}
