// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The plural units' validators (plural_rules.h): the compiled-in units
// pass, and a copy with one field spoiled is refused.

#include "data_view.h"
#include "kinds.h"
#include "plural.h"
#include "plural_rules.h"
#include "test_harness.h"

#include "maul-locale/locale.h"
#include "maul-locale/plural.h"

#include <stdlib.h>
#include <string.h>

static DataView s_view;

static bool ValidUnit(const uint8_t* unit, size_t size)
{
    return mlocValidPlurals(&s_view, (DataSpan){unit, (uint32_t)size});
}

// The first rule set of a type.
static uint8_t FirstRule(uint8_t ordinal)
{
    for (uint32_t r = 0; r < mlocPluralRuleSetCount; r++)
    {
        if (mlocPluralRuleSets[r].ordinal == ordinal)
        {
            return (uint8_t)r;
        }
    }
    return pluralNone;
}

static void TestUnits(void)
{
    DataSpan ranges;
    CHECK(mlocFindDataUnit(&s_view, kindPluralRanges, 0xFFFFFFFF, &ranges) &&
              mlocValidPluralRanges(&s_view, ranges),
          "the compiled-in range tables are valid");
    uint8_t cardinal = FirstRule(0);
    uint8_t ordinal = FirstRule(1);
    uint8_t unit[pluralUnitSize] = {cardinal, ordinal, 0};
    CHECK(ValidUnit(unit, sizeof(unit)), "a unit of valid indexes");
    unit[2] = pluralNone;
    unit[0] = pluralNone;
    unit[1] = pluralNone;
    CHECK(ValidUnit(unit, sizeof(unit)), "and one of none");
    uint8_t spoiled[pluralUnitSize] = {cardinal, ordinal, 0};
    spoiled[0] = ordinal;
    CHECK(!ValidUnit(spoiled, sizeof(spoiled)), "an ordinal rule as the cardinal one");
    spoiled[0] = cardinal;
    spoiled[1] = cardinal;
    CHECK(!ValidUnit(spoiled, sizeof(spoiled)), "a cardinal rule as the ordinal one");
    spoiled[1] = (uint8_t)mlocPluralRuleSetCount;
    CHECK(!ValidUnit(spoiled, sizeof(spoiled)), "a rule past the sets");
    spoiled[1] = ordinal;
    spoiled[2] = ranges.bytes[0];
    CHECK(ValidUnit(spoiled, sizeof(spoiled)),
          "a range table past the tables, which selecting takes as none");
    spoiled[2] = 0;
    spoiled[3] = 1;
    CHECK(!ValidUnit(spoiled, sizeof(spoiled)), "a reserved byte");
    spoiled[3] = 0;
    spoiled[7] = 1;
    CHECK(!ValidUnit(spoiled, sizeof(spoiled)), "the last reserved byte");
    spoiled[7] = 0;
    CHECK(!ValidUnit(spoiled, sizeof(spoiled) - 1), "a short unit");
    // The validator reads no other unit: during opening, later units'
    // bounds are not checked yet. Looking one up in this view would read
    // past its 8 bytes, which the sanitizer builds catch.
    uint8_t bytes[8] = {0};
    DataView hostile = {.bytes = bytes, .size = sizeof(bytes), .unitCount = 1};
    uint8_t named[pluralUnitSize] = {cardinal, ordinal, 0};
    CHECK(mlocValidPlurals(&hostile, (DataSpan){named, sizeof(named)}),
          "a unit validated without the ranges unit");
}

static void TestRanges(void)
{
    DataSpan ranges;
    CHECK(mlocFindDataUnit(&s_view, kindPluralRanges, 0xFFFFFFFF, &ranges), "the tables");
    uint8_t copy[pluralRangesHeaderSize + 64 * pluralRangeSize];
    if (ranges.size > sizeof(copy))
    {
        return;
    }
    memcpy(copy, ranges.bytes, ranges.size);
    DataSpan span = {copy, ranges.size};
    copy[pluralRangesHeaderSize] = 6;
    CHECK(!mlocValidPluralRanges(&s_view, span), "an entry of no category");
    copy[pluralRangesHeaderSize] = ranges.bytes[pluralRangesHeaderSize];
    copy[1] = 1;
    CHECK(!mlocValidPluralRanges(&s_view, span), "the header's reserved bytes");
    copy[1] = 0;
    copy[4] = 1;
    CHECK(!mlocValidPluralRanges(&s_view, span), "and its last word");
    copy[4] = 0;
    CHECK(!mlocValidPluralRanges(&s_view, (DataSpan){copy, ranges.size - 1}),
          "a size the count does not give");
    CHECK(!mlocValidPluralRanges(&s_view, (DataSpan){copy, 4}), "shorter than the header");
    CHECK(mlocValidPluralRanges(&s_view, span), "the copy restored");
    uint8_t none[pluralRangesHeaderSize] = {0};
    CHECK(mlocValidPluralRanges(&s_view, (DataSpan){none, sizeof(none)}), "a unit of no tables");
}

extern const uint8_t mlocBuiltinData[];
extern const uint32_t mlocBuiltinDataSize;

// English's unit made to name the table one past the last, opened as a
// file: its rules then have no table, so a range takes its end's
// category (English's table makes other to one other).
static void TestTablePastTheLast(void)
{
    uint8_t* file = malloc(mlocBuiltinDataSize);
    if (file == nullptr)
    {
        return;
    }
    memcpy(file, mlocBuiltinData, mlocBuiltinDataSize);
    uint32_t index = 0;
    DataSpan unit;
    DataSpan ranges;
    CHECK(mlocFindDataLocale(&s_view, "en", 2, &index) &&
              mlocFindDataUnit(&s_view, kindPlurals, index, &unit) &&
              mlocFindDataUnit(&s_view, kindPluralRanges, 0xFFFFFFFF, &ranges),
          "English's unit");
    file[unit.bytes - mlocBuiltinData + 2] = ranges.bytes[0];
    mlocData opened;
    mlocDataSet set = {&opened, 1};
    mlocLocale locale;
    mlocPluralRules rules;
    mlocPluralCategory category = mloc_pluralZero;
    CHECK(mlocOpenData(file, mlocBuiltinDataSize, &opened) == mloc_success &&
              mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocInitPluralRules(&set, &locale, mloc_pluralCardinal, &rules) == mloc_success &&
              mlocPluralRules_SelectRange(&rules, mloc_pluralOther, mloc_pluralOne, &category) ==
                  mloc_success &&
              category == mloc_pluralOne,
          "a table past the last is none");
    free(file);
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_view = mlocViewOf(&builtin);
    TestUnits();
    TestRanges();
    TestTablePastTheLast();
    return s_failures == 0 ? 0 : 1;
}
