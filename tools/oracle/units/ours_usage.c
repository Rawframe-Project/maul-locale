// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Lines "locale unit usage width value": the library's text with usage.
#include "maul-locale/data.h"
#include "maul-locale/unit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
    static mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    FILE* file = fopen("src/generated/units.mloc", "rb");
    static char names[8 << 20];
    size_t size = fread(names, 1, sizeof(names), file);
    fclose(file);
    if (mlocOpenData(names, size, &sources[1]) != mloc_success)
    {
        return 1;
    }
    mlocDataSet data = {sources, 2};
    char line[512];
    while (fgets(line, sizeof(line), stdin))
    {
        line[strcspn(line, "\n")] = 0;
        char loc[64], unit[128], usage[64], width[16], value[64], value2[64] = "";
        if (sscanf(line, "%63s %127s %63s %15s %63s %63s", loc, unit, usage, width, value, value2) <
            5)
        {
            continue;
        }
        mlocLocale locale;
        mlocUnitOptions o = {.width = strcmp(width, "long") == 0     ? mloc_unitWidthLong
                                      : strcmp(width, "narrow") == 0 ? mloc_unitWidthNarrow
                                                                     : mloc_unitWidthShort,
                             .usage = usage,
                             .usageLength = strlen(usage),
                             .number.precision.mode = mloc_roundHalfEven};
        mlocUnitFormatter f;
        mlocDecimal d;
        char out[512];
        mlocOutput output = {.buffer = out, .capacity = sizeof(out)};
        mlocResult r;
        if (mlocParseLocale(loc, strlen(loc), &locale).status < 0 ||
            (r = mlocInitUnitFormatter(&data, &locale, unit, strlen(unit), &o, &f)) < 0 ||
            mlocParseDecimal(value, strlen(value), &d).status < 0 ||
            (value2[0] == 0 ? (r = mlocUnitFormatter_Format(&f, &d, &output)) < 0
                            : (
                                  {
                                      mlocDecimal e;
                                      mlocParseDecimal(value2, strlen(value2), &e).status < 0 ||
                                          (r = mlocUnitFormatter_FormatRange(&f, &d, &e, &output)) <
                                              0;
                                  })))
        {
            printf("%s\tERR\n", line);
            continue;
        }
        printf("%s\t%.*s\n", line, (int)output.length, out);
    }
}
