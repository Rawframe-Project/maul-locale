// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The list patterns (list_gen.h). Each locale's nine styles are resolved
// as CLDR resolves them, root's aliases among the styles followed from
// the locale, which is what ICU's ListPatternsSink reads. A locale whose
// sets match those of the nearest locale with another key along its
// chain files nothing; one under its parent's key is checked to match.

#include "list_gen.h"

#include "kinds.h"
#include "list_tables.h"
#include "table_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxKey = 64,
    maxChain = 160,
};

static const char* const s_styles[listStyleCount] = {"",     "standard-short", "standard-narrow",
                                                     "or",   "or-short",       "or-narrow",
                                                     "unit", "unit-short",     "unit-narrow"};
static const char* const s_parts[listPatternCount] = {"2", "start", "middle", "end"};

typedef struct Lists
{
    uint16_t sets[listStyleCount];
} Lists;

// Whether a pattern holds "{0}" and "{1}" once each and no other brace.
static bool ValidPattern(const char* text)
{
    const char* zero = strstr(text, "{0}");
    const char* one = strstr(text, "{1}");
    if (zero == nullptr || one == nullptr || strstr(zero + 3, "{0}") != nullptr ||
        strstr(one + 3, "{1}") != nullptr)
    {
        return false;
    }
    int braces = 0;
    for (const char* at = text; *at != '\0'; at++)
    {
        braces += *at == '{' || *at == '}';
    }
    return braces == 4;
}

static void Resolve(Cldr* cldr, Table* sets, const char* locale, Lists* out)
{
    for (int s = 0; s < listStyleCount; s++)
    {
        uint8_t record[listSetSize];
        for (int p = 0; p < listPatternCount; p++)
        {
            char chain[maxChain];
            snprintf(
                chain, sizeof(chain),
                s_styles[s][0] == '\0'
                    ? "//ldml/listPatterns/listPattern%s/listPatternPart[@type=\"%s\"]"
                    : "//ldml/listPatterns/listPattern[@type=\"%s\"]/listPatternPart[@type=\"%s\"]",
                s_styles[s], s_parts[p]);
            const Leaf* leaf = CldrResolve(cldr, locale, chain, componentMain);
            if (leaf == nullptr || !ValidPattern(leaf->text))
            {
                GenDie("a list pattern missing or not of two items", chain);
            }
            GenPut(record + 2 * p, TableAddString(sets, leaf->text, strlen(leaf->text)), 2);
        }
        out->sets[s] = TableAddRecord(sets, record);
    }
}

// The nearest locale along a locale's chain whose key differs from its
// own, or null for root.
static const char* NextKeyed(const ListInput* in, const char* locale, const char* key)
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

static int IndexOf(const ListInput* in, const char* id)
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

void GenerateListUnits(const ListInput* in, DataFile* file)
{
    static Table s_sets;
    s_sets = (Table){.recordSize = listSetSize, .distinctRecords = true};
    Lists* all = GenAllocate(sizeof(Lists) * (size_t)in->count);
    for (int i = 0; i < in->count; i++)
    {
        Resolve(in->cldr, &s_sets, in->ids[i], &all[i]);
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
            if (memcmp(&all[IndexOf(in, parent)], &all[i], sizeof(Lists)) != 0)
            {
                GenDie("a locale's lists differ from those of its parent's key", in->ids[i]);
            }
            continue;
        }
        const char* next = NextKeyed(in, in->ids[i], key);
        if (next == nullptr || memcmp(&all[IndexOf(in, next)], &all[i], sizeof(Lists)) != 0)
        {
            uint8_t unit[listLocaleSize];
            for (int s = 0; s < listStyleCount; s++)
            {
                GenPut(unit + 2 * s, all[i].sets[s], 2);
            }
            DataFileAdd(file, kindLists, key, unit, sizeof(unit));
        }
    }
    TableAddUnit(&s_sets, kindListPatterns, nullptr, file);
    TableClear(&s_sets);
    free(all);
}
