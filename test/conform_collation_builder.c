// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailoring builder (record mloc-0071) on every rule string of
// CLDR's common/collation: each builds or is refused, with its imports
// read from the files given. --queries writes, for each type, its
// rule string and the strings its rules name (NFD, a prefix before its
// string), as the oracle reads them; --answers writes the elements the
// built tailoring gives each string, as the oracle writes ICU4C's.
// --units writes each built tailoring as a unit, reads it back and checks
// its count of mapped code points and that the runtime's iterator over the root and it gives the
// builder's elements for those strings and 2,000 random ones of its mappings and marks. Alone it
// prints how many types built and a hash of the answers (a refusal counted too), failing on a
// refusal; with --expect it fails unless the hash is the given one, which CI checks on the pinned
// release (the answers ICU4C 79 gives, but for Han's root order in the unihan types and the
// quaternary bits its iterator drops).
//
// usage: conform_collation_builder <FractionalUCA.txt>
//            [--queries | --answers | --units | --expect HASH]
//            common/collation/*.xml

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_builder.h"
#include "collation_catalog.h"
#include "collation_iterator.h"
#include "collation_rules.h"
#include "collation_tailoring.h"
#include "collation_unit.h"

#include "maul-unicode/normalize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxQuery = 64,
};

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
static void Queries(const CollationType* t, CollationMappings* m, bool answers)
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

// --units: each tailoring written as a unit, read back, and the runtime's
// iterator over the root and it against the builder's lookup.
static CollationRoot s_root;
static size_t s_checked;
static size_t s_differ;
static size_t s_unreadable;
static uint32_t s_seed = 1;

static size_t Utf8(const uint32_t* text, size_t length, char* out)
{
    size_t n = 0;
    for (size_t i = 0; i < length; i++)
    {
        uint32_t c = text[i];
        if (c < 0x80)
        {
            out[n++] = (char)c;
        }
        else if (c < 0x800)
        {
            out[n++] = (char)(0xC0 | c >> 6);
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            out[n++] = (char)(0xE0 | c >> 12);
            out[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            out[n++] = (char)(0xF0 | c >> 18);
            out[n++] = (char)(0x80 | (c >> 12 & 0x3F));
            out[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    return n;
}

// One string (NFD) through both; a difference counted and shown.
static void Check(const CollationType* t, CollationMappings* m, const CollationTailoring* unit,
                  const uint32_t* text, size_t length)
{
    char utf8[maxQuery * 4];
    size_t size = Utf8(text, length, utf8);
    CollationIterator it;
    mlocCollationIteratorStart(&it, &s_root, (const uint8_t*)utf8, size);
    it.tailoring = &unit->view;
    uint64_t theirs[collationMaxExpansion];
    uint32_t count = 0;
    uint64_t e = 0;
    while (mlocCollationNextElement(&it, &e))
    {
        if (e != 0 && count < collationMaxExpansion)
        {
            theirs[count] = e;
        }
        count += e != 0;
    }
    uint64_t ours[collationMaxExpansion];
    uint32_t mine = CollationMappingsElements(m, text, (uint32_t)length, 0, ours, 0);
    uint32_t kept = count < collationMaxExpansion ? count : collationMaxExpansion;
    s_checked++;
    if (mine != count || memcmp(ours, theirs, kept * sizeof *ours) != 0)
    {
        if (s_differ++ < 10)
        {
            printf("  %s %s:", t->file, t->type);
            for (size_t i = 0; i < length; i++)
            {
                printf(" %04X", text[i]);
            }
            printf(" (%u elements against %u)\n", count, mine);
        }
    }
}

static uint32_t Random(uint32_t below)
{
    s_seed = s_seed * 1103515245U + 12345U;
    return (s_seed >> 8) % below;
}

// Random strings of the tailoring's own mappings and marks.
static void RandomChecks(const CollationType* t, CollationMappings* m,
                         const CollationTailoring* unit)
{
    static const uint32_t marks[] = {0x0300, 0x0301, 0x0302, 0x0308,
                                     0x0323, 0x0327, 0x0331, 0x3099};
    for (int s = 0; m->count != 0 && s < 2000; s++)
    {
        uint32_t text[maxQuery];
        size_t length = 0;
        for (uint32_t pieces = 1 + Random(4); pieces > 0; pieces--)
        {
            const CollationMapping* x = &m->mappings[Random((uint32_t)m->count)];
            for (uint32_t k = 0; k < x->prefixLength && length < maxQuery; k++)
            {
                text[length++] = x->prefix[k];
            }
            for (uint32_t k = 0; k < x->textLength && length < maxQuery; k++)
            {
                text[length++] = x->text[k];
            }
            if (Random(3) == 0 && length < maxQuery)
            {
                text[length++] = marks[Random(sizeof(marks) / sizeof(marks[0]))];
            }
        }
        uint32_t nfd[maxQuery];
        size_t n = Nfd(text, length, nfd);
        if (n != 0)
        {
            Check(t, m, unit, nfd, n);
        }
    }
}

// The type's unit written, read back and checked; false when it does not
// read.
static bool CheckUnit(const CollationType* t, CollationBuilder* b)
{
    char error[256] = "";
    size_t size = 0;
    CollationMappings* m = &b->mappings;
    uint8_t* bytes = CollationTailoringUnit(m, &b->settings, &size, error, sizeof(error));
    CollationTailoring unit;
    if (bytes == nullptr || !mlocReadCollationTailoring((DataSpan){bytes, (uint32_t)size}, &unit))
    {
        printf("%s %s: the unit %s\n", t->file, t->type,
               bytes == nullptr ? error : "does not read back");
        free(bytes);
        s_unreadable++;
        return false;
    }
    uint32_t mapped = 0;
    for (uint32_t c = 0; c <= 0x10FFFF; c++)
    {
        mapped += mlocTrieValue(&unit.view.trie, c) != collationFallbackCe32;
    }
    if (mapped != unit.mappedCount)
    {
        printf("%s %s: %u code points mapped, the head says %u\n", t->file, t->type, mapped,
               unit.mappedCount);
        s_differ++;
    }
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
        if (n != 0)
        {
            Check(t, m, &unit, nfd, n);
        }
    }
    RandomChecks(t, m, &unit);
    free(bytes);
    return true;
}

// One type built; its answers written or hashed, or its refusal.
static bool BuildType(const CollationType* t, const RootTable* root, CollationCatalog* catalog,
                      bool answers, bool units)
{
    char error[256] = "";
    CollationBuilder b;
    bool ok =
        CollationBuilderInit(&b, root, CollationCatalogImport, catalog, error, sizeof(error)) &&
        CollationBuilderAdd(&b, &t->rules) && CollationBuilderFinish(&b);
    if (!ok && !answers)
    {
        printf("%s %s%s%s: %s\n", t->file, t->type, t->alt[0] == '\0' ? "" : " ", t->alt, error);
    }
    if (ok && units)
    {
        ok = CheckUnit(t, &b);
    }
    else if (ok)
    {
        Queries(t, &b.mappings, true);
    }
    else
    {
        // A refusal counts in the hash, as the oracle's "error" does.
        s_hash = s_hash * 1000003U + 0xE770;
    }
    if (answers)
    {
        printf(ok ? "end\n" : "error\nend\n");
    }
    CollationBuilderFree(&b);
    return ok;
}

// Each parsed type's queries written, or the type built and checked as
// the mode asks; how many built and how many were refused.
static void RunTypes(CollationCatalog* catalog, const RootTable* root, bool queries, bool answers,
                     bool units, size_t* built, size_t* refused)
{
    for (size_t i = 0; i < catalog->count; i++)
    {
        const CollationType* t = &catalog->types[i];
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
        bool ok = BuildType(t, root, catalog, answers, units);
        *built += ok;
        *refused += !ok;
    }
}

// The files' types, sorted; false with a message for a file that does
// not read.
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
        fprintf(stderr,
                "usage: %s <FractionalUCA.txt> [--queries | --answers | --expect HASH] "
                "common/collation/*.xml\n",
                argv[0]);
        return 2;
    }
    int first = 2;
    bool queries = strcmp(argv[2], "--queries") == 0;
    bool answers = strcmp(argv[2], "--answers") == 0;
    bool units = strcmp(argv[2], "--units") == 0;
    const char* expected = argc > 3 && strcmp(argv[2], "--expect") == 0 ? argv[3] : nullptr;
    first += queries || answers || units ? 1 : expected != nullptr ? 2 : 0;
    s_print = answers;
    RootTable root = {0};
    char error[256] = "";
    if (!RootRead(argv[1], &root, error, sizeof(error)))
    {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    size_t rootSize = 0;
    uint8_t* rootUnit = units ? CollationRootUnit(&root, &rootSize, error, sizeof(error)) : nullptr;
    if (units && (rootUnit == nullptr ||
                  !mlocReadCollationRoot((DataSpan){rootUnit, (uint32_t)rootSize}, &s_root)))
    {
        fprintf(stderr, "the root unit: %s\n", rootUnit == nullptr ? error : "does not read");
        free(rootUnit);
        RootFree(&root);
        return 1;
    }
    CollationCatalog catalog = {0};
    if (!ReadCatalog(&catalog, argv + first, argc - first))
    {
        CollationCatalogFree(&catalog);
        free(rootUnit);
        RootFree(&root);
        return 1;
    }
    size_t built = 0;
    size_t refused = 0;
    RunTypes(&catalog, &root, queries, answers, units, &built, &refused);
    char hash[32];
    snprintf(hash, sizeof(hash), "%016llx", (unsigned long long)s_hash);
    if (units)
    {
        printf("units: %zu types written and read back, %zu refused, %zu unreadable; %zu "
               "strings, %zu differ\n",
               built, refused - s_unreadable, s_unreadable, s_checked, s_differ);
    }
    free(rootUnit);
    if (!queries && !answers && !units)
    {
        printf("builder: %zu types built, %zu refused, answers %s\n", built, refused, hash);
    }
    bool mismatch = expected != nullptr && strcmp(expected, hash) != 0;
    if (mismatch)
    {
        fprintf(stderr, "expected answers %s\n", expected);
    }
    CollationCatalogFree(&catalog);
    RootFree(&root);
    return expected != nullptr ? mismatch : units ? s_unreadable + s_differ != 0 : refused != 0;
}
