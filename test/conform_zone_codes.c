// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Answers questions about time zones by BCP 47 id and Windows name, for
// tools/check_zone_codes.py to compare with its own reading of CLDR's
// bcp47/timezone.xml and supplemental/windowsZones.xml. Each line of
// standard input is a question; each answer is one line, "-" for none:
//   b ID            the identifier of the zone the id finds
//   z IDENTIFIER    the zone's BCP 47 id and Windows name, "|" between
//   w REGION NAME   the identifier of the zone the Windows name finds in
//                   the region ("-" for none)
//
// usage: conform_zone_codes < questions

#include "maul-locale/data.h"
#include "maul-locale/zone.h"

#include <stdio.h>
#include <string.h>

static mlocData s_builtin;
static mlocDataSet s_data = {&s_builtin, 1};

static void PrintZone(mlocResult result, const mlocTimeZone* zone)
{
    char text[128];
    mlocOutput out = {.buffer = text, .capacity = sizeof text};
    if (result == mloc_success && mlocFormatTimeZoneId(zone, &out) == mloc_success)
    {
        printf("%.*s\n", (int)out.length, text);
    }
    else
    {
        puts("-");
    }
}

static void PrintCodes(const char* id)
{
    mlocTimeZone zone;
    char bcp47[16];
    char windows[128];
    mlocOutput one = {.buffer = bcp47, .capacity = sizeof bcp47};
    mlocOutput two = {.buffer = windows, .capacity = sizeof windows};
    if (mlocFindTimeZone(&s_data, id, strlen(id), &zone) != mloc_success)
    {
        puts("?");
        return;
    }
    bool hasId = mlocFormatTimeZoneBcp47(&s_data, &zone, &one) == mloc_success;
    bool hasName = mlocFormatTimeZoneWindows(&s_data, &zone, &two) == mloc_success;
    printf("%.*s|%.*s\n", hasId ? (int)one.length : 1, hasId ? bcp47 : "-",
           hasName ? (int)two.length : 1, hasName ? windows : "-");
}

int main(void)
{
    mlocOpenBuiltinData(&s_builtin);
    char line[512];
    while (fgets(line, sizeof line, stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        mlocTimeZone zone;
        if (line[0] == 'b' && line[1] == ' ')
        {
            PrintZone(mlocFindTimeZoneBcp47(&s_data, line + 2, strlen(line + 2), &zone), &zone);
        }
        else if (line[0] == 'z' && line[1] == ' ')
        {
            PrintCodes(line + 2);
        }
        else if (line[0] == 'w' && line[1] == ' ' && strchr(line + 2, ' ') != nullptr)
        {
            const char* region = line + 2;
            const char* name = strchr(region, ' ') + 1;
            size_t regionLength = region[0] == '-' ? 0 : (size_t)(name - 1 - region);
            PrintZone(
                mlocFindTimeZoneWindows(&s_data, name, strlen(name), region, regionLength, &zone),
                &zone);
        }
        else
        {
            puts("?");
        }
    }
    return 0;
}
