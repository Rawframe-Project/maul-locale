// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The locale part's units (locale_gen.h).

#include "locale_gen.h"

#include "alias_gen.h"
#include "extension_gen.h"
#include "kinds.h"
#include "locale_tables.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Entry
{
    uint64_t key;
    uint64_t value;
} Entry;

// Packs "xx_Yyyy_ZZ" (script and region each optional); 0 when a part
// is not a subtag the table can hold. Sets the parts it found.
static uint64_t PackId(const char* id, bool* scriptOut, bool* regionOut)
{
    char parts[3][9] = {{0}};
    int count = 0;
    for (const char* at = id; count < 3;)
    {
        size_t length = strcspn(at, "_");
        if (length == 0 || length > 8)
        {
            return 0;
        }
        memcpy(parts[count++], at, length);
        at += length;
        if (*at == '\0')
        {
            break;
        }
        at++;
        if (count == 3)
        {
            return 0;
        }
    }
    uint32_t language = mlocPackLanguage(parts[0], (uint32_t)strlen(parts[0]));
    uint32_t script = 0;
    uint32_t region = 0;
    int next = 1;
    if (next < count && strlen(parts[next]) == 4)
    {
        script = mlocPackScript(parts[next], 4);
        if (script == 0)
        {
            return 0;
        }
        next++;
    }
    if (next < count)
    {
        region = mlocPackRegion(parts[next], (uint32_t)strlen(parts[next]));
        if (region == 0)
        {
            return 0;
        }
        next++;
    }
    if (language == 0 || next != count)
    {
        return 0;
    }
    *scriptOut = script != 0;
    *regionOut = region != 0;
    return mlocPackTag(language, script, region);
}

static int CompareEntries(const void* a, const void* b)
{
    uint64_t x = ((const Entry*)a)->key;
    uint64_t y = ((const Entry*)b)->key;
    return (x > y) - (x < y);
}

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// Writes the table of src/locale_tables.h from entries sorted by key.
static uint8_t* LikelyTable(const Entry* general, size_t generalCount, const Entry* language,
                            size_t languageCount, size_t* sizeOut)
{
    size_t size = 16 + generalCount * 16 + languageCount * 6;
    uint8_t* table = calloc(size, 1);
    if (table == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    Put(table, languageCount, 4);
    Put(table + 4, generalCount, 4);
    uint8_t* at = table + 16;
    for (size_t i = 0; i < generalCount; i++)
    {
        Put(at + i * 8, general[i].key, 8);
        Put(at + (generalCount + i) * 8, general[i].value, 8);
    }
    at += generalCount * 16;
    for (size_t i = 0; i < languageCount; i++)
    {
        Put(at + i * 4, language[i].value & 0x7FFFFFFF, 4);
        Put(at + languageCount * 4 + i * 2, language[i].key >> 31, 2);
    }
    *sizeOut = size;
    return table;
}

static int AddLikely(const XmlNode* root, DataFile* file, char* errorOut, size_t errorCapacity)
{
    const XmlNode* list = XmlFindChild(root, "likelySubtags");
    size_t capacity = list == nullptr ? 0 : (size_t)list->childCount;
    Entry* general = calloc(capacity + 1, sizeof(Entry));
    Entry* language = calloc(capacity + 1, sizeof(Entry));
    size_t generalCount = 0;
    size_t languageCount = 0;
    int ok = list != nullptr && general != nullptr && language != nullptr;
    uint64_t und = mlocPackTag(mlocPackLanguage("und", 3), 0, 0);
    for (size_t i = 0; ok && i < capacity; i++)
    {
        const XmlNode* entry = list->children[i];
        if (strcmp(entry->name, "likelySubtag") != 0)
        {
            continue;
        }
        const char* from = XmlGetAttribute(entry, "from");
        const char* to = XmlGetAttribute(entry, "to");
        bool script = false;
        bool region = false;
        bool fullScript = false;
        bool fullRegion = false;
        uint64_t key = from == nullptr ? 0 : PackId(from, &script, &region);
        uint64_t value = to == nullptr ? 0 : PackId(to, &fullScript, &fullRegion);
        if (key == 0 || value == 0 || !fullScript || !fullRegion)
        {
            snprintf(errorOut, errorCapacity,
                     "likelySubtags.xml:%d: an entry the table cannot hold", entry->line);
            ok = 0;
        }
        else if (!script && !region && key != und)
        {
            language[languageCount++] = (Entry){key, value};
        }
        else
        {
            general[generalCount++] = (Entry){key, value};
        }
    }
    if (ok)
    {
        qsort(general, generalCount, sizeof(Entry), CompareEntries);
        qsort(language, languageCount, sizeof(Entry), CompareEntries);
        size_t size = 0;
        uint8_t* table = LikelyTable(general, generalCount, language, languageCount, &size);
        DataFileAdd(file, kindLocaleLikely, nullptr, table, size);
        free(table);
    }
    else if (list == nullptr)
    {
        snprintf(errorOut, errorCapacity, "likelySubtags.xml has no likelySubtags");
    }
    free(general);
    free(language);
    return ok;
}

int GenerateLocaleUnits(const char* commonDirectory, DataFile* file, char* errorOut,
                        size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/likelySubtags.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    int ok = AddLikely(root, file, errorOut, errorCapacity);
    XmlFree(root);
    return ok && GenerateAliasUnit(commonDirectory, file, errorOut, errorCapacity) &&
           GenerateExtensionUnit(commonDirectory, file, errorOut, errorCapacity);
}
