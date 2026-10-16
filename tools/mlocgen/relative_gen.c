// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The relative time data file (record mloc-0043): each locale's fields
// resolved along its chain, widths through root's aliases, filed where
// the library's lookup along the chain would give another value.

#include "relative_gen.h"

#include "kinds.h"
#include "relative_tables.h"
#include "table_gen.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxKey = 64,
    maxChain = 256,
};

static const char s_prefix[] = "//ldml/dates/fields/field[@type=\"";

// RelativeUnit's CLDR field names.
static const char* const s_units[relativeUnitCount] = {
    "year", "quarter", "month", "week", "day", "hour", "minute", "second",
    "sun",  "mon",     "tue",   "wed",  "thu", "fri",  "sat",
};

static const char* const s_widths[relativeWidthCount] = {"", "-short", "-narrow"};

static const char* const s_offsets[relativeNames] = {"-2", "-1", "0", "1", "2"};

static const char* const s_categories[6] = {"zero", "one", "two", "few", "many", "other"};

// A locale's filed values, one per key, or null.
typedef struct Filed
{
    const char* values[relativeKeyCount];
} Filed;

typedef struct Relative
{
    const NamesInput* input;
    // Locale id to Filed.
    Map filed;
} Relative;

// The chain of a key.
static void ChainOf(uint32_t key, char* out, size_t capacity)
{
    uint32_t slot = key % relativeSlots;
    uint32_t item = key / relativeSlots;
    const char* unit = s_units[item / relativeWidthCount];
    const char* width = s_widths[item % relativeWidthCount];
    if (slot < relativeNames)
    {
        snprintf(out, capacity, "%s%s%s\"]/relative[@type=\"%s\"]", s_prefix, unit, width,
                 s_offsets[slot]);
        return;
    }
    bool future = slot < relativePast;
    uint32_t category = slot - (future ? relativeFuture : relativePast);
    snprintf(out, capacity,
             "%s%s%s\"]/relativeTime[@type=\"%s\"]/relativeTimePattern[@count=\"%s\"]", s_prefix,
             unit, width, future ? "future" : "past", s_categories[category]);
}

// CLDR's value of a key at a locale, or null.
static const char* Resolved(const Relative* r, const char* locale, uint32_t key)
{
    char chain[maxChain];
    ChainOf(key, chain, sizeof(chain));
    const Leaf* leaf = CldrResolve(r->input->cldr, locale, chain, componentMain);
    return leaf != nullptr ? leaf->text : nullptr;
}

// The value a key files at a locale, or null: a category's pattern equal
// to other's is left to the run time's fallback to other (CLDR's count
// inheritance gives every category other's where the locale has none).
static const char* Target(const Relative* r, const char* locale, uint32_t key)
{
    const char* value = Resolved(r, locale, key);
    uint32_t slot = key % relativeSlots;
    uint32_t other = slot < relativePast ? relativePast - 1 : relativeSlots - 1;
    if (value != nullptr && slot >= relativeFuture && slot != other)
    {
        const char* fallback = Resolved(r, locale, key - slot + other);
        return fallback != nullptr && strcmp(fallback, value) == 0 ? nullptr : value;
    }
    return value;
}

// Whether a value is text the unit holds: no control byte, a pattern's
// "{0}" at most once, a name's never.
static bool ValidValue(uint32_t key, const char* value)
{
    for (const unsigned char* at = (const unsigned char*)value; *at != '\0';)
    {
        int length = *at < 0x20 ? 0 : GenCharLength(at);
        if (length == 0)
        {
            return false;
        }
        at += length;
    }
    const char* first = strstr(value, "{0}");
    bool once = first == nullptr || strstr(first + 1, "{0}") == nullptr;
    return key % relativeSlots >= relativeFuture ? once : first == nullptr;
}

// The first value of a key filed along a locale's chain.
static const char* FiledValue(const Relative* r, const char* locale, uint32_t key)
{
    for (const char* at = locale; at != nullptr; at = CldrParent(r->input->cldr, at, componentMain))
    {
        const Filed* filed = MapGet(&r->filed, at);
        if (filed != nullptr && filed->values[key] != nullptr)
        {
            return filed->values[key];
        }
    }
    return nullptr;
}

// The library's lookup of a key: its width's along the chain, else the
// next wider width's (narrow, short, long).
static const char* LookedValue(const Relative* r, const char* locale, uint32_t key)
{
    uint32_t width = key / relativeSlots % relativeWidthCount;
    for (uint32_t w = 0; w <= width; w++)
    {
        const char* value = FiledValue(r, locale, key - w * relativeSlots);
        if (value != nullptr)
        {
            return value;
        }
    }
    return nullptr;
}

// Files a value at a locale, or checks a locale that files nothing. A
// value CLDR leaves out where the chain gives one is filed as empty.
static void Take(const Relative* r, Filed* filed, const char* locale, uint32_t key,
                 const char* value)
{
    const char* looked = LookedValue(r, locale, key);
    bool none = value == nullptr || value[0] == '\0';
    bool lookedNone = looked == nullptr || looked[0] == '\0';
    if ((none && lookedNone) || (!none && !lookedNone && strcmp(looked, value) == 0))
    {
        return;
    }
    if (filed == nullptr)
    {
        GenDie("a locale's relative time differs from that of its parent's key", locale);
    }
    if (value != nullptr && !ValidValue(key, value))
    {
        GenDie("a relative time text of a control or malformed byte, a pattern of two {0} or "
               "a name of one",
               value);
    }
    filed->values[key] = value != nullptr ? value : "";
}

// The index of a value among a unit's strings, added when new.
static size_t StringOf(const char** strings, size_t* count, const char* value)
{
    for (size_t i = 0; i < *count; i++)
    {
        if (strcmp(strings[i], value) == 0)
        {
            return i;
        }
    }
    strings[*count] = value;
    return (*count)++;
}

// Adds a locale key's unit, each distinct text once.
static void AddUnit(const Filed* filed, const char* key, DataFile* file)
{
    static const char* s_strings[relativeKeyCount];
    uint16_t indexes[relativeKeyCount];
    size_t count = 0;
    size_t stringCount = 0;
    size_t text = 0;
    for (uint32_t k = 0; k < relativeKeyCount; k++)
    {
        if (filed->values[k] != nullptr)
        {
            size_t before = stringCount;
            indexes[k] = (uint16_t)StringOf(s_strings, &stringCount, filed->values[k]);
            text += stringCount > before ? strlen(filed->values[k]) : 0;
            count++;
        }
    }
    if (count == 0)
    {
        return;
    }
    size_t size = relativeHeaderSize + 4 * count + 4 * (stringCount + 1) + text;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, count, 4);
    GenPut(unit + 4, stringCount, 4);
    GenPut(unit + 8, text, 4);
    uint8_t* keys = unit + relativeHeaderSize;
    uint8_t* strings = keys + 2 * count;
    uint8_t* offsets = strings + 2 * count;
    uint8_t* textAt = offsets + 4 * (stringCount + 1);
    size_t entry = 0;
    for (uint32_t k = 0; k < relativeKeyCount; k++)
    {
        if (filed->values[k] != nullptr)
        {
            GenPut(keys + 2 * entry, k, 2);
            GenPut(strings + 2 * entry, indexes[k], 2);
            entry++;
        }
    }
    size_t at = 0;
    for (size_t i = 0; i < stringCount; i++)
    {
        size_t length = strlen(s_strings[i]);
        GenPut(offsets + 4 * i, at, 4);
        memcpy(textAt + at, s_strings[i], length);
        at += length;
    }
    GenPut(offsets + 4 * stringCount, at, 4);
    DataFileAdd(file, kindRelative, key, unit, size);
    free(unit);
}

// Takes a locale after its parent.
static void TakeChain(Relative* r, const char* locale, DataFile* file)
{
    if (MapGet(&r->filed, locale) != nullptr)
    {
        return;
    }
    const NamesInput* input = r->input;
    const char* parent = CldrParent(input->cldr, locale, componentMain);
    if (parent != nullptr)
    {
        TakeChain(r, parent, file);
    }
    char key[maxKey];
    char parentKey[maxKey] = "";
    KeyOf(input->keys, locale, key, sizeof(key));
    if (parent != nullptr)
    {
        KeyOf(input->keys, parent, parentKey, sizeof(parentKey));
    }
    Filed* filed = GenAllocate(sizeof(Filed));
    bool own = strcmp(key, parentKey) != 0;
    MapPut(&r->filed, locale, filed);
    for (uint32_t k = 0; k < relativeKeyCount; k++)
    {
        Take(r, own ? filed : nullptr, locale, k, Target(r, locale, k));
    }
    if (own)
    {
        AddUnit(filed, key, file);
    }
}

int GenerateRelative(const NamesInput* input, DataFile* file, char* errorOut, size_t errorCapacity)
{
    (void)errorOut;
    (void)errorCapacity;
    static Relative s_relative;
    s_relative = (Relative){.input = input};
    for (int i = 0; i < input->count; i++)
    {
        TakeChain(&s_relative, input->ids[i], file);
    }
    return 1;
}
