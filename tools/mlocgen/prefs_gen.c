// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Regional preferences (prefs_gen.h). Each region gathers the values its
// lists give it; a field no list gives is 001's.

#include "prefs_gen.h"

#include "kinds.h"
#include "locale_tables.h"
#include "table_gen.h"
#include "xml.h"

#include "maul-locale/locale.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    slots = 2048,
    maxHours = 64,
    maxLocales = 64,
    // The fields of a region, in the order its entry stores them.
    fieldHours = 0,
    fieldFirstDay,
    fieldMinimalDays,
    fieldWeekendStart,
    fieldWeekendEnd,
    fieldMeasurement,
    fieldPaper,
    fieldCalendar,
    fieldCount,
};

typedef struct Hours
{
    uint8_t preferred;
    uint8_t count;
    uint8_t allowed[preferHoursMax];
} Hours;

typedef struct LocaleHours
{
    uint32_t language;
    uint32_t region;
    int hours;
} LocaleHours;

typedef struct Prefs
{
    // Each packed region's fields, -1 for none.
    int fields[slots][fieldCount];
    Hours hours[maxHours];
    int hoursCount;
    LocaleHours locales[maxLocales];
    int localeCount;
} Prefs;

[[noreturn]] static void Die(const char* message, const char* detail)
{
    fprintf(stderr, "mlocgen: %s: %s\n", message, detail);
    exit(1);
}

static uint32_t Region(const char* code, size_t length)
{
    uint32_t packed = mlocPackRegion(code, (uint32_t)length);
    if (packed == 0)
    {
        Die("not a region code", code);
    }
    return packed;
}

// A day as ISO 8601 numbers it, Monday 1.
static int Day(const char* name)
{
    static const char* const s_days[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    for (int i = 0; name != nullptr && i < 7; i++)
    {
        if (strcmp(name, s_days[i]) == 0)
        {
            return i + 1;
        }
    }
    Die("not a day", name == nullptr ? "(none)" : name);
}

static int Format(const char* token, size_t length)
{
    static const char* const s_formats[] = {"K", "h", "H", "k", "hb", "hB"};
    for (int i = 0; i < 6; i++)
    {
        if (strlen(s_formats[i]) == length && strncmp(token, s_formats[i], length) == 0)
        {
            return i;
        }
    }
    Die("an hour format the library has no value for", token);
}

// The index of an hours entry, added when new.
static int AddHours(Prefs* p, const char* preferred, const char* allowed)
{
    Hours h = {.preferred = (uint8_t)Format(preferred, strlen(preferred))};
    if (h.preferred > mloc_hourFormatH24)
    {
        Die("a preferred hour format with a day period", preferred);
    }
    for (const char* at = allowed + strspn(allowed, " \t\n"); *at != '\0';)
    {
        size_t length = strcspn(at, " \t\n");
        if (h.count == preferHoursMax)
        {
            Die("too many allowed hour formats", allowed);
        }
        h.allowed[h.count++] = (uint8_t)Format(at, length);
        at += length;
        at += strspn(at, " \t\n");
    }
    if (h.count == 0)
    {
        Die("no allowed hour format", allowed);
    }
    for (int i = 0; i < p->hoursCount; i++)
    {
        if (memcmp(&p->hours[i], &h, sizeof(h)) == 0)
        {
            return i;
        }
    }
    if (p->hoursCount == maxHours)
    {
        Die("too many hour entries", allowed);
    }
    p->hours[p->hoursCount] = h;
    return p->hoursCount++;
}

// Gives each code of a list the value of a field; a code with "_" is a
// language and region, which only hours have.
static void Assign(Prefs* p, const char* list, int field, int value)
{
    if (list == nullptr)
    {
        Die("a preference element without its list", "supplementalData.xml");
    }
    for (const char* at = list + strspn(list, " \t\n"); *at != '\0';)
    {
        size_t length = strcspn(at, " \t\n");
        const char* underscore = memchr(at, '_', length);
        if (underscore == nullptr)
        {
            uint32_t region = Region(at, length);
            if (p->fields[region][field] >= 0)
            {
                Die("a region given a field twice", at);
            }
            p->fields[region][field] = value;
        }
        else if (field != fieldHours || p->localeCount == maxLocales)
        {
            Die("a language and region where only regions go, or too many", at);
        }
        else
        {
            size_t languageLength = (size_t)(underscore - at);
            LocaleHours* l = &p->locales[p->localeCount++];
            l->language = mlocPackLanguage(at, (uint32_t)languageLength);
            l->region = Region(underscore + 1, length - languageLength - 1);
            l->hours = value;
            if (l->language == 0)
            {
                Die("not a language", at);
            }
        }
        at += length;
        at += strspn(at, " \t\n");
    }
}

static int Measurement(const char* type)
{
    static const char* const s_types[] = {"metric", "US", "UK"};
    for (int i = 0; type != nullptr && i < 3; i++)
    {
        if (strcmp(type, s_types[i]) == 0)
        {
            return i;
        }
    }
    Die("not a measurement system", type == nullptr ? "(none)" : type);
}

static void ReadWeek(Prefs* p, const XmlNode* week)
{
    for (int i = 0; week != nullptr && i < week->childCount; i++)
    {
        const XmlNode* e = week->children[i];
        if (XmlGetAttribute(e, "alt") != nullptr)
        {
            continue;
        }
        const char* territories = XmlGetAttribute(e, "territories");
        if (strcmp(e->name, "minDays") == 0)
        {
            const char* count = XmlGetAttribute(e, "count");
            int days = count == nullptr ? 0 : atoi(count);
            if (days < 1 || days > 7)
            {
                Die("minDays not of 1 to 7", count == nullptr ? "(none)" : count);
            }
            Assign(p, territories, fieldMinimalDays, days);
        }
        static const char* const s_days[3] = {"firstDay", "weekendStart", "weekendEnd"};
        static const int s_fields[3] = {fieldFirstDay, fieldWeekendStart, fieldWeekendEnd};
        for (int d = 0; d < 3; d++)
        {
            if (strcmp(e->name, s_days[d]) == 0)
            {
                Assign(p, territories, s_fields[d], Day(XmlGetAttribute(e, "day")));
            }
        }
    }
}

static void ReadTimeAndMeasurement(Prefs* p, const XmlNode* time, const XmlNode* measurement)
{
    for (int i = 0; time != nullptr && i < time->childCount; i++)
    {
        const XmlNode* e = time->children[i];
        const char* preferred = XmlGetAttribute(e, "preferred");
        const char* allowed = XmlGetAttribute(e, "allowed");
        if (strcmp(e->name, "hours") != 0 || preferred == nullptr || allowed == nullptr)
        {
            Die("a timeData element not hours with preferred and allowed", e->name);
        }
        Assign(p, XmlGetAttribute(e, "regions"), fieldHours, AddHours(p, preferred, allowed));
    }
    for (int i = 0; measurement != nullptr && i < measurement->childCount; i++)
    {
        const XmlNode* e = measurement->children[i];
        const char* territories = XmlGetAttribute(e, "territories");
        if (strcmp(e->name, "measurementSystem") == 0)
        {
            // The temperature category is left to the units' preferences.
            if (XmlGetAttribute(e, "category") == nullptr)
            {
                Assign(p, territories, fieldMeasurement, Measurement(XmlGetAttribute(e, "type")));
            }
        }
        else if (strcmp(e->name, "paperSize") == 0)
        {
            const char* type = XmlGetAttribute(e, "type");
            bool letter = type != nullptr && strcmp(type, "US-Letter") == 0;
            if (!letter && (type == nullptr || strcmp(type, "A4") != 0))
            {
                Die("not a paper size", type == nullptr ? "(none)" : type);
            }
            Assign(p, territories, fieldPaper, letter ? mloc_paperUsLetter : mloc_paperA4);
        }
    }
}

static void Store(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

static int CompareLocales(const void* a, const void* b)
{
    const LocaleHours* x = a;
    const LocaleHours* y = b;
    uint32_t kx = x->region << 16 | x->language;
    uint32_t ky = y->region << 16 | y->language;
    return kx < ky ? -1 : kx > ky;
}

// calendarPreferenceData: each region's first calendar, as an
// mlocCalendar.
static void ReadCalendars(Prefs* p, const XmlNode* data)
{
    static const char* const s_types[18] = {"gregorian",    "iso8601",
                                            "buddhist",     "roc",
                                            "japanese",     "coptic",
                                            "ethiopic",     "ethiopic-amete-alem",
                                            "indian",       "persian",
                                            "hebrew",       "islamic-civil",
                                            "islamic-tbla", "islamic-umalqura",
                                            "islamic",      "islamic-rgsa",
                                            "chinese",      "dangi"};
    for (int i = 0; data != nullptr && i < data->childCount; i++)
    {
        const char* ordering = XmlGetAttribute(data->children[i], "ordering");
        size_t length = ordering == nullptr ? 0 : strcspn(ordering, " ");
        int calendar = -1;
        for (int c = 0; c < 18 && calendar < 0; c++)
        {
            calendar =
                strlen(s_types[c]) == length && strncmp(ordering, s_types[c], length) == 0 ? c : -1;
        }
        if (calendar < 0)
        {
            Die("a calendar preference of no known calendar", ordering ? ordering : "(none)");
        }
        Assign(p, XmlGetAttribute(data->children[i], "territories"), fieldCalendar, calendar);
    }
}

// Fills every region's missing fields from 001 and writes the unit.
static uint8_t* Write(Prefs* p, size_t* sizeOut)
{
    const int* world = p->fields[mlocPackRegion("001", 3)];
    for (int f = 0; f < fieldCount; f++)
    {
        if (world[f] < 0)
        {
            Die("001 lacks a preference", "supplementalData.xml");
        }
    }
    size_t regionCount = 0;
    for (uint32_t slot = 0; slot < slots; slot++)
    {
        bool any = false;
        for (int f = 0; f < fieldCount; f++)
        {
            any |= p->fields[slot][f] >= 0;
        }
        regionCount += any;
    }
    GenSort(p->locales, (size_t)p->localeCount, sizeof(LocaleHours), CompareLocales);
    size_t hoursAt = preferHeaderSize;
    size_t regionsAt = hoursAt + (size_t)p->hoursCount * preferEntrySize;
    size_t localesAt = regionsAt + regionCount * preferEntrySize;
    size_t size = localesAt + (size_t)p->localeCount * preferEntrySize;
    uint8_t* out = calloc(size, 1);
    if (out == nullptr)
    {
        Die("out of memory", "preferences");
    }
    Store(out, regionCount, 2);
    Store(out + 2, (uint64_t)p->localeCount, 2);
    Store(out + 4, (uint64_t)p->hoursCount, 2);
    for (int i = 0; i < p->hoursCount; i++)
    {
        uint8_t* e = out + hoursAt + (size_t)i * preferEntrySize;
        e[0] = p->hours[i].preferred;
        e[1] = p->hours[i].count;
        memcpy(e + 2, p->hours[i].allowed, p->hours[i].count);
    }
    size_t r = 0;
    for (uint32_t slot = 0; slot < slots; slot++)
    {
        int v[fieldCount];
        bool any = false;
        for (int f = 0; f < fieldCount; f++)
        {
            any |= p->fields[slot][f] >= 0;
            v[f] = p->fields[slot][f] >= 0 ? p->fields[slot][f] : world[f];
        }
        if (!any)
        {
            continue;
        }
        uint8_t* e = out + regionsAt + r++ * preferEntrySize;
        Store(e, slot, 2);
        e[2] = (uint8_t)v[fieldHours];
        e[3] = (uint8_t)(v[fieldFirstDay] | v[fieldMinimalDays] << 4);
        e[4] = (uint8_t)v[fieldWeekendStart];
        e[5] = (uint8_t)v[fieldWeekendEnd];
        e[6] = (uint8_t)(v[fieldMeasurement] | v[fieldPaper] << 4);
        e[7] = (uint8_t)v[fieldCalendar];
    }
    for (int i = 0; i < p->localeCount; i++)
    {
        uint8_t* e = out + localesAt + (size_t)i * preferEntrySize;
        if (i > 0 && CompareLocales(&p->locales[i - 1], &p->locales[i]) == 0)
        {
            Die("a language and region given hours twice", "timeData");
        }
        Store(e, p->locales[i].language, 2);
        Store(e + 2, p->locales[i].region, 2);
        e[4] = (uint8_t)p->locales[i].hours;
    }
    *sizeOut = size;
    return out;
}

int GeneratePreferenceUnit(const char* commonDirectory, DataFile* file, char* errorOut,
                           size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/supplementalData.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    static Prefs s_prefs;
    memset(&s_prefs, 0xFF, sizeof(s_prefs.fields));
    s_prefs.hoursCount = 0;
    s_prefs.localeCount = 0;
    ReadWeek(&s_prefs, XmlFindChild(root, "weekData"));
    ReadTimeAndMeasurement(&s_prefs, XmlFindChild(root, "timeData"),
                           XmlFindChild(root, "measurementData"));
    ReadCalendars(&s_prefs, XmlFindChild(root, "calendarPreferenceData"));
    XmlFree(root);
    size_t size = 0;
    uint8_t* unit = Write(&s_prefs, &size);
    DataFileAdd(file, kindLocalePreferences, nullptr, unit, size);
    free(unit);
    return 1;
}
