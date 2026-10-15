// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Writes the regional preferences of locale ids read one per line from
// standard input, on the compiled-in data, for tools/check_preferences.py
// to compare with CLDR's JSON of the pinned release. Each line is the id,
// then tab-separated: the region; the hour cycle and the allowed formats
// as mlocHourFormat digits; the first day, minimal days, weekend start
// and end; the measurement system and paper size; the calendar. Ids are
// parsed, not
// canonicalized, so that a deprecated region keeps its own data; one
// that does not parse writes "error".
//
// usage: conform_preferences < ids

#include "maul-locale/locale.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    char line[256];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        mlocLocale locale;
        mlocLocalePreferences p;
        if (mlocParseLocale(line, strlen(line), &locale).status != mloc_success ||
            mlocGetLocalePreferences(&data, &locale, &p) != mloc_success)
        {
            printf("%s\terror\n", line);
            continue;
        }
        printf("%s\t%.4s\t%d ", line, p.region, p.hourCycle);
        for (int i = 0; i < p.allowedHourCount; i++)
        {
            printf("%d", p.allowedHours[i]);
        }
        printf("\t%d %d %d %d\t%d %d\t%d\n", p.firstDay, p.minimalDays, p.weekendStart,
               p.weekendEnd, p.measurementSystem, p.paperSize, p.calendar);
    }
    return 0;
}
