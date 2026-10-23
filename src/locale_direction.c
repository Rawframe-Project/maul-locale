// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A locale's direction (maul-locale/locale.h): its script's, or its likely
// script's, against CLDR's scripts written right to left.

#include "script_directions.h"

#include "maul-locale/locale.h"

#include <string.h>

// Whether a script is written right to left: a binary search of the
// sorted codes.
static bool RightToLeft(const char script[4])
{
    uint32_t low = 0;
    uint32_t high = mlocRightToLeftScriptCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        int order = memcmp(mlocRightToLeftScripts[middle], script, 4);
        if (order == 0)
        {
            return true;
        }
        if (order < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return false;
}

mlocResult mlocGetLocaleDirection(const mlocDataSet* data, const mlocLocale* locale,
                                  mlocDirection* directionOut)
{
    if (data == NULL || locale == NULL || directionOut == NULL)
    {
        return mloc_errorInvalid;
    }
    const char* script = locale->script;
    mlocLocale likely;
    if (script[0] == '\0' || memcmp(script, "Zzzz", 4) == 0)
    {
        if (mlocMaximizeLocale(data, locale, &likely) != mloc_success || likely.script[0] == '\0')
        {
            return mloc_errorNotFound;
        }
        script = likely.script;
    }
    *directionOut = RightToLeft(script) ? mloc_directionRightToLeft : mloc_directionLeftToRight;
    return mloc_success;
}
