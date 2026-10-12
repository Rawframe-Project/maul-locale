// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A part's lookup along a locale's chain (record mloc-0016): a pack with
// a few locales of a test kind, searched before the compiled-in data
// that holds the parents and likely subtags; and the parents table's
// validator, with one field of a copy spoiled for each rule.

#include "bytes.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "locale_tables.h"
#include "test_harness.h"

#include <stdlib.h>
#include <string.h>

enum
{
    kindTest = 0x7F000001,
};

static bool ValidAny(const DataView* view, DataSpan payload)
{
    (void)view;
    (void)payload;
    return true;
}

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

alignas(8) static uint8_t s_pack[1024];

// Writes a pack with one unit of kindTest per locale key, its payload
// the key itself; the keys are given sorted.
static uint32_t BuildPack(const char* const* keys, uint32_t count)
{
    memset(s_pack, 0, sizeof(s_pack));
    uint32_t at = (dataHeaderSize + count * dataUnitSize + count * dataLocaleSize + 7) / 8 * 8;
    uint32_t payloads = at;
    for (uint32_t i = 0; i < count; i++)
    {
        at = (at + 7) / 8 * 8;
        uint8_t* unit = s_pack + dataHeaderSize + i * dataUnitSize;
        Put(unit, kindTest, 4);
        Put(unit + 4, i, 4);
        Put(unit + 8, at, 4);
        Put(unit + 12, strlen(keys[i]), 4);
        memcpy(s_pack + at, keys[i], strlen(keys[i]));
        at += (uint32_t)strlen(keys[i]);
    }
    (void)payloads;
    uint32_t pool = at;
    for (uint32_t i = 0; i < count; i++)
    {
        uint8_t* entry = s_pack + dataHeaderSize + count * dataUnitSize + i * dataLocaleSize;
        Put(entry, at - pool, 4);
        Put(entry + 4, strlen(keys[i]), 4);
        memcpy(s_pack + at, keys[i], strlen(keys[i]));
        at += (uint32_t)strlen(keys[i]);
    }
    mlocDataVersion version;
    mlocGetDataVersion(&version);
    memcpy(s_pack, MLOC_DATA_MAGIC, 4);
    Put(s_pack + headerFormat, dataFormatVersion, 2);
    Put(s_pack + headerHeaderSize, dataHeaderSize, 2);
    Put(s_pack + headerFileSize, at, 4);
    Put(s_pack + headerCldr,
        mlocPackVersion(version.cldr.major, version.cldr.minor, version.cldr.patch), 4);
    Put(s_pack + headerUnicode,
        mlocPackVersion(version.unicode.major, version.unicode.minor, version.unicode.patch), 4);
    Put(s_pack + headerLibrary,
        mlocPackVersion(version.library.major, version.library.minor, version.library.patch), 4);
    memcpy(s_pack + headerTzdb, version.tzdb, 8);
    Put(s_pack + headerTables, version.tables, 8);
    Put(s_pack + headerUnitCount, count, 4);
    Put(s_pack + headerLocaleCount, count, 4);
    Put(s_pack + headerPoolOffset, pool, 4);
    Put(s_pack + headerPoolSize, at - pool, 4);
    return at;
}

static mlocDataSet s_set;
static mlocData s_sources[2];

static bool OpenPack(const char* const* keys, uint32_t count)
{
    static const DataKind s_kinds[] = {{kindTest, ValidAny}};
    DataView view;
    if (mlocOpenDataWithKinds(&view, s_pack, BuildPack(keys, count), s_kinds, 1) != mloc_success)
    {
        return false;
    }
    memcpy(&s_sources[0], &view, sizeof(view));
    mlocOpenBuiltinData(&s_sources[1]);
    s_set = (mlocDataSet){s_sources, 2};
    return true;
}

// Whether the lookup of text finds the unit filed under key, and
// whether that was a fallback.
static bool Finds(mlocComponent component, const char* text, const char* key, bool fellBack)
{
    mlocLocale locale;
    if (mlocParseLocale(text, strlen(text), &locale).status != mloc_success)
    {
        return false;
    }
    DataSpan span = {0};
    bool fell = !fellBack;
    if (!mlocFindLocaleUnit(&s_set, component, kindTest, &locale, &span, &fell))
    {
        return key == nullptr;
    }
    return key != nullptr && span.size == strlen(key) && memcmp(span.bytes, key, span.size) == 0 &&
           fell == fellBack;
}

static void TestLookups(void)
{
    static const char* const s_keys[] = {"de", "en_001", "pt", "root", "zh_Hant"};
    CHECK(OpenPack(s_keys, 5), "a pack of five locales opens");
    mlocComponent m = mloc_componentMain;
    CHECK(Finds(m, "pt", "pt", false), "a locale's own key");
    CHECK(Finds(m, "pt-BR", "pt", true), "an authored region finds CLDR's language");
    CHECK(Finds(m, "pt-Latn-BR", "pt", true), "and a maximized one");
    CHECK(Finds(m, "de-AT-u-co-phonebk", "de", true), "extensions do not count");
    CHECK(Finds(m, "zh-TW", "zh_Hant", true), "zh-TW finds Traditional Chinese");
    CHECK(Finds(m, "zh", "root", true), "zh does not take Traditional data");
    CHECK(Finds(m, "en-AU", "en_001", true), "an explicit parent the pack has");
    CHECK(Finds(m, "fr-CA", "root", true), "root last");
    CHECK(Finds(mloc_componentCollation, "zh-Hant-TW", "zh_Hant", true),
          "collation by its own chain");
    static const char* const s_noRoot[] = {"de"};
    CHECK(OpenPack(s_noRoot, 1), "a pack without root");
    CHECK(Finds(m, "fr", nullptr, false), "nothing found");
    mlocDataSet empty = {nullptr, 0};
    mlocLocale locale = {.language = "de"};
    DataSpan span;
    bool fell = false;
    CHECK(!mlocFindLocaleUnit(&empty, m, kindTest, &locale, &span, &fell),
          "data without the parents table");
}

static DataSpan s_table;

// Whether the parents table is valid with value written at offset of a
// copy.
static bool ValidWith(size_t offset, uint64_t value, int bytes)
{
    uint8_t* copy = malloc(s_table.size);
    if (copy == nullptr)
    {
        return true;
    }
    memcpy(copy, s_table.bytes, s_table.size);
    Put(copy + offset, value, bytes);
    bool valid = mlocValidParents(nullptr, (DataSpan){copy, s_table.size});
    free(copy);
    return valid;
}

static void TestValidator(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet set = {&builtin, 1};
    CHECK(mlocFindDataSetUnit(&set, kindLocaleParents, nullptr, 0, &s_table),
          "the compiled-in data has the parents");
    CHECK(mlocValidParents(nullptr, s_table), "the compiled-in table is valid");
    uint32_t count = mlocLoad32(s_table.bytes);
    size_t entries = parentHeaderSize;
    size_t text = entries + (size_t)count * parentEntrySize;
    size_t last = entries + (size_t)(count - 1) * parentEntrySize;
    uint8_t* copy = nullptr;
    CHECK(!ValidWith(8, 1, 8), "the reserved word is zero");
    CHECK(!ValidWith(4, mlocLoad32(s_table.bytes + 4) + 1, 4), "the counts give the size");
    CHECK(!ValidWith(last, mloc_componentGrammar + 1, 1), "a component past the last");
    CHECK(!ValidWith(entries + 3, 1, 1), "an entry's reserved byte");
    CHECK(!ValidWith(entries + 1, 0, 1), "an empty child");
    CHECK(!ValidWith(entries + 4, 0xFFFFFF, 4), "a child past the text");
    CHECK(!ValidWith(entries + 8, 0xFFFFFF, 4), "a parent past the text");
    copy = malloc(s_table.size);
    if (copy != nullptr)
    {
        // The second entry's child made the first's.
        memcpy(copy, s_table.bytes, s_table.size);
        copy[entries + parentEntrySize + 1] = copy[entries + 1];
        memcpy(copy + entries + parentEntrySize + 4, copy + entries + 4, 4);
        CHECK(!mlocValidParents(nullptr, (DataSpan){copy, s_table.size}), "entries in order");
        free(copy);
    }
    CHECK(!ValidWith(text, '-', 1), "a key that begins with a separator");
    copy = malloc(s_table.size);
    if (copy != nullptr)
    {
        // en_AU made its own parent: a loop.
        memcpy(copy, s_table.bytes, s_table.size);
        for (uint32_t i = 0; i < count; i++)
        {
            uint8_t* e = copy + entries + (size_t)i * parentEntrySize;
            const char* child = (const char*)copy + text + mlocLoad32(e + 4);
            if (e[0] == 0 && e[1] == 5 && memcmp(child, "en_AU", 5) == 0)
            {
                memcpy(e + 8, e + 4, 4);
                e[2] = e[1];
            }
        }
        CHECK(!mlocValidParents(nullptr, (DataSpan){copy, s_table.size}),
              "explicit parents that loop");
        free(copy);
    }
    CHECK(!ValidWith(text + 2, '-', 1), "a key with a hyphen");
    CHECK(!mlocValidParents(nullptr, (DataSpan){s_table.bytes, 8}), "shorter than the header");
}

int main(void)
{
    TestLookups();
    TestValidator();
    return s_failures == 0 ? 0 : 1;
}
