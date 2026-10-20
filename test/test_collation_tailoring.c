// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A tailoring written as a unit and read back (record mloc-0071) on a
// small root: the runtime's iterator over the root and the unit gives
// the builder's elements for a contraction, a prefix, a prefix with
// contractions only falling back to a shorter one's list, and code
// points left to the root; the head holds the rules' settings and
// reorder codes; and the validator refuses a unit whose head, lists or
// sizes are wrong; a locale's unit of its types finds each by name, and
// its validator refuses names out of order or not co's, and tailorings
// out of place. conform_collation_builder --units checks every CLDR
// type.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_builder.h"
#include "collation_iterator.h"
#include "collation_root.h"
#include "collation_tailoring.h"
#include "collation_unit.h"
#include "test_harness.h"

#include "maul-unicode/properties.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_path = "collation_tailoring.txt";

static const char s_file[] = "[Unified_Ideograph 4E00..4E05]\n"
                             "0061; [2A 10, 05, 05]\n"
                             "0062; [2A 12, 05, 05]\n"
                             "0063; [2A 14, 05, 05]\n"
                             "0064; [2A 16, 05, 05]\n"
                             "0078; [2A 20, 05, 05]\n"
                             "0301; [, 88, 05]\n";

static RootTable s_root;
static CollationRoot s_rootUnit;
static uint8_t* s_rootBytes;

typedef struct Built
{
    CollationBuilder builder;
    CollationRules rules;
    uint8_t* bytes;
    size_t size;
    CollationTailoring unit;
} Built;

static bool Build(const char* text, Built* out)
{
    char error[256] = "";
    *out = (Built){0};
    if (!CollationRulesParse(text, strlen(text), &out->rules, error, sizeof(error)) ||
        !CollationBuilderInit(&out->builder, &s_root, nullptr, nullptr, error, sizeof(error)) ||
        !CollationBuilderAdd(&out->builder, &out->rules) || !CollationBuilderFinish(&out->builder))
    {
        fprintf(stderr, "  %s: %s\n", text, error);
        return false;
    }
    out->bytes = CollationTailoringUnit(&out->builder.mappings, &out->rules, &out->size, error,
                                        sizeof(error));
    return out->bytes != nullptr &&
           mlocReadCollationTailoring((DataSpan){out->bytes, (uint32_t)out->size}, &out->unit);
}

static void Free(Built* b)
{
    CollationBuilderFree(&b->builder);
    CollationRulesFree(&b->rules);
    free(b->bytes);
}

// Whether the runtime gives a string of the BMP the
// builder's elements.
static bool Same(Built* b, const uint32_t* text, size_t length)
{
    char utf8[64];
    size_t n = 0;
    for (size_t i = 0; i < length; i++)
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
        else
        {
            utf8[n++] = (char)(0xE0 | c >> 12);
            utf8[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    CollationIterator it;
    mlocCollationIteratorStart(&it, &s_rootUnit, (const uint8_t*)utf8, n);
    it.tailoring = &b->unit.view;
    uint64_t theirs[collationMaxExpansion];
    uint32_t count = 0;
    uint64_t e = 0;
    while (mlocCollationNextElement(&it, &e) && count < collationMaxExpansion)
    {
        if (e != 0)
        {
            theirs[count++] = e;
        }
    }
    uint64_t ours[collationMaxExpansion];
    uint32_t mine =
        CollationMappingsElements(&b->builder.mappings, text, (uint32_t)length, 0, ours, 0);
    return mine == count && memcmp(ours, theirs, count * sizeof *ours) == 0;
}

#define SAME(b, ...)                                                                               \
    Same(b, (const uint32_t[]){__VA_ARGS__}, sizeof((const uint32_t[]){__VA_ARGS__}) / 4)

static void TestElements(void)
{
    Built b;
    bool ok = Build("&a<xc &a<b|x &d<b|xd &a<cb|xd", &b);
    CHECK(ok, "the rules build and their unit reads back");
    CHECK(ok && SAME(&b, 'x') && SAME(&b, 'x', 'c') && SAME(&b, 'x', 'd') && SAME(&b, 'a'),
          "a code point alone, a contraction, one the tailoring leaves to the root");
    CHECK(ok && SAME(&b, 'b', 'x') && SAME(&b, 'b', 'x', 'd') && SAME(&b, 'a', 'x'),
          "a prefix and its contraction; no prefix before x");
    CHECK(ok && SAME(&b, 'c', 'b', 'x', 'd') && SAME(&b, 'c', 'b', 'x') &&
              SAME(&b, 'c', 'b', 'x', 'c'),
          "a prefix with contractions only falls back to the shorter prefix's list");
    CHECK(ok && SAME(&b, 'x', 0x301) && SAME(&b, 'x', 0x301, 'c') && SAME(&b, 0x4E01),
          "marks after, and Han from the root");
    CHECK(ok && mlocTrieValue(&b.unit.view.trie, 'q') == collationFallbackCe32 &&
              mlocTrieValue(&b.unit.view.trie, 0x10FFFF) == collationFallbackCe32,
          "a code point not tailored is the fallback word, the last one too");
    if (ok)
    {
        Free(&b);
    }
}

// The code points a unit's trie maps.
static uint32_t Mapped(const Built* b)
{
    uint32_t mapped = 0;
    for (uint32_t c = 0; c <= 0x10FFFF; c++)
    {
        mapped += mlocTrieValue(&b->unit.view.trie, c) != collationFallbackCe32;
    }
    return mapped;
}

static void TestMapped(void)
{
    Built b;
    bool ok = Build("&a<xc &a<b|x &d<b|xd &a<cb|xd", &b);
    CHECK(ok && Mapped(&b) == 1 && b.unit.mappedCount == 1,
          "the code points mapped: x alone, its contractions and prefixes in lists");
    if (ok)
    {
        Free(&b);
    }
    ok = Build("&a<x<y<z<w", &b);
    CHECK(ok && Mapped(&b) == 4 && b.unit.mappedCount == 4, "four code points mapped");
    if (ok)
    {
        Free(&b);
    }
    ok = Build("[reorder Grek][caseFirst upper]", &b);
    CHECK(ok && Mapped(&b) == 0 && b.unit.mappedCount == 0, "settings alone map none");
    if (ok)
    {
        Free(&b);
    }
}

static void TestHead(void)
{
    Built b;
    bool ok = Build("[strength 2][alternate shifted][caseFirst upper][backwards 2][reorder Grek "
                    "Latn]&a<x",
                    &b);
    CHECK(ok && b.unit.settings == (2 | 2 << 3 | 3 << 8 | 2 << 14 | collationReorderGiven) &&
              b.unit.reorderCount == 2 &&
              mlocLoad32(b.unit.reorder) == MUNI_SCRIPT('G', 'r', 'e', 'k') &&
              mlocLoad32(b.unit.reorder + 4) == MUNI_SCRIPT('L', 'a', 't', 'n'),
          "the rules' settings and reorder codes in the head");
    if (ok)
    {
        Free(&b);
    }
    ok = Build("&a<x", &b);
    CHECK(ok && b.unit.settings == 0 && b.unit.reorderCount == 0, "no settings, no codes");
    if (ok)
    {
        Free(&b);
    }
}

// Whether the unit, with one word changed, still reads.
static bool ReadsWith(const Built* b, size_t at, uint32_t word, size_t size)
{
    uint8_t* copy = malloc(b->size);
    if (copy == nullptr)
    {
        return false;
    }
    memcpy(copy, b->bytes, b->size);
    if (at + 4 <= b->size)
    {
        copy[at] = (uint8_t)word;
        copy[at + 1] = (uint8_t)(word >> 8);
        copy[at + 2] = (uint8_t)(word >> 16);
        copy[at + 3] = (uint8_t)(word >> 24);
    }
    CollationTailoring t;
    bool reads = mlocReadCollationTailoring((DataSpan){copy, (uint32_t)size}, &t);
    free(copy);
    return reads;
}

static void TestValidator(void)
{
    Built b;
    if (!Build("&a<xc &a<b|x", &b))
    {
        CHECK(false, "the rules build");
        return;
    }
    CHECK(ReadsWith(&b, 0, mlocLoad32(b.bytes), b.size), "the unit reads as written");
    CHECK(!ReadsWith(&b, 0, 0, collationHead - 1) && !ReadsWith(&b, 0, 0, b.size - 4),
          "a unit cut short");
    CHECK(!ReadsWith(&b, 28, 1, b.size), "a reserved word set");
    CHECK(ReadsWith(&b, 24, 0x110000, b.size) && !ReadsWith(&b, 24, 0x110001, b.size),
          "more code points mapped than there are");
    CHECK(!ReadsWith(&b, 20, 1, b.size), "a reorder code past the end");
    Free(&b);
}

static void TestValidatorSettings(void)
{
    Built b;
    if (!Build("&a<x", &b))
    {
        CHECK(false, "the rules build");
        return;
    }
    CHECK(!ReadsWith(&b, 16, 6, b.size) && !ReadsWith(&b, 16, 3 << 3, b.size) &&
              !ReadsWith(&b, 16, 5 << 5, b.size) && !ReadsWith(&b, 16, 3 << 10, b.size) &&
              !ReadsWith(&b, 16, 3 << 12, b.size) && !ReadsWith(&b, 16, 3 << 14, b.size) &&
              !ReadsWith(&b, 16, 1 << 17, b.size) &&
              ReadsWith(&b, 16,
                        5 | 2 << 3 | 4 << 5 | 3 << 8 | 2 << 10 | 2 << 12 | 2 << 14 | 1 << 16,
                        b.size),
          "a setting out of range");
    Free(&b);
}

static void TestValidatorLists(void)
{
    Built b;
    if (!Build("&a<xc &a<b|x", &b))
    {
        CHECK(false, "the rules build");
        return;
    }
    size_t contexts = (size_t)(b.unit.view.contexts - b.bytes);
    CHECK(!ReadsWith(&b, contexts, collationFallbackCe32, b.size) &&
              !ReadsWith(&b, contexts, collationSpecialByte | collationTagContraction, b.size) &&
              !ReadsWith(&b, contexts, collationSpecialByte | collationTagPrefix, b.size),
          "a list's default as the fallback word, its own list or a prefix");
    Free(&b);
}

// Whether a unit of types, with one word changed, still reads.
static bool TypesRead(const uint8_t* bytes, size_t size, size_t at, uint32_t word, size_t length)
{
    uint8_t* copy = malloc(size);
    if (copy == nullptr)
    {
        return false;
    }
    memcpy(copy, bytes, size);
    if (at + 4 <= size)
    {
        copy[at] = (uint8_t)word;
        copy[at + 1] = (uint8_t)(word >> 8);
        copy[at + 2] = (uint8_t)(word >> 16);
        copy[at + 3] = (uint8_t)(word >> 24);
    }
    bool reads = mlocValidCollationTypes(nullptr, (DataSpan){copy, (uint32_t)length});
    free(copy);
    return reads;
}

// The unit of TestTypes' two types, a word changed at a time.
static void TypesRefused(const uint8_t* unit, size_t size, size_t bSize)
{
    // The names after the head and two entries: "phonebk", "phonebk",
    // "search".
    size_t names = collationTypesHead + 2 * collationTypeSize;
    uint32_t first = mlocLoad32(unit + names + 7);
    uint32_t bodyAt = mlocLoad32(unit + collationTypesHead + 8);
    CHECK(!TypesRead(unit, size, 0, 0, size) && !TypesRead(unit, size, 0, 0x10000000, size) &&
              !TypesRead(unit, size, 8, 1, size) && !TypesRead(unit, size, 12, 1, size) &&
              !TypesRead(unit, size, 0, 2, size - 8) && !TypesRead(unit, size, 0, 2, 8),
          "no types and no default, too many types, a reserved word, cut short");
    CHECK(!TypesRead(unit, size, names + 7, first ^ 0x20, size) &&
              !TypesRead(unit, size, names + 7, first ^ 0x0E, size) &&
              !TypesRead(unit, size, collationTypesHead + 4, 8, size) &&
              !TypesRead(unit, size, 4, 33, size),
          "a name not co's, names out of order, overlapping or overlong");
    CHECK(!TypesRead(unit, size, collationTypesHead + 8, bodyAt + 8, size) &&
              !TypesRead(unit, size, collationTypesHead + 12, (uint32_t)bSize + 8, size) &&
              !TypesRead(unit, size, bodyAt + 28, 1, size),
          "a tailoring out of place, too long, or not one");
}

static void TestTypes(void)
{
    Built a;
    Built b;
    bool built = Build("&a<x", &a);
    built = Build("&b<y", &b) && built;
    CHECK(built, "two types build");
    if (!built)
    {
        return;
    }
    CollationTypeBody types[] = {{"search", a.bytes, a.size}, {"phonebk", b.bytes, b.size}};
    size_t size = 0;
    uint8_t* unit = CollationTypesUnit(types, 2, "phonebk", &size);
    DataSpan span = {unit, (uint32_t)size};
    CollationTypes t = mlocReadCollationTypes(span);
    DataSpan found = {0};
    CHECK(mlocValidCollationTypes(nullptr, span) && t.count == 2 && t.defaultLength == 7 &&
              memcmp(t.defaultName, "phonebk", 7) == 0,
          "the unit reads, its default named");
    CHECK(mlocFindCollationType(&t, "search", 6, &found) && found.size == a.size &&
              memcmp(found.bytes, a.bytes, a.size) == 0 &&
              mlocFindCollationType(&t, "phonebk", 7, &found) && found.size == b.size &&
              !mlocFindCollationType(&t, "phone", 5, &found) &&
              !mlocFindCollationType(&t, "standard", 8, &found),
          "a type found by its name, none for another");
    TypesRefused(unit, size, b.size);
    free(unit);
    uint8_t alone[24] = {0, 0, 0, 0, 6, 0, 0, 0};
    memcpy(alone + collationTypesHead, "stroke\0\0", 8);
    CHECK(mlocValidCollationTypes(nullptr, (DataSpan){alone, 24}),
          "a default alone, as zh_Hant's unit holds");
    alone[4] = 7;
    CHECK(!mlocValidCollationTypes(nullptr, (DataSpan){alone, 24}), "a NUL in its name");
    alone[4] = 0;
    CHECK(!mlocValidCollationTypes(nullptr, (DataSpan){alone, 24}), "an empty unit");
    Free(&a);
    Free(&b);
}

int main(void)
{
    FILE* f = fopen(s_path, "wb");
    if (f == nullptr)
    {
        return 1;
    }
    fputs(s_file, f);
    fclose(f);
    char error[256] = "";
    size_t size = 0;
    bool read = RootRead(s_path, &s_root, error, sizeof(error));
    s_rootBytes = read ? CollationRootUnit(&s_root, &size, error, sizeof(error)) : nullptr;
    if (s_rootBytes == nullptr ||
        !mlocReadCollationRoot((DataSpan){s_rootBytes, (uint32_t)size}, &s_rootUnit))
    {
        CHECK(false, error);
        return 1;
    }
    TestElements();
    TestMapped();
    TestHead();
    TestValidator();
    TestValidatorSettings();
    TestValidatorLists();
    TestTypes();
    free(s_rootBytes);
    RootFree(&s_root);
    remove(s_path);
    return s_failures == 0 ? 0 : 1;
}
