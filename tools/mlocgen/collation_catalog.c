// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation files' rule strings (collation_catalog.h).

#include "collation_catalog.h"

#include "table_gen.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// BCP 47's co values whose CLDR type names differ (bcp47/collation.xml's
// aliases).
static const char* const s_codes[][2] = {{"phonebk", "phonebook"},
                                         {"trad", "traditional"},
                                         {"dict", "dictionary"},
                                         {"gb2312", "gb2312han"}};

enum
{
    codeCount = sizeof(s_codes) / sizeof(s_codes[0]),
};

static void* Grow(void* items, size_t* capacity, size_t needed, size_t size)
{
    if (needed <= *capacity)
    {
        return items;
    }
    size_t grown = *capacity * 2 + 16;
    void* more = realloc(items, grown * size);
    if (more == nullptr)
    {
        return nullptr;
    }
    memset((char*)more + *capacity * size, 0, (grown - *capacity) * size);
    *capacity = grown;
    return more;
}

// The file's name from its path, without ".xml"; false for another.
static bool NameOf(const char* path, char* out, size_t capacity)
{
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    const char* name = backslash != nullptr && (slash == nullptr || backslash > slash)
                           ? backslash + 1
                       : slash != nullptr ? slash + 1
                                          : path;
    size_t length = strlen(name);
    if (length <= 4 || length - 4 >= capacity || strcmp(name + length - 4, ".xml") != 0)
    {
        return false;
    }
    memcpy(out, name, length - 4);
    out[length - 4] = '\0';
    return true;
}

// Adds a collation element's type; null, or what is wrong.
static const char* AddType(CollationCatalog* c, const char* file, const XmlNode* collation,
                           const XmlNode* cr)
{
    const char* type = XmlGetAttribute(collation, "type");
    const char* alt = XmlGetAttribute(collation, "alt");
    type = type == nullptr ? "standard" : type;
    alt = alt == nullptr ? "" : alt;
    CollationType probe;
    if (!GenCopy(probe.type, sizeof(probe.type), type, strlen(type)) ||
        !GenCopy(probe.alt, sizeof(probe.alt), alt, strlen(alt)))
    {
        return "a collation type or alt longer than the generator holds";
    }
    CollationType* types = Grow(c->types, &c->capacity, c->count + 1, sizeof(CollationType));
    size_t size = strlen(cr->text) + 1;
    char* text = types == nullptr ? nullptr : malloc(size);
    if (text == nullptr)
    {
        c->types = types != nullptr ? types : c->types;
        return "out of memory";
    }
    c->types = types;
    CollationType* t = &c->types[c->count];
    t->order = c->count++;
    // The file's name is NameOf's, which holds as many bytes as t->file.
    snprintf(t->file, sizeof(t->file), "%s", file);
    memcpy(t->type, probe.type, sizeof(t->type));
    memcpy(t->alt, probe.alt, sizeof(t->alt));
    memcpy(text, cr->text, size);
    t->text = text;
    t->parsed = CollationRulesParse(text, size - 1, &t->rules, t->error, sizeof(t->error));
    return nullptr;
}

bool CollationCatalogRead(CollationCatalog* catalog, const char* path, char* error,
                          size_t errorSize)
{
    char name[64];
    if (!NameOf(path, name, sizeof(name)))
    {
        snprintf(error, errorSize, "%s: not an XML file", path);
        return false;
    }
    XmlNode* root = XmlParseFile(path, error, errorSize);
    if (root == nullptr)
    {
        return false;
    }
    CollationFile* files =
        Grow(catalog->files, &catalog->fileCapacity, catalog->fileCount + 1, sizeof(CollationFile));
    const char* problem = files == nullptr ? "out of memory" : nullptr;
    catalog->files = files != nullptr ? files : catalog->files;
    CollationFile* file = files != nullptr ? &catalog->files[catalog->fileCount++] : nullptr;
    if (file != nullptr)
    {
        // name holds as many bytes as file->name.
        snprintf(file->name, sizeof(file->name), "%s", name);
    }
    const XmlNode* collations = XmlFindChild(root, "collations");
    for (int i = 0; problem == nullptr && collations != nullptr && i < collations->childCount; i++)
    {
        const XmlNode* child = collations->children[i];
        if (strcmp(child->name, "defaultCollation") == 0)
        {
            if (!GenCopy(file->defaultType, sizeof(file->defaultType), child->text,
                         strlen(child->text)))
            {
                problem = "a default collation longer than the generator holds";
            }
            continue;
        }
        const XmlNode* cr =
            strcmp(child->name, "collation") == 0 ? XmlFindChild(child, "cr") : nullptr;
        problem = cr == nullptr ? nullptr : AddType(catalog, name, child, cr);
    }
    bool ok = problem == nullptr;
    if (!ok)
    {
        snprintf(error, errorSize, "%s: %s", path, problem);
    }
    XmlFree(root);
    return ok;
}

static int ByFile(const void* a, const void* b)
{
    const CollationType* x = a;
    const CollationType* y = b;
    int c = strcmp(x->file, y->file);
    return c != 0 ? c : x->order < y->order ? -1 : x->order > y->order;
}

void CollationCatalogSort(CollationCatalog* catalog)
{
    if (catalog->count > 0)
    {
        qsort(catalog->types, catalog->count, sizeof(CollationType), ByFile);
    }
}

const char* CollationTypeCode(const char* type)
{
    for (size_t i = 0; i < codeCount; i++)
    {
        if (strcmp(type, s_codes[i][1]) == 0)
        {
            return s_codes[i][0];
        }
    }
    return type;
}

const CollationRules* CollationCatalogImport(void* context, const char* tag)
{
    const CollationCatalog* c = context;
    char language[64];
    char type[64] = "standard";
    const char* u = strstr(tag, "-u-co-");
    // A tag too long to hold names no file's type, rather than the one its
    // first bytes spell.
    if (!GenCopy(language, sizeof(language), tag, u == nullptr ? strlen(tag) : (size_t)(u - tag)) ||
        (u != nullptr && !GenCopy(type, sizeof(type), u + 6, strlen(u + 6))))
    {
        return nullptr;
    }
    if (u != nullptr)
    {
        for (size_t i = 0; i < codeCount; i++)
        {
            if (strcmp(type, s_codes[i][0]) == 0)
            {
                snprintf(type, sizeof(type), "%s", s_codes[i][1]);
            }
        }
    }
    if (strcmp(language, "und") == 0)
    {
        snprintf(language, sizeof(language), "root");
    }
    for (char* p = language; *p != '\0'; p++)
    {
        *p = *p == '-' ? '_' : *p;
    }
    for (size_t i = 0; i < c->count; i++)
    {
        const CollationType* t = &c->types[i];
        if (t->parsed && t->alt[0] == '\0' && strcmp(t->file, language) == 0 &&
            strcmp(t->type, type) == 0)
        {
            return &t->rules;
        }
    }
    return nullptr;
}

void CollationCatalogFree(CollationCatalog* catalog)
{
    for (size_t i = 0; i < catalog->count; i++)
    {
        free(catalog->types[i].text);
        if (catalog->types[i].parsed)
        {
            CollationRulesFree(&catalog->types[i].rules);
        }
    }
    free(catalog->types);
    free(catalog->files);
    *catalog = (CollationCatalog){0};
}
