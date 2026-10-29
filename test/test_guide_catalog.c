// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The catalog snippet of docs/user/catalogs.md, as written there
// (tools/check_guide.py checks it): the page's source and two
// translations compiled by the catalog tool into the directory given,
// opened, and formatted through a chain.

#include "compile.h"
#include "file.h"
#include "test_harness.h"

#include "maul-locale/catalog.h"
#include "maul-locale/message.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_directory;

// The catalogs' bytes, which the data opened from them reads until the end.
static void* s_bytes[3];
static size_t s_byteCount;

// The source of the page, a French translation, and a Canadian French
// one that has not translated the key yet.
static const char s_source[] = "@locale en\n"
                               "---\n"
                               "# Shown in the toolbar.\n"
                               "@param $count the number of files selected\n"
                               "files.count =\n"
                               "  .input {$count :integer}\n"
                               "  .match $count\n"
                               "  one {{{$count} file}}\n"
                               "  *   {{{$count} files}}\n"
                               "\n"
                               "[dialog]\n"
                               "@mloc-max-length 24\n"
                               "save = Save changes?\n";
static const char s_french[] = "@locale fr\n"
                               "---\n"
                               "files.count =\n"
                               "  .input {$count :integer}\n"
                               "  .match $count\n"
                               "  one {{{$count} fichier}}\n"
                               "  *   {{{$count} fichiers}}\n"
                               "\n"
                               "[dialog]\n"
                               "save = Enregistrer les modifications ?\n";
static const char s_canadian[] = "@locale fr-CA\n"
                                 "---\n"
                                 "[dialog]\n"
                                 "save = Enregistrer les changements?\n";

static bool Write(const char* name, const char* text, char* path, size_t capacity)
{
    snprintf(path, capacity, "%s/%s", s_directory, name);
    return FileWrite(path, text, strlen(text)) != 0;
}

static bool Compile(void)
{
    char paths[3][1024];
    if (!Write("en.mf2", s_source, paths[0], sizeof(paths[0])) ||
        !Write("fr.mf2", s_french, paths[1], sizeof(paths[1])) ||
        !Write("fr-CA.mf2", s_canadian, paths[2], sizeof(paths[2])))
    {
        return false;
    }
    char* set[] = {paths[0], paths[1], paths[2]};
    FILE* report = tmpfile();
    bool ok = CatalogCompile(s_directory, set, 3, report != nullptr ? report : stderr) != 0;
    if (report != nullptr)
    {
        fclose(report);
    }
    return ok;
}

// A compiled catalog of the directory and a formatter of its locale.
static bool Open(const char* locale, mlocCatalog* catalog, mlocMessageFormatter* formatter)
{
    static mlocData s_builtin;
    static mlocDataSet s_set;
    mlocOpenBuiltinData(&s_builtin);
    s_set = (mlocDataSet){&s_builtin, 1};
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.mlct", s_directory, locale);
    size_t size = 0;
    void* bytes = FileRead(path, &size);
    if (bytes != nullptr && s_byteCount < 3)
    {
        s_bytes[s_byteCount++] = bytes;
    }
    mlocLocale parsed;
    return bytes != nullptr && mlocOpenCatalog(bytes, size, catalog) == mloc_success &&
           mlocParseLocale(locale, strlen(locale), &parsed).status == mloc_success &&
           mlocInitMessageFormatter(&s_set, &parsed, nullptr, formatter) == mloc_success;
}

static void GuideChain(void)
{
    mlocCatalog catalogs[3];
    mlocMessageFormatter formatters[3];
    const char* locales[3] = {"fr-CA", "fr", "en"};
    for (int i = 0; i < 3; i++)
    {
        CHECK(Open(locales[i], &catalogs[i], &formatters[i]), locales[i]);
    }
    // catalogs: fr-CA, fr and the source en, opened in that order;
    // formatters: a message formatter of each catalog's locale
    size_t cacheSize = 0;
    mlocCatalogChain chain;
    mlocResult measured = mlocMeasureCatalogChain(catalogs, formatters, 3, &cacheSize);
    void* cache = measured == mloc_success ? malloc(cacheSize) : nullptr;
    if (cache != nullptr &&
        mlocInitCatalogChain(catalogs, formatters, 3, cache, cacheSize, &chain) == mloc_success &&
        mlocCatalogChain_Bind(&chain) == mloc_success)
    {
        mlocMessageArgument count = {
            "count", 5, {.kind = mloc_messageValueInteger, .as.integer = 2}};
        char text[128];
        mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
        mlocCatalogResult how;
        mlocResult formatted = mlocCatalogChain_FormatKey(&chain, "files.count", 11, &count, 1,
                                                          &output, nullptr, &how);
        // formatted is mloc_success; text holds "2 fichiers" from fr,
        // how.reason is mloc_catalogFallback: fr-CA has no translation of its own
        (void)formatted;
    }
    CHECK(cache != nullptr, "the chain's cache");
    mlocMessageArgument count = {"count", 5, {.kind = mloc_messageValueInteger, .as.integer = 2}};
    char text[128];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocCatalogResult how = {0};
    CHECK(mlocCatalogChain_FormatKey(&chain, "files.count", 11, &count, 1, &output, nullptr,
                                     &how) == mloc_success &&
              output.length == 10 && memcmp(text, "2 fichiers", 10) == 0,
          "the French message");
    CHECK(how.reason == mloc_catalogFallback && how.position == 1, "found in fr");
    output.length = 0;
    CHECK(mlocCatalogChain_FormatKey(&chain, "dialog.save", 11, nullptr, 0, &output, nullptr,
                                     &how) == mloc_success &&
              how.reason == mloc_catalogFound && how.position == 0,
          "fr-CA's own translation");
    free(cache);
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("usage: test_guide_catalog DIRECTORY\n");
        return 1;
    }
    s_directory = argv[1];
    CHECK(Compile(), "the set compiles");
    GuideChain();
    for (size_t i = 0; i < s_byteCount; i++)
    {
        free(s_bytes[i]);
    }
    return s_failures == 0 ? 0 : 1;
}
