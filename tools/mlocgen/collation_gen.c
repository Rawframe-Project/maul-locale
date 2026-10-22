// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_gen.h"

#include "collation_builder.h"
#include "collation_catalog.h"
#include "collation_fast_table.h"
#include "collation_root.h"
#include "collation_tailoring.h"
#include "collation_unit.h"
#include "collation_unsafe_table.h"
#include "index_gen.h"
#include "keys.h"
#include "kinds.h"
#include "table_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Whether the file of a type also gives it as alt="short".
static bool HasShort(const CollationCatalog* catalog, const CollationType* t)
{
    for (size_t i = 0; i < catalog->count; i++)
    {
        const CollationType* other = &catalog->types[i];
        if (strcmp(other->file, t->file) == 0 && strcmp(other->type, t->type) == 0 &&
            strcmp(other->alt, "short") == 0)
        {
            return true;
        }
    }
    return false;
}

// Whether a type is one a locale's co may name: not one kept for imports
// (CLDR's "private-"), and its alt="short" form where the file has one,
// as ICU4C's data takes it, else the type without an alt.
static bool Selectable(const CollationCatalog* catalog, const CollationType* t)
{
    return strncmp(t->type, "private-", 8) != 0 &&
           (strcmp(t->alt, "short") == 0 || (t->alt[0] == '\0' && !HasShort(catalog, t)));
}

// The two roots a type is built over: the root, and the unihan root
// (Han in radical-stroke order) for the unihan types; the root's unit
// read back and its fast Latin table, which a type's may be.
typedef struct Roots
{
    const RootTable* root;
    const RootTable* unihan;
    CollationRoot view;
    const uint16_t* fastLatin;
    size_t fastLatinCount;
} Roots;

// One type built over the root and written; the program stops on a
// refusal, as CLDR's rules all build.
static CollationTypeBody Build(const CollationType* t, const RootTable* root, const Roots* roots,
                               CollationCatalog* catalog)
{
    char error[256] = "";
    if (!t->parsed)
    {
        fprintf(stderr, "%s %s: %s\n", t->file, t->type, t->error);
        GenDie("a collation type does not parse", t->file);
    }
    CollationBuilder b;
    bool ok =
        CollationBuilderInit(&b, root, CollationCatalogImport, catalog, error, sizeof(error)) &&
        CollationBuilderAdd(&b, &t->rules) && CollationBuilderFinish(&b);
    CollationTypeBody built = {.code = CollationTypeCode(t->type)};
    built.body =
        ok ? CollationTailoringUnit(&b.mappings, &b.settings, &built.size, error, sizeof(error))
           : nullptr;
    if (built.body != nullptr)
    {
        built.body = CollationFastLatinSection(built.body, &built.size, &b.mappings, &roots->view,
                                               roots->fastLatin, roots->fastLatinCount, t->type);
    }
    CollationBuilderFree(&b);
    if (built.body == nullptr)
    {
        fprintf(stderr, "%s %s: %s\n", t->file, t->type, error);
        GenDie("a collation type does not build", t->file);
    }
    CollationTailoring read;
    if (!mlocReadCollationTailoring((DataSpan){built.body, (uint32_t)built.size}, &read))
    {
        GenDie("a collation type does not read back", t->file);
    }
    built.body = CollationUnsafeAppend(built.body, &built.size, &read.view);
    if (!mlocReadCollationTailoring((DataSpan){built.body, (uint32_t)built.size}, &read))
    {
        GenDie("a collation type does not read back with its unsafe ranges", t->file);
    }
    return built;
}

// Each file's selectable types as a unit of its locale's key.
static void AddTailorings(CollationCatalog* catalog, const Roots* roots, const Keys* keys,
                          DataFile* file)
{
    CollationTypeBody* built = GenAllocate(sizeof(CollationTypeBody) * (catalog->count + 1));
    size_t next = 0;
    for (size_t f = 0; f < catalog->fileCount; f++)
    {
        const CollationFile* source = &catalog->files[f];
        size_t count = 0;
        for (; next < catalog->count && strcmp(catalog->types[next].file, source->name) == 0;
             next++)
        {
            const CollationType* t = &catalog->types[next];
            if (Selectable(catalog, t))
            {
                bool unihan = strcmp(t->type, "unihan") == 0;
                built[count++] = Build(t, unihan ? roots->unihan : roots->root, roots, catalog);
            }
        }
        if (count > 0 || source->defaultType[0] != '\0')
        {
            char key[64];
            KeyOf(keys, source->name, key, sizeof(key));
            size_t size = 0;
            uint8_t* unit =
                CollationTypesUnit(built, count, CollationTypeCode(source->defaultType), &size);
            DataFileAdd(file, kindCollationTailorings, key, unit, size);
            free(unit);
        }
        for (size_t i = 0; i < count; i++)
        {
            free(built[i].body);
        }
    }
    free(built);
}

static void ReadCatalog(const char* commonDirectory, CollationCatalog* catalog)
{
    int count = 0;
    char** names = GenListXml(commonDirectory, "collation", &count);
    for (int i = 0; i < count; i++)
    {
        char path[4096];
        char error[256] = "";
        snprintf(path, sizeof(path), "%s/collation/%s.xml", commonDirectory, names[i]);
        if (!CollationCatalogRead(catalog, path, error, sizeof(error)))
        {
            GenDie("cannot read", error);
        }
    }
    GenFreeList(names, count);
    CollationCatalogSort(catalog);
}

// The index labels, from common/main's exemplars.
static int AddIndexLabels(const char* commonDirectory, const Keys* keys, DataFile* file,
                          char* errorOut, size_t errorCapacity)
{
    static Cldr s_cldr;
    if (!CldrOpen(&s_cldr, commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
    int count = 0;
    char** ids = GenListXml(commonDirectory, "main", &count);
    GenerateIndexUnits(&(IndexInput){&s_cldr, keys, (const char* const*)ids, count}, file);
    GenFreeList(ids, count);
    CldrClose(&s_cldr);
    return 1;
}

int GenerateCollation(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/uca/FractionalUCA.txt", commonDirectory);
    RootTable table;
    RootTable unihan;
    if (!RootRead(path, &table, errorOut, errorCapacity))
    {
        return 0;
    }
    if (!RootReadUnihan(path, &unihan, errorOut, errorCapacity))
    {
        RootFree(&table);
        return 0;
    }
    size_t size = 0;
    uint8_t* unit = CollationRootUnit(&table, &size, errorOut, errorCapacity);
    Keys keys;
    if (unit == nullptr || !KeysOpen(&keys, commonDirectory, errorOut, errorCapacity))
    {
        free(unit);
        RootFree(&table);
        RootFree(&unihan);
        return 0;
    }
    // The root's table is built over its unit read back without one; the
    // view then reads the unit with it, kept until the types are built.
    Roots roots = {.root = &table, .unihan = &unihan};
    CollationMappings mappings;
    if (!CollationMappingsInit(&mappings, &table) ||
        !mlocReadCollationRoot((DataSpan){unit, (uint32_t)size}, &roots.view))
    {
        GenDie("the collation root", "does not read back");
    }
    uint16_t* fastLatin = CollationFastLatinTable(&mappings, &roots.view, &roots.fastLatinCount);
    CollationMappingsFree(&mappings);
    uint8_t* rootUnit = CollationFastLatinAppend(unit, &size, fastLatin, roots.fastLatinCount);
    roots.fastLatin = fastLatin;
    if (!mlocReadCollationRoot((DataSpan){rootUnit, (uint32_t)size}, &roots.view))
    {
        GenDie("the collation root", "does not read back with its fast Latin table");
    }
    rootUnit = CollationUnsafeAppend(rootUnit, &size, &roots.view);
    if (!mlocReadCollationRoot((DataSpan){rootUnit, (uint32_t)size}, &roots.view))
    {
        GenDie("the collation root", "does not read back with its unsafe ranges");
    }
    CollationSeal(rootUnit, size, collationRootHashAt);
    DataFileAdd(file, kindCollationRoot, nullptr, rootUnit, size);
    unit = CollationHanUnit(&unihan, &size);
    DataFileAdd(file, kindCollationHanRadical, nullptr, unit, size);
    free(unit);
    CollationCatalog catalog = {0};
    ReadCatalog(commonDirectory, &catalog);
    AddTailorings(&catalog, &roots, &keys, file);
    int added = AddIndexLabels(commonDirectory, &keys, file, errorOut, errorCapacity);
    free(fastLatin);
    free(rootUnit);
    CollationCatalogFree(&catalog);
    KeysClose(&keys);
    RootFree(&table);
    RootFree(&unihan);
    return added;
}
