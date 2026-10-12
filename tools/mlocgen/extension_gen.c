// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The aliases of extension values (UTS #35 Annex C, Processing
// LocaleIds) from common/bcp47/*.xml and the subdivision aliases of
// supplementalMetadata.xml. A type's alias values that can be a value
// in a tag ("imperial", "cet") map to its name; a deprecated type maps
// to its preferred one and its own aliases are left out, since they
// would point back at it (islamicc is an alias of islamic-civil and
// deprecated in its favor). A subdivision with several replacements
// takes the first, as ICU does. The layout is src/locale_tables.h's.

#include "extension_gen.h"

#include "kinds.h"
#include "locale_tables.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TEXT 64

typedef struct Pair
{
    char key[3];
    char from[MAX_TEXT];
    char to[MAX_TEXT];
} Pair;

typedef struct Pairs
{
    Pair* items;
    size_t count;
    size_t capacity;
} Pairs;

[[noreturn]] static void Die(const char* message, const char* detail)
{
    fprintf(stderr, "mlocgen: %s: %s\n", message, detail);
    exit(1);
}

static void Lower(const char* text, char* out, size_t capacity)
{
    size_t length = strlen(text);
    if (length >= capacity)
    {
        Die("an extension value longer than the generator holds", text);
    }
    for (size_t i = 0; i <= length; i++)
    {
        out[i] = text[i] >= 'A' && text[i] <= 'Z' ? (char)(text[i] + 32) : text[i];
    }
}

// Whether text is a value a tag can hold: subtags of 3 to 8 lowercase
// letters and digits joined by "-".
static bool IsValue(const char* text)
{
    size_t run = 0;
    for (const char* p = text;; p++)
    {
        if (*p == '-' || *p == '\0')
        {
            if (run < 3 || run > 8)
            {
                return false;
            }
            if (*p == '\0')
            {
                return true;
            }
            run = 0;
        }
        else if ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))
        {
            run++;
        }
        else
        {
            return false;
        }
    }
}

static void Add(Pairs* pairs, const char* key, const char* from, const char* to)
{
    if (pairs->count == pairs->capacity)
    {
        pairs->capacity = pairs->capacity == 0 ? 256 : pairs->capacity * 2;
        pairs->items = realloc(pairs->items, pairs->capacity * sizeof(Pair));
        if (pairs->items == nullptr)
        {
            Die("out of memory", "extension aliases");
        }
    }
    Pair* pair = &pairs->items[pairs->count++];
    snprintf(pair->key, sizeof(pair->key), "%s", key);
    Lower(from, pair->from, sizeof(pair->from));
    Lower(to, pair->to, sizeof(pair->to));
}

// One type of a key: its deprecation, or its aliases.
static void AddType(Pairs* pairs, const char* key, const XmlNode* type)
{
    const char* name = XmlGetAttribute(type, "name");
    const char* deprecated = XmlGetAttribute(type, "deprecated");
    const char* preferred = XmlGetAttribute(type, "preferred");
    const char* alias = XmlGetAttribute(type, "alias");
    char lowered[MAX_TEXT];
    if (name == nullptr)
    {
        return;
    }
    if (deprecated != nullptr && strcmp(deprecated, "true") == 0)
    {
        Lower(name, lowered, sizeof(lowered));
        if (preferred != nullptr && IsValue(lowered))
        {
            Add(pairs, key, name, preferred);
        }
        return;
    }
    if (alias == nullptr)
    {
        return;
    }
    char list[512];
    snprintf(list, sizeof(list), "%s", alias);
    for (char* one = list; *one != '\0';)
    {
        char* end = strchr(one, ' ');
        if (end != nullptr)
        {
            *end = '\0';
        }
        if (strlen(one) < sizeof(lowered))
        {
            Lower(one, lowered, sizeof(lowered));
            if (IsValue(lowered) && strcmp(lowered, name) != 0)
            {
                Add(pairs, key, lowered, name);
            }
        }
        one = end == nullptr ? one + strlen(one) : end + 1;
    }
}

static int AddFile(Pairs* pairs, const char* path, char* errorOut, size_t errorCapacity)
{
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    const XmlNode* keyword = XmlFindChild(root, "keyword");
    for (int k = 0; keyword != nullptr && k < keyword->childCount; k++)
    {
        const XmlNode* key = keyword->children[k];
        const char* name = XmlGetAttribute(key, "name");
        if (strcmp(key->name, "key") != 0 || name == nullptr || strlen(name) != 2)
        {
            continue;
        }
        for (int t = 0; t < key->childCount; t++)
        {
            if (strcmp(key->children[t]->name, "type") == 0)
            {
                AddType(pairs, name, key->children[t]);
            }
        }
    }
    XmlFree(root);
    return 1;
}

static int ComparePairs(const void* a, const void* b)
{
    const Pair* x = a;
    const Pair* y = b;
    int order = strcmp(x->key, y->key);
    return order != 0 ? order : strcmp(x->from, y->from);
}

static int AddSubdivisions(Pairs* pairs, const char* common, char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/supplementalMetadata.xml", common);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    const XmlNode* metadata = XmlFindChild(root, "metadata");
    const XmlNode* list = metadata == nullptr ? nullptr : XmlFindChild(metadata, "alias");
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        const XmlNode* alias = list->children[i];
        const char* type = XmlGetAttribute(alias, "type");
        const char* replacement = XmlGetAttribute(alias, "replacement");
        if (strcmp(alias->name, "subdivisionAlias") != 0 || type == nullptr ||
            replacement == nullptr)
        {
            continue;
        }
        char first[MAX_TEXT];
        snprintf(first, sizeof(first), "%.*s", (int)strcspn(replacement, " "), replacement);
        char to[MAX_TEXT + 4];
        snprintf(to, sizeof(to), "%s%s", first, strlen(first) == 2 ? "zzzz" : "");
        Add(pairs, "", type, to);
    }
    XmlFree(root);
    return list != nullptr;
}

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// Writes the unit; each from and to text is stored once in text.
static uint8_t* Write(const Pairs* types, const Pairs* subdivisions, size_t* sizeOut)
{
    size_t text = 0;
    const Pairs* both[2] = {types, subdivisions};
    for (int b = 0; b < 2; b++)
    {
        for (size_t i = 0; i < both[b]->count; i++)
        {
            text += strlen(both[b]->items[i].from) + strlen(both[b]->items[i].to);
        }
    }
    size_t typesAt = extensionHeaderSize;
    size_t subdivisionsAt = typesAt + types->count * extensionTypeSize;
    size_t textAt = subdivisionsAt + subdivisions->count * extensionSubdivisionSize;
    size_t size = textAt + text;
    uint8_t* out = calloc(size, 1);
    if (out == nullptr)
    {
        Die("out of memory", "extension aliases");
    }
    Put(out, types->count, 4);
    Put(out + 4, subdivisions->count, 4);
    Put(out + 8, text, 4);
    size_t at = 0;
    for (int b = 0; b < 2; b++)
    {
        for (size_t i = 0; i < both[b]->count; i++)
        {
            const Pair* pair = &both[b]->items[i];
            size_t fromLength = strlen(pair->from);
            size_t toLength = strlen(pair->to);
            uint8_t* entry = b == 0 ? out + typesAt + i * extensionTypeSize
                                    : out + subdivisionsAt + i * extensionSubdivisionSize;
            if (b == 0)
            {
                memcpy(entry, pair->key, 2);
            }
            entry[b == 0 ? 2 : 0] = (uint8_t)fromLength;
            entry[b == 0 ? 3 : 1] = (uint8_t)toLength;
            Put(entry + 4, at, 4);
            Put(entry + 8, at + fromLength, 4);
            memcpy(out + textAt + at, pair->from, fromLength);
            memcpy(out + textAt + at + fromLength, pair->to, toLength);
            at += fromLength + toLength;
        }
    }
    *sizeOut = size;
    return out;
}

int GenerateExtensionUnit(const char* commonDirectory, DataFile* file, char* errorOut,
                          size_t errorCapacity)
{
    static const char* const s_files[] = {
        "calendar",
        "collation",
        "currency",
        "measure",
        "number",
        "segmentation",
        "timezone",
        "transform",
        "transform_hybrid",
        "transform_ime",
        "transform_keyboard",
        "transform_mt",
        "transform_private_use",
        "transform-destination",
        "variant",
    };
    Pairs types = {0};
    Pairs subdivisions = {0};
    char path[4096];
    for (size_t f = 0; f < sizeof(s_files) / sizeof(s_files[0]); f++)
    {
        snprintf(path, sizeof(path), "%s/bcp47/%s.xml", commonDirectory, s_files[f]);
        if (!AddFile(&types, path, errorOut, errorCapacity))
        {
            free(types.items);
            return 0;
        }
    }
    if (!AddSubdivisions(&subdivisions, commonDirectory, errorOut, errorCapacity))
    {
        free(types.items);
        free(subdivisions.items);
        return 0;
    }
    qsort(types.items, types.count, sizeof(Pair), ComparePairs);
    qsort(subdivisions.items, subdivisions.count, sizeof(Pair), ComparePairs);
    for (size_t i = 1; i < types.count; i++)
    {
        if (ComparePairs(&types.items[i - 1], &types.items[i]) == 0)
        {
            Die("an extension value with two aliases", types.items[i].from);
        }
    }
    size_t size = 0;
    uint8_t* unit = Write(&types, &subdivisions, &size);
    DataFileAdd(file, kindLocaleExtensionAliases, nullptr, unit, size);
    free(unit);
    free(types.items);
    free(subdivisions.items);
    return 1;
}
