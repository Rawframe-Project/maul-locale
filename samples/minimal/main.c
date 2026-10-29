// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Uses the installed library as a consumer would, in C17: it checks that
// the linked library has the version it was compiled against, and writes
// a number in Swiss German from the compiled-in data.

#include "maul-locale/data.h"
#include "maul-locale/locale.h"
#include "maul-locale/number.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    int failures = 0;
    mlocVersion version = mlocGetVersion();
    if (version.major != MLOC_VERSION_MAJOR || version.minor != MLOC_VERSION_MINOR ||
        version.patch != MLOC_VERSION_PATCH)
    {
        printf("FAIL: linked %u.%u.%u, compiled against %d.%d.%d\n", (unsigned)version.major,
               (unsigned)version.minor, (unsigned)version.patch, MLOC_VERSION_MAJOR,
               MLOC_VERSION_MINOR, MLOC_VERSION_PATCH);
        failures += 1;
    }
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet set = {&builtin, 1};
    mlocLocale locale;
    mlocNumberFormatter formatter;
    mlocDecimal value;
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocParseLocale("de-CH", 5, &locale).status != mloc_success ||
        mlocInitNumberFormatter(&set, &locale, NULL, &formatter) < 0 ||
        mlocMakeShortestDecimal(1234567.891, &value) != mloc_success ||
        mlocNumberFormatter_Format(&formatter, &value, &output) != mloc_success ||
        output.length != 13 || memcmp(text, "1'234'567.891", 13) != 0)
    {
        printf("FAIL: 1234567.891 in de-CH\n");
        failures += 1;
    }
    return failures == 0 ? 0 : 1;
}
