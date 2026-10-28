// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for catalogs. The input is a catalog, its magic and
// format version stamped as the library's, so that the fuzzer reaches the
// fields and their validators. A catalog that opens finds every key it
// stores at its own
// index, refuses the input's tail as a key unless it stores it, and gives
// every message from inside its own bytes; a chain of it binds in its
// measured cache and formats every key with a stated result.

#include "asset.h"
#include "catalog_tables.h"

#include "maul-locale/catalog.h"

#include <stdlib.h>
#include <string.h>

#define MAX_FILE 65536

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static void Stamp(uint8_t* file)
{
    memcpy(file, MLOC_CATALOG_MAGIC, 4);
    file[assetFormat] = (uint8_t)catalogFormatVersion;
    file[assetFormat + 1] = 0;
}

static uint32_t Load32(const uint8_t* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

// Every stored key is found at its index, and every index's message lies
// in the file.
static void CheckCatalog(const mlocCatalog* catalog, const uint8_t* file, size_t size)
{
    mlocCatalogInfo info;
    Require(mlocGetCatalogInfo(catalog, &info) == mloc_success);
    // The keys are the second field, after the locale's.
    uint32_t locale = Load32(file + assetHeaderSize + 4);
    const uint8_t* head = file + assetHeaderSize + mlocAssetFieldBytes(locale);
    Require(Load32(head) == catalogTags[catalogKeys]);
    DataSpan span = {head + assetFieldHead, Load32(head + 4)};
    CatalogKeys keys;
    Require(mlocReadCatalogKeys(span, &keys));
    for (uint32_t i = 0; i < keys.count; i++)
    {
        uint32_t length = 0;
        const uint8_t* key = mlocCatalogKeyAt(&keys, i, &length);
        uint32_t index = UINT32_MAX;
        Require(mlocFindCatalogKey(catalog, (const char*)key, length, &index) == mloc_success &&
                index == i);
        const void* message = nullptr;
        size_t bytes = 0;
        mlocResult r = mlocGetCatalogMessage(catalog, i, &message, &bytes);
        Require(r == mloc_errorNotFound || (r == mloc_success && (const uint8_t*)message >= file &&
                                            (const uint8_t*)message + bytes <= file + size));
    }
    const void* message = nullptr;
    size_t bytes = 0;
    Require(mlocGetCatalogMessage(catalog, keys.count, &message, &bytes) == mloc_errorInvalid);
}

// A chain of the catalog alone: measured, bound, every key formatted.
static void CheckChain(const mlocCatalog* catalog)
{
    static mlocData s_builtin;
    static mlocDataSet s_set;
    static mlocMessageFormatter s_formatter;
    static bool s_made = false;
    if (!s_made)
    {
        mlocLocale locale;
        mlocOpenBuiltinData(&s_builtin);
        s_set = (mlocDataSet){&s_builtin, 1};
        Require(mlocParseLocale("en", 2, &locale).status == mloc_success &&
                mlocInitMessageFormatter(&s_formatter, &s_set, &locale, nullptr) == mloc_success);
        s_made = true;
    }
    alignas(8) static uint8_t s_cache[1 << 20];
    size_t size = 0;
    Require(mlocMeasureCatalogChain(catalog, &s_formatter, 1, &size) == mloc_success);
    bool fits = size <= sizeof(s_cache);
    mlocCatalogChain chain;
    if (mlocInitCatalogChain(&chain, catalog, &s_formatter, 1, s_cache,
                             fits ? size : sizeof(s_cache)) != mloc_success)
    {
        Require(!fits);
        return;
    }
    mlocResult bound = mlocBindCatalogChain(&chain);
    Require(bound == mloc_success || (!fits && bound == mloc_errorCapacity));
    mlocCatalogInfo info;
    Require(mlocGetCatalogInfo(catalog, &info) == mloc_success);
    for (uint32_t i = 0; i < info.keyCount && i < 256; i++)
    {
        char text[256];
        mlocOutput output = {text, sizeof(text), 0, nullptr, nullptr};
        mlocCatalogResult result;
        mlocResult r = mlocFormatCatalogMessage(&chain, i, nullptr, 0, &output, nullptr, &result);
        Require((r == mloc_success || r == mloc_withFallbacks || r == mloc_errorCapacity) &&
                result.reason <= mloc_catalogCacheFull && result.position <= 1);
        Require(result.reason != mloc_catalogCacheFull || !fits);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    alignas(8) static uint8_t s_file[MAX_FILE];
    if (size > MAX_FILE || size < assetHeaderSize)
    {
        return 0;
    }
    memcpy(s_file, data, size);
    Stamp(s_file);
    mlocCatalog catalog;
    mlocResult opened = mlocOpenCatalog(&catalog, s_file, size);
    Require(opened == mloc_success || opened == mloc_errorFormat || opened == mloc_errorVersion);
    if (opened != mloc_success)
    {
        return 0;
    }
    CheckCatalog(&catalog, s_file, size);
    CheckChain(&catalog);
    // The last bytes as a key: found only where stored.
    size_t tail = size < 16 ? size : 16;
    uint32_t index = 0;
    if (mlocFindCatalogKey(&catalog, (const char*)data + size - tail, tail, &index) == mloc_success)
    {
        const void* message = nullptr;
        size_t length = 0;
        mlocResult r = mlocGetCatalogMessage(&catalog, index, &message, &length);
        Require(r == mloc_success || r == mloc_errorNotFound);
    }
    return 0;
}
