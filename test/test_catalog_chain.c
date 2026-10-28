// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Catalog chains: a set of three locales with holes compiled by the
// catalog tool, formatted through a chain, each failure's stated text and
// reason, a cache measured, a cache too small, a chain bound at once. Run
// with a directory to write the catalogs into.

#include "compile.h"
#include "file.h"
#include "test_harness.h"

#include "maul-locale/catalog.h"
#include "maul-locale/message.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_directory;

enum
{
    chainCount = 3,
};

// The catalogs, opened, and their locales' formatters.
typedef struct Set
{
    uint8_t* bytes[chainCount];
    mlocCatalog catalogs[chainCount];
    mlocMessageFormatter formatters[chainCount];
} Set;

static void Write(const char* name, const char* text)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", s_directory, name);
    (void)FileWrite(path, text, strlen(text));
}

static bool Compile(void)
{
    Write("en.mf2",
          "@locale en\n---\nhello = Hello\nbye = Goodbye\n"
          "count = .input {$n :integer}\n .match $n\n one {{One file}}\n * {{{$n} files}}\n"
          "wide = Wide\nbroken = Broken\nfaulty = Hi {$name}\n");
    Write("fr.mf2", "@locale fr\n---\nhello = Bonjour\n"
                    "count = .input {$n :integer}\n .match $n\n one {{Un fichier}}\n * {{{$n} "
                    "fichiers}}\nwide = Large\nbroken = Casse\nfaulty = Salut {$nmae}\n");
    Write("fr-CA.mf2", "@locale fr-CA\n---\nhello = Allo\n");
    Write("de.mf2", "@locale de\n---\nhello = Hallo\n");
    char paths[4][1024];
    const char* names[] = {"en", "fr", "fr-CA", "de"};
    for (int i = 0; i < 4; i++)
    {
        snprintf(paths[i], sizeof(paths[i]), "%s/%s.mf2", s_directory, names[i]);
    }
    char* set[] = {paths[0], paths[1], paths[2]};
    char* other[] = {paths[3]};
    FILE* report = tmpfile();
    FILE* to = report != nullptr ? report : stderr;
    bool ok = CatalogCompile(s_directory, set, 3, to) && CatalogCompile(s_directory, other, 1, to);
    if (report != nullptr)
    {
        fclose(report);
    }
    return ok;
}

static bool OpenOne(const char* locale, uint8_t** bytes, mlocCatalog* catalog,
                    mlocMessageFormatter* formatter)
{
    static mlocData s_builtin;
    static mlocDataSet s_data;
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.mlct", s_directory, locale);
    size_t size = 0;
    *bytes = (uint8_t*)FileRead(path, &size);
    mlocLocale parsed;
    return *bytes != nullptr && mlocOpenCatalog(catalog, *bytes, size) == mloc_success &&
           mlocParseLocale(locale, strlen(locale), &parsed).status == mloc_success &&
           mlocInitMessageFormatter(formatter, &s_data, &parsed, nullptr) == mloc_success;
}

// The chain fr-CA, fr, en.
static bool OpenSet(Set* s)
{
    const char* locales[chainCount] = {"fr-CA", "fr", "en"};
    bool ok = true;
    for (int i = 0; i < chainCount; i++)
    {
        ok = OpenOne(locales[i], &s->bytes[i], &s->catalogs[i], &s->formatters[i]) && ok;
    }
    return ok;
}

static void CloseSet(Set* s)
{
    for (int i = 0; i < chainCount; i++)
    {
        free(s->bytes[i]);
    }
}

// Formats a key through the chain and compares its text, result and reason.
static bool Says(const mlocCatalogChain* chain, const char* key, int64_t n, const char* text,
                 mlocResult expected, mlocCatalogReason reason, uint32_t position)
{
    char buffer[256];
    mlocOutput output = {buffer, sizeof(buffer), 0, nullptr, nullptr};
    mlocMessageArgument argument = {"n", 1, {mloc_messageValueInteger, 0, {.integer = n}}};
    mlocCatalogResult result = {mloc_catalogFailed, 99};
    mlocResult r =
        mlocFormatCatalogKey(chain, key, strlen(key), &argument, 1, &output, nullptr, &result);
    bool same = r == expected && output.length == strlen(text) &&
                memcmp(buffer, text, output.length) == 0 && result.reason == reason &&
                result.position == position;
    if (!same)
    {
        fprintf(stderr, "  %s: %d '%.*s' reason %d at %u\n", key, (int)r, (int)output.length,
                buffer, (int)result.reason, result.position);
    }
    return same;
}

// Every key of the set through a chain with room for all.
static bool EveryReason(const mlocCatalogChain* chain)
{
    return Says(chain, "hello", 0, "Allo", mloc_success, mloc_catalogFound, 0) &&
           Says(chain, "bye", 0, "Goodbye", mloc_success, mloc_catalogFallback, 2) &&
           Says(chain, "count", 1, "Un fichier", mloc_success, mloc_catalogFallback, 1) &&
           Says(chain, "count", 3, "3 fichiers", mloc_success, mloc_catalogFallback, 1) &&
           Says(chain, "wide", 0, "Large", mloc_success, mloc_catalogFallback, 1) &&
           Says(chain, "broken", 0, "Casse", mloc_success, mloc_catalogFallback, 1) &&
           Says(chain, "faulty", 0, "Salut \xE2\x81\xA8{$nmae}\xE2\x81\xA9", mloc_withFallbacks,
                mloc_catalogFallback, 1) &&
           Says(chain, "nope", 0, "nope", mloc_success, mloc_catalogMissingKey, 3);
}

static void TestReasons(Set* s)
{
    size_t size = 0;
    mlocCatalogChain chain;
    CHECK(mlocMeasureCatalogChain(s->catalogs, s->formatters, chainCount, &size) == mloc_success,
          "a chain's cache is measured");
    uint8_t* cache = malloc(size + 1);
    // Bound lazily, at an odd address.
    CHECK(cache != nullptr &&
              mlocInitCatalogChain(&chain, s->catalogs, s->formatters, chainCount, cache + 1,
                                   size) == mloc_success &&
              EveryReason(&chain) && EveryReason(&chain),
          "each key's text and reason, bound lazily, then from the cache");
    // Bound at once into the measured size: it all fits, and the texts
    // are the same.
    CHECK(cache != nullptr &&
              mlocInitCatalogChain(&chain, s->catalogs, s->formatters, chainCount, cache, size) ==
                  mloc_success &&
              mlocBindCatalogChain(&chain) == mloc_success && EveryReason(&chain) &&
              mlocBindCatalogChain(&chain) == mloc_success,
          "a chain bound at once in the measured cache");
    CHECK(cache != nullptr &&
              mlocInitCatalogChain(&chain, s->catalogs, s->formatters, chainCount, cache,
                                   size / 2) == mloc_success &&
              mlocBindCatalogChain(&chain) == mloc_errorCapacity &&
              Says(&chain, "hello", 0, "Allo", mloc_success, mloc_catalogFound, 0),
          "half the room: not every message fits, those bound still serve");
    free(cache);
}

// A formatter the library did not make binds nothing, so its catalog's
// messages fall back as ones that do not bind: to the source's, then to
// the key.
static void TestUnbound(Set* s)
{
    mlocMessageFormatter formatters[chainCount];
    memcpy(formatters, s->formatters, sizeof(formatters));
    memset(&formatters[1], 0, sizeof(formatters[1]));
    size_t size = 0;
    mlocCatalogChain chain;
    CHECK(mlocMeasureCatalogChain(s->catalogs, formatters, chainCount, &size) == mloc_success,
          "a chain with a formatter not made is measured");
    uint8_t* cache = malloc(size);
    CHECK(cache != nullptr &&
              mlocInitCatalogChain(&chain, s->catalogs, formatters, chainCount, cache, size) ==
                  mloc_success &&
              Says(&chain, "hello", 0, "Allo", mloc_success, mloc_catalogFound, 0) &&
              Says(&chain, "wide", 0, "Wide", mloc_success, mloc_catalogSource, 2) &&
              Says(&chain, "count", 1, "One file", mloc_success, mloc_catalogSource, 2),
          "a message that does not bind falls to the source's");
    memset(&formatters[2], 0, sizeof(formatters[2]));
    CHECK(cache != nullptr &&
              mlocInitCatalogChain(&chain, s->catalogs, formatters, chainCount, cache, size) ==
                  mloc_success &&
              Says(&chain, "broken", 0, "broken", mloc_success, mloc_catalogFailed, 3) &&
              Says(&chain, "hello", 0, "Allo", mloc_success, mloc_catalogFound, 0),
          "and to its key when the source's does not bind either");
    free(cache);
}

static void TestWithoutSource(Set* s)
{
    // The two catalogs alone in memory of their own size, so that a look
    // past them shows under the sanitizers.
    mlocCatalog* catalogs = malloc(2 * sizeof(mlocCatalog));
    mlocMessageFormatter* formatters = malloc(2 * sizeof(mlocMessageFormatter));
    if (catalogs == nullptr || formatters == nullptr)
    {
        CHECK(false, "the arrays are made");
        free(catalogs);
        free(formatters);
        return;
    }
    memcpy(catalogs, s->catalogs, 2 * sizeof(mlocCatalog));
    memcpy(formatters, s->formatters, 2 * sizeof(mlocMessageFormatter));
    memset(&formatters[1], 0, sizeof(mlocMessageFormatter));
    size_t size = 0;
    mlocCatalogChain chain;
    CHECK(mlocMeasureCatalogChain(catalogs, formatters, 2, &size) == mloc_success,
          "a chain without its source is measured");
    uint8_t* cache = malloc(size);
    CHECK(cache != nullptr &&
              mlocInitCatalogChain(&chain, catalogs, formatters, 2, cache, size) == mloc_success &&
              Says(&chain, "bye", 0, "bye", mloc_success, mloc_catalogAbsent, 2) &&
              Says(&chain, "wide", 0, "wide", mloc_success, mloc_catalogFailed, 2) &&
              Says(&chain, "hello", 0, "Allo", mloc_success, mloc_catalogFound, 0),
          "without the source: an absent message and one that fails are their keys");
    free(cache);
    free(catalogs);
    free(formatters);
}

// The most catalogs a chain takes: the set's three, then the source
// again to fill it.
static void TestLongest(Set* s)
{
    mlocCatalog catalogs[MLOC_MAX_CATALOG_CHAIN];
    mlocMessageFormatter formatters[MLOC_MAX_CATALOG_CHAIN];
    for (int i = 0; i < MLOC_MAX_CATALOG_CHAIN; i++)
    {
        catalogs[i] = s->catalogs[i < chainCount ? i : chainCount - 1];
        formatters[i] = s->formatters[i < chainCount ? i : chainCount - 1];
    }
    size_t size = 0;
    (void)mlocMeasureCatalogChain(catalogs, formatters, MLOC_MAX_CATALOG_CHAIN, &size);
    uint8_t* cache = malloc(size > 0 ? size : 1);
    mlocCatalogChain chain;
    CHECK(cache != nullptr &&
              mlocInitCatalogChain(&chain, catalogs, formatters, MLOC_MAX_CATALOG_CHAIN, cache,
                                   size) == mloc_success &&
              Says(&chain, "bye", 0, "Goodbye", mloc_success, mloc_catalogFallback, 2) &&
              Says(&chain, "nope", 0, "nope", mloc_success, mloc_catalogMissingKey,
                   MLOC_MAX_CATALOG_CHAIN),
          "a chain of eight catalogs");
    free(cache);
}

static void TestCacheFull(Set* s)
{
    // Room for the head and the slots alone.
    size_t slots = 16 + 8 * 6;
    uint8_t* cache = malloc(slots);
    mlocCatalogChain chain;
    CHECK(cache != nullptr &&
              mlocInitCatalogChain(&chain, s->catalogs, s->formatters, chainCount, cache,
                                   slots - 1) == mloc_errorCapacity &&
              mlocInitCatalogChain(&chain, s->catalogs, s->formatters, chainCount, cache, slots) ==
                  mloc_success &&
              Says(&chain, "hello", 0, "hello", mloc_success, mloc_catalogCacheFull, 3) &&
              Says(&chain, "nope", 0, "nope", mloc_success, mloc_catalogMissingKey, 3) &&
              mlocBindCatalogChain(&chain) == mloc_errorCapacity,
          "a cache with no room writes keys, refuses no lookup");
    free(cache);
}

static void Part(void* context, uint32_t part, size_t start, size_t length)
{
    size_t* seen = context;
    seen[0]++;
    seen[1] = part;
    seen[2] = start + length;
}

static void TestOutputs(Set* s)
{
    size_t size = 0;
    mlocCatalogChain chain;
    (void)mlocMeasureCatalogChain(s->catalogs, s->formatters, chainCount, &size);
    uint8_t* cache = malloc(size);
    if (cache == nullptr || mlocInitCatalogChain(&chain, s->catalogs, s->formatters, chainCount,
                                                 cache, size) != mloc_success)
    {
        CHECK(false, "a chain is made");
        free(cache);
        return;
    }
    char small[3];
    mlocOutput output = {small, sizeof(small), 0, nullptr, nullptr};
    mlocResult key =
        mlocFormatCatalogKey(&chain, "missing", 7, nullptr, 0, &output, nullptr, nullptr);
    size_t keyLength = output.length;
    mlocResult text = mlocFormatCatalogKey(&chain, "bye", 3, nullptr, 0, &output, nullptr, nullptr);
    CHECK(key == mloc_errorCapacity && keyLength == 7 && text == mloc_errorCapacity &&
              output.length == 7,
          "a key or a text too long for the buffer gives the length needed");
    char buffer[32];
    size_t seen[3] = {0, 0, 0};
    output = (mlocOutput){buffer, sizeof(buffer), 0, Part, seen};
    CHECK(mlocFormatCatalogKey(&chain, "missing", 7, nullptr, 0, &output, nullptr, nullptr) ==
                  mloc_success &&
              seen[0] == 1 && seen[1] == mloc_messagePartFallback && seen[2] == 7,
          "a key written is one fallback part");
    output = (mlocOutput){nullptr, 0, 0, nullptr, nullptr};
    CHECK(mlocFormatCatalogKey(&chain, "missing", 7, nullptr, 0, &output, nullptr, nullptr) ==
                  mloc_errorCapacity &&
              output.length == 7 &&
              mlocFormatCatalogKey(&chain, "", 0, nullptr, 0, &output, nullptr, nullptr) ==
                  mloc_success &&
              output.length == 0,
          "a key measured; the empty key written as nothing");
    mlocCatalogChain zero;
    memset(&zero, 0, sizeof(zero));
    output = (mlocOutput){buffer, sizeof(buffer), 0, nullptr, nullptr};
    CHECK(mlocFormatCatalogMessage(&chain, 6, nullptr, 0, &output, nullptr, nullptr) ==
                  mloc_errorInvalid &&
              mlocFormatCatalogMessage(&zero, 0, nullptr, 0, &output, nullptr, nullptr) ==
                  mloc_errorInvalid &&
              mlocFormatCatalogMessage(&chain, 0, nullptr, 0, nullptr, nullptr, nullptr) ==
                  mloc_errorInvalid &&
              mlocFormatCatalogKey(&chain, nullptr, 1, nullptr, 0, &output, nullptr, nullptr) ==
                  mloc_errorInvalid &&
              mlocFormatCatalogKey(&zero, "a", 1, nullptr, 0, &output, nullptr, nullptr) ==
                  mloc_errorInvalid &&
              mlocBindCatalogChain(&zero) == mloc_errorInvalid &&
              mlocBindCatalogChain(nullptr) == mloc_errorInvalid,
          "an index past the keys, a chain not made, null pointers");
    free(cache);
}

static void TestRefusals(Set* s)
{
    uint8_t* bytes = nullptr;
    Set other = *s;
    bool opened = OpenOne("de", &bytes, &other.catalogs[1], &other.formatters[1]);
    uint8_t cache[256];
    size_t size = 0;
    mlocCatalogChain chain;
    mlocCatalog zero;
    memset(&zero, 0, sizeof(zero));
    CHECK(opened &&
              mlocInitCatalogChain(&chain, other.catalogs, other.formatters, 2, cache,
                                   sizeof(cache)) == mloc_errorInvalid &&
              mlocMeasureCatalogChain(other.catalogs, other.formatters, 2, &size) ==
                  mloc_errorInvalid,
          "catalogs of different sets");
    CHECK(mlocInitCatalogChain(&chain, s->catalogs, s->formatters, 0, cache, sizeof(cache)) ==
                  mloc_errorInvalid &&
              mlocInitCatalogChain(&chain, s->catalogs, s->formatters, MLOC_MAX_CATALOG_CHAIN + 1,
                                   cache, sizeof(cache)) == mloc_errorLimit &&
              mlocInitCatalogChain(nullptr, s->catalogs, s->formatters, 1, cache, sizeof(cache)) ==
                  mloc_errorInvalid &&
              mlocInitCatalogChain(&chain, nullptr, s->formatters, 1, cache, sizeof(cache)) ==
                  mloc_errorInvalid &&
              mlocInitCatalogChain(&chain, s->catalogs, nullptr, 1, cache, sizeof(cache)) ==
                  mloc_errorInvalid &&
              mlocInitCatalogChain(&chain, s->catalogs, s->formatters, 1, nullptr, 0) ==
                  mloc_errorInvalid &&
              mlocInitCatalogChain(&chain, &zero, s->formatters, 1, cache, sizeof(cache)) ==
                  mloc_errorInvalid &&
              mlocMeasureCatalogChain(s->catalogs, s->formatters, 1, nullptr) == mloc_errorInvalid,
          "counts out of range, null pointers, a catalog not opened");
    free(bytes);
}

int main(int argc, char** argv)
{
    s_directory = argc > 1 ? argv[1] : ".";
    Set set;
    memset(&set, 0, sizeof(set));
    if (!Compile() || !OpenSet(&set))
    {
        CHECK(false, "the set compiles and opens");
        CloseSet(&set);
        return 1;
    }
    TestReasons(&set);
    TestUnbound(&set);
    TestWithoutSource(&set);
    TestLongest(&set);
    TestCacheFull(&set);
    TestOutputs(&set);
    TestRefusals(&set);
    CloseSet(&set);
    return s_failures == 0 ? 0 : 1;
}
