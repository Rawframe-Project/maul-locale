// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Converts values read from standard input, on the compiled-in data, for
// tools/check_units.py to compare with exact fractions from CLDR's unit
// test data. Each line is a value, the unit it is in and the unit
// wanted, tab-separated; the line is written back, a tab, and the value
// converted with the default precision (exact, or 38 significant digits
// half even) as mlocFormatDecimal writes it, or the result's name when
// the conversion fails.
//
// usage: conform_units < lines

#include "maul-locale/unit.h"

#include <stdio.h>
#include <string.h>

enum
{
    maxLine = 512,
};

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    char line[maxLine];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        char* from = strchr(line, '\t');
        char* to = from == nullptr ? nullptr : strchr(from + 1, '\t');
        mlocDecimal value;
        mlocDecimal converted;
        char text[maxLine];
        mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
        mlocResult r = mloc_errorSyntax;
        if (to != nullptr &&
            mlocParseDecimal(line, (size_t)(from - line), &value).status == mloc_success)
        {
            r = mlocConvertUnit(&data, from + 1, (size_t)(to - from - 1), to + 1, strlen(to + 1),
                                nullptr, &value, &converted);
        }
        if (r == mloc_success)
        {
            r = mlocFormatDecimal(&converted, &output);
        }
        if (r == mloc_success)
        {
            printf("%s\t%.*s\n", line, (int)output.length, text);
        }
        else
        {
            printf("%s\t%s\n", line, mlocGetResultName(r));
        }
    }
    return 0;
}
