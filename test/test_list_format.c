// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The list formatter (maul-locale/list.h): lists of none to four items
// in each type and width, ICU's contextual changes through the public
// call, the parts (items a pattern puts side by side stay two), the
// output protocol and every refusal. The joins themselves agree with
// ICU4C 79 on every compiled-in locale and style (test_list).

#include "test_harness.h"

#include "maul-locale/list.h"

#include <stdio.h>
#include <string.h>

static mlocData s_builtin;
static mlocDataSet s_data;

static bool Make(const char* id, mlocListType type, mlocListWidth width, mlocListFormatter* out)
{
    mlocLocale locale;
    mlocListOptions options = {.type = type, .width = width};
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocInitListFormatter(out, &s_data, &locale, &options) == mloc_success;
}

// Whether items (a NUL-separated run ended by an empty one, count given)
// join as expected.
static bool Joins(const char* id, mlocListType type, mlocListWidth width, const char* const* texts,
                  size_t count, const char* expected)
{
    mlocListFormatter formatter;
    mlocListItem items[8];
    for (size_t i = 0; i < count; i++)
    {
        items[i] = (mlocListItem){texts[i], strlen(texts[i])};
    }
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    bool same = Make(id, type, width, &formatter) &&
                mlocFormatList(&formatter, items, count, &output) == mloc_success &&
                output.length == strlen(expected) && memcmp(buffer, expected, output.length) == 0;
    if (!same)
    {
        printf("%s %d %d: \"%.*s\"\n", id, (int)type, (int)width, (int)output.length, buffer);
    }
    return same;
}

static void TestJoins(void)
{
    static const char* const s_abcd[] = {"a", "b", "c", "d"};
    CHECK(Joins("en", mloc_listAnd, mloc_listWide, s_abcd, 0, "") &&
              Joins("en", mloc_listAnd, mloc_listWide, s_abcd, 1, "a") &&
              Joins("en", mloc_listAnd, mloc_listWide, s_abcd, 2, "a and b") &&
              Joins("en", mloc_listAnd, mloc_listWide, s_abcd, 3, "a, b, and c") &&
              Joins("en", mloc_listAnd, mloc_listWide, s_abcd, 4, "a, b, c, and d"),
          "none to four items");
    CHECK(Joins("en", mloc_listAnd, mloc_listShort, s_abcd, 3, "a, b, & c") &&
              Joins("en", mloc_listAnd, mloc_listNarrow, s_abcd, 3, "a, b, c") &&
              Joins("en", mloc_listOr, mloc_listWide, s_abcd, 3, "a, b, or c") &&
              Joins("en", mloc_listUnit, mloc_listWide, s_abcd, 3, "a, b, c") &&
              Joins("en", mloc_listUnit, mloc_listNarrow, s_abcd, 3, "a b c") &&
              Joins("en-GB", mloc_listAnd, mloc_listWide, s_abcd, 3, "a, b and c"),
          "each type and width, a region's own patterns");
    static const char* const s_ice[] = {"agua", "iglesia"};
    static const char* const s_eight[] = {"siete", "ocho"};
    static const char* const s_latin[] = {"\xD7\x90", "Paris"};
    CHECK(Joins("es", mloc_listAnd, mloc_listWide, s_ice, 2, "agua e iglesia") &&
              Joins("es", mloc_listOr, mloc_listWide, s_eight, 2, "siete u ocho") &&
              Joins("he", mloc_listAnd, mloc_listWide, s_latin, 2, "\xD7\x90 \xD7\x95-Paris"),
          "ICU's contextual changes");
}

typedef struct Parts
{
    uint32_t kind[16];
    size_t start[16];
    size_t length[16];
    int count;
} Parts;

static void Collect(void* context, uint32_t part, size_t start, size_t length)
{
    Parts* p = context;
    if (p->count < 16)
    {
        p->kind[p->count] = part;
        p->start[p->count] = start;
        p->length[p->count] = length;
    }
    p->count++;
}

static bool HasParts(const Parts* p, int count, const uint32_t* kind, const size_t* start,
                     const size_t* length)
{
    bool same = p->count == count;
    for (int i = 0; same && i < count; i++)
    {
        same = p->kind[i] == kind[i] && p->start[i] == start[i] && p->length[i] == length[i];
    }
    return same;
}

static void TestParts(void)
{
    enum
    {
        e = mloc_listPartElement,
        l = mloc_listPartLiteral,
    };
    mlocListFormatter en;
    mlocListFormatter ja;
    mlocListItem abc[] = {{"a", 1}, {"b", 1}, {"c", 1}};
    char buffer[64];
    Parts p = {0};
    mlocOutput output = {
        .buffer = buffer, .capacity = sizeof(buffer), .part = Collect, .context = &p};
    static const uint32_t s_kinds[] = {e, l, e, l, e};
    static const size_t s_starts[] = {0, 1, 3, 4, 10};
    static const size_t s_lengths[] = {1, 2, 1, 6, 1};
    CHECK(Make("en", mloc_listAnd, mloc_listWide, &en) &&
              mlocFormatList(&en, abc, 3, &output) == mloc_success &&
              HasParts(&p, 5, s_kinds, s_starts, s_lengths),
          "items and literals as parts");
    // Japanese joins unit amounts in the narrow width with no text
    // between them: the two items stay two parts.
    mlocListItem amounts[] = {{"1", 1}, {"2", 1}};
    Parts q = {0};
    mlocOutput side = {
        .buffer = buffer, .capacity = sizeof(buffer), .part = Collect, .context = &q};
    static const uint32_t s_two[] = {e, e};
    static const size_t s_twoStarts[] = {0, 1};
    static const size_t s_ones[] = {1, 1};
    CHECK(Make("ja", mloc_listUnit, mloc_listNarrow, &ja) &&
              mlocFormatList(&ja, amounts, 2, &side) == mloc_success && side.length == 2 &&
              HasParts(&q, 2, s_two, s_twoStarts, s_ones),
          "items side by side, two parts");
    // An empty item has no part; no items, no parts.
    mlocListItem empty[] = {{"", 0}, {"b", 1}};
    Parts r = {0};
    mlocOutput lone = {
        .buffer = buffer, .capacity = sizeof(buffer), .part = Collect, .context = &r};
    static const uint32_t s_lit[] = {l, e};
    static const size_t s_litStarts[] = {0, 5};
    static const size_t s_litLengths[] = {5, 1};
    Parts none = {0};
    mlocOutput nothing = {
        .buffer = buffer, .capacity = sizeof(buffer), .part = Collect, .context = &none};
    CHECK(mlocFormatList(&en, empty, 2, &lone) == mloc_success && lone.length == 6 &&
              HasParts(&r, 2, s_lit, s_litStarts, s_litLengths) &&
              mlocFormatList(&en, nullptr, 0, &nothing) == mloc_success && nothing.length == 0 &&
              none.count == 0,
          "an empty item, no items");
}

static void TestOutput(void)
{
    mlocListFormatter en;
    mlocListItem abc[] = {{"a", 1}, {"b", 1}, {"c", 1}};
    char small[4];
    Parts p = {0};
    mlocOutput tight = {.buffer = small, .capacity = sizeof(small), .part = Collect, .context = &p};
    mlocOutput measure = {0};
    CHECK(Make("en", mloc_listAnd, mloc_listWide, &en) &&
              mlocFormatList(&en, abc, 3, &tight) == mloc_errorCapacity && tight.length == 11 &&
              p.count == 0 && mlocFormatList(&en, abc, 3, &measure) == mloc_errorCapacity &&
              measure.length == 11,
          "a buffer too small or none gives the size needed, and no parts");
}

static void TestRefusals(void)
{
    mlocLocale locale;
    mlocListFormatter formatter;
    mlocListOptions badType = {.type = (mlocListType)3};
    mlocListOptions badWidth = {.width = (mlocListWidth)3};
    mlocDataSet empty = {nullptr, 0};
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocInitListFormatter(nullptr, &s_data, &locale, nullptr) == mloc_errorInvalid &&
              mlocInitListFormatter(&formatter, nullptr, &locale, nullptr) == mloc_errorInvalid &&
              mlocInitListFormatter(&formatter, &s_data, nullptr, nullptr) == mloc_errorInvalid &&
              mlocInitListFormatter(&formatter, &s_data, &locale, &badType) == mloc_errorInvalid &&
              mlocInitListFormatter(&formatter, &s_data, &locale, &badWidth) == mloc_errorInvalid &&
              mlocInitListFormatter(&formatter, &empty, &locale, nullptr) == mloc_errorNotFound,
          "null pointers, options out of range, data without lists");
    char buffer[16];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    mlocOutput noBuffer = {.capacity = 4};
    mlocListItem items[] = {{"a", 1}, {nullptr, 1}};
    mlocListFormatter forged;
    memset(&forged, 0, sizeof(forged));
    CHECK(mlocInitListFormatter(&formatter, &s_data, &locale, nullptr) == mloc_success &&
              mlocFormatList(&forged, items, 1, &output) == mloc_errorInvalid &&
              mlocFormatList(nullptr, items, 1, &output) == mloc_errorInvalid &&
              mlocFormatList(&formatter, items, 1, nullptr) == mloc_errorInvalid &&
              mlocFormatList(&formatter, items, 1, &noBuffer) == mloc_errorInvalid &&
              mlocFormatList(&formatter, nullptr, 1, &output) == mloc_errorInvalid &&
              mlocFormatList(&formatter, items, 2, &output) == mloc_errorInvalid &&
              (SIZE_MAX == UINT32_MAX || mlocFormatList(&formatter, items, (size_t)UINT32_MAX + 1,
                                                        &output) == mloc_errorInvalid),
          "a forged formatter, null pointers, an item without text, a count past 32 bits");
}

int main(void)
{
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    TestJoins();
    TestParts();
    TestOutput();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
