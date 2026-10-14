// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Japan's eras (calendar_gen.h). Each era's first day is computed with
// the library's own Gregorian arithmetic.

#include "calendar_gen.h"

#include "calendar_arith.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxEras = 64,
    // The longest code the library's table holds, without its NUL.
    maxCode = 7,
    // The source's bytes: its fixed text, then a line an era.
    headRoom = 1024,
    lineRoom = 64,
};

typedef struct Era
{
    char code[maxCode + 1];
    int32_t year;
    int32_t day;
} Era;

// A start date "YYYY-MM-DD" of a valid Gregorian day; false otherwise.
static bool ParseStart(const char* text, int32_t* yearOut, int32_t* dayOut)
{
    if (strlen(text) != 10 || text[4] != '-' || text[7] != '-')
    {
        return false;
    }
    int32_t parts[3] = {0};
    static const int s_from[3] = {0, 5, 8};
    static const int s_length[3] = {4, 2, 2};
    for (int p = 0; p < 3; p++)
    {
        for (int i = 0; i < s_length[p]; i++)
        {
            char c = text[s_from[p] + i];
            if (c < '0' || c > '9')
            {
                return false;
            }
            parts[p] = parts[p] * 10 + (c - '0');
        }
    }
    if (parts[1] < 1 || parts[1] > 12 || parts[2] < 1 ||
        (uint32_t)parts[2] >
            mlocArithmeticDaysInMonth(mloc_calendarGregorian, parts[0], (uint32_t)parts[1]))
    {
        return false;
    }
    *yearOut = parts[0];
    *dayOut = (int32_t)mlocArithmeticToDay(mloc_calendarGregorian, parts[0], (uint32_t)parts[1],
                                           (uint32_t)parts[2]);
    return true;
}

// A code of 1 to maxCode lowercase letters that is not one of the
// Gregorian eras' the Japanese calendar also takes.
static bool ValidCode(const char* code)
{
    size_t length = strlen(code);
    if (length == 0 || length > maxCode)
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        if (code[i] < 'a' || code[i] > 'z')
        {
            return false;
        }
    }
    static const char* const s_gregorian[] = {"ce", "bce", "ad", "bc"};
    for (size_t i = 0; i < sizeof(s_gregorian) / sizeof(s_gregorian[0]); i++)
    {
        if (strcmp(code, s_gregorian[i]) == 0)
        {
            return false;
        }
    }
    return true;
}

// The eras of calendarData's japanese calendar into eras; their count,
// or 0 with a message.
static int ReadEras(const XmlNode* root, Era* eras, char* errorOut, size_t errorCapacity)
{
    const XmlNode* calendars = XmlFindChild(root, "calendarData");
    const XmlNode* japanese = nullptr;
    for (int i = 0; calendars != nullptr && i < calendars->childCount; i++)
    {
        const XmlNode* c = calendars->children[i];
        const char* type = XmlGetAttribute(c, "type");
        if (strcmp(c->name, "calendar") == 0 && type != nullptr && strcmp(type, "japanese") == 0)
        {
            japanese = c;
        }
    }
    const XmlNode* list = japanese != nullptr ? XmlFindChild(japanese, "eras") : nullptr;
    int count = 0;
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        const XmlNode* e = list->children[i];
        const char* code = XmlGetAttribute(e, "code");
        const char* start = XmlGetAttribute(e, "start");
        Era era = {0};
        if (strcmp(e->name, "era") != 0 || code == nullptr || start == nullptr ||
            !ValidCode(code) || !ParseStart(start, &era.year, &era.day) || count == maxEras ||
            (count > 0 && era.day <= eras[count - 1].day))
        {
            snprintf(errorOut, errorCapacity,
                     "supplementalData.xml: japanese era at line %d: malformed, out of order or "
                     "too many",
                     e->line);
            return 0;
        }
        memcpy(era.code, code, strlen(code));
        eras[count++] = era;
    }
    if (count == 0)
    {
        snprintf(errorOut, errorCapacity, "supplementalData.xml has no japanese eras");
    }
    return count;
}

int GenerateCalendarEras(const char* commonDirectory, char** sourceOut, size_t* sourceSizeOut,
                         char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/supplementalData.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    int ok = CalendarErasSource(root, sourceOut, sourceSizeOut, errorOut, errorCapacity);
    XmlFree(root);
    return ok;
}

int CalendarErasSource(const XmlNode* supplemental, char** sourceOut, size_t* sourceSizeOut,
                       char* errorOut, size_t errorCapacity)
{
    Era eras[maxEras];
    int count = ReadEras(supplemental, eras, errorOut, errorCapacity);
    if (count == 0)
    {
        return 0;
    }
    size_t capacity = headRoom + (size_t)count * lineRoom;
    char* source = malloc(capacity);
    if (source == nullptr)
    {
        snprintf(errorOut, errorCapacity, "out of memory");
        return 0;
    }
    int size = snprintf(source, capacity,
                        "// SPDX-License-Identifier: MIT\n"
                        "// Copyright (c) 2026 Sirac Ozmen\n"
                        "//\n"
                        "// Japan's eras since Meiji (record mloc-0040), from CLDR's\n"
                        "// supplementalData.xml: each era's code, the Gregorian year it began\n"
                        "// in and its first day.\n"
                        "// Generated by tools/mlocgen; do not edit.\n"
                        "// clang-format off\n"
                        "\n"
                        "#include \"calendar_eras.h\"\n"
                        "\n"
                        "const JapaneseEra mlocJapaneseEras[] = {\n");
    for (int i = 0; i < count; i++)
    {
        size += snprintf(source + size, capacity - (size_t)size, "    {\"%s\", %d, %d},\n",
                         eras[i].code, (int)eras[i].year, (int)eras[i].day);
    }
    size += snprintf(source + size, capacity - (size_t)size,
                     "};\n\nconst uint32_t mlocJapaneseEraCount = %d;\n", count);
    *sourceOut = source;
    *sourceSizeOut = (size_t)size;
    return 1;
}
