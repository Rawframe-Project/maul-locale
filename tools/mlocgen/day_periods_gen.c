// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's day period rules (day_periods_gen.h).

#include "day_periods_gen.h"

#include "date_tables.h"
#include "xml.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    unset = 0xFF,
};

// The periods by DateDayPeriod.
static const char* const s_periods[dateDayPeriodCount] = {
    "am",         "pm",         "midnight", "noon",     "morning1", "morning2",
    "afternoon1", "afternoon2", "evening1", "evening2", "night1",   "night2"};

static int PeriodOf(const char* type)
{
    for (int i = 0; type != nullptr && i < dateDayPeriodCount; i++)
    {
        if (strcmp(type, s_periods[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

// The hour of "HH:00", or -1 for any other time.
static int HourOf(const char* time)
{
    if (time == nullptr || strlen(time) != 5 || strcmp(time + 2, ":00") != 0 || time[0] < '0' ||
        time[0] > '2' || time[1] < '0' || time[1] > '9')
    {
        return -1;
    }
    int hour = (time[0] - '0') * 10 + (time[1] - '0');
    return hour <= 24 ? hour : -1;
}

// One rule applied to a locale's bytes; false for a rule CLDR's schema
// does not allow.
static bool Apply(const XmlNode* rule, uint8_t bytes[dateRuleSize])
{
    int period = PeriodOf(XmlGetAttribute(rule, "type"));
    const char* at = XmlGetAttribute(rule, "at");
    if (period < 0)
    {
        return false;
    }
    if (at != nullptr)
    {
        int hour = HourOf(at);
        if (period == dateDayPeriodNoon && hour == 12)
        {
            bytes[24] |= dateRuleNoon;
            return true;
        }
        if (period == dateDayPeriodMidnight && (hour == 0 || hour == 24))
        {
            bytes[24] |= dateRuleMidnight;
            return true;
        }
        return false;
    }
    int from = HourOf(XmlGetAttribute(rule, "from"));
    int before = HourOf(XmlGetAttribute(rule, "before"));
    if (from < 0 || before < 0 || from == 24 || before == 0 || period == dateDayPeriodNoon ||
        period == dateDayPeriodMidnight)
    {
        return false;
    }
    int hour = from;
    do
    {
        bytes[hour] = (uint8_t)period;
        hour = (hour + 1) % 24;
    } while (hour != before % 24);
    return true;
}

static int Fail(char* errorOut, size_t errorCapacity, const char* message, const char* detail)
{
    snprintf(errorOut, errorCapacity, "dayPeriods.xml: %s: %s", message, detail);
    return 0;
}

// A locale list's rules, added for each locale it names.
static int AddRules(Map* rules, const XmlNode* list, char* errorOut, size_t errorCapacity)
{
    const char* locales = XmlGetAttribute(list, "locales");
    if (locales == nullptr)
    {
        return Fail(errorOut, errorCapacity, "rules without locales", "");
    }
    uint8_t bytes[dateRuleSize];
    memset(bytes, unset, 24);
    bytes[24] = 0;
    for (int i = 0; i < list->childCount; i++)
    {
        if (!Apply(list->children[i], bytes))
        {
            return Fail(errorOut, errorCapacity, "a rule out of the schema", locales);
        }
    }
    for (int hour = 0; hour < 24; hour++)
    {
        if (bytes[hour] == unset)
        {
            return Fail(errorOut, errorCapacity, "an hour without a period", locales);
        }
    }
    for (const char* at = locales; *at != '\0';)
    {
        char key[32];
        size_t length = strcspn(at, " ");
        snprintf(key, sizeof(key), "%.*s", (int)length, at);
        uint8_t* copy = malloc(dateRuleSize);
        if (copy == nullptr)
        {
            return Fail(errorOut, errorCapacity, "out of memory", key);
        }
        memcpy(copy, bytes, dateRuleSize);
        free(MapGet(rules, key));
        MapPut(rules, key, copy);
        at += length + (at[length] == ' ');
    }
    return 1;
}

int DayPeriodRulesLoad(Map* rules, const char* commonDirectory, char* errorOut,
                       size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/dayPeriods.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    int ok = 1;
    for (int i = 0; ok && i < root->childCount; i++)
    {
        const XmlNode* set = root->children[i];
        if (strcmp(set->name, "dayPeriodRuleSet") != 0 || XmlGetAttribute(set, "type") != nullptr)
        {
            continue;
        }
        for (int j = 0; ok && j < set->childCount; j++)
        {
            ok = AddRules(rules, set->children[j], errorOut, errorCapacity);
        }
    }
    XmlFree(root);
    if (ok && MapGet(rules, "root") == nullptr)
    {
        return Fail(errorOut, errorCapacity, "no rules for root", path);
    }
    return ok;
}

static void FreeValue(const char* key, void* value, void* context)
{
    (void)key;
    (void)context;
    free(value);
}

void DayPeriodRulesFree(Map* rules)
{
    MapEach(rules, FreeValue, nullptr);
    MapFree(rules);
}
