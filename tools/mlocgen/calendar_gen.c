// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Japan's eras (calendar_gen.h). Each era's first day is computed with
// the library's own Gregorian arithmetic.

#include "calendar_gen.h"

#include "calendar_arith.h"
#include "calendar_chinese.h"
#include "calendar_years.h"

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
                        "// Japan's eras since Meiji, from CLDR's supplementalData.xml: each\n"
                        "// era's code, the Gregorian year it began in and its first day.\n"
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

// The model's years with the overrides in place, the years before and
// after the table included.
typedef struct LunarYears
{
    LunisolarYear years[lunarYearCount + 2];
    bool overridden[lunarYearCount + 2];
} LunarYears;

// The model's years of both calendars, the years before and after the
// table included, computed once: they do not depend on the overrides,
// and each source made in a run starts from them.
static const LunisolarYear* ModelYears(void)
{
    static LunisolarYear s_years[2][lunarYearCount + 2];
    static bool s_made;
    if (!s_made)
    {
        for (int c = 0; c < 2; c++)
        {
            for (int i = 0; i < lunarYearCount + 2; i++)
            {
                mlocChineseModelYear(c == 1, lunarFirstYear - 1 + i, &s_years[c][i]);
            }
        }
        s_made = true;
    }
    return &s_years[0][0];
}

// Reads an override's months: 12 or 13 of '0' and '1'.
static bool ParseLengths(const char* text, LunisolarYear* out)
{
    size_t count = strlen(text);
    if (count != 12 && count != 13)
    {
        return false;
    }
    out->count = (uint8_t)count;
    for (size_t m = 0; m < count; m++)
    {
        if (text[m] != '0' && text[m] != '1')
        {
            return false;
        }
        out->start[m + 1] = out->start[m] + 29 + (text[m] - '0');
    }
    return true;
}

// One override line into its calendar's years; false for a malformed
// one.
static bool ReadOverride(char* line, LunarYears* lunar)
{
    char calendar[16];
    char date[16];
    char lengths[32];
    char tag[32];
    int year = 0;
    int leap = -1;
    int32_t first = 0;
    int32_t ignored = 0;
    if (sscanf(line, "%15s %d %15s %31s %d %31s", calendar, &year, date, lengths, &leap, tag) !=
            6 ||
        (strcmp(calendar, "chinese") != 0 && strcmp(calendar, "dangi") != 0) ||
        year < lunarFirstYear || year > lunarLastYear || !ParseStart(date, &ignored, &first))
    {
        return false;
    }
    bool korea = strcmp(calendar, "dangi") == 0;
    size_t i = (size_t)(year - lunarFirstYear + 1);
    LunarYears* y = &lunar[korea ? 1 : 0];
    LunisolarYear entry = {.year = year};
    entry.start[0] = first;
    int64_t july = mlocArithmeticToDay(mloc_calendarGregorian, year, 7, 1);
    if (y->overridden[i] || !ParseLengths(lengths, &entry) ||
        (entry.count == 12 ? leap != 0 : leap < 1 || leap > 13) || july < first ||
        july >= entry.start[entry.count])
    {
        return false;
    }
    entry.leap = (uint8_t)leap;
    y->years[i] = entry;
    y->overridden[i] = true;
    return true;
}

static bool ReadOverrides(const char* text, LunarYears* lunar, char* errorOut, size_t errorCapacity)
{
    int number = 0;
    for (const char* at = text; *at != '\0';)
    {
        const char* end = strchr(at, '\n');
        size_t length = end != nullptr ? (size_t)(end - at) : strlen(at);
        char line[256];
        number++;
        if (length >= sizeof(line))
        {
            snprintf(errorOut, errorCapacity, "calendar-overrides.txt:%d: too long", number);
            return false;
        }
        memcpy(line, at, length);
        line[length] = '\0';
        if (line[0] != '#' && line[0] != '\0' && !ReadOverride(line, lunar))
        {
            snprintf(errorOut, errorCapacity, "calendar-overrides.txt:%d: malformed or repeated",
                     number);
            return false;
        }
        at += length + (end != nullptr ? 1 : 0);
    }
    return true;
}

// Packs a year, which begins at most 127 days into its Gregorian year.
static bool Pack(const LunisolarYear* y, uint32_t* out)
{
    int64_t offset = y->start[0] - mlocArithmeticToDay(mloc_calendarGregorian, y->year, 1, 1);
    if (offset < 0 || offset > 127)
    {
        return false;
    }
    uint32_t bits = (uint32_t)offset << lunarOffsetShift | (uint32_t)y->leap << lunarLeapShift;
    for (uint32_t m = 0; m < y->count; m++)
    {
        bits |= (uint32_t)(y->start[m + 1] - y->start[m] - 29) << m;
    }
    *out = bits;
    return true;
}

static int AppendTable(char* source, size_t capacity, int size, const char* name,
                       const uint32_t* table)
{
    size += snprintf(source + size, capacity - (size_t)size,
                     "\nconst uint32_t %s[lunarYearCount] = {\n", name);
    for (int i = 0; i < lunarYearCount; i++)
    {
        size += snprintf(source + size, capacity - (size_t)size, "%s0x%06X,%s",
                         i % 8 == 0 ? "    " : " ", (unsigned)table[i],
                         i % 8 == 7 || i == lunarYearCount - 1 ? "\n" : "");
    }
    return size + snprintf(source + size, capacity - (size_t)size, "};\n");
}

static int AppendMonths(char* source, size_t capacity, int size, const char* name,
                        const uint16_t* months)
{
    size += snprintf(source + size, capacity - (size_t)size,
                     "\nconst uint16_t %s[lunarYearCount] = {\n", name);
    for (int i = 0; i < lunarYearCount; i++)
    {
        size +=
            snprintf(source + size, capacity - (size_t)size, "%s%d,%s", i % 12 == 0 ? "    " : " ",
                     (int)months[i], i % 12 == 11 || i == lunarYearCount - 1 ? "\n" : "");
    }
    return size + snprintf(source + size, capacity - (size_t)size, "};\n");
}

int CalendarYearsSource(const char* overrides, char** sourceOut, size_t* sourceSizeOut,
                        char* errorOut, size_t errorCapacity)
{
    LunarYears* lunar = calloc(2, sizeof(LunarYears));
    uint32_t* tables = malloc(2 * lunarYearCount * sizeof(uint32_t));
    uint16_t* months = malloc(2 * lunarYearCount * sizeof(uint16_t));
    size_t capacity = headRoom + 2 * (size_t)lunarYearCount * 18;
    char* source = malloc(capacity);
    if (lunar == nullptr || tables == nullptr || months == nullptr || source == nullptr)
    {
        snprintf(errorOut, errorCapacity, "out of memory");
        free(lunar);
        free(tables);
        free(months);
        free(source);
        return 0;
    }
    int ok = ReadOverrides(overrides, lunar, errorOut, errorCapacity);
    int32_t firstLunation[2] = {0};
    const LunisolarYear* model = ok ? ModelYears() : nullptr;
    for (int c = 0; ok && c < 2; c++)
    {
        for (int i = 0; i < lunarYearCount + 2; i++)
        {
            if (!lunar[c].overridden[i])
            {
                lunar[c].years[i] = model[c * (lunarYearCount + 2) + i];
            }
        }
        for (int i = 1; ok && i < lunarYearCount + 1; i++)
        {
            const LunisolarYear* y = &lunar[c].years[i];
            ok = y->start[0] == lunar[c].years[i - 1].start[lunar[c].years[i - 1].count] &&
                 y->start[y->count] == lunar[c].years[i + 1].start[0] &&
                 Pack(y, &tables[c * lunarYearCount + i - 1]);
            if (!ok)
            {
                snprintf(errorOut, errorCapacity,
                         "calendar-overrides.txt: the %s year %d does not meet its neighbours",
                         c == 1 ? "dangi" : "chinese", (int)y->year);
            }
        }
        // The table's months are numbered on from the model's year before
        // it, and must reach the model's year after it.
        const LunisolarYear* before = &lunar[c].years[0];
        int64_t lunation = (int64_t)before->lunation + before->count;
        firstLunation[c] = (int32_t)lunation;
        for (int i = 1; i < lunarYearCount + 1; i++)
        {
            months[c * lunarYearCount + i - 1] = (uint16_t)(lunation - firstLunation[c]);
            lunation += lunar[c].years[i].count;
        }
        if (ok && lunation != lunar[c].years[lunarYearCount + 1].lunation)
        {
            ok = 0;
            snprintf(errorOut, errorCapacity,
                     "calendar-overrides.txt: the %s table's months do not reach the model's",
                     c == 1 ? "dangi" : "chinese");
        }
    }
    if (ok)
    {
        int size = snprintf(source, capacity,
                            "// SPDX-License-Identifier: MIT\n"
                            "// Copyright (c) 2026 Sirac Ozmen\n"
                            "//\n"
                            "// The Chinese and Dangi years from 1600 to 2400, packed as\n"
                            "// calendar_years.h says: the model's, with the years of\n"
                            "// tools/calendar-overrides.txt in their place.\n"
                            "// Generated by tools/mlocgen; do not edit.\n"
                            "// clang-format off\n"
                            "\n"
                            "#include \"calendar_years.h\"\n");
        size = AppendTable(source, capacity, size, "mlocChineseYears", tables);
        size = AppendTable(source, capacity, size, "mlocDangiYears", tables + lunarYearCount);
        size = AppendMonths(source, capacity, size, "mlocChineseMonthsBefore", months);
        size =
            AppendMonths(source, capacity, size, "mlocDangiMonthsBefore", months + lunarYearCount);
        size += snprintf(source + size, capacity - (size_t)size,
                         "\nconst int32_t mlocChineseFirstLunation = %d;\n"
                         "const int32_t mlocDangiFirstLunation = %d;\n",
                         (int)firstLunation[0], (int)firstLunation[1]);
        *sourceOut = source;
        *sourceSizeOut = (size_t)size;
        source = nullptr;
    }
    free(lunar);
    free(tables);
    free(months);
    free(source);
    return ok;
}

int GenerateCalendarYears(const char* root, char** sourceOut, size_t* sourceSizeOut, char* errorOut,
                          size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/tools/calendar-overrides.txt", root);
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        snprintf(errorOut, errorCapacity, "cannot read %s", path);
        return 0;
    }
    char* text = malloc(65536);
    size_t length = text != nullptr ? fread(text, 1, 65535, file) : 0;
    bool whole = text != nullptr && feof(file);
    fclose(file);
    if (!whole)
    {
        snprintf(errorOut, errorCapacity, "%s is too long or unreadable", path);
        free(text);
        return 0;
    }
    text[length] = '\0';
    int ok = CalendarYearsSource(text, sourceOut, sourceSizeOut, errorOut, errorCapacity);
    free(text);
    return ok;
}
