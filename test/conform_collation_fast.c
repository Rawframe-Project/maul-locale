// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin tables in a data file (record mloc-0072), as ICU4C 79
// builds them for the same tags: "und" for the root's, then each
// locale's types as "locale-u-co-type" and the default as the locale
// alone, each line the tag, the count of words and the words in hex,
// the root's words for a type that shares them. Diffed against
// tools/oracles' ICU4C program reading the same tags.
//
// usage: conform_collation_fast <collation.mloc>

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mlocData s_source;
static CollationRoot s_root;

static void Print(const char* tag, const CollationRoot* unit)
{
    const CollationRoot* table = unit->fastLatinCount == fastLatinRootTable ? &s_root : unit;
    printf("%s %u", tag, table->fastLatinCount);
    for (uint32_t i = 0; i < table->fastLatinCount && table->fastLatin != nullptr; i++)
    {
        printf(" %x", mlocFastLatinWord(table->fastLatin, i));
    }
    printf("\n");
}

// A locale's tag: root as und, underscores as hyphens.
static void TagOf(const char* id, size_t length, char* out, size_t capacity)
{
    bool root = length == 4 && memcmp(id, "root", 4) == 0;
    snprintf(out, capacity, "%.*s", root ? 3 : (int)length, root ? "und" : id);
    for (char* p = out; *p != '\0'; p++)
    {
        *p = *p == '_' ? '-' : *p;
    }
}

static void Types(const DataView* view, uint32_t locale, const char* base)
{
    DataSpan unit = {0};
    if (!mlocFindDataUnit(view, kindCollationTailorings, locale, &unit))
    {
        return;
    }
    CollationTypes types = mlocReadCollationTypes(unit);
    for (uint32_t i = 0; i < types.count; i++)
    {
        const uint8_t* entry = types.bytes + collationTypesHead + (size_t)i * collationTypeSize;
        DataSpan body = {types.bytes + mlocLoad32(entry + 8), mlocLoad32(entry + 12)};
        CollationTailoring t;
        char tag[160];
        bool isDefault =
            types.defaultLength == mlocLoad32(entry + 4) &&
            memcmp(types.defaultName, types.names + mlocLoad32(entry), types.defaultLength) == 0;
        snprintf(tag, sizeof(tag), "%s-u-co-%.*s", base, (int)mlocLoad32(entry + 4),
                 (const char*)types.names + mlocLoad32(entry));
        if (mlocReadCollationTailoring(body, &t))
        {
            Print(isDefault ? base : tag, &t.view);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <collation.mloc>\n", argv[0]);
        return 2;
    }
    FILE* f = fopen(argv[1], "rb");
    long size = f != nullptr && fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
    uint8_t* bytes = size > 0 && fseek(f, 0, SEEK_SET) == 0 ? malloc((size_t)size) : nullptr;
    bool ok = bytes != nullptr && fread(bytes, 1, (size_t)size, f) == (size_t)size &&
              mlocOpenData(&s_source, bytes, (size_t)size) == mloc_success;
    if (f != nullptr)
    {
        fclose(f);
    }
    DataView view = mlocViewOf(&s_source);
    DataSpan span = {0};
    ok = ok && mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) &&
         mlocReadCollationRoot(span, &s_root);
    if (!ok)
    {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        free(bytes);
        return 1;
    }
    Print("und", &s_root);
    for (size_t i = 0; i < mlocGetDataLocaleCount(&s_source); i++)
    {
        const char* id = nullptr;
        size_t length = 0;
        uint32_t locale = 0;
        char base[64];
        if (mlocGetDataLocale(&s_source, i, &id, &length) == mloc_success &&
            mlocFindDataLocale(&view, id, length, &locale))
        {
            TagOf(id, length, base, sizeof(base));
            Types(&view, locale, base);
        }
    }
    free(bytes);
    return 0;
}
