// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Catalogs: XXH64 against the reference values, the perfect hash over many
// keys, and a source and a translation compiled by the catalog tool,
// opened, looked up and refused when damaged. Run with a directory to
// write the catalogs into.

#include "catalog_hash.h"
#include "catalog_tables.h"
#include "compile.h"
#include "file.h"
#include "table.h"
#include "test_harness.h"

#include "maul-locale/catalog.h"
#include "maul-locale/message.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_directory;

static void TestXxh64(void)
{
    // The values of the reference implementation (xxHash 0.8, by its
    // Python binding), seeds 0 and 0x9E3779B97F4A7C15.
    static const struct
    {
        size_t length;
        uint64_t seed0;
        uint64_t seed1;
    } s_cases[] = {
        {0, 0xEF46DB3751D8E999U, 0xC4349FC93C010000U},
        {1, 0xD24EC4F1A98C6E5BU, 0x9A7C6D2EA45568C9U},
        {3, 0x44BC2CF5AD770999U, 0x2ED0F59D6B43AC8BU},
        {31, 0x60DD0D01083B99F0U, 0x4C66AEB653E8836AU},
        {32, 0xE2DF261FC2EC30EBU, 0xABF01487893340B0U},
        {33, 0xB3FA465F554208A6U, 0x3068A8E9EE0E7B68U},
    };
    uint8_t text[64];
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        size_t n = s_cases[i].length;
        memset(text, 'x', sizeof(text));
        if (n == 1 || n == 3)
        {
            memcpy(text, "abc", 3);
        }
        CHECK(mlocXxh64(text, n, 0) == s_cases[i].seed0 &&
                  mlocXxh64(text, n, 0x9E3779B97F4A7C15U) == s_cases[i].seed1,
              "XXH64 as the reference gives it");
    }
    uint8_t ramp[100];
    for (size_t i = 0; i < sizeof(ramp); i++)
    {
        ramp[i] = (uint8_t)i;
    }
    CHECK(mlocXxh64(ramp, 100, 0) == 0x6AC1E58032166597U &&
              mlocXxh64(ramp, 100, 0x9E3779B97F4A7C15U) == 0x3B97D91EBA03E785U,
          "XXH64 of 100 bytes: stripes, lanes of 8 and 4, and single bytes");
    static const struct
    {
        size_t length;
        uint64_t hash;
    } s_lanes[] = {{8, 0xF3844DCDA4EEABBFU},
                   {16, 0x0572AC5BED1C5A11U},
                   {40, 0x926F564E1B3E18D5U},
                   {64, 0xA97161D5402F516EU}};
    memset(text, 'x', sizeof(text));
    for (size_t i = 0; i < sizeof(s_lanes) / sizeof(s_lanes[0]); i++)
    {
        CHECK(mlocXxh64(text, s_lanes[i].length, 0) == s_lanes[i].hash,
              "XXH64 at a lane's end: 8, 16, 40 and 64 bytes");
    }
    // The places and buckets are the format: these values never change
    // within a data version.
    CHECK(mlocCatalogPlace(0x0123456789ABCDEFU, 7, 0, 1000) == 310 &&
              mlocCatalogPlace(0xFEDCBA9876543210U, 0, 3, 1U << 20) == 977421 &&
              mlocCatalogPlace(42, 65535, 0xFFFFFFFFFFFFFFFFU, 5) == 2 &&
              mlocCatalogBucket(0xFEDCBA9876543210U, 250001) == 248889,
          "the perfect hash's places and buckets as the format fixes them");
    memcpy(text, "hello.world", 11);
    CHECK(mlocXxh64(text, 11, 0) == 0xBF333E98D7B058E1U, "XXH64 of a key");
}

static void TestManyKeys(void)
{
    enum
    {
        keyCount = 200000,
    };
    char (*texts)[24] = malloc(sizeof(*texts) * keyCount);
    const char** keys = (const char**)malloc(sizeof(*keys) * keyCount);
    size_t* lengths = malloc(sizeof(*lengths) * keyCount);
    uint8_t* seen = calloc(keyCount, 1);
    if (texts == nullptr || keys == nullptr || lengths == nullptr || seen == nullptr)
    {
        CHECK(false, "memory for the keys");
        free(texts);
        free((void*)keys);
        free(lengths);
        free(seen);
        return;
    }
    for (uint32_t i = 0; i < keyCount; i++)
    {
        lengths[i] = (size_t)snprintf(texts[i], sizeof(texts[i]), "menu.item%u.label", i);
        keys[i] = texts[i];
    }
    KeyTable table;
    bool built = KeyTableBuild(keys, lengths, keyCount, &table);
    bool distinct = built;
    for (uint32_t i = 0; distinct && i < keyCount; i++)
    {
        distinct = table.index[i] < keyCount && !seen[table.index[i]];
        seen[distinct ? table.index[i] : 0] = 1;
    }
    CHECK(built && distinct && table.places == keyCount + keyCount / 50 + 1,
          "200,000 keys get distinct indexes below their count, the table 2% larger");
    KeyTable empty;
    CHECK(KeyTableBuild(keys, lengths, 0, &empty) && empty.buckets == 1 && empty.places == 0,
          "no keys: one bucket, no places");
    KeyTableFree(&empty);
    KeyTableFree(&table);
    free(texts);
    free((void*)keys);
    free(lengths);
    free(seen);
}

static void Write(const char* name, const char* text)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", s_directory, name);
    (void)FileWrite(path, text, strlen(text));
}

// Opens a catalog file into data and a catalog; the bytes are the
// caller's to free.
static uint8_t* Open(const char* locale, mlocData* data, mlocCatalog* catalog, mlocResult* out)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.mloc", s_directory, locale);
    size_t size = 0;
    uint8_t* bytes = (uint8_t*)FileRead(path, &size);
    *out = bytes == nullptr                                  ? mloc_errorNotFound
           : mlocOpenData(data, bytes, size) != mloc_success ? mloc_errorData
                                                             : mlocOpenCatalog(catalog, data);
    return bytes;
}

static bool SameMessage(const mlocCatalog* catalog, uint32_t index, const char* text)
{
    const void* message = nullptr;
    size_t size = 0;
    uint8_t compiled[512];
    size_t expected = 0;
    return mlocGetCatalogMessage(catalog, index, &message, &size) == mloc_success &&
           mlocCompileMessage(text, strlen(text), compiled, sizeof(compiled), &expected, nullptr)
                   .status == mloc_success &&
           size == expected && memcmp(message, compiled, size) == 0;
}

static void TestCompiledSet(void)
{
    Write("en.mf2", "@locale en\n---\nhello = Hello, {$name}!\nbye = Goodbye.\n[menu]\n"
                    "open = Open\n@translate no\nbrand = Maul\n@obsolete\nold = Gone\n");
    Write("tr.mf2", "@locale tr\n---\nhello = Merhaba {$name}!\n[menu]\nopen = A\xC3\xA7\n"
                    "brand = Moul\nextra = Fazla\n");
    char en[1024];
    char tr[1024];
    snprintf(en, sizeof(en), "%s/en.mf2", s_directory);
    snprintf(tr, sizeof(tr), "%s/tr.mf2", s_directory);
    char* paths[] = {en, tr};
    FILE* report = tmpfile();
    CHECK(CatalogCompile(s_directory, paths, 2, report != nullptr ? report : stderr),
          "a source and a translation compile");
    if (report != nullptr)
    {
        fclose(report);
    }
    mlocData enData;
    mlocData trData;
    mlocCatalog enCatalog;
    mlocCatalog trCatalog;
    mlocResult enOpened = mloc_errorInvalid;
    mlocResult trOpened = mloc_errorInvalid;
    uint8_t* enBytes = Open("en", &enData, &enCatalog, &enOpened);
    uint8_t* trBytes = Open("tr", &trData, &trCatalog, &trOpened);
    CHECK(enOpened == mloc_success && trOpened == mloc_success, "both catalogs open");
    mlocCatalogInfo enInfo;
    mlocCatalogInfo trInfo;
    if (enOpened == mloc_success && trOpened == mloc_success &&
        mlocGetCatalogInfo(&enCatalog, &enInfo) == mloc_success &&
        mlocGetCatalogInfo(&trCatalog, &trInfo) == mloc_success)
    {
        CHECK(enInfo.keyCount == 4 && enInfo.messageCount == 4 && enInfo.source &&
                  trInfo.keyCount == 4 && trInfo.messageCount == 3 && !trInfo.source &&
                  enInfo.setId == trInfo.setId && strcmp(trInfo.locale.language, "tr") == 0,
              "counts, the source flag, one set id, the locale; the obsolete key left out");
        uint32_t hello = 0;
        uint32_t bye = 0;
        uint32_t brand = 0;
        uint32_t again = 0;
        CHECK(mlocFindCatalogKey(&enCatalog, "hello", 5, &hello) == mloc_success &&
                  mlocFindCatalogKey(&trCatalog, "hello", 5, &again) == mloc_success &&
                  hello == again &&
                  mlocFindCatalogKey(&trCatalog, "bye", 3, &bye) == mloc_success &&
                  mlocFindCatalogKey(&enCatalog, "menu.brand", 10, &brand) == mloc_success,
              "a key has one index in every catalog of the set");
        CHECK(SameMessage(&trCatalog, hello, "Merhaba {$name}!") &&
                  SameMessage(&enCatalog, bye, "Goodbye.") &&
                  SameMessage(&trCatalog, brand, "Maul"),
              "the messages as compiled; a @translate no key keeps the source's");
        const void* message = nullptr;
        size_t size = 0;
        CHECK(mlocGetCatalogMessage(&trCatalog, bye, &message, &size) == mloc_errorNotFound &&
                  mlocGetCatalogMessage(&trCatalog, 4, &message, &size) == mloc_errorInvalid,
              "an untranslated key is not found; an index past the count is refused");
        uint32_t index = 0;
        CHECK(mlocFindCatalogKey(&enCatalog, "hell", 4, &index) == mloc_errorNotFound &&
                  mlocFindCatalogKey(&enCatalog, "hello!", 6, &index) == mloc_errorNotFound &&
                  mlocFindCatalogKey(&enCatalog, "", 0, &index) == mloc_errorNotFound &&
                  mlocFindCatalogKey(&enCatalog, "menu.extra", 10, &index) == mloc_errorNotFound &&
                  mlocFindCatalogKey(&enCatalog, nullptr, 1, &index) == mloc_errorInvalid,
              "keys the set lacks are not found, the orphan included");
    }
    free(enBytes);
    free(trBytes);
}

// A catalog's bytes with one byte changed opens no more.
static void TestDamage(void)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/en.mloc", s_directory);
    size_t size = 0;
    uint8_t* bytes = (uint8_t*)FileRead(path, &size);
    if (bytes == nullptr)
    {
        CHECK(false, "the catalog reads");
        return;
    }
    uint32_t refused = 0;
    uint32_t tried = 0;
    for (size_t at = 64; at < size; at++)
    {
        bytes[at] ^= 0x5A;
        mlocData data;
        mlocCatalog catalog;
        mlocResult opened = mlocOpenData(&data, bytes, size);
        if (opened == mloc_success)
        {
            opened = mlocOpenCatalog(&catalog, &data);
        }
        refused += opened != mloc_success;
        tried++;
        bytes[at] ^= 0x5A;
    }
    // A changed byte of a message's literal text, or of padding, is still
    // a valid catalog; the tables' bytes are not.
    CHECK(refused > tried / 2, "damaged catalogs are refused");
    free(bytes);
}

// A catalog's bytes with one 32-bit word set, opened again.
static mlocResult Reopen(uint8_t* bytes, size_t size, size_t at, uint32_t value)
{
    uint8_t saved[4];
    memcpy(saved, bytes + at, 4);
    for (int i = 0; i < 4; i++)
    {
        bytes[at + (size_t)i] = (uint8_t)(value >> (8 * i));
    }
    mlocData data;
    mlocCatalog catalog;
    mlocResult r = mlocOpenData(&data, bytes, size);
    r = r == mloc_success ? mlocOpenCatalog(&catalog, &data) : mloc_errorData;
    memcpy(bytes + at, saved, 4);
    return r;
}

static uint32_t Word(const uint8_t* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

// Each table check refuses its own damage.
static void TestEachRefusal(void)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/en.mloc", s_directory);
    size_t size = 0;
    uint8_t* bytes = (uint8_t*)FileRead(path, &size);
    mlocData data;
    if (bytes == nullptr || mlocOpenData(&data, bytes, size) != mloc_success)
    {
        CHECK(false, "the catalog opens");
        free(bytes);
        return;
    }
    // The units are found by the set id each stores (at 24 in the keys,
    // at 8 in the messages), as a shared build hides the data's directory.
    mlocCatalog opened;
    mlocCatalogInfo info;
    size_t at[2] = {0, 0};
    int seen = 0;
    if (mlocOpenCatalog(&opened, &data) == mloc_success &&
        mlocGetCatalogInfo(&opened, &info) == mloc_success)
    {
        uint8_t id[8];
        for (int i = 0; i < 8; i++)
        {
            id[i] = (uint8_t)(info.setId >> (8 * i));
        }
        for (size_t i = 0; seen < 2 && i + 8 <= size; i += 4)
        {
            if (memcmp(bytes + i, id, 8) == 0)
            {
                at[seen++] = i;
            }
        }
    }
    DataSpan keys = {0};
    DataSpan messages = {0};
    if (seen == 2 && at[0] >= 24 && at[1] >= 8)
    {
        keys.bytes = bytes + at[0] - 24;
        keys.size = (uint32_t)(at[1] - 8 - (at[0] - 24));
        messages.bytes = bytes + at[1] - 8;
        messages.size = (uint32_t)(size - (at[1] - 8));
    }
    CatalogKeys k;
    if (keys.bytes == nullptr || !mlocReadCatalogKeys(keys, &k) || k.places <= k.count ||
        k.count < 2)
    {
        CHECK(false, "the catalog's units");
        free(bytes);
        return;
    }
    size_t K = (size_t)(keys.bytes - bytes);
    size_t M = (size_t)(messages.bytes - bytes);
    size_t remap = (size_t)(k.remap - bytes);
    size_t offsets = (size_t)(k.offsets - bytes);
    size_t text = (size_t)(k.text - bytes);
    uint32_t firstMessage = Word(messages.bytes + 16);
    size_t messageText = M + 16 + ((size_t)k.count + 1) * 4 + firstMessage;
    CHECK(Reopen(bytes, size, K + 12, 1) == mloc_errorData &&
              Reopen(bytes, size, K + 4, 0) == mloc_errorData &&
              Reopen(bytes, size, K + 8, k.count - 1) == mloc_errorData &&
              Reopen(bytes, size, remap, k.count) == mloc_errorData &&
              Reopen(bytes, size, offsets, 1) == mloc_errorData &&
              Reopen(bytes, size, offsets + 4, Word(bytes + offsets + 8) + 1) == mloc_errorData &&
              Reopen(bytes, size, offsets + (size_t)k.count * 4, k.textSize + 1) == mloc_errorData,
          "keys: the reserved word, no buckets, too few places, a remap past the count, offsets "
          "not from 0, falling or past the text");
    CHECK(Reopen(bytes, size, text, Word(bytes + text) ^ 0x20) == mloc_errorData &&
              Reopen(bytes, size, text, Word(bytes + text) & 0xFFFFFF00U) == mloc_errorData,
          "keys: a key changed is not at its index; a key holding a NUL");
    CHECK(Reopen(bytes, size, M + 4, 2) == mloc_errorData &&
              Reopen(bytes, size, M, 0xFFFF) == mloc_errorData &&
              Reopen(bytes, size, M + 16, 1) == mloc_errorData &&
              Reopen(bytes, size, messageText, Word(bytes + messageText) ^ 0xFF) == mloc_errorData,
          "messages: an unknown flag, a count past the unit, offsets not from 0, a message that "
          "is not one");
    CHECK(Reopen(bytes, size, M + 8, Word(bytes + M + 8) ^ 1) == mloc_errorData &&
              Reopen(bytes, size, K + 24, Word(bytes + K + 24) ^ 1) == mloc_errorData,
          "the set ids disagree, or the keys' own is not theirs");
    mlocCatalog catalog;
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    CHECK(mlocOpenCatalog(&catalog, &builtin) == mloc_errorNotFound &&
              mlocOpenCatalog(nullptr, &data) == mloc_errorInvalid &&
              mlocOpenCatalog(&catalog, nullptr) == mloc_errorInvalid,
          "data without a catalog; null pointers");
    free(bytes);
}

static void TestArguments(void)
{
    mlocCatalog zero;
    memset(&zero, 0, sizeof(zero));
    mlocCatalogInfo info;
    uint32_t index = 0;
    const void* message = nullptr;
    size_t size = 0;
    CHECK(mlocGetCatalogInfo(&zero, &info) == mloc_errorInvalid &&
              mlocGetCatalogInfo(nullptr, &info) == mloc_errorInvalid &&
              mlocFindCatalogKey(&zero, "a", 1, &index) == mloc_errorInvalid &&
              mlocGetCatalogMessage(&zero, 0, &message, &size) == mloc_errorInvalid,
          "a catalog the library did not open is refused");
    char path[1024];
    snprintf(path, sizeof(path), "%s/en.mloc", s_directory);
    uint8_t* bytes = (uint8_t*)FileRead(path, &size);
    mlocData data;
    mlocCatalog catalog;
    if (bytes != nullptr && mlocOpenData(&data, bytes, size) == mloc_success &&
        mlocOpenCatalog(&catalog, &data) == mloc_success)
    {
        CHECK(mlocGetCatalogInfo(&catalog, nullptr) == mloc_errorInvalid &&
                  mlocFindCatalogKey(&catalog, "a", 1, nullptr) == mloc_errorInvalid &&
                  mlocFindCatalogKey(&catalog, nullptr, 0, &index) == mloc_errorNotFound &&
                  mlocGetCatalogMessage(&catalog, 0, nullptr, &size) == mloc_errorInvalid &&
                  mlocGetCatalogMessage(&catalog, 0, &message, nullptr) == mloc_errorInvalid,
              "null outputs refused; a null empty key is no key");
    }
    free(bytes);
}

static void TestCompileFailures(void)
{
    Write("bad.mf2", "@locale fr\n---\nk = {$x\n");
    Write("de.mf2", "@locale de\n---\n@translate no\n[brand]\nname = Maul\nother = X\n");
    Write("de-tr.mf2", "@locale tr\n---\n[brand]\nname = Moul\nother = Y\n");
    char bad[1024];
    char de[1024];
    char deTr[1024];
    char missing[1024];
    snprintf(bad, sizeof(bad), "%s/bad.mf2", s_directory);
    snprintf(de, sizeof(de), "%s/de.mf2", s_directory);
    snprintf(deTr, sizeof(deTr), "%s/de-tr.mf2", s_directory);
    snprintf(missing, sizeof(missing), "%s/no-such-directory", s_directory);
    FILE* report = tmpfile();
    FILE* to = report != nullptr ? report : stderr;
    char* badPaths[] = {de, bad};
    char* goodPaths[] = {de, deTr};
    CHECK(!CatalogCompile(s_directory, badPaths, 2, to) &&
              !CatalogCompile(s_directory, badPaths, 0, to) &&
              !CatalogCompile(missing, goodPaths, 2, to),
          "a message that does not compile, no resources, a directory that is not");
    CHECK(CatalogCompile(s_directory, goodPaths, 2, to), "a set with a section kept whole");
    if (report != nullptr)
    {
        char said[4096] = {0};
        rewind(report);
        size_t read = fread(said, 1, sizeof(said) - 1, report);
        said[read] = '\0';
        CHECK(strstr(said, "no-such-directory/de.mloc: cannot write") != nullptr,
              "the report names the catalog it could not write");
        fclose(report);
    }
    mlocData data;
    mlocCatalog catalog;
    mlocResult opened = mloc_errorInvalid;
    uint8_t* bytes = Open("tr", &data, &catalog, &opened);
    uint32_t name = 0;
    uint32_t other = 0;
    CHECK(opened == mloc_success &&
              mlocFindCatalogKey(&catalog, "brand.name", 10, &name) == mloc_success &&
              mlocFindCatalogKey(&catalog, "brand.other", 11, &other) == mloc_success &&
              SameMessage(&catalog, name, "Maul") && SameMessage(&catalog, other, "X"),
          "@translate no on a section keeps every message of it");
    free(bytes);
}

int main(int argc, char** argv)
{
    s_directory = argc > 1 ? argv[1] : ".";
    TestXxh64();
    TestManyKeys();
    TestCompiledSet();
    TestDamage();
    TestEachRefusal();
    TestArguments();
    TestCompileFailures();
    return s_failures == 0 ? 0 : 1;
}
