// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailoring builder (record mloc-0071) on every rule string of
// CLDR's common/collation: each builds or is refused, with its imports
// read from the files beside it. --queries writes, for each type, its
// rule string and the strings its rules name (NFD, a prefix before its
// string), as the oracle reads them; --answers writes the elements the
// built tailoring gives each string, as the oracle writes ICU4C's.
// Alone it prints how many types built and a hash of the answers; with
// --expect it fails unless the hash is the given one, which CI checks on
// the pinned release (the answers ICU4C 79 gives, but for Han's root
// order in the unihan types and the quaternary bits its iterator drops).
//
// usage: conform_collation_builder <FractionalUCA.txt> <common/collation>
//            [--queries | --answers | --expect HASH]

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_builder.h"
#include "collation_rules.h"
#include "xml.h"

#include "maul-unicode/normalize.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxTypes = 512,
    maxQuery = 64,
};

// One type's rules, kept for imports.
typedef struct Type
{
    char file[64];
    char type[64];
    char alt[32];
    char* text;
    CollationRules rules;
    bool parsed;
} Type;

typedef struct Catalog
{
    Type types[maxTypes];
    size_t count;
} Catalog;

static int ByName(const void* a, const void* b)
{
    return strcmp(*(char* const*)a, *(char* const*)b);
}

static void ReadFile(Catalog* c, const char* directory, const char* name)
{
    char path[1024];
    char error[256] = "";
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    XmlNode* root = XmlParseFile(path, error, sizeof(error));
    XmlNode* collations = root == nullptr ? nullptr : XmlFindChild(root, "collations");
    for (int i = 0; collations != nullptr && i < collations->childCount && c->count < maxTypes; i++)
    {
        const XmlNode* collation = collations->children[i];
        const XmlNode* cr =
            strcmp(collation->name, "collation") == 0 ? XmlFindChild(collation, "cr") : nullptr;
        if (cr == nullptr)
        {
            continue;
        }
        Type* t = &c->types[c->count++];
        const char* type = XmlGetAttribute(collation, "type");
        const char* alt = XmlGetAttribute(collation, "alt");
        snprintf(t->file, sizeof(t->file), "%.*s", (int)(strlen(name) - 4), name);
        snprintf(t->type, sizeof(t->type), "%s", type == nullptr ? "standard" : type);
        snprintf(t->alt, sizeof(t->alt), "%s", alt == nullptr ? "" : alt);
        t->text = strdup(cr->text);
        t->parsed = t->text != nullptr &&
                    CollationRulesParse(t->text, strlen(t->text), &t->rules, error, sizeof(error));
    }
    XmlFree(root);
}

static void ReadCatalog(Catalog* c, const char* directory)
{
    DIR* d = opendir(directory);
    char* names[1024];
    size_t count = 0;
    for (struct dirent* e = d == nullptr ? nullptr : readdir(d); e != nullptr && count < 1024;
         e = readdir(d))
    {
        size_t n = strlen(e->d_name);
        if (n > 4 && strcmp(e->d_name + n - 4, ".xml") == 0)
        {
            names[count++] = strdup(e->d_name);
        }
    }
    if (d != nullptr)
    {
        closedir(d);
    }
    qsort((void*)names, count, sizeof(names[0]), ByName);
    for (size_t i = 0; i < count; i++)
    {
        ReadFile(c, directory, names[i]);
        free(names[i]);
    }
}

// [import tag]: the language's file (und is root) and the -u-co- type,
// its BCP 47 values spelled as CLDR's files spell them.
static const CollationRules* Import(void* context, const char* tag)
{
    const Catalog* c = context;
    char language[64];
    char type[64] = "standard";
    const char* u = strstr(tag, "-u-co-");
    snprintf(language, sizeof(language), "%.*s",
             (int)(u == nullptr ? strlen(tag) : (size_t)(u - tag)), tag);
    if (u != nullptr)
    {
        static const char* const aliases[][2] = {{"phonebk", "phonebook"},
                                                 {"trad", "traditional"},
                                                 {"dict", "dictionary"},
                                                 {"gb2312", "gb2312han"}};
        snprintf(type, sizeof(type), "%s", u + 6);
        for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++)
        {
            if (strcmp(type, aliases[i][0]) == 0)
            {
                snprintf(type, sizeof(type), "%s", aliases[i][1]);
            }
        }
    }
    if (strcmp(language, "und") == 0)
    {
        snprintf(language, sizeof(language), "root");
    }
    for (char* p = language; *p != '\0'; p++)
    {
        *p = *p == '-' ? '_' : *p;
    }
    for (size_t i = 0; i < c->count; i++)
    {
        const Type* t = &c->types[i];
        if (t->parsed && t->alt[0] == '\0' && strcmp(t->file, language) == 0 &&
            strcmp(t->type, type) == 0)
        {
            return &t->rules;
        }
    }
    return nullptr;
}

// NFD of code points, through UTF-8; 0 when too long.
static size_t Nfd(const uint32_t* text, size_t length, uint32_t* out)
{
    char utf8[maxQuery * 4];
    size_t n = 0;
    for (size_t i = 0; i < length && n + 4 <= sizeof(utf8); i++)
    {
        uint32_t c = text[i];
        if (c < 0x80)
        {
            utf8[n++] = (char)c;
        }
        else if (c < 0x800)
        {
            utf8[n++] = (char)(0xC0 | c >> 6);
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            utf8[n++] = (char)(0xE0 | c >> 12);
            utf8[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            utf8[n++] = (char)(0xF0 | c >> 18);
            utf8[n++] = (char)(0x80 | (c >> 12 & 0x3F));
            utf8[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    char nfd[maxQuery * 4 * 3];
    size_t needed = 0;
    muniTextResult r =
        muniNormalize(utf8, n, muni_nfd, muni_convertReplace, nfd, sizeof(nfd), &needed);
    size_t count = 0;
    for (size_t i = 0; r.status == muni_success && i < needed && count < maxQuery;)
    {
        unsigned char lead = (unsigned char)nfd[i];
        size_t extra = lead < 0x80 ? 0 : lead < 0xE0 ? 1 : lead < 0xF0 ? 2 : 3;
        uint32_t c = extra == 0 ? lead : lead & (0x3Fu >> extra);
        for (size_t k = 1; k <= extra; k++)
        {
            c = c << 6 | ((unsigned char)nfd[i + k] & 0x3F);
        }
        out[count++] = c;
        i += 1 + extra;
    }
    return count;
}

// A hash of every answer (hash = hash * 1000003 + value, 64-bit), and
// whether the answers are written.
static uint64_t s_hash;
static bool s_print;

// Each string the type's own rules name: a reset's, a relation's prefix
// and string.
static void Queries(const Type* t, CollationMappings* m, bool answers)
{
    const CollationRules* r = &t->rules;
    for (size_t i = 0; i < r->count; i++)
    {
        const CollationRule* rule = &r->rules[i];
        if (rule->kind == ruleImport || rule->textLength == 0)
        {
            continue;
        }
        uint32_t raw[maxQuery];
        size_t length = 0;
        for (uint32_t k = 0; k < rule->prefixLength && length < maxQuery; k++)
        {
            raw[length++] = r->points[rule->prefix + k];
        }
        for (uint32_t k = 0; k < rule->textLength && length < maxQuery; k++)
        {
            raw[length++] = r->points[rule->text + k];
        }
        uint32_t nfd[maxQuery];
        size_t n = Nfd(raw, length, nfd);
        if (n == 0)
        {
            continue;
        }
        if (!answers)
        {
            printf("q");
            for (size_t k = 0; k < n; k++)
            {
                printf(" %04X", nfd[k]);
            }
            printf("\n");
            continue;
        }
        uint64_t out[collationMaxExpansion];
        uint32_t count = CollationMappingsElements(m, nfd, (uint32_t)n, 0, out, 0);
        s_hash = s_hash * 1000003U + count;
        for (uint32_t k = 0; k < count && k < collationMaxExpansion; k++)
        {
            s_hash = s_hash * 1000003U + out[k];
        }
        if (!s_print)
        {
            continue;
        }
        printf("%u", count);
        for (uint32_t k = 0; k < count && k < collationMaxExpansion; k++)
        {
            printf(" %016llX", (unsigned long long)out[k]);
        }
        printf("\n");
    }
}

// One type built; its answers written or hashed, or its refusal.
static bool BuildType(const Type* t, const RootTable* root, Catalog* catalog, bool answers)
{
    char error[256] = "";
    CollationBuilder b;
    bool ok = CollationBuilderInit(&b, root, Import, catalog, error, sizeof(error)) &&
              CollationBuilderAdd(&b, &t->rules) && CollationBuilderFinish(&b);
    if (!ok && !answers)
    {
        printf("%s %s%s%s: %s\n", t->file, t->type, t->alt[0] == '\0' ? "" : " ", t->alt, error);
    }
    if (ok)
    {
        Queries(t, &b.mappings, true);
    }
    if (answers)
    {
        printf(ok ? "end\n" : "error\nend\n");
    }
    CollationBuilderFree(&b);
    return ok;
}

static void FreeCatalog(Catalog* catalog)
{
    for (size_t i = 0; i < catalog->count; i++)
    {
        free(catalog->types[i].text);
        if (catalog->types[i].parsed)
        {
            CollationRulesFree(&catalog->types[i].rules);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr,
                "usage: %s <FractionalUCA.txt> <common/collation> [--queries | --answers | "
                "--expect HASH]\n",
                argv[0]);
        return 2;
    }
    bool queries = argc > 3 && strcmp(argv[3], "--queries") == 0;
    bool answers = argc > 3 && strcmp(argv[3], "--answers") == 0;
    const char* expected = argc > 4 && strcmp(argv[3], "--expect") == 0 ? argv[4] : nullptr;
    s_print = answers;
    RootTable root = {0};
    char error[256] = "";
    if (!RootRead(argv[1], &root, error, sizeof(error)))
    {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    static Catalog catalog;
    ReadCatalog(&catalog, argv[2]);
    size_t built = 0;
    size_t refused = 0;
    for (size_t i = 0; i < catalog.count; i++)
    {
        const Type* t = &catalog.types[i];
        if (!t->parsed)
        {
            continue;
        }
        if (queries || answers)
        {
            printf("type %s %s%s%s\n", t->file, t->type, t->alt[0] == '\0' ? "" : " ", t->alt);
        }
        if (queries)
        {
            printf("rules %zu\n%s\n", strlen(t->text), t->text);
            Queries(t, nullptr, false);
            printf("end\n");
            continue;
        }
        bool ok = BuildType(t, &root, &catalog, answers);
        built += ok;
        refused += !ok;
    }
    char hash[32];
    snprintf(hash, sizeof(hash), "%016llx", (unsigned long long)s_hash);
    if (!queries && !answers)
    {
        printf("builder: %zu types built, %zu refused, answers %s\n", built, refused, hash);
    }
    if (expected != nullptr && strcmp(expected, hash) != 0)
    {
        fprintf(stderr, "expected answers %s\n", expected);
        refused++;
    }
    FreeCatalog(&catalog);
    RootFree(&root);
    return refused == 0 ? 0 : 1;
}
