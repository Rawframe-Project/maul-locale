// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The catalog's benchmark cases (bench_catalog.h).

// The C runtime of Windows reports fopen as deprecated.
#define _CRT_SECURE_NO_WARNINGS

#include "bench_catalog.h"

#include "maul-locale/catalog.h"
#include "maul-locale/data.h"
#include "maul-locale/locale.h"
#include "maul-locale/message.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    keyCount = 2000,
    // The message formatted: "Hello {$name}, item 1" and its French.
    formatKey = 1,
};

static uint8_t* s_bytes[2];
static size_t s_sizes[2];
static mlocCatalog s_catalogs[2];
static mlocMessageFormatter s_formatters[2];
static mlocCatalogChain s_chain;
static uint8_t* s_bindCache;
static size_t s_cacheSize;
static uint32_t s_formatIndex;
static uint8_t s_bound[4096];
static char s_keys[keyCount][24];
static size_t s_keyLengths[keyCount];
static uint32_t s_next;
static char s_text[256];
static volatile size_t s_sink;

static const mlocMessageArgument s_arguments[] = {
    {"name", 4, {mloc_messageValueString, 0, {.string = {"Ada", 3}}}},
    {"n", 1, {mloc_messageValueInteger, 0, {.integer = 3}}},
    {"total", 5, {mloc_messageValueInteger, 0, {.integer = 7}}},
};

enum
{
    argumentCount = sizeof(s_arguments) / sizeof(s_arguments[0]),
};

size_t g_catalogCaseCount;

static void FindKey(void)
{
    uint32_t i = s_next++ % keyCount;
    uint32_t index = 0;
    mlocResult r = mlocFindCatalogKey(&s_catalogs[0], s_keys[i], s_keyLengths[i], &index);
    s_sink += (size_t)r + index;
}

static void GetMessage(void)
{
    const void* message = nullptr;
    size_t size = 0;
    mlocResult r = mlocGetCatalogMessage(&s_catalogs[0], s_next++ % keyCount, &message, &size);
    s_sink += (size_t)r + size;
}

static void Open(void)
{
    mlocCatalog catalog;
    s_sink += (size_t)mlocOpenCatalog(&catalog, s_bytes[0], s_sizes[0]);
}

static void Bind(void)
{
    mlocCatalogChain chain;
    mlocResult r =
        mlocInitCatalogChain(&chain, s_catalogs, s_formatters, 2, s_bindCache, s_cacheSize);
    s_sink += (size_t)r + (size_t)mlocBindCatalogChain(&chain);
}

static void FormatIndex(void)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult r = mlocFormatCatalogMessage(&s_chain, s_formatIndex, s_arguments, argumentCount,
                                            &output, nullptr, nullptr);
    s_sink += (size_t)r + output.length;
}

static void FormatKey(void)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult r = mlocFormatCatalogKey(&s_chain, s_keys[formatKey], s_keyLengths[formatKey],
                                        s_arguments, argumentCount, &output, nullptr, nullptr);
    s_sink += (size_t)r + output.length;
}

static void FormatBound(void)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult r = mlocFormatMessage(s_bound, s_arguments, argumentCount, &output, nullptr);
    s_sink += (size_t)r + output.length;
}

const BenchCase g_catalogCases[] = {
    {"catalog-find-key", FindKey, 5000000},
    {"catalog-message", GetMessage, 20000000},
    {"catalog-open", Open, 200000},
    {"catalog-bind", Bind, 500},
    {"catalog-format-index", FormatIndex, 2000000},
    {"catalog-format-key", FormatKey, 2000000},
    {"catalog-format-bound", FormatBound, 2000000},
};

#ifdef MLOC_BENCH_CATALOG_DIR

static uint8_t* s_cache;

static uint8_t* ReadFile(const char* path, size_t* sizeOut)
{
    FILE* f = fopen(path, "rb");
    long size = -1;
    if (f != nullptr && fseek(f, 0, SEEK_END) == 0)
    {
        size = ftell(f);
    }
    uint8_t* bytes = size > 0 && fseek(f, 0, SEEK_SET) == 0 ? malloc((size_t)size) : nullptr;
    if (bytes != nullptr && fread(bytes, 1, (size_t)size, f) != (size_t)size)
    {
        free(bytes);
        bytes = nullptr;
    }
    if (f != nullptr)
    {
        fclose(f);
    }
    *sizeOut = bytes != nullptr ? (size_t)size : 0;
    return bytes;
}

// The chain fr, en: its catalogs, formatters and bound cache, and the fr
// message bound alone.
static bool MakeChain(void)
{
    static mlocData builtin;
    static mlocDataSet data;
    mlocOpenBuiltinData(&builtin);
    data = (mlocDataSet){&builtin, 1};
    static const char* const locales[] = {"fr", "en"};
    bool ok = true;
    for (int i = 0; ok && i < 2; i++)
    {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s.mlct", MLOC_BENCH_CATALOG_DIR, locales[i]);
        mlocLocale locale;
        s_bytes[i] = ReadFile(path, &s_sizes[i]);
        ok = s_bytes[i] != nullptr &&
             mlocOpenCatalog(&s_catalogs[i], s_bytes[i], s_sizes[i]) == mloc_success &&
             mlocParseLocale(locales[i], 2, &locale).status == mloc_success &&
             mlocInitMessageFormatter(&s_formatters[i], &data, &locale, nullptr) == mloc_success;
    }
    ok = ok && mlocMeasureCatalogChain(s_catalogs, s_formatters, 2, &s_cacheSize) == mloc_success;
    s_cache = ok ? malloc(s_cacheSize) : nullptr;
    s_bindCache = ok ? malloc(s_cacheSize) : nullptr;
    ok = s_cache != nullptr && s_bindCache != nullptr &&
         mlocInitCatalogChain(&s_chain, s_catalogs, s_formatters, 2, s_cache, s_cacheSize) ==
             mloc_success &&
         mlocBindCatalogChain(&s_chain) == mloc_success;
    const void* message = nullptr;
    size_t size = 0;
    size_t bound = 0;
    return ok &&
           mlocFindCatalogKey(&s_catalogs[0], s_keys[formatKey], s_keyLengths[formatKey],
                              &s_formatIndex) == mloc_success &&
           mlocGetCatalogMessage(&s_catalogs[0], s_formatIndex, &message, &size) == mloc_success &&
           mlocBindMessage(&s_formatters[0], message, size, s_bound, sizeof(s_bound), &bound) ==
               mloc_success;
}

bool PrepareCatalogBench(void)
{
    for (int i = 0; i < keyCount; i++)
    {
        int n = snprintf(s_keys[i], sizeof(s_keys[i]), "screen.item.%04d", i);
        if (n < 0 || (size_t)n >= sizeof(s_keys[i]))
        {
            return false;
        }
        s_keyLengths[i] = (size_t)n;
    }
    if (!MakeChain())
    {
        return false;
    }
    g_catalogCaseCount = sizeof(g_catalogCases) / sizeof(g_catalogCases[0]);
    return true;
}

#else

bool PrepareCatalogBench(void)
{
    return true;
}

#endif
