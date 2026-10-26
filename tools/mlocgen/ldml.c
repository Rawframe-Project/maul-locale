// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Flattening a CLDR XML file into element chains (ldml.h).

#include "ldml.h"

#include "table_gen.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// No CLDR chain comes near this.
#define MAX_CHAIN 2048

static char* Copy(const char* text)
{
    size_t length = strlen(text) + 1;
    char* copy = malloc(length);
    if (copy == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    return memcpy(copy, text, length);
}

static Draft ParseDraft(const char* text, Draft inherited)
{
    if (text == nullptr)
    {
        return inherited;
    }
    if (strcmp(text, "contributed") == 0)
    {
        return draftContributed;
    }
    if (strcmp(text, "provisional") == 0)
    {
        return draftProvisional;
    }
    if (strcmp(text, "unconfirmed") == 0 || strcmp(text, "true") == 0)
    {
        return draftUnconfirmed;
    }
    return draftApproved;
}

static int CompareAttributes(const void* a, const void* b)
{
    return strcmp((*(const XmlAttribute* const*)a)->name, (*(const XmlAttribute* const*)b)->name);
}

// Appends "/name[@a="x"][@b="y"]" for the node's distinguishing
// attributes, alphabetically; returns 0 when the chain would not fit.
static int AppendStep(char* chain, size_t* length, const XmlNode* node, const Dtd* dtd)
{
    const XmlAttribute* sorted[64];
    int count = 0;
    for (int i = 0; i < node->attributeCount && count < 64; i++)
    {
        if (DtdAttributeKind(dtd, node->name, node->attributes[i].name) == attributeDistinguishing)
        {
            sorted[count++] = &node->attributes[i];
        }
    }
    qsort(sorted, (size_t)count, sizeof(sorted[0]), CompareAttributes);
    size_t at = *length;
    bool fits = GenPrint(chain, MAX_CHAIN, &at, "/%s", node->name);
    for (int i = 0; fits && i < count; i++)
    {
        fits = GenPrint(chain, MAX_CHAIN, &at, "[@%s=\"%s\"]", sorted[i]->name, sorted[i]->value);
    }
    if (!fits)
    {
        return 0;
    }
    *length = at;
    return 1;
}

static Leaf* MakeLeaf(const XmlNode* node, const Dtd* dtd, Draft draft)
{
    size_t capacity = (size_t)node->attributeCount + 1;
    Leaf* leaf = calloc(1, sizeof(Leaf));
    char** names = calloc(capacity, sizeof(char*));
    char** values = calloc(capacity, sizeof(char*));
    if (leaf == nullptr || names == nullptr || values == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    leaf->valueNames = names;
    leaf->values = values;
    for (int i = 0; i < node->attributeCount; i++)
    {
        if (DtdAttributeKind(dtd, node->name, node->attributes[i].name) == attributeValue)
        {
            leaf->valueNames[leaf->valueCount] = Copy(node->attributes[i].name);
            leaf->values[leaf->valueCount++] = Copy(node->attributes[i].value);
        }
    }
    leaf->text = Copy(node->text);
    leaf->draft = draft;
    leaf->line = node->line;
    return leaf;
}

typedef struct Flattener
{
    LdmlFile* file;
    const Dtd* dtd;
    char chain[MAX_CHAIN];
    int failed;
    int failedLine;
} Flattener;

static void Flatten(Flattener* flattener, const XmlNode* node, size_t length, Draft draft)
{
    if (flattener->failed)
    {
        return;
    }
    if (strcmp(node->name, "alias") == 0)
    {
        flattener->chain[length] = '\0';
        const char* path = XmlGetAttribute(node, "path");
        MapPut(&flattener->file->aliases, flattener->chain, Copy(path == nullptr ? "" : path));
        return;
    }
    if (!AppendStep(flattener->chain, &length, node, flattener->dtd))
    {
        flattener->failed = 1;
        flattener->failedLine = node->line;
        return;
    }
    draft = ParseDraft(XmlGetAttribute(node, "draft"), draft);
    if (node->childCount == 0)
    {
        Leaf* old = MapGet(&flattener->file->leaves, flattener->chain);
        if (old != nullptr)
        {
            flattener->failed = 1;
            flattener->failedLine = node->line;
            return;
        }
        MapPut(&flattener->file->leaves, flattener->chain, MakeLeaf(node, flattener->dtd, draft));
        return;
    }
    for (int i = 0; i < node->childCount; i++)
    {
        Flatten(flattener, node->children[i], length, draft);
    }
}

LdmlFile* LdmlLoad(const char* path, const Dtd* dtd, char* errorOut, size_t errorCapacity)
{
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return nullptr;
    }
    Flattener* flattener = calloc(1, sizeof(Flattener));
    LdmlFile* file = calloc(1, sizeof(LdmlFile));
    if (flattener == nullptr || file == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    flattener->file = file;
    flattener->dtd = dtd;
    flattener->chain[0] = '/';
    Flatten(flattener, root, 1, draftApproved);
    int failed = flattener->failed;
    if (failed)
    {
        snprintf(errorOut, errorCapacity, "%s: line %d: a chain is too long or given twice", path,
                 flattener->failedLine);
    }
    free(flattener);
    XmlFree(root);
    if (failed)
    {
        LdmlFree(file);
        return nullptr;
    }
    return file;
}

const char* LeafValue(const Leaf* leaf, const char* name)
{
    for (int i = 0; i < leaf->valueCount; i++)
    {
        if (strcmp(leaf->valueNames[i], name) == 0)
        {
            return leaf->values[i];
        }
    }
    return nullptr;
}

static void FreeLeaf(const char* key, void* value, void* context)
{
    (void)key;
    (void)context;
    Leaf* leaf = value;
    for (int i = 0; i < leaf->valueCount; i++)
    {
        free(leaf->valueNames[i]);
        free(leaf->values[i]);
    }
    free(leaf->valueNames);
    free(leaf->values);
    free(leaf->text);
    free(leaf);
}

static void FreeText(const char* key, void* value, void* context)
{
    (void)key;
    (void)context;
    free(value);
}

void LdmlFree(LdmlFile* file)
{
    if (file == nullptr)
    {
        return;
    }
    MapEach(&file->leaves, FreeLeaf, nullptr);
    MapEach(&file->aliases, FreeText, nullptr);
    MapFree(&file->leaves);
    MapFree(&file->aliases);
    free(file);
}
