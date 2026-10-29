// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Chooses preferred units for lines read from standard input, on the
// compiled-in data, for tools/check_unit_prefs.py to compare with CLDR's
// unit preference test data. Each line is a locale id, a unit, a usage
// and a value, tab-separated; the line is written back, a tab, the
// chosen unit, a tab and its amounts separated by spaces, unrounded, as
// mlocFormatDecimal writes them; or the result's name when it fails.
//
// usage: conform_unit_prefs < lines

#include "maul-locale/unit.h"

#include <stdio.h>
#include <string.h>

enum
{
    maxLine = 512,
};

static mlocResult Choose(const mlocDataSet* data, char* fields[4])
{
    mlocLocale locale;
    mlocDecimal value;
    if (mlocParseLocale(fields[0], strlen(fields[0]), &locale).status != mloc_success ||
        mlocParseDecimal(fields[3], strlen(fields[3]), &value).status != mloc_success)
    {
        return mloc_errorSyntax;
    }
    char unit[256];
    mlocOutput unitOut = {.buffer = unit, .capacity = sizeof(unit)};
    mlocUnitAmounts amounts;
    mlocResult r = mlocGetUnitPreference(data, &locale, fields[1], strlen(fields[1]), fields[2],
                                         strlen(fields[2]), &value, nullptr, &unitOut, &amounts);
    if (r != mloc_success)
    {
        return r;
    }
    printf("%s\t%s\t%s\t%s\t%.*s\t", fields[0], fields[1], fields[2], fields[3],
           (int)unitOut.length, unit);
    for (uint8_t i = 0; i < amounts.count; i++)
    {
        char text[maxLine];
        mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
        if (mlocFormatDecimal(&amounts.amounts[i], &output) != mloc_success)
        {
            return mloc_errorCapacity;
        }
        printf("%s%.*s", i == 0 ? "" : " ", (int)output.length, text);
    }
    printf("\n");
    return mloc_success;
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    char line[maxLine];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        char copy[maxLine];
        memcpy(copy, line, sizeof(copy));
        char* fields[4] = {copy};
        int n = 1;
        for (char* c = copy; *c != '\0' && n < 4; c++)
        {
            if (*c == '\t')
            {
                *c = '\0';
                fields[n++] = c + 1;
            }
        }
        mlocResult r = n == 4 ? Choose(&data, fields) : mloc_errorSyntax;
        if (r != mloc_success)
        {
            printf("%s\t%s\n", line, mlocGetResultName(r));
        }
    }
    return 0;
}
