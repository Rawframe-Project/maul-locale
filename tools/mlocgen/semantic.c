// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Semantic skeletons mapped to classical ones (semantic.h).

#include "semantic.h"

#include <string.h>

enum
{
    dateFields = semanticYear | semanticMonth | semanticDay | semanticWeekday,
};

// UTS #35's field sets: dates, calendar periods, time, zone, and dates
// or time with time and zone.
static bool ValidFields(unsigned fields)
{
    static const unsigned s_dates[] = {
        semanticDay,
        semanticWeekday,
        semanticDay | semanticWeekday,
        semanticMonth | semanticDay,
        semanticMonth | semanticDay | semanticWeekday,
        semanticYear | semanticMonth | semanticDay,
        semanticYear | semanticMonth | semanticDay | semanticWeekday,
    };
    unsigned date = fields & dateFields;
    unsigned rest = fields & ~(unsigned)dateFields;
    if (rest & ~(unsigned)(semanticTime | semanticZone))
    {
        return false;
    }
    if (date == semanticMonth || date == semanticYear || date == (semanticYear | semanticMonth))
    {
        return rest == 0;
    }
    if (date == 0)
    {
        return rest != 0;
    }
    for (size_t i = 0; i < sizeof(s_dates) / sizeof(s_dates[0]); i++)
    {
        if (date == s_dates[i])
        {
            return true;
        }
    }
    return false;
}

static void Append(char* out, size_t capacity, const char* text)
{
    size_t length = strlen(out);
    size_t more = strlen(text);
    if (length + more < capacity)
    {
        memcpy(out + length, text, more + 1);
    }
}

// The run of letters of a set in a skeleton ("GGGGG" of "GGGGGyMd" for
// "G"), appended.
static void AppendRun(char* out, size_t capacity, const char* skeleton, const char* letters)
{
    char run[16] = "";
    size_t n = 0;
    for (const char* p = skeleton; *p != '\0' && n + 1 < sizeof(run); p++)
    {
        if (strchr(letters, *p) != nullptr)
        {
            run[n++] = *p;
        }
    }
    run[n] = '\0';
    Append(out, capacity, run);
}

static void AppendYear(const SemanticSkeleton* s, const char* datetimeSkeleton, char* out,
                       size_t capacity)
{
    char era[16] = "";
    char year[16] = "";
    AppendRun(era, sizeof(era), datetimeSkeleton, "G");
    AppendRun(year, sizeof(year), datetimeSkeleton, "yYur");
    if (year[0] == '\0')
    {
        strcpy(year, "y");
    }
    if (s->yearStyle != semanticYearAuto && strcmp(year, "yy") == 0)
    {
        strcpy(year, "y");
    }
    if (s->yearStyle == semanticYearWithEra && era[0] == '\0')
    {
        strcpy(era, "G");
    }
    Append(out, capacity, era);
    Append(out, capacity, year);
}

static void AppendTime(const SemanticSkeleton* s, char* out, size_t capacity)
{
    static const char s_hours[] = {'j', 'h', 'h', 'H', 'H', 'h', 'H'};
    char hour[2] = {s_hours[s->hourCycle], '\0'};
    Append(out, capacity, hour);
    if (s->precision >= 1)
    {
        Append(out, capacity, "m");
    }
    if (s->precision >= 2)
    {
        Append(out, capacity, "s");
    }
    for (int i = 2; i < s->precision && i < 11; i++)
    {
        Append(out, capacity, "S");
    }
}

static void AppendZone(const SemanticSkeleton* s, bool alone, char* out, size_t capacity)
{
    bool wide = alone && s->length != semanticShort;
    switch (s->zoneStyle)
    {
    case semanticZoneGeneric:
        Append(out, capacity, wide ? "vvvv" : "v");
        break;
    case semanticZoneLocation:
        Append(out, capacity, "VVVV");
        break;
    case semanticZoneOffset:
        Append(out, capacity, "O");
        break;
    case semanticZoneSpecific:
    default:
        Append(out, capacity, wide ? "zzzz" : "z");
        break;
    }
}

int SemanticClassical(const SemanticSkeleton* s, const char* datetimeSkeleton, char* out,
                      size_t capacity)
{
    out[0] = '\0';
    if (!ValidFields(s->fields))
    {
        return 0;
    }
    static const char* const s_lone[3] = {"LLLL", "LLL", "L"};
    static const char* const s_weekday[3] = {"EEEE", "EEE", "EEE"};
    static const char* const s_loneWeekday[3] = {"EEEE", "EEE", "EEEEE"};
    bool alone = (s->fields & (s->fields - 1)) == 0;
    if (s->fields & semanticYear)
    {
        AppendYear(s, datetimeSkeleton, out, capacity);
    }
    if (s->fields & semanticMonth)
    {
        if (alone)
        {
            Append(out, capacity, s_lone[s->length]);
        }
        else
        {
            AppendRun(out, capacity, datetimeSkeleton, "ML");
        }
    }
    if (s->fields & semanticDay)
    {
        AppendRun(out, capacity, datetimeSkeleton, "d");
    }
    if (s->fields & semanticWeekday)
    {
        Append(out, capacity, alone ? s_loneWeekday[s->length] : s_weekday[s->length]);
    }
    if (s->fields & semanticTime)
    {
        AppendTime(s, out, capacity);
    }
    if (s->fields & semanticZone)
    {
        AppendZone(s, alone, out, capacity);
    }
    return 1;
}
