// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Parent locales by key (parents_gen.h). The components of
// supplementalData.xml's parentLocales map to the library's: none is the
// main one, then collations, plurals, segmentations and
// grammaticalFeatures, each standing alone (UTS #35).

#include "parents_gen.h"

#include "cldr.h"
#include "keys.h"
#include "kinds.h"
#include "locale_tables.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_KEY 64

typedef struct Parent
{
    int component;
    char child[MAX_KEY];
    char parent[MAX_KEY];
} Parent;

typedef struct Parents
{
    Parent* items;
    size_t count;
    size_t capacity;
} Parents;

[[noreturn]] static void Die(const char* message, const char* detail)
{
    fprintf(stderr, "mlocgen: %s: %s\n", message, detail);
    exit(1);
}

// The library's components a parentLocales element's attribute names;
// a bit for each.
static unsigned Components(const char* attribute)
{
    if (attribute == nullptr)
    {
        return 1u << 0;
    }
    static const char* const s_names[] = {"", "collations", "plurals", "segmentations",
                                          "grammaticalFeatures"};
    unsigned bits = 0;
    char list[256];
    snprintf(list, sizeof(list), "%s", attribute);
    for (char* name = list; *name != '\0';)
    {
        size_t length = strcspn(name, " ");
        for (int c = 1; c < 5; c++)
        {
            if (strlen(s_names[c]) == length && strncmp(name, s_names[c], length) == 0)
            {
                bits |= 1u << c;
            }
        }
        name += length + (name[length] == ' ');
    }
    return bits;
}

static void Add(Parents* parents, int component, const char* child, const char* parent)
{
    if (parents->count == parents->capacity)
    {
        parents->capacity = parents->capacity == 0 ? 256 : parents->capacity * 2;
        parents->items = realloc(parents->items, parents->capacity * sizeof(Parent));
        if (parents->items == nullptr)
        {
            Die("out of memory", "parents");
        }
    }
    Parent* p = &parents->items[parents->count++];
    p->component = component;
    snprintf(p->child, sizeof(p->child), "%s", child);
    snprintf(p->parent, sizeof(p->parent), "%s", parent);
}

static int CompareParents(const void* a, const void* b)
{
    const Parent* x = a;
    const Parent* y = b;
    return x->component != y->component ? x->component - y->component : strcmp(x->child, y->child);
}

static void AddList(Parents* parents, const Keys* keys, const XmlNode* list)
{
    unsigned components = Components(XmlGetAttribute(list, "component"));
    for (int i = 0; i < list->childCount; i++)
    {
        const char* parent = XmlGetAttribute(list->children[i], "parent");
        const char* locales = XmlGetAttribute(list->children[i], "locales");
        if (parent == nullptr || locales == nullptr)
        {
            continue;
        }
        char parentKey[MAX_KEY];
        KeyOf(keys, parent, parentKey, sizeof(parentKey));
        char copy[8192];
        snprintf(copy, sizeof(copy), "%s", locales);
        for (char* child = copy; *child != '\0';)
        {
            size_t length = strcspn(child, " ");
            char id[MAX_KEY];
            snprintf(id, sizeof(id), "%.*s", (int)length, child);
            char childKey[MAX_KEY];
            KeyOf(keys, id, childKey, sizeof(childKey));
            for (int c = 0; c < 5; c++)
            {
                if (components & 1u << c)
                {
                    Add(parents, c, childKey, parentKey);
                }
            }
            child += length;
            child += strspn(child, " ");
        }
    }
}

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

static uint8_t* Write(const Parents* parents, size_t* sizeOut)
{
    size_t text = 0;
    for (size_t i = 0; i < parents->count; i++)
    {
        text += strlen(parents->items[i].child) + strlen(parents->items[i].parent);
    }
    size_t textAt = parentHeaderSize + parents->count * parentEntrySize;
    uint8_t* out = calloc(textAt + text, 1);
    if (out == nullptr)
    {
        Die("out of memory", "parents");
    }
    Put(out, parents->count, 4);
    Put(out + 4, text, 4);
    size_t at = 0;
    for (size_t i = 0; i < parents->count; i++)
    {
        const Parent* p = &parents->items[i];
        uint8_t* entry = out + parentHeaderSize + i * parentEntrySize;
        size_t childLength = strlen(p->child);
        size_t parentLength = strlen(p->parent);
        entry[0] = (uint8_t)p->component;
        entry[1] = (uint8_t)childLength;
        entry[2] = (uint8_t)parentLength;
        Put(entry + 4, at, 4);
        Put(entry + 8, at + childLength, 4);
        memcpy(out + textAt + at, p->child, childLength);
        memcpy(out + textAt + at + childLength, p->parent, parentLength);
        at += childLength + parentLength;
    }
    *sizeOut = textAt + text;
    return out;
}

int GenerateParentUnit(const char* commonDirectory, DataFile* file, char* errorOut,
                       size_t errorCapacity)
{
    Keys keys;
    if (!KeysOpen(&keys, commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/supplementalData.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        KeysClose(&keys);
        return 0;
    }
    Parents parents = {0};
    for (int i = 0; i < root->childCount; i++)
    {
        if (strcmp(root->children[i]->name, "parentLocales") == 0)
        {
            AddList(&parents, &keys, root->children[i]);
        }
    }
    XmlFree(root);
    KeysClose(&keys);
    qsort(parents.items, parents.count, sizeof(Parent), CompareParents);
    // Ids that share a key (yue and yue_Hant) may list the same parent;
    // it is kept once.
    size_t kept = 0;
    for (size_t i = 0; i < parents.count; i++)
    {
        if (kept > 0 && CompareParents(&parents.items[kept - 1], &parents.items[i]) == 0)
        {
            if (strcmp(parents.items[kept - 1].parent, parents.items[i].parent) != 0)
            {
                Die("a locale key with two parents", parents.items[i].child);
            }
            continue;
        }
        parents.items[kept++] = parents.items[i];
    }
    parents.count = kept;
    size_t size = 0;
    uint8_t* unit = Write(&parents, &size);
    DataFileAdd(file, kindLocaleParents, nullptr, unit, size);
    free(unit);
    free(parents.items);
    return 1;
}

int PrintChains(const char* commonDirectory, char* errorOut, size_t errorCapacity)
{
    Keys keys;
    Cldr cldr;
    if (!KeysOpen(&keys, commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
    if (!CldrOpen(&cldr, commonDirectory, errorOut, errorCapacity))
    {
        KeysClose(&keys);
        return 0;
    }
    char line[256];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        char* tab = strchr(line, '\t');
        int c = tab == nullptr ? -1 : strncmp(line, "main", 4) == 0 ? 0 : 1;
        if (tab == nullptr || (c == 1 && strncmp(line, "collation", 9) != 0))
        {
            snprintf(errorOut, errorCapacity, "a line is not \"main|collation<TAB>id\": %s", line);
            CldrClose(&cldr);
            KeysClose(&keys);
            return 0;
        }
        const char* id = tab + 1;
        fputs(line, stdout);
        putchar('\t');
        char last[MAX_KEY];
        KeyOf(&keys, id, last, sizeof(last));
        const char* separator = "";
        for (const char* at = CldrParent(&cldr, id, (Component)c); at != nullptr;
             at = CldrParent(&cldr, at, (Component)c))
        {
            char key[MAX_KEY];
            KeyOf(&keys, at, key, sizeof(key));
            if (strcmp(key, last) != 0)
            {
                printf("%s%s", separator, key);
                separator = " ";
                snprintf(last, sizeof(last), "%s", key);
            }
        }
        putchar('\n');
    }
    CldrClose(&cldr);
    KeysClose(&keys);
    return 1;
}
