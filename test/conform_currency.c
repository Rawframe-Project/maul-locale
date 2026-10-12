// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Writes the currency data of lines read from standard input, on the
// compiled-in data, for tools/check_currency.py to compare with CLDR's
// JSON of the pinned release. A line "fraction CODE" writes the line,
// a tab, the digits and rounding increment, then the cash digits and
// increment; a line "region RR" writes the line, a tab, and each of the
// region's currencies in order, separated by spaces: the code, its first
// and last days and its tender flag, joined by commas ("min" and "max"
// for the open ends). A line the library refuses writes "error".
//
// usage: conform_currency < lines

#include "maul-locale/currency.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
    maxCurrencies = 64,
};

static void WriteDay(int32_t day, int32_t open, const char* name)
{
    if (day == open)
    {
        printf("%s", name);
    }
    else
    {
        printf("%ld", (long)day);
    }
}

static bool Fraction(const mlocDataSet* data, const char* code)
{
    uint8_t digits = 0;
    uint8_t cashDigits = 0;
    uint16_t increment = 0;
    uint16_t cashIncrement = 0;
    if (mlocGetCurrencyDigits(data, code, strlen(code), mloc_currencyUsageStandard, &digits,
                              &increment) != mloc_success ||
        mlocGetCurrencyDigits(data, code, strlen(code), mloc_currencyUsageCash, &cashDigits,
                              &cashIncrement) != mloc_success)
    {
        return false;
    }
    printf("fraction %s\t%d %d %d %d\n", code, digits, increment, cashDigits, cashIncrement);
    return true;
}

static bool Region(const mlocDataSet* data, const char* region)
{
    mlocRegionCurrency list[maxCurrencies];
    size_t count = 0;
    if (mlocGetRegionCurrencies(data, region, strlen(region), list, maxCurrencies, &count) !=
        mloc_success)
    {
        return false;
    }
    printf("region %s\t", region);
    for (size_t i = 0; i < count; i++)
    {
        printf("%s%s,", i == 0 ? "" : " ", list[i].code);
        WriteDay(list[i].from, INT32_MIN, "min");
        printf(",");
        WriteDay(list[i].to, INT32_MAX, "max");
        printf(",%d", list[i].tender);
    }
    printf("\n");
    return true;
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    char line[256];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        bool done = strncmp(line, "fraction ", 9) == 0 ? Fraction(&data, line + 9)
                    : strncmp(line, "region ", 7) == 0 ? Region(&data, line + 7)
                                                       : false;
        if (!done)
        {
            printf("%s\terror\n", line);
        }
    }
    return 0;
}
