// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Lines "locale type width item|item|...": the library's join.
#include "list_join.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <string.h>
int main(void)
{
    static mlocData source;
    mlocOpenBuiltinData(&source);
    mlocDataSet data = {&source, 1};
    char line[1024];
    while (fgets(line, sizeof(line), stdin))
    {
        line[strcspn(line, "\n")] = 0;
        char loc[64], type[16], width[16];
        int used = 0;
        if (sscanf(line, "%63s %15s %15s %n", loc, type, width, &used) < 3)
        {
            continue;
        }
        const char* items = line + used;
        ListItem list[16];
        uint32_t count = 0;
        for (const char* at = items;;)
        {
            const char* bar = strchr(at, '|');
            list[count].text = at;
            list[count].length = bar ? (size_t)(bar - at) : strlen(at);
            count++;
            if (!bar)
            {
                break;
            }
            at = bar + 1;
        }
        int t = strcmp(type, "and") == 0 ? 0 : strcmp(type, "or") == 0 ? 3 : 6;
        int w = strcmp(width, "wide") == 0 ? 0 : strcmp(width, "short") == 0 ? 1 : 2;
        mlocLocale locale;
        ListPatterns p;
        char out[2048];
        if (mlocParseLocale(loc, strlen(loc), &locale).status < 0 ||
            !mlocFindListPatterns(&data, &locale, (ListStyle)(t + w), &p))
        {
            printf("%s\tERR\n", line);
            continue;
        }
        NumberWriter writer = {.buffer = out, .capacity = sizeof(out)};
        mlocWriteList(&p, list, count, nullptr, nullptr, &writer);
        printf("%s\t%.*s\n", line, (int)writer.length, out);
    }
}
