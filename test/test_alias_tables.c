// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The aliases table's validator: the compiled-in table passes, and a
// copy with one field spoiled is refused for each rule of the layout
// (locale_tables.h).

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_canonical.h"
#include "locale_tables.h"
#include "test_harness.h"

#include "maul-locale/locale.h"

#include <stdlib.h>
#include <string.h>

extern const uint8_t mlocBuiltinData[];
extern const uint32_t mlocBuiltinDataSize;

static DataSpan s_builtin;
static uint8_t* s_copy;

typedef struct Layout
{
    uint32_t rules;
    uint32_t variants;
    uint32_t regions;
    uint32_t legacy;
    size_t keysAt;
    size_t keyRulesAt;
    uint32_t slots;
    size_t slotsAt;
    size_t variantsAt;
    size_t regionsAt;
    size_t legacyAt;
} Layout;

static Layout s_layout;

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

static void Restore(void)
{
    memcpy(s_copy, s_builtin.bytes, s_builtin.size);
}

static bool Valid(void)
{
    return mlocValidAliases(nullptr, (DataSpan){s_copy, s_builtin.size});
}

// Restores the copy, writes value at offset, and asks the validator.
static bool ValidWith(size_t offset, uint64_t value, int bytes)
{
    Restore();
    Put(s_copy + offset, value, bytes);
    return Valid();
}

static size_t RuleAt(uint32_t rule)
{
    return aliasHeaderSize + (size_t)rule * aliasRuleSize;
}

// The first rule whose byte at offset satisfies a test.
static uint32_t FindRule(size_t offset, bool (*test)(uint8_t))
{
    for (uint32_t r = 0; r < s_layout.rules; r++)
    {
        if (test(s_builtin.bytes[RuleAt(r) + offset]))
        {
            return r;
        }
    }
    return 0;
}

static bool SeveralRegions(uint8_t count)
{
    return count > 1;
}

static bool TypeVariants(uint8_t counts)
{
    return (counts & 15) > 0;
}

static void ReadLayout(void)
{
    const uint8_t* b = s_builtin.bytes;
    s_layout.rules = mlocLoad32(b);
    s_layout.variants = mlocLoad32(b + 8);
    s_layout.regions = mlocLoad32(b + 12);
    s_layout.legacy = mlocLoad32(b + 16);
    s_layout.keysAt = aliasHeaderSize + (size_t)s_layout.rules * aliasRuleSize;
    s_layout.keyRulesAt = s_layout.keysAt + (size_t)s_layout.rules * 4;
    s_layout.slots = mlocLoad32(b + 24);
    s_layout.slotsAt = s_layout.keyRulesAt + (size_t)s_layout.rules * 2;
    s_layout.variantsAt = (s_layout.slotsAt + (size_t)s_layout.slots * 2 + 7) / 8 * 8;
    s_layout.regionsAt = s_layout.variantsAt + (size_t)s_layout.variants * 8;
    s_layout.legacyAt = s_layout.regionsAt + (size_t)s_layout.regions * 2;
}

static void TestHeader(void)
{
    CHECK(!ValidWith(28, 1, 4), "the reserved word is zero");
    // One slot fewer leaves the padded size as it is, so only the count
    // being no power of two refuses it.
    CHECK((s_layout.slotsAt + s_layout.slots * 2 + 7) / 8 ==
              (s_layout.slotsAt + s_layout.slots * 2 + 5) / 8,
          "one slot fewer keeps the size");
    CHECK(!ValidWith(24, s_layout.slots - 1, 4), "a slot count that is no power of two");
    CHECK(!ValidWith(16, s_layout.legacy + 1, 4), "the counts give the size");
    CHECK(!ValidWith(4, s_layout.rules - 1, 4), "one key per rule");
    CHECK(!mlocValidAliases(nullptr, (DataSpan){s_copy, 16}), "shorter than the header");
}

static void TestRules(void)
{
    uint32_t several = FindRule(26, SeveralRegions);
    uint32_t withVariants = FindRule(27, TypeVariants);
    size_t rule = RuleAt(withVariants);
    CHECK(s_builtin.bytes[RuleAt(several) + 26] > 1 && TypeVariants(s_builtin.bytes[rule + 27]),
          "the table has the rules these cases spoil");
    CHECK(!ValidWith(rule + 27, (s_builtin.bytes[rule + 27] & 0xF0) | 15, 1),
          "a type with more variants than a rule holds");
    CHECK(!ValidWith(rule + 27, (s_builtin.bytes[rule + 27] & 0x0F) | 3 << 4, 1),
          "a replacement with more variants than a rule holds");
    CHECK(!ValidWith(rule + 12, s_layout.variants, 2), "a variant past the table");
    CHECK(!ValidWith(RuleAt(several) + 26, 1, 1), "a list of one region");
    CHECK(!ValidWith(RuleAt(several) + 24, s_layout.regions, 2), "a region list past the table");
    CHECK(!ValidWith(RuleAt(0) + 8, 1, 2), "a type language of one letter");
    CHECK(!ValidWith(RuleAt(0) + 16, 27 << 10, 2), "a replacement language past z");
    CHECK(!ValidWith(RuleAt(0) + 4, 1, 4), "a replacement script of one letter");
    CHECK(!ValidWith(RuleAt(0) + 18, 2024, 2), "a replacement region past 999");
    Restore();
    memset(s_copy + RuleAt(0), 0, 16);
    s_copy[RuleAt(0) + 27] = 0;
    CHECK(!Valid(), "a rule whose type is empty");
}

static void TestIndex(void)
{
    uint32_t last = s_layout.rules - 1;
    uint32_t key = mlocLoad32(s_builtin.bytes + s_layout.keysAt + (size_t)last * 4);
    CHECK(!ValidWith(s_layout.keysAt + (size_t)last * 4, key + 1, 4),
          "a key that is not its rule's first field");
    CHECK(!ValidWith(s_layout.keysAt, 3u << 30 | 0xFFFF, 4), "keys in order");
    CHECK(!ValidWith(s_layout.keyRulesAt, s_layout.rules, 2), "a key's rule past the rules");
    uint32_t second = mlocLoad16(s_builtin.bytes + s_layout.keyRulesAt + 2);
    CHECK(!ValidWith(s_layout.keyRulesAt, second, 2), "a rule filed twice");
}

static uint32_t KeyAt(uint32_t index)
{
    return mlocLoad32(s_builtin.bytes + s_layout.keysAt + (size_t)index * 4);
}

// The offset of the slot that names a key's first index.
static size_t SlotOf(uint32_t index)
{
    uint32_t at = mlocAliasSlot(KeyAt(index), s_layout.slots);
    while (mlocLoad16(s_builtin.bytes + s_layout.slotsAt + (size_t)at * 2) != index + 1)
    {
        at = (at + 1) & (s_layout.slots - 1);
    }
    return s_layout.slotsAt + (size_t)at * 2;
}

static size_t EmptySlot(void)
{
    size_t at = s_layout.slotsAt;
    while (mlocLoad16(s_builtin.bytes + at) != 0)
    {
        at += 2;
    }
    return at;
}

// A table of one rule, iw to he, with slotCount slots: every slot
// naming it, or only its own.
static bool ValidOneRule(uint32_t slotCount, bool everySlot)
{
    uint8_t table[aliasHeaderSize + aliasRuleSize + 16] = {0};
    Put(table, 1, 4);
    Put(table + 4, 1, 4);
    Put(table + 24, slotCount, 4);
    uint8_t* rule = table + aliasHeaderSize;
    uint32_t iw = mlocPackLanguage("iw", 2);
    Put(rule + 8, iw, 2);
    Put(rule + 16, mlocPackLanguage("he", 2), 2);
    uint8_t* key = rule + aliasRuleSize;
    Put(key, iw, 4);
    for (uint32_t i = 0; i < slotCount; i++)
    {
        if (everySlot || i == mlocAliasSlot(iw, slotCount))
        {
            Put(key + 6 + i * 2, 1, 2);
        }
    }
    uint32_t size = (aliasHeaderSize + aliasRuleSize + 6 + slotCount * 2 + 7) / 8 * 8;
    return mlocValidAliases(nullptr, (DataSpan){table, size});
}

static void TestSlots(void)
{
    uint32_t run = 0;
    while (KeyAt(run) != KeyAt(run + 1))
    {
        run++;
    }
    CHECK(!ValidWith(SlotOf(run), run + 2, 2), "a slot that names no key's first index");
    CHECK(!ValidWith(EmptySlot(), s_layout.rules + 1, 2), "a slot past the keys");
    CHECK(!ValidWith(EmptySlot(), 1, 2), "a key in two slots");
    CHECK(!ValidWith(SlotOf(0), 0, 2), "a key that no slot names");
    CHECK(ValidOneRule(2, false), "a table of one rule");
    CHECK(!ValidOneRule(1, true), "slots with no 0 to end a probe");
}

static void TestTables(void)
{
    CHECK(!ValidWith(s_layout.variantsAt, 'A', 1), "a variant name in uppercase");
    CHECK(!ValidWith(s_layout.variantsAt + 1, '-', 1), "a variant name with a separator");
    CHECK(!ValidWith(s_layout.variantsAt + 4, 0, 4), "a variant name of four that is not one");
    Restore();
    memcpy(s_copy + s_layout.variantsAt, s_builtin.bytes + s_layout.variantsAt + 8, 8);
    CHECK(!Valid(), "variant names in order");
    CHECK(!ValidWith(s_layout.regionsAt, 0, 2), "a region in a list that is none");
    CHECK(!ValidWith(s_layout.legacyAt, 0xFFFF, 2), "a legacy type past the text");
    CHECK(!ValidWith(s_layout.legacyAt + 6, 0, 2), "an empty legacy replacement");
    CHECK(!ValidWith(s_layout.legacyAt + 2, 300, 2), "a legacy type too long to compare");
    Restore();
    memcpy(s_copy + s_layout.legacyAt, s_builtin.bytes + s_layout.legacyAt + aliasLegacySize,
           aliasLegacySize);
    memcpy(s_copy + s_layout.legacyAt + aliasLegacySize, s_builtin.bytes + s_layout.legacyAt,
           aliasLegacySize);
    CHECK(!Valid(), "legacy types in order");
    Restore();
    memcpy(s_copy + s_layout.legacyAt + aliasLegacySize, s_builtin.bytes + s_layout.legacyAt,
           aliasLegacySize);
    CHECK(!Valid(), "a legacy type twice");
}

// Rules that never end: the compiled-in data with "iw" made to replace
// itself, opened as a file, refuses to canonicalize "iw" instead of
// looping.
static void TestEndlessRules(void)
{
    uint8_t* file = malloc(mlocBuiltinDataSize);
    if (file == nullptr)
    {
        return;
    }
    memcpy(file, mlocBuiltinData, mlocBuiltinDataSize);
    uint8_t* table = file + (s_builtin.bytes - mlocBuiltinData);
    uint32_t iw = mlocPackLanguage("iw", 2);
    for (uint32_t r = 0; r < s_layout.rules; r++)
    {
        uint8_t* rule = table + RuleAt(r);
        if (mlocLoad16(rule + 8) == iw && mlocLoad32(rule) == 0 && mlocLoad16(rule + 10) == 0 &&
            (rule[27] & 15) == 0)
        {
            Put(rule + 16, iw, 2);
        }
    }
    DataView view;
    if (mlocOpenDataWithKinds(&view, file, mlocBuiltinDataSize, mlocDataKinds, mlocDataKindCount) !=
        mloc_success)
    {
        // The view is unset: nothing below can run.
        CHECK(false, "a file whose rules loop still validates");
        free(file);
        return;
    }
    mlocData opened = {0};
    memcpy(&opened, &view, sizeof(view));
    mlocDataSet set = {&opened, 1};
    mlocLocale locale;
    CHECK(mlocCanonicalizeLocale(&set, "iw", 2, &locale, nullptr).status == mloc_errorData,
          "rules that never end are refused");
    CHECK(mlocCanonicalizeLocale(&set, "en", 2, &locale, nullptr).status == mloc_success,
          "and other tags still canonicalize");
    free(file);
}

// The extension aliases table: valid as compiled in, and refused with
// one field spoiled.
static void TestExtensionTable(const mlocDataSet* data)
{
    DataSpan table;
    CHECK(mlocFindDataSetUnit(data, kindLocaleExtensionAliases, nullptr, 0, &table),
          "the compiled-in data has the extension aliases");
    uint8_t* copy = malloc(table.size);
    if (copy == nullptr)
    {
        return;
    }
    uint32_t types = mlocLoad32(table.bytes);
    uint32_t subdivisions = mlocLoad32(table.bytes + 4);
    size_t typesAt = extensionHeaderSize;
    size_t subdivisionsAt = typesAt + (size_t)types * extensionTypeSize;
    size_t textAt = subdivisionsAt + (size_t)subdivisions * extensionSubdivisionSize;
    struct
    {
        size_t offset;
        uint32_t value;
        int bytes;
        const char* what;
    } spoils[] = {
        {12, 1, 4, "the reserved word is zero"},
        {8, mlocLoad32(table.bytes + 8) + 1, 4, "the counts give the size"},
        {typesAt, 'A', 1, "a key in uppercase"},
        {typesAt + 2, 0, 1, "an empty from text"},
        {typesAt + 4, 0xFFFFFF, 4, "a from text past the text"},
        {typesAt + 12, 1, 4, "a type's reserved word"},
        {typesAt + extensionTypeSize, 0, 2, "types in order"},
        {subdivisionsAt + 2, 1, 2, "a subdivision's reserved field"},
        {subdivisionsAt + 1, 2, 1, "a replacement of two characters"},
        {textAt, '-', 1, "a value that begins with a separator"},
        {textAt, 'Q', 1, "a value in uppercase"},
    };
    for (size_t i = 0; i < sizeof(spoils) / sizeof(spoils[0]); i++)
    {
        memcpy(copy, table.bytes, table.size);
        Put(copy + spoils[i].offset, spoils[i].value, spoils[i].bytes);
        CHECK(!mlocValidExtensionAliases(nullptr, (DataSpan){copy, table.size}), spoils[i].what);
    }
    // A value's separator made a letter gives a subtag past 8.
    for (uint32_t t = 0; t < types; t++)
    {
        const uint8_t* entry = table.bytes + typesAt + (size_t)t * extensionTypeSize;
        size_t to = textAt + mlocLoad32(entry + 8);
        const uint8_t* dash = memchr(table.bytes + to, '-', entry[3]);
        if (dash != nullptr)
        {
            memcpy(copy, table.bytes, table.size);
            copy[dash - table.bytes] = 'x';
            CHECK(!mlocValidExtensionAliases(nullptr, (DataSpan){copy, table.size}),
                  "a value with a subtag past 8");
            break;
        }
    }
    memcpy(copy, table.bytes, table.size);
    memcpy(copy + subdivisionsAt, table.bytes + subdivisionsAt + extensionSubdivisionSize,
           extensionSubdivisionSize);
    CHECK(!mlocValidExtensionAliases(nullptr, (DataSpan){copy, table.size}),
          "subdivisions in order");
    CHECK(mlocValidExtensionAliases(nullptr, table), "the compiled-in table is valid");
    CHECK(!mlocValidExtensionAliases(nullptr, (DataSpan){copy, 8}), "shorter than the header");
    free(copy);
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    CHECK(mlocFindDataSetUnit(&data, kindLocaleAliases, nullptr, 0, &s_builtin),
          "the compiled-in data has the table");
    s_copy = malloc(s_builtin.size);
    if (s_copy == nullptr)
    {
        return 1;
    }
    Restore();
    CHECK(Valid(), "the compiled-in table is valid");
    ReadLayout();
    TestHeader();
    TestRules();
    TestIndex();
    TestSlots();
    TestTables();
    TestEndlessRules();
    TestExtensionTable(&data);
    free(s_copy);
    return s_failures == 0 ? 0 : 1;
}
