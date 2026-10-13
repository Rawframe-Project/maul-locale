// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The duration and coordinate patterns (unit_patterns_gen.h). Each
// locale's fifteen patterns are resolved as CLDR resolves them, root's
// aliases of widths followed; a locale whose patterns match those of
// the nearest locale with another key along its chain files nothing, and
// one under its parent's key is checked to match.

#include "unit_patterns_gen.h"

#include "kinds.h"
#include "table_gen.h"
#include "unit_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxKey = 64,
    maxChain = 160,
};

typedef struct Patterns
{
    const char* text[unitPatternCount];
} Patterns;

static const char* const s_durations[3] = {"hm", "ms", "hms"};
static const char* const s_widths[3] = {"long", "short", "narrow"};
static const char* const s_directions[4] = {"east", "north", "south", "west"};

static void Resolve(Cldr* cldr, const char* locale, Patterns* out)
{
    for (int i = 0; i < unitPatternCount; i++)
    {
        char chain[maxChain];
        if (i < unitPatternCoordinates)
        {
            snprintf(chain, sizeof(chain),
                     "//ldml/units/durationUnit[@type=\"%s\"]/durationUnitPattern", s_durations[i]);
        }
        else
        {
            int c = i - unitPatternCoordinates;
            snprintf(chain, sizeof(chain),
                     "//ldml/units/unitLength[@type=\"%s\"]/coordinateUnit/"
                     "coordinateUnitPattern[@type=\"%s\"]",
                     s_widths[c / 4], s_directions[c % 4]);
        }
        const Leaf* leaf = CldrResolve(cldr, locale, chain, componentMain);
        const char* zero = leaf == nullptr ? nullptr : strstr(leaf->text, "{0}");
        if (leaf == nullptr || (i >= unitPatternCoordinates &&
                                (zero == nullptr || strstr(zero + 3, "{0}") != nullptr)))
        {
            GenDie("a duration or coordinate pattern missing or not of one {0}", chain);
        }
        out->text[i] = leaf->text;
    }
}

static bool Same(const Patterns* a, const Patterns* b)
{
    for (int i = 0; i < unitPatternCount; i++)
    {
        if (strcmp(a->text[i], b->text[i]) != 0)
        {
            return false;
        }
    }
    return true;
}

static const char* NextKeyed(const UnitNamesInput* in, const char* locale, const char* key)
{
    for (const char* at = CldrParent(in->cldr, locale, componentMain); at != nullptr;
         at = CldrParent(in->cldr, at, componentMain))
    {
        char other[maxKey];
        KeyOf(in->keys, at, other, sizeof(other));
        if (strcmp(other, key) != 0)
        {
            return at;
        }
    }
    return nullptr;
}

static int IndexOf(const UnitNamesInput* in, const char* id)
{
    for (int i = 0; i < in->count; i++)
    {
        if (strcmp(in->ids[i], id) == 0)
        {
            return i;
        }
    }
    GenDie("a parent locale without a file", id);
}

static void File(const Patterns* p, const char* key, DataFile* file)
{
    static Table s_table;
    s_table = (Table){.recordSize = 2 * unitPatternCount};
    uint8_t record[2 * unitPatternCount];
    for (int i = 0; i < unitPatternCount; i++)
    {
        GenPut(record + 2 * i, TableAddString(&s_table, p->text[i], strlen(p->text[i])), 2);
    }
    (void)TableAddRecord(&s_table, record);
    TableAddUnit(&s_table, kindUnitPatterns, key, file);
    TableClear(&s_table);
}

void GenerateUnitPatterns(const UnitNamesInput* in, DataFile* file)
{
    Patterns* all = GenAllocate(sizeof(Patterns) * (size_t)in->count);
    for (int i = 0; i < in->count; i++)
    {
        Resolve(in->cldr, in->ids[i], &all[i]);
    }
    for (int i = 0; i < in->count; i++)
    {
        char key[maxKey];
        KeyOf(in->keys, in->ids[i], key, sizeof(key));
        const char* parent = CldrParent(in->cldr, in->ids[i], componentMain);
        char parentKey[maxKey] = "";
        if (parent != nullptr)
        {
            KeyOf(in->keys, parent, parentKey, sizeof(parentKey));
        }
        if (parent != nullptr && strcmp(parentKey, key) == 0)
        {
            if (!Same(&all[IndexOf(in, parent)], &all[i]))
            {
                GenDie("a locale's unit patterns differ from those of its parent's key",
                       in->ids[i]);
            }
            continue;
        }
        const char* next = NextKeyed(in, in->ids[i], key);
        if (next == nullptr || !Same(&all[IndexOf(in, next)], &all[i]))
        {
            File(&all[i], key, file);
        }
    }
    free(all);
}
