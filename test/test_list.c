// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The list part's units and joins (list_data.h, list_join.h): the
// validators on units built here, valid ones passing and copies with one
// field spoiled refused; a locale's patterns found in the compiled-in
// data and refused from units that disagree; and items joined by
// patterns given here, ICU's contextual changes included. Every list the
// compiled-in data joins agrees with ICU4C 79 (tools/oracle/lists).

#include "kinds.h"
#include "list_data.h"
#include "list_join.h"
#include "number_tables.h"
#include "test_harness.h"
#include "test_pack.h"

#include <stdio.h>
#include <string.h>

static uint8_t s_unit[512];

// Builds a pattern sets unit of the strings and one set naming strings
// 0 to 3, or the indexes given; its size.
static uint32_t BuildSets(const char* const* strings, uint32_t count, const uint16_t* indexes)
{
    memset(s_unit, 0, sizeof(s_unit));
    uint32_t text = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        text += (uint32_t)strlen(strings[i]);
    }
    PackPut(s_unit, count, 2);
    PackPut(s_unit + 2, 1, 2);
    PackPut(s_unit + 4, text, 4);
    uint8_t* offsets = s_unit + numberTableHeaderSize;
    uint8_t* records = offsets + 2 * (count + 1);
    uint8_t* textAt = records + listSetSize;
    uint32_t at = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        PackPut(offsets + 2 * i, at, 2);
        memcpy(textAt + at, strings[i], strlen(strings[i]));
        at += (uint32_t)strlen(strings[i]);
    }
    PackPut(offsets + 2 * count, at, 2);
    for (uint32_t p = 0; p < listPatternCount; p++)
    {
        PackPut(records + 2 * p, indexes != nullptr ? indexes[p] : p, 2);
    }
    return numberTableHeaderSize + 2 * (count + 1) + listSetSize + text;
}

static bool ValidSets(const char* const* strings, uint32_t count, const uint16_t* indexes)
{
    uint32_t size = BuildSets(strings, count, indexes);
    return mlocValidListPatterns(nullptr, (DataSpan){s_unit, size});
}

static void TestValidators(void)
{
    static const char* const s_good[4] = {"{0} and {1}", "{0}, {1}", "{1} ; {0}", "x{0}{1}"};
    CHECK(ValidSets(s_good, 4, nullptr), "a set of patterns, the placeholders in either order");
    static const uint16_t s_past[4] = {0, 1, 2, 4};
    CHECK(!ValidSets(s_good, 4, s_past), "a set naming a string past the table");
    static const char* const s_twice[4] = {"{0} {0} {1}", "{0}, {1}", "{0}, {1}", "{0}, {1}"};
    static const char* const s_missing[4] = {"{0} and", "{0}, {1}", "{0}, {1}", "{0}, {1}"};
    static const char* const s_brace[4] = {"{0} {1} }", "{0}, {1}", "{0}, {1}", "{0}, {1}"};
    static const char* const s_other[4] = {"{0} {2}", "{0}, {1}", "{0}, {1}", "{0}, {1}"};
    static const char* const s_cut[4] = {"{0} {1", "{0}, {1}", "{0}, {1}", "{0}, {1}"};
    static const char* const s_noZero[4] = {"{1} {}", "{0}, {1}", "{0}, {1}", "{0}, {1}"};
    static const char* const s_noOne[4] = {"{0} {}", "{0}, {1}", "{0}, {1}", "{0}, {1}"};
    CHECK(!ValidSets(s_twice, 4, nullptr) && !ValidSets(s_missing, 4, nullptr) &&
              !ValidSets(s_brace, 4, nullptr) && !ValidSets(s_other, 4, nullptr) &&
              !ValidSets(s_cut, 4, nullptr) && !ValidSets(s_noZero, 4, nullptr) &&
              !ValidSets(s_noOne, 4, nullptr),
          "a placeholder twice, missing, another brace, another number, cut short");
    uint32_t size = BuildSets(s_good, 4, nullptr);
    CHECK(!mlocValidListPatterns(nullptr, (DataSpan){s_unit, size - 1}), "a table cut short");
    uint8_t lists[listLocaleSize + 1] = {0};
    CHECK(mlocValidLists(nullptr, (DataSpan){lists, listLocaleSize}) &&
              !mlocValidLists(nullptr, (DataSpan){lists, listLocaleSize - 1}) &&
              !mlocValidLists(nullptr, (DataSpan){lists, listLocaleSize + 1}),
          "a locale's lists of its size alone");
}

static bool Locale(const char* id, mlocLocale* out)
{
    return mlocParseLocale(id, strlen(id), out).status == mloc_success;
}

static bool Pattern(const ListPatterns* p, int which, const char* expected)
{
    return p->length[which] == strlen(expected) &&
           memcmp(p->text[which], expected, p->length[which]) == 0;
}

static void TestFind(void)
{
    static mlocData source;
    mlocOpenBuiltinData(&source);
    mlocDataSet data = {&source, 1};
    mlocLocale locale;
    ListPatterns p;
    CHECK(Locale("en", &locale) && mlocFindListPatterns(&data, &locale, listStandard, &p) &&
              Pattern(&p, listPatternTwo, "{0} and {1}") &&
              Pattern(&p, listPatternStart, "{0}, {1}") &&
              Pattern(&p, listPatternMiddle, "{0}, {1}") &&
              Pattern(&p, listPatternEnd, "{0}, and {1}") && p.rule == listRuleNone,
          "English's standard list");
    CHECK(mlocFindListPatterns(&data, &locale, listUnitNarrow, &p) &&
              Pattern(&p, listPatternTwo, "{0} {1}") && Pattern(&p, listPatternEnd, "{0} {1}"),
          "English's narrow unit list");
    CHECK(Locale("es_MX", &locale) && mlocFindListPatterns(&data, &locale, listOr, &p) &&
              p.rule == listRuleSpanish && Locale("he", &locale) &&
              mlocFindListPatterns(&data, &locale, listOr, &p) && p.rule == listRuleHebrew,
          "the languages with contextual patterns");
    static const char* const s_strings[4] = {"{0} a {1}", "{0} b {1}", "{0} c {1}", "{0} d {1}"};
    uint32_t size = BuildSets(s_strings, 4, nullptr);
    static uint8_t s_sets[256];
    memcpy(s_sets, s_unit, size);
    uint8_t lists[listLocaleSize] = {0};
    lists[2 * listOr] = 1;
    PackUnit units[2] = {
        {kindListPatterns, {s_sets, size}, false},
        {kindLists, {lists, sizeof(lists)}, true},
    };
    mlocData pack;
    mlocDataSet set = {&pack, 1};
    CHECK(OpenRootPack(units, 2, &pack) && Locale("root", &locale) &&
              mlocFindListPatterns(&set, &locale, listStandard, &p) &&
              Pattern(&p, listPatternMiddle, "{0} c {1}") &&
              !mlocFindListPatterns(&set, &locale, listOr, &p),
          "a set index past the sets unit");
    CHECK(OpenRootPack(units, 1, &pack) && !mlocFindListPatterns(&set, &locale, listStandard, &p),
          "data without lists");
}

static ListPatterns Patterns(const char* two, const char* start, const char* middle,
                             const char* end, ListRule rule)
{
    ListPatterns p = {.text = {two, start, middle, end}, .rule = rule};
    for (int i = 0; i < listPatternCount; i++)
    {
        p.length[i] = (uint16_t)strlen(p.text[i]);
    }
    return p;
}

// Joins items given as "a|b|c".
static bool Joins(const ListPatterns* p, const char* items, const char* expected)
{
    ListItem list[8];
    uint32_t count = 0;
    for (const char* at = items;;)
    {
        const char* bar = strchr(at, '|');
        list[count].text = at;
        list[count].length = bar != nullptr ? (size_t)(bar - at) : strlen(at);
        count++;
        if (bar == nullptr)
        {
            break;
        }
        at = bar + 1;
    }
    char out[256];
    NumberWriter writer = {.buffer = out, .capacity = sizeof(out)};
    mlocWriteList(p, list, count, nullptr, nullptr, &writer);
    return writer.length == strlen(expected) && memcmp(out, expected, writer.length) == 0;
}

static void TestJoin(void)
{
    ListPatterns p = Patterns("{0} & {1}", "{0}, {1}", "{0}; {1}", "{0}, & {1}", listRuleNone);
    CHECK(Joins(&p, "a", "a") && Joins(&p, "a|b", "a & b") && Joins(&p, "a|b|c", "a, b, & c") &&
              Joins(&p, "a|b|c|d|e", "a, b; c; d, & e"),
          "one item, two, and more");
    p = Patterns("<{0}|{1}>", "[{1}+{0}]", "({0}~{1})", "{1} {0}", listRuleNone);
    CHECK(Joins(&p, "a|b", "<a|b>") && Joins(&p, "a|b|c|d", "d ([b+a]~c)"),
          "patterns with text around and the items in either order");
    p = Patterns("{0} y {1}", "{0} y {1}", "{0} o {1}", "{0} o {1}", listRuleSpanish);
    CHECK(Joins(&p, "a|isla", "a e isla") && Joins(&p, "a|Isla", "a e Isla") &&
              Joins(&p, "a|hilo", "a e hilo") && Joins(&p, "a|HILO", "a e HILO") &&
              Joins(&p, "a|hi", "a e hi") && Joins(&p, "a|hiato", "a y hiato") &&
              Joins(&p, "a|hiedra", "a y hiedra") && Joins(&p, "a|HIA", "a y HIA") &&
              Joins(&p, "a|HIE", "a y HIE") && Joins(&p, "a|h", "a y h") &&
              Joins(&p, "a|", "a y ") && Joins(&p, "a|b", "a y b"),
          "Spanish's y before i and hi");
    CHECK(Joins(&p, "a|b|ola", "a y b u ola") && Joins(&p, "a|b|Ola", "a y b u Ola") &&
              Joins(&p, "a|b|hola", "a y b u hola") && Joins(&p, "a|b|HOLA", "a y b u HOLA") &&
              Joins(&p, "a|b|8", "a y b u 8") && Joins(&p, "a|b|11", "a y b u 11") &&
              Joins(&p, "a|b|11 x", "a y b u 11 x") && Joins(&p, "a|b|110", "a y b o 110") &&
              Joins(&p, "a|b|1", "a y b o 1") && Joins(&p, "a|b|h", "a y b o h") &&
              Joins(&p, "a|b|", "a y b o "),
          "Spanish's o before o, ho, 8 and 11");
    CHECK(Joins(&p, "a|isla|b", "a y isla o b") && Joins(&p, "a|b|isla|c", "a y b o isla o c"),
          "the start and middle patterns unchanged");
    p = Patterns("{0} \xD7\x95{1}", "{0}, {1}", "{0}, {1}", "{0} \xD7\x95{1}", listRuleHebrew);
    CHECK(Joins(&p, "a|\xD7\x91", "a \xD7\x95\xD7\x91") && Joins(&p, "a|b", "a \xD7\x95-b") &&
              Joins(&p, "a|\xEF\xAC\xA0", "a \xD7\x95\xEF\xAC\xA0") &&
              Joins(&p, "a|\xD7\xAA", "a \xD7\x95\xD7\xAA") &&
              Joins(&p, "a|\xF0\x9F\x98\x80", "a \xD7\x95-\xF0\x9F\x98\x80") &&
              Joins(&p, "a|\xC3\xA9", "a \xD7\x95-\xC3\xA9") && Joins(&p, "a|", "a \xD7\x95"),
          "Hebrew's vav takes a dash before a code point of another script");
    p = Patterns("{0} y {1}", "{0}, {1}", "{0}, {1}", "{0} y {1}", listRuleNone);
    CHECK(Joins(&p, "a|isla", "a y isla"), "no change without the language's rule");
}

typedef struct Calls
{
    uint32_t seen[8];
    uint32_t count;
} Calls;

static void WriteNumbered(void* context, uint32_t index, NumberWriter* writer)
{
    Calls* calls = context;
    calls->seen[calls->count++] = index;
    char text[4] = {(char)('0' + index)};
    mlocWriteLiteral(writer, text, 1);
}

static void TestWriter(void)
{
    ListPatterns p = Patterns("{0}+{1}", "{0}+{1}", "{0}+{1}", "{0}+{1}", listRuleNone);
    ListItem items[3] = {{"x", 1}, {"y", 1}, {"z", 1}};
    Calls calls = {0};
    char out[16];
    NumberWriter writer = {.buffer = out, .capacity = sizeof(out)};
    mlocWriteList(&p, items, 3, WriteNumbered, &calls, &writer);
    CHECK(writer.length == 5 && memcmp(out, "0+1+2", 5) == 0 && calls.count == 3 &&
              calls.seen[0] == 0 && calls.seen[1] == 1 && calls.seen[2] == 2,
          "items written by the caller, in order");
}

int main(void)
{
    TestValidators();
    TestFind();
    TestJoin();
    TestWriter();
    return s_failures == 0 ? 0 : 1;
}
