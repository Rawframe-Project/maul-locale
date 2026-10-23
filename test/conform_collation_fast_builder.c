// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin tables the generator builds for the root and every type
// of CLDR's common/collation (not "private-", no alt but "short"), each
// written after its unit and read back; a type the builder refuses is
// counted. --tables writes each as conform_collation_fast writes a data
// file's, the tag ("und" for the root's, "locale-u-co-type" for a type's)
// then the count and the words in hex, for tools/oracles' ICU4C program;
// alone it prints how many tables of each kind and a hash of them, and
// with --expect it fails unless the hash is the given one, which CI checks
// on the pinned release.
//
// usage: conform_collation_fast_builder <FractionalUCA.txt>
//            [--tables | --expect HASH] common/collation/*.xml

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_builder.h"
#include "collation_catalog.h"
#include "collation_fast_table.h"
#include "collation_tailoring.h"
#include "collation_unit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Root
{
    const RootTable* table;
    const RootTable* unihan;
    CollationRoot view;
    uint16_t* words;
    size_t count;
} Root;

static uint64_t s_hash;
static bool s_print;
static size_t s_own;
static size_t s_shared;
static size_t s_none;
static size_t s_failed;
static size_t s_refused;

// A table's words hashed or written under its tag.
static void Table(const char* tag, const uint8_t* words, uint32_t count)
{
    s_hash = s_hash * 1000003U + count;
    if (s_print)
    {
        printf("%s %u", tag, count);
    }
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t word = mlocFastLatinWord(words, i);
        s_hash = s_hash * 1000003U + word;
        if (s_print)
        {
            printf(" %x", word);
        }
    }
    if (s_print)
    {
        printf("\n");
    }
}

// BCP 47's co value of a CLDR type name.
static const char* Code(const char* type)
{
    static const char* const codes[][2] = {{"phonebook", "phonebk"},
                                           {"traditional", "trad"},
                                           {"dictionary", "dict"},
                                           {"gb2312han", "gb2312"}};
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
    {
        if (strcmp(type, codes[i][0]) == 0)
        {
            return codes[i][1];
        }
    }
    return type;
}

// One type built, its unit written with its table and read back, and
// the table it reads as hashed or written.
static void Type(const CollationType* t, const Root* root, CollationCatalog* catalog)
{
    char error[256] = "";
    bool unihan = strcmp(t->type, "unihan") == 0;
    CollationBuilder b;
    size_t size = 0;
    uint8_t* unit = nullptr;
    if (CollationBuilderInit(&b, unihan ? root->unihan : root->table, CollationCatalogImport,
                             catalog, error, sizeof(error)) &&
        CollationBuilderAdd(&b, &t->rules) && CollationBuilderFinish(&b))
    {
        unit = CollationTailoringUnit(&b.mappings, &b.settings, &size, error, sizeof(error));
    }
    if (unit == nullptr)
    {
        // A refusal is the builder's to check (conform_collation_builder);
        // it counts in the hash.
        CollationBuilderFree(&b);
        s_hash = s_hash * 1000003U + 0xE770;
        s_refused++;
        return;
    }
    unit = CollationFastLatinSection(unit, &size, &b.mappings, &root->view, root->words,
                                     root->count, t->type);
    CollationBuilderFree(&b);
    CollationTailoring read;
    if (!mlocReadCollationTailoring((DataSpan){unit, (uint32_t)size}, &read))
    {
        printf("%s %s: the unit does not read back\n", t->file, t->type);
        s_failed++;
        free(unit);
        return;
    }
    char tag[160];
    snprintf(tag, sizeof(tag), "%s-u-co-%s", strcmp(t->file, "root") == 0 ? "und" : t->file,
             Code(t->type));
    for (char* p = tag; *p != '\0'; p++)
    {
        *p = *p == '_' ? '-' : *p;
    }
    uint32_t count = read.view.fastLatinCount;
    bool shared = count == fastLatinRootTable;
    s_shared += shared;
    s_none += count == 0;
    s_own += read.view.fastLatin != nullptr;
    Table(tag, shared ? root->view.fastLatin : read.view.fastLatin,
          shared ? root->view.fastLatinCount : count);
    free(unit);
}

// The root's unit with its table, read back into root->view; null when
// it does not build or read.
static uint8_t* RootUnit(Root* root)
{
    char error[256] = "";
    size_t size = 0;
    uint8_t* unit = CollationRootUnit(root->table, &size, error, sizeof(error));
    CollationMappings m;
    bool ok = unit != nullptr && CollationMappingsInit(&m, root->table);
    if (ok)
    {
        ok = mlocReadCollationRoot((DataSpan){unit, (uint32_t)size}, &root->view);
        root->words = ok ? CollationFastLatinTable(&m, &root->view, &root->count) : nullptr;
        CollationMappingsFree(&m);
        unit = CollationFastLatinAppend(unit, &size, root->words, root->count);
        ok = ok && root->words != nullptr &&
             mlocReadCollationRoot((DataSpan){unit, (uint32_t)size}, &root->view);
    }
    if (!ok)
    {
        fprintf(stderr, "the root's unit: %s\n", error[0] != '\0' ? error : "does not read");
        free(unit);
        return nullptr;
    }
    return unit;
}

static bool ReadCatalog(CollationCatalog* catalog, char** paths, int count)
{
    char error[256] = "";
    for (int i = 0; i < count; i++)
    {
        if (!CollationCatalogRead(catalog, paths[i], error, sizeof(error)))
        {
            fprintf(stderr, "%s\n", error);
            return false;
        }
    }
    CollationCatalogSort(catalog);
    return true;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <FractionalUCA.txt> [--tables | --expect HASH] files\n",
                argv[0]);
        return 2;
    }
    s_print = strcmp(argv[2], "--tables") == 0;
    const char* expected = argc > 3 && strcmp(argv[2], "--expect") == 0 ? argv[3] : nullptr;
    int first = s_print ? 3 : expected != nullptr ? 4 : 2;
    RootTable table = {0};
    RootTable unihan = {0};
    char error[256] = "";
    Root root = {.table = &table, .unihan = &unihan};
    CollationCatalog catalog = {0};
    uint8_t* unit = nullptr;
    bool ok = RootRead(argv[1], &table, error, sizeof(error)) &&
              RootReadUnihan(argv[1], &unihan, error, sizeof(error)) &&
              (unit = RootUnit(&root)) != nullptr &&
              ReadCatalog(&catalog, argv + first, argc - first);
    if (ok)
    {
        Table("und", root.view.fastLatin, root.view.fastLatinCount);
        for (size_t i = 0; i < catalog.count; i++)
        {
            const CollationType* t = &catalog.types[i];
            if (t->parsed && strncmp(t->type, "private-", 8) != 0 &&
                (t->alt[0] == '\0' || strcmp(t->alt, "short") == 0))
            {
                Type(t, &root, &catalog);
            }
        }
    }
    else if (error[0] != '\0')
    {
        fprintf(stderr, "%s\n", error);
    }
    CollationCatalogFree(&catalog);
    free(unit);
    free(root.words);
    RootFree(&table);
    RootFree(&unihan);
    if (!ok || s_print)
    {
        return ok ? 0 : 1;
    }
    char hash[32];
    snprintf(hash, sizeof(hash), "%016llx", (unsigned long long)s_hash);
    printf("fast Latin tables: %zu own, %zu the root's, %zu none; %zu types refused, %zu "
           "unreadable; hash %s\n",
           s_own, s_shared, s_none, s_refused, s_failed, hash);
    bool mismatch = expected != nullptr && strcmp(expected, hash) != 0;
    if (mismatch)
    {
        fprintf(stderr, "expected tables %s\n", expected);
    }
    return mismatch || s_failed != 0;
}
