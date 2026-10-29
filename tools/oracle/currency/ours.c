// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Ours for the currency oracle: the same lines as icu_cur reads.
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
        char loc[64], code[8], display[16], sign[16], usage[16], value[64], id[96];
        if (sscanf(line, "%63s %7s %15s %15s %15s %63s", loc, code, display, sign, usage, value) !=
            6)
        {
            continue;
        }
        snprintf(id, sizeof(id), "%s%s", loc, strcmp(sign, "account") == 0 ? "-u-cf-account" : "");
        for (char* c = id; *c; c++)
        {
            if (*c == '_')
            {
                *c = '-';
            }
        }
        mlocLocale locale;
        mlocCurrencyOptions o = {0};
        const char* displays[] = {"symbol",  "narrow", "code", "formal",
                                  "variant", "hidden", "name"};
        for (int i = 0; i < 7; i++)
        {
            if (strcmp(display, displays[i]) == 0)
            {
                o.display = (mlocCurrencyDisplay)i;
            }
        }
        o.usage = strcmp(usage, "cash") == 0 ? mloc_currencyUsageCash : mloc_currencyUsageStandard;
        mlocCurrencyFormatter f;
        mlocDecimal d;
        char out[256];
        mlocOutput output = {.buffer = out, .capacity = sizeof(out)};
        if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success ||
            mlocInitCurrencyFormatter(&data, &locale, code, strlen(code), &o, &f) < 0 ||
            mlocParseDecimal(value, strlen(value), &d).status != mloc_success ||
            mlocCurrencyFormatter_Format(&f, &d, &output) != mloc_success)
        {
            printf("%s\tERR\n", line);
            continue;
        }
        printf("%s\t%.*s\n", line, (int)output.length, out);
    }
    return 0;
}
