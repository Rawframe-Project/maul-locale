// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The context transforms (context_gen.h), resolved as CLDR resolves them
// (ICU reads the same values through its resource fallback):
// "titlecase-firstword" sets a context, "no-change" or nothing leaves it.
// A locale whose transforms match those of the nearest locale with
// another key along its chain files nothing; one under its parent's key
// is checked to match.

#include "context_gen.h"

#include "kinds.h"
#include "locale_context.h"
#include "table_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxKey = 64,
    maxChain = 192,
};

// CLDR's usage of each of the unit's bytes.
static const char* const s_usages[contextUsageCount] = {
    "number-spellout", "languages", "script", "territory", "variant", "key", "keyValue"};

typedef struct Contexts
{
    uint8_t usage[contextUsageCount];
} Contexts;

static void Resolve(Cldr* cldr, const char* locale, Contexts* out)
{
    static const struct
    {
        const char* type;
        uint8_t bit;
    } s_contexts[] = {{"uiListOrMenu", contextListOrMenu}, {"stand-alone", contextStandAlone}};
    for (int u = 0; u < contextUsageCount; u++)
    {
        out->usage[u] = 0;
        for (size_t c = 0; c < sizeof(s_contexts) / sizeof(s_contexts[0]); c++)
        {
            char chain[maxChain];
            snprintf(chain, sizeof(chain),
                     "//ldml/contextTransforms/contextTransformUsage[@type=\"%s\"]/"
                     "contextTransform[@type=\"%s\"]",
                     s_usages[u], s_contexts[c].type);
            const Leaf* leaf = CldrResolve(cldr, locale, chain, componentMain);
            if (leaf == nullptr || strcmp(leaf->text, "no-change") == 0)
            {
                continue;
            }
            if (strcmp(leaf->text, "titlecase-firstword") != 0)
            {
                GenDie("a context transform the generator does not read", chain);
            }
            out->usage[u] |= s_contexts[c].bit;
        }
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

void GenerateContextUnits(const ListInput* in, DataFile* file)
{
    Contexts* all = GenAllocate(sizeof(Contexts) * (size_t)in->count);
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
            if (memcmp(&all[IndexOf(in, parent)], &all[i], sizeof(Contexts)) != 0)
            {
                GenDie("a locale's context transforms differ from its parent key's", in->ids[i]);
            }
            continue;
        }
        const char* next = NextKeyed(in, in->ids[i], key);
        if (next == nullptr || memcmp(&all[IndexOf(in, next)], &all[i], sizeof(Contexts)) != 0)
        {
            DataFileAdd(file, kindLocaleContext, key, all[i].usage, sizeof(all[i].usage));
        }
    }
    free(all);
}
