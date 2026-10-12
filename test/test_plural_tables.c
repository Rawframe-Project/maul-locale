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
    CHECK(!ValidUnit(spoiled, sizeof(spoiled)), "a range table past the tables");
    spoiled[2] = 0;
    spoiled[3] = 1;
    CHECK(!ValidUnit(spoiled, sizeof(spoiled)), "a reserved byte");
    spoiled[3] = 0;
    spoiled[7] = 1;
    CHECK(!ValidUnit(spoiled, sizeof(spoiled)), "the last reserved byte");
    spoiled[7] = 0;
    CHECK(!ValidUnit(spoiled, sizeof(spoiled) - 1), "a short unit");
    // A unit that names a table in a file without the tables.
    DataView empty = s_view;
    empty.unitCount = 0;
    uint8_t named[pluralUnitSize] = {cardinal, ordinal, 0};
    CHECK(!mlocValidPlurals(&empty, (DataSpan){named, sizeof(named)}),
          "a table in a file without tables");
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
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_view = mlocViewOf(&builtin);
    TestUnits();
    TestRanges();
    return s_failures == 0 ? 0 : 1;
}
