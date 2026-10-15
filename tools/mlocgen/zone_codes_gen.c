// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone codes unit (zone_codes_gen.h): the ids and names read, sorted
// as src/zone_code_tables.h lays them out, and written.

#include "zone_codes_gen.h"

#include "kinds.h"
#include "tzdb.h"
#include "xml.h"
#include "zone_code_tables.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxCodes = 1024,
    maxAliases = 4096,
    maxMappings = 1024,
    maxWindowsName = 128,
    maxId = 16,
};

typedef struct Code
{
    char id[maxId];
    // CLDR's identifier for it, or for a deprecated id its replacement's
    // id.
    char name[tzdbMaxName];
    bool deprecated;
    int preferred;
    int windows;
} Code;

typedef struct Alias
{
    char name[tzdbMaxName];
    char id[maxId];
    int code;
} Alias;

typedef struct Mapping
{
    char windows[maxWindowsName];
    char region[4];
    // The identifiers it lists, space-separated; the first is its own.
    char names[1024];
} Mapping;

typedef struct Codes
{
    Code codes[maxCodes];
    Alias aliases[maxAliases];
    Mapping mappings[maxMappings];
    // The first mapping of each Windows name, in the sorted mappings.
    int firsts[maxMappings];
    int codeCount;
    int aliasCount;
    int mappingCount;
    int windowsCount;
} Codes;

static int Fail(char* errorOut, size_t errorCapacity, const char* what, const char* name)
{
    snprintf(errorOut, errorCapacity, "zone codes: %s: %s", what, name);
    return 0;
}

static int FoldedCompare(const char* a, const char* b)
{
    for (;; a++, b++)
    {
        int x = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
        int y = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
        if (x != y || x == 0)
        {
            return x - y;
        }
    }
}

static int CompareCodes(const void* a, const void* b)
{
    return strcmp(((const Code*)a)->id, ((const Code*)b)->id);
}

static int CompareAliases(const void* a, const void* b)
{
    return FoldedCompare(((const Alias*)a)->name, ((const Alias*)b)->name);
}

static int CompareMappings(const void* a, const void* b)
{
    const Mapping* x = a;
    const Mapping* y = b;
    int order = FoldedCompare(x->windows, y->windows);
    return order != 0 ? order : strcmp(x->region, y->region);
}

static int FindCode(const Codes* c, const char* id)
{
    Code key = {0};
    snprintf(key.id, sizeof(key.id), "%s", id);
    const Code* found = bsearch(&key, c->codes, (size_t)c->codeCount, sizeof(Code), CompareCodes);
    return found == nullptr ? -1 : (int)(found - c->codes);
}

static int FindAlias(const Codes* c, const char* name)
{
    Alias key = {0};
    snprintf(key.name, sizeof(key.name), "%s", name);
    const Alias* found =
        bsearch(&key, c->aliases, (size_t)c->aliasCount, sizeof(Alias), CompareAliases);
    return found == nullptr ? -1 : found->code;
}

// One type of the tz key: its code, and its aliases unless deprecated.
static int AddType(Codes* c, const XmlNode* type, char* errorOut, size_t errorCapacity)
{
    const char* id = XmlGetAttribute(type, "name");
    const char* deprecated = XmlGetAttribute(type, "deprecated");
    const char* list = XmlGetAttribute(type, "alias");
    if (id == nullptr || strcmp(id, "unk") == 0)
    {
        return 1;
    }
    if (c->codeCount == maxCodes || strlen(id) >= maxId)
    {
        return Fail(errorOut, errorCapacity, "too many ids or too long", id);
    }
    Code* code = &c->codes[c->codeCount++];
    snprintf(code->id, sizeof(code->id), "%s", id);
    code->windows = zoneCodeNone;
    code->deprecated = deprecated != nullptr && strcmp(deprecated, "true") == 0;
    if (code->deprecated)
    {
        const char* preferred = XmlGetAttribute(type, "preferred");
        snprintf(code->name, sizeof(code->name), "%s", preferred != nullptr ? preferred : "");
        return 1;
    }
    if (list == nullptr)
    {
        return Fail(errorOut, errorCapacity, "an id without aliases", id);
    }
    // CLDR's identifier for the id: the IANA name, else the first alias.
    const char* iana = XmlGetAttribute(type, "iana");
    const char* from = iana != nullptr ? iana : list;
    snprintf(code->name, sizeof(code->name), "%.*s", (int)strcspn(from, " "), from);
    for (const char* at = list; *at != '\0';)
    {
        size_t length = strcspn(at, " ");
        if (c->aliasCount == maxAliases || length >= tzdbMaxName)
        {
            return Fail(errorOut, errorCapacity, "too many aliases or too long", id);
        }
        Alias* alias = &c->aliases[c->aliasCount++];
        memcpy(alias->name, at, length);
        alias->name[length] = '\0';
        snprintf(alias->id, sizeof(alias->id), "%s", id);
        at += length + (at[length] == ' ');
    }
    return 1;
}

// The ids, sorted, each deprecated one's replacement and each alias's id
// resolved.
static int ReadCodes(Codes* c, const char* commonDirectory, char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/bcp47/timezone.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    const XmlNode* keyword = root == nullptr ? nullptr : XmlFindChild(root, "keyword");
    const XmlNode* key = nullptr;
    for (int i = 0; keyword != nullptr && i < keyword->childCount && key == nullptr; i++)
    {
        const char* name = XmlGetAttribute(keyword->children[i], "name");
        key = name != nullptr && strcmp(name, "tz") == 0 ? keyword->children[i] : nullptr;
    }
    int ok = key != nullptr || Fail(errorOut, errorCapacity, "no tz key", path);
    for (int i = 0; ok && i < key->childCount; i++)
    {
        ok = AddType(c, key->children[i], errorOut, errorCapacity);
    }
    XmlFree(root);
    qsort(c->codes, (size_t)c->codeCount, sizeof(Code), CompareCodes);
    qsort(c->aliases, (size_t)c->aliasCount, sizeof(Alias), CompareAliases);
    for (int i = 0; ok && i < c->codeCount; i++)
    {
        Code* code = &c->codes[i];
        code->preferred = code->deprecated ? FindCode(c, code->name) : i;
        if (code->preferred < 0 || c->codes[code->preferred].deprecated)
        {
            ok = Fail(errorOut, errorCapacity, "a replacement missing or deprecated", code->id);
        }
    }
    for (int i = 0; ok && i < c->aliasCount; i++)
    {
        c->aliases[i].code = FindCode(c, c->aliases[i].id);
        if (i > 0 && CompareAliases(&c->aliases[i - 1], &c->aliases[i]) == 0)
        {
            ok = Fail(errorOut, errorCapacity, "a name two ids list", c->aliases[i].name);
        }
    }
    return ok;
}

static int AddMapping(Codes* c, const XmlNode* node, char* errorOut, size_t errorCapacity)
{
    const char* windows = XmlGetAttribute(node, "other");
    const char* region = XmlGetAttribute(node, "territory");
    const char* names = XmlGetAttribute(node, "type");
    if (windows == nullptr || region == nullptr || names == nullptr ||
        c->mappingCount == maxMappings || strlen(windows) >= maxWindowsName || strlen(region) > 3 ||
        strlen(names) >= sizeof(c->mappings[0].names) || names[0] == '\0')
    {
        return Fail(errorOut, errorCapacity, "a mapZone out of bounds",
                    windows != nullptr ? windows : "?");
    }
    Mapping* m = &c->mappings[c->mappingCount++];
    snprintf(m->windows, sizeof(m->windows), "%s", windows);
    snprintf(m->region, sizeof(m->region), "%s", region);
    snprintf(m->names, sizeof(m->names), "%s", names);
    return 1;
}

// Each identifier a mapping lists gives its id the mapping's Windows name.
static int AssignWindows(Codes* c, const Mapping* m, int windows, char* errorOut,
                         size_t errorCapacity)
{
    for (const char* at = m->names; *at != '\0';)
    {
        char name[tzdbMaxName];
        size_t length = strcspn(at, " ");
        snprintf(name, sizeof(name), "%.*s", (int)length, at);
        at += length + (at[length] == ' ');
        int code = length < tzdbMaxName ? FindAlias(c, name) : -1;
        if (code < 0)
        {
            return Fail(errorOut, errorCapacity, "a mapped identifier no id lists", name);
        }
        if (c->codes[code].windows != zoneCodeNone && c->codes[code].windows != windows)
        {
            return Fail(errorOut, errorCapacity, "an id mapped to two Windows names", name);
        }
        c->codes[code].windows = windows;
    }
    return 1;
}

// The mappings, sorted; the Windows names' firsts; every listed
// identifier's id given its name.
static int ReadMappings(Codes* c, const char* commonDirectory, char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/windowsZones.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    const XmlNode* zones = root == nullptr ? nullptr : XmlFindChild(root, "windowsZones");
    const XmlNode* map = zones == nullptr ? nullptr : XmlFindChild(zones, "mapTimezones");
    int ok = map != nullptr || Fail(errorOut, errorCapacity, "no mapTimezones", path);
    for (int i = 0; ok && i < map->childCount; i++)
    {
        ok = strcmp(map->children[i]->name, "mapZone") != 0 ||
             AddMapping(c, map->children[i], errorOut, errorCapacity);
    }
    XmlFree(root);
    qsort(c->mappings, (size_t)c->mappingCount, sizeof(Mapping), CompareMappings);
    for (int i = 0; ok && i < c->mappingCount; i++)
    {
        const Mapping* m = &c->mappings[i];
        bool first = i == 0 || FoldedCompare(c->mappings[i - 1].windows, m->windows) != 0;
        if (first)
        {
            c->firsts[c->windowsCount++] = i;
        }
        if (i > 0 && CompareMappings(&c->mappings[i - 1], m) == 0)
        {
            ok = Fail(errorOut, errorCapacity, "a region mapped twice", m->windows);
        }
        else if (first && strcmp(m->region, "001") != 0)
        {
            ok = Fail(errorOut, errorCapacity, "a Windows name without a 001 mapping", m->windows);
        }
        ok = ok && AssignWindows(c, m, c->windowsCount - 1, errorOut, errorCapacity);
    }
    return ok;
}

// A growing run of little-endian bytes.
typedef struct Out
{
    uint8_t* data;
    size_t size;
    size_t capacity;
} Out;

static void Put(Out* o, uint64_t value, int count)
{
    if (o->size + (size_t)count > o->capacity)
    {
        o->capacity = o->capacity == 0 ? 4096 : o->capacity * 2;
        uint8_t* grown = realloc(o->data, o->capacity);
        if (grown == nullptr)
        {
            fputs("mlocgen: cannot build the zone codes unit\n", stderr);
            exit(1);
        }
        o->data = grown;
    }
    for (int i = 0; i < count; i++)
    {
        o->data[o->size++] = (uint8_t)(value >> (8 * i));
    }
}

// A name's offset in the text: where the text already holds its bytes,
// else appended.
static uint32_t Text(Out* text, const char* name, size_t length)
{
    for (size_t from = 0; from + length <= text->size; from++)
    {
        if (memcmp(text->data + from, name, length) == 0)
        {
            return (uint32_t)from;
        }
    }
    uint32_t at = (uint32_t)text->size;
    for (size_t i = 0; i < length; i++)
    {
        Put(text, (uint8_t)name[i], 1);
    }
    return at;
}

static void PutCodes(const Codes* c, Out* sections, Out* text)
{
    for (int i = 0; i < c->codeCount; i++)
    {
        const Code* code = &c->codes[i];
        size_t nameLength = code->deprecated ? 0 : strlen(code->name);
        Put(&sections[0], Text(text, code->id, strlen(code->id)), 4);
        Put(&sections[0], nameLength == 0 ? 0 : Text(text, code->name, nameLength), 4);
        Put(&sections[0], strlen(code->id), 1);
        Put(&sections[0], nameLength, 1);
        Put(&sections[0], (uint64_t)code->preferred, 2);
        Put(&sections[0], (uint64_t)code->windows, 2);
        Put(&sections[0], 0, 2);
    }
    for (int i = 0; i < c->aliasCount; i++)
    {
        const Alias* alias = &c->aliases[i];
        Put(&sections[1], Text(text, alias->name, strlen(alias->name)), 4);
        Put(&sections[1], strlen(alias->name), 1);
        Put(&sections[1], 0, 1);
        Put(&sections[1], (uint64_t)alias->code, 2);
    }
}

static void PutWindows(const Codes* c, Out* sections, Out* text)
{
    for (int w = 0; w < c->windowsCount; w++)
    {
        const char* name = c->mappings[c->firsts[w]].windows;
        Put(&sections[2], Text(text, name, strlen(name)), 4);
        Put(&sections[2], strlen(name), 1);
        Put(&sections[2], 0, 1);
        Put(&sections[2], (uint64_t)c->firsts[w], 2);
    }
    for (int i = 0; i < c->mappingCount; i++)
    {
        const Mapping* m = &c->mappings[i];
        size_t length = strcspn(m->names, " ");
        Put(&sections[3], Text(text, m->names, length), 4);
        Put(&sections[3], length, 1);
        for (int k = 0; k < 3; k++)
        {
            Put(&sections[3], (uint8_t)m->region[k], 1);
        }
    }
}

int GenerateZoneCodes(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity)
{
    Codes* c = calloc(1, sizeof(Codes));
    if (c == nullptr || !ReadCodes(c, commonDirectory, errorOut, errorCapacity) ||
        !ReadMappings(c, commonDirectory, errorOut, errorCapacity))
    {
        free(c);
        return 0;
    }
    Out sections[zoneCodeSectionCount] = {0};
    PutCodes(c, sections, &sections[4]);
    PutWindows(c, sections, &sections[4]);
    const uint32_t counts[zoneCodeSectionCount] = {
        (uint32_t)c->codeCount, (uint32_t)c->aliasCount, (uint32_t)c->windowsCount,
        (uint32_t)c->mappingCount, (uint32_t)sections[4].size};
    Out unit = {0};
    for (int i = 0; i < zoneCodeSectionCount; i++)
    {
        Put(&unit, counts[i], 4);
    }
    Put(&unit, 0, 4);
    for (int i = 0; i < zoneCodeSectionCount; i++)
    {
        while (unit.size % 8 != 0)
        {
            Put(&unit, 0, 1);
        }
        for (size_t k = 0; k < sections[i].size; k++)
        {
            Put(&unit, sections[i].data[k], 1);
        }
        free(sections[i].data);
    }
    DataFileAdd(file, kindZoneCodes, nullptr, unit.data, unit.size);
    free(unit.data);
    free(c);
    return 1;
}
