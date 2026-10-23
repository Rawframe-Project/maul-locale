// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// One number case run N times, for instruction counts.
#include "maul-locale/currency.h"
#include "maul-locale/number.h"
#include "maul-locale/unit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t file[8 << 20];
int main(int argc, char** argv)
{
    const char* c = argv[1];
    long n = atol(argv[2]);
    static mlocData d[2];
    mlocOpenBuiltinData(&d[0]);
    FILE* f = fopen("src/generated/units.mloc", "rb");
    size_t s = fread(file, 1, sizeof file, f);
    fclose(f);
    if (mlocOpenData(&d[1], file, s) != mloc_success)
    {
        return 3;
    }
    mlocDataSet builtin = {d, 1}, all = {d, 2};
    mlocLocale de, en;
    (void)mlocParseLocale("de", 2, &de);
    (void)mlocParseLocale("en", 2, &en);
    mlocDecimal value, ratio, length;
    (void)mlocParseDecimal("1234567.891", 11, &value);
    (void)mlocParseDecimal("0.256", 5, &ratio);
    (void)mlocParseDecimal("1234.5", 6, &length);
    static mlocNumberFormatter nde, pen;
    static mlocUnitFormatter meter, m2;
    mlocNumberOptions percent = {.style = mloc_numberPercent};
    mlocUnitOptions lo = {.width = getenv("SHORT") ? mloc_unitWidthShort : mloc_unitWidthLong};
    if (mlocInitNumberFormatter(&nde, &builtin, &de, NULL) < 0 ||
        mlocInitNumberFormatter(&pen, &builtin, &en, &percent) < 0 ||
        mlocInitUnitFormatter(&meter, &all, &en, "meter", 5, &lo) < 0)
    {
        return 4;
    }
    char text[128];
    size_t sink = 0;
    mlocOutput o = {text, sizeof text, 0, NULL, NULL};
    for (long i = 0; i < n; i++)
    {
        o.length = 0;
        if (!strcmp(c, "number-format"))
        {
            sink += mlocFormatNumber(&nde, &value, &o);
        }
        else if (!strcmp(c, "percent-format"))
        {
            sink += mlocFormatNumber(&pen, &ratio, &o);
        }
        else if (!strcmp(c, "unit-format"))
        {
            sink += mlocFormatUnit(&meter, &length, &o);
        }
        else if (!strcmp(c, "unit-init"))
        {
            sink += mlocInitUnitFormatter(&m2, &all, &en, "meter", 5, &lo);
        }
        else if (!strcmp(c, "currency-init"))
        {
            static mlocCurrencyFormatter cf;
            sink += mlocInitCurrencyFormatter(&cf, &builtin, &en, "EUR", 3, NULL);
        }
        else if (!strcmp(c, "number-init"))
        {
            sink += mlocInitNumberFormatter(&nde, &builtin, &de, NULL);
        }
    }
    printf("%zu [%.*s]\n", sink, (int)o.length, text);
}
