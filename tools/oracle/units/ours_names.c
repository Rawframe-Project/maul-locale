// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's side of icu_names.cpp.
#include "maul-locale/unit.h"

#include <stdio.h>
#include <string.h>
int main(void)
{
    static mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    static _Alignas(8) unsigned char names[8 << 20];
    FILE* file = fopen("src/generated/units.mloc", "rb");
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
        char loc[64], unit[80], width[16];
        if (sscanf(line, "%63s %79s %15s", loc, unit, width) != 3)
        {
            continue;
        }
        for (char* c = loc; *c; c++)
        {
            if (*c == '_')
            {
                *c = '-';
            }
        }
        mlocLocale locale;
        mlocUnitWidth w = strcmp(width, "long") == 0     ? mloc_unitWidthLong
                          : strcmp(width, "narrow") == 0 ? mloc_unitWidthNarrow
                                                         : mloc_unitWidthShort;
        char out[256];
        mlocOutput output = {.buffer = out, .capacity = sizeof(out)};
        if (mlocParseLocale(loc, strlen(loc), &locale).status != mloc_success ||
            mlocGetUnitDisplayName(&data, &locale, unit, strlen(unit), w, &output) != mloc_success)
        {
            printf("%s\tERR\n", line);
            continue;
        }
        printf("%s\t%.*s\n", line, (int)output.length, out);
    }
    return 0;
}
