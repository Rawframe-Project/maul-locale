// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Each locale's direction against the character order CLDR gives it
// (record mloc-0045): reads "id<TAB>ltr|rtl" lines, as
// tools/cldr_directions.py writes them from the pinned release's
// common/main, and checks mlocGetLocaleDirection on the compiled-in
// data. CI runs it on the pinned release.
//
// usage: python3 tools/cldr_directions.py <common> | conform_direction

#include "maul-locale/data.h"
#include "maul-locale/locale.h"

#include <stdio.h>
#include <string.h>

static bool CheckLine(const mlocDataSet* data, char* line)
{
    char* tab = strchr(line, '\t');
    if (tab == nullptr)
    {
        return false;
    }
    *tab = '\0';
    const char* expected = tab + 1;
    tab[1 + strcspn(tab + 1, "\r\n")] = '\0';
    mlocLocale locale;
    mlocDirection direction = mloc_directionUnknown;
    if (mlocParseLocale(line, strlen(line), &locale).status != mloc_success ||
        mlocGetLocaleDirection(data, &locale, &direction) != mloc_success)
    {
        fprintf(stderr, "differs: %s: no direction\n", line);
        return false;
    }
    const char* got = direction == mloc_directionRightToLeft ? "rtl" : "ltr";
    if (strcmp(got, expected) != 0)
    {
        fprintf(stderr, "differs: %s: CLDR %s, library %s\n", line, expected, got);
        return false;
    }
    return true;
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    char line[256];
    int passed = 0;
    int failed = 0;
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        if (CheckLine(&data, line))
        {
            passed++;
        }
        else
        {
            failed++;
        }
    }
    printf("direction: %d of %d locales agree with CLDR\n", passed, passed + failed);
    return failed == 0 && passed > 0 ? 0 : 1;
}
