// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit table's validator and reads (unit_data.h, unit_tables.h): the
// compiled-in table passes and its lookups find what it holds; copies
// with one field spoiled are refused.

#include "bytes.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"
#include "unit_data.h"
#include "unit_tables.h"

#include <stdlib.h>
#include <string.h>

static DataView s_view;
static DataSpan s_units;
static uint8_t* s_copy;

static bool ValidCopy(uint32_t size)
{
    return mlocValidUnits(&s_view, (DataSpan){s_copy, size});
}

static void Restore(void)
{
    memcpy(s_copy, s_units.bytes, s_units.size);
}

static void TestLookups(void)
{
    UnitTable t = mlocReadUnitTable(s_units);
    int32_t meter = mlocFindSimpleUnit(&t, "meter", 5);
    int32_t kilo = mlocFindUnitPrefix(&t, "kilometer", 9);
    const char* replacement = nullptr;
    size_t n = mlocFindUnitAlias(&t, "metric-ton", 10, &replacement);
    CHECK(meter >= 0 && mlocSimpleUnit(&t, (uint32_t)meter)[simpleBase] != unitNoBase,
          "meter converts");
    CHECK(kilo >= 0 && mlocUnitPrefix(&t, (uint32_t)kilo)[5] == 3, "kilo is 10^3");
    CHECK(mlocFindUnitPrefix(&t, "kilo", 4) < 0 && mlocFindUnitPrefix(&t, "meter", 5) < 0,
          "a prefix needs something after it");
    CHECK(n == 5 && memcmp(replacement, "tonne", 5) == 0, "metric-ton is tonne");
    CHECK(mlocFindSimpleUnit(&t, "mete", 4) < 0 && mlocFindSimpleUnit(&t, "meters", 6) < 0 &&
              mlocFindUnitAlias(&t, "meter", 5, &replacement) == 0,
          "near misses find nothing");
    int32_t first = mlocFindSimpleUnit(&t, (const char*)t.text + mlocLoad32(t.simples),
                                       t.simples[simpleLength]);
    const uint8_t* lastRecord = mlocSimpleUnit(&t, t.simpleCount - 1);
    int32_t last = mlocFindSimpleUnit(&t, (const char*)t.text + mlocLoad32(lastRecord),
                                      lastRecord[simpleLength]);
    CHECK(first == 0 && last == (int32_t)t.simpleCount - 1, "the first and last units");
    UnitFactor f;
    mlocReadUnitFactor(&t, (uint32_t)mlocFindSimpleUnit(&t, "fahrenheit", 10), &f);
    CHECK(f.numerator.limb[0] == 5 && f.denominator.limb[0] == 9 &&
              !mlocWideIsZero(&f.offsetNumerator),
          "fahrenheit's factor and offset");
    mlocReadUnitFactor(&t, (uint32_t)meter, &f);
    CHECK(mlocWideIsZero(&f.offsetNumerator) && f.offsetDenominator.limb[0] == 1,
          "a unit without an offset");
}

// Spoils one byte of the copy and checks it is refused.
static void Spoil(size_t offset, uint8_t value, const char* what)
{
    Restore();
    s_copy[offset] = value;
    CHECK(!ValidCopy(s_units.size), what);
}

static void TestSpoiled(void)
{
    Restore();
    CHECK(ValidCopy(s_units.size), "the copy passes");
    CHECK(!ValidCopy(s_units.size - 1) && !ValidCopy(unitsHeaderSize - 1), "a wrong size");
    UnitTable t = mlocReadUnitTable(s_units);
    size_t simples = (size_t)(t.simples - s_units.bytes);
    size_t prefixes = (size_t)(t.prefixes - s_units.bytes);
    size_t aliases = (size_t)(t.aliases - s_units.bytes);
    size_t bases = (size_t)(t.bases - s_units.bytes);
    size_t beaufort = (size_t)(t.beaufort - s_units.bytes);
    size_t text = (size_t)(t.text - s_units.bytes);
    size_t numbers = (size_t)(t.numbers - s_units.bytes);
    Spoil(10, unitMaxDimensions + 1, "too many dimensions");
    Spoil(simples + simpleLength, 0, "an empty identifier");
    Restore();
    uint32_t textSize = mlocLoad32(s_units.bytes + 12);
    for (int i = 0; i < 4; i++)
    {
        s_copy[simples + simpleText + i] = (uint8_t)(textSize >> (8 * i));
    }
    CHECK(!ValidCopy(s_units.size), "an identifier past the text");
    Spoil(simples + simpleBase, (uint8_t)t.baseCount, "a base past the bases");
    Spoil(simples + simpleFlags, 4, "an unknown flag");
    Spoil(simples + simpleFlags, unitOffset | unitBeaufort, "an offset and the Beaufort table");
    // Text and number sizes each 2^31 larger: their sum is the same modulo
    // 2^32, which a 32-bit size would not tell apart.
    Restore();
    s_copy[19] = (uint8_t)(s_copy[19] + 0x80);
    s_copy[23] = (uint8_t)(s_copy[23] + 0x80);
    CHECK(!ValidCopy(s_units.size), "sizes that wrap 32 bits");
    Spoil(text, 'A', "a character no identifier uses");
    Restore();
    memcpy(s_copy + simples, s_units.bytes + simples + unitSimpleSize, 5);
    memcpy(s_copy + simples + unitSimpleSize, s_units.bytes + simples, 5);
    CHECK(!ValidCopy(s_units.size), "units out of order");
    Spoil(prefixes + 7, 1, "a prefix's padding");
    Spoil(prefixes + 5, 31, "a power of ten past 30");
    Spoil(prefixes + 6, 11, "a power of 1024 past 10");
    Restore();
    s_copy[prefixes + 5] = 0;
    s_copy[prefixes + 6] = 0;
    CHECK(!ValidCopy(s_units.size), "a prefix without a power");
    Restore();
    s_copy[prefixes + 5] = s_copy[prefixes + 5] == 0 ? 3 : s_copy[prefixes + 5];
    s_copy[prefixes + 6] = 1;
    CHECK(!ValidCopy(s_units.size), "a prefix with both powers");
    Spoil(prefixes + 4, 0, "an empty prefix");
    Spoil(aliases + 10, 1, "an alias's padding");
    Spoil(aliases + 9, 0, "an empty replacement");
    Spoil(bases + 5, 1, "a base's padding");
    Spoil(bases + baseNumerator + unitMaxDimensions - 1, 1, "a dimension past the count");
    Spoil(bases + baseDenominator + unitMaxDimensions - 1, 1,
          "a denominator dimension past the count");
    Spoil(14, 1, "the header's padding");
    Spoil(simples + simpleSystems + 1, 4, "a system the table does not name");
    Spoil(simples + 14, 1, "a unit's padding");
    size_t dimensions = (size_t)(t.dimensions - s_units.bytes);
    size_t quantities = (size_t)(t.quantities - s_units.bytes);
    Spoil(dimensions + 4, 0, "an empty dimension name");
    Spoil(dimensions + 6, 1, "a dimension's padding");
    Spoil(quantities + 8, 0, "an empty quantity name");
    Spoil(quantities + 9, 0, "an empty quantity base unit");
    Spoil(quantities + 10, 1, "a quantity's padding");
    Spoil(quantities + quantityNumerator + unitMaxDimensions - 1, 1,
          "a quantity dimension past the count");
    Spoil(quantities + quantityDenominator + unitMaxDimensions - 1, 1,
          "a quantity denominator dimension past the count");
    Spoil(beaufort + 2, 0, "Beaufort steps that do not ascend");
    // The first unit's factor: a numerator's length past the numbers, a
    // top byte 0, a zero numerator, a zero denominator.
    uint32_t factor = mlocLoad32(s_units.bytes + simples + simpleFactor);
    Spoil(numbers + factor, unitMaxNumberBytes + 1, "a number longer than the limit");
    uint8_t length = s_units.bytes[numbers + factor];
    Spoil(numbers + factor + length, 0, "a number whose top byte is 0");
    Spoil(numbers + factor, 0, "a zero numerator");
    Restore();
    uint32_t past = (uint32_t)(s_units.size - numbers);
    for (int i = 0; i < 4; i++)
    {
        s_copy[simples + simpleFactor + i] = (uint8_t)(past >> (8 * i));
    }
    CHECK(!ValidCopy(s_units.size), "a factor past the numbers");
    Restore();
    s_copy[simples + unitSimpleSize + simpleBase] = unitNoBase;
    CHECK(!ValidCopy(s_units.size), "a unit without a base that has a factor");
    Restore();
    s_copy[simples + simpleBase] = unitNoBase;
    s_copy[simples + simpleFlags] = unitOffset;
    CHECK(!ValidCopy(s_units.size), "a unit without a base that has flags");
}

static DataSpan s_prefs;

static bool ValidPrefsCopy(uint32_t size)
{
    return mlocValidUnitPreferences(&s_view, (DataSpan){s_copy, size});
}

// Spoils one byte of a copy of the preferences and checks it is refused.
static void SpoilPrefs(size_t offset, uint8_t value, const char* what)
{
    memcpy(s_copy, s_prefs.bytes, s_prefs.size);
    s_copy[offset] = value;
    CHECK(!ValidPrefsCopy(s_prefs.size), what);
}

static void TestPreferences(void)
{
    UnitPrefsTable p = mlocReadUnitPrefs(s_prefs);
    int32_t road = mlocFindUnitPrefSet(&p, "length", 6, "road", 4);
    int32_t length = mlocFindUnitPrefSet(&p, "length", 6, nullptr, 0);
    CHECK(road >= 0 && length >= 0 && length <= road &&
              mlocFindUnitPrefSet(&p, "length", 6, "roads", 5) < 0 &&
              mlocFindUnitPrefSet(&p, "lengths", 7, nullptr, 0) < 0 &&
              mlocFindUnitPrefSet(&p, "area", 4, nullptr, 0) == 0,
          "sets by category and usage");
    uint32_t us = mlocFindUnitPrefRun(&p, (uint32_t)road, 0x2B3);
    uint32_t world = mlocFindUnitPrefRun(&p, (uint32_t)road, 1024 + 1);
    uint32_t none = mlocFindUnitPrefRun(&p, (uint32_t)road, 1024 + 999);
    const uint8_t* entry = nullptr;
    CHECK(us != world && none == world &&
              mlocLoad16(p.runs + (size_t)world * unitRunSize) == 1024 + 1 &&
              mlocUnitPrefEntries(&p, world, &entry) > 1 && entry[4] == 9 &&
              memcmp(p.text + mlocLoad32(entry), "kilometer", 9) == 0,
          "runs by region, 001 for a region without one");
    memcpy(s_copy, s_prefs.bytes, s_prefs.size);
    CHECK(ValidPrefsCopy(s_prefs.size), "the preferences copy passes");
    CHECK(!ValidPrefsCopy(s_prefs.size - 1) && !ValidPrefsCopy(unitPrefsHeaderSize - 1),
          "a wrong size");
    size_t sets = (size_t)(p.sets - s_prefs.bytes);
    size_t runs = (size_t)(p.runs - s_prefs.bytes);
    size_t entries = (size_t)(p.entries - s_prefs.bytes);
    size_t text = (size_t)(p.text - s_prefs.bytes);
    SpoilPrefs(6, 1, "the header's padding");
    SpoilPrefs(sets + 14, 1, "a set's padding");
    SpoilPrefs(sets + 8, 0, "an empty category");
    SpoilPrefs(sets + 12, 0, "a set without runs");
    SpoilPrefs(sets + 10, 1, "a first set not starting at run 0");
    SpoilPrefs(text, 'A', "a character no identifier uses");
    SpoilPrefs(runs + 2, 1, "a first run not starting at entry 0");
    SpoilPrefs(runs, 0, "a run of no region");
    SpoilPrefs(entries + 5, unitMaxPlaces + 1, "too many decimal places");
    SpoilPrefs(entries + 6, 3, "an increment the precision does not take");
    SpoilPrefs(entries + 4, 0, "an empty unit");
    // The first set's runs: none of them 001.
    memcpy(s_copy, s_prefs.bytes, s_prefs.size);
    uint32_t first = mlocLoad16(s_prefs.bytes + sets + 12);
    for (uint32_t r = 0; r < first; r++)
    {
        uint8_t* at = s_copy + runs + (size_t)r * unitRunSize;
        if (mlocLoad16(at) == 1024 + 1)
        {
            at[0] = 2;
            at[1] = 4;
        }
    }
    CHECK(!ValidPrefsCopy(s_prefs.size), "a set without 001");
    // A run's last entry with a threshold.
    memcpy(s_copy, s_prefs.bytes, s_prefs.size);
    uint32_t end = mlocLoad16(s_prefs.bytes + runs + unitRunSize + 2);
    s_copy[entries + (size_t)(end - 1) * unitEntrySize + 8] = 1;
    CHECK(!ValidPrefsCopy(s_prefs.size), "a run whose last threshold is not 0");
    // Sets out of order: the first two swapped.
    memcpy(s_copy, s_prefs.bytes, s_prefs.size);
    memcpy(s_copy + sets, s_prefs.bytes + sets + unitSetSize, 10);
    memcpy(s_copy + sets + unitSetSize, s_prefs.bytes + sets, 10);
    CHECK(!ValidPrefsCopy(s_prefs.size), "sets out of order");
    memcpy(s_copy, s_prefs.bytes, s_prefs.size);
    s_copy[11] = (uint8_t)(s_copy[11] + 0x80);
    CHECK(!ValidPrefsCopy(s_prefs.size), "a text size past the unit");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_view = mlocViewOf(&builtin);
    CHECK(mlocFindDataUnit(&s_view, kindUnits, dataNoLocale, &s_units) &&
              mlocValidUnits(&s_view, s_units),
          "the compiled-in data has a valid unit table");
    if (s_failures > 0)
    {
        return 1;
    }
    CHECK(mlocFindDataUnit(&s_view, kindUnitPreferences, dataNoLocale, &s_prefs) &&
              mlocValidUnitPreferences(&s_view, s_prefs),
          "the compiled-in data has valid unit preferences");
    s_copy = malloc(s_units.size > s_prefs.size ? s_units.size : s_prefs.size);
    if (s_copy == nullptr)
    {
        return 1;
    }
    TestLookups();
    TestSpoiled();
    TestPreferences();
    free(s_copy);
    return s_failures == 0 ? 0 : 1;
}
