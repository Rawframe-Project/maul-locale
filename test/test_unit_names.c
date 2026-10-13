// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names' validators and identifier lookup (unit_names.h,
// unit_tables.h) on units built here: valid ones pass and their
// identifiers are found; copies with one field spoiled are refused.

#include "bytes.h"
#include "currency_tables.h"
#include "data_view.h"
#include "test_harness.h"
#include "unit_names.h"
#include "unit_tables.h"

#include <string.h>

static uint8_t s_unit[512];

static void Put(uint8_t* p, uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// Builds an identifiers unit; its size.
static uint32_t BuildIds(const char* const* ids, const uint8_t* kinds, uint32_t count)
{
    memset(s_unit, 0, sizeof(s_unit));
    uint32_t text = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        text += (uint32_t)strlen(ids[i]);
    }
    uint8_t* offsets = s_unit + unitIdsHeaderSize;
    uint8_t* kindsAt = offsets + 4 * (count + 1);
    uint8_t* textAt = kindsAt + count;
    Put(s_unit, count, 2);
    Put(s_unit + 4, text, 4);
    uint32_t at = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        Put(offsets + 4 * i, at, 4);
        kindsAt[i] = kinds[i];
        memcpy(textAt + at, ids[i], strlen(ids[i]));
        at += (uint32_t)strlen(ids[i]);
    }
    Put(offsets + 4 * count, at, 4);
    return unitIdsHeaderSize + 4 * (count + 1) + count + text;
}

static bool ValidIds(uint32_t size)
{
    return mlocValidUnitIds(nullptr, (DataSpan){s_unit, size});
}

static void TestIds(void)
{
    static const char* const s_ids[] = {"day", "meter", "per", "power2:feminine"};
    static const uint8_t s_kinds[] = {unitIdUnit, unitIdUnit, unitIdCompound, unitIdPower};
    uint32_t size = BuildIds(s_ids, s_kinds, 4);
    CHECK(ValidIds(size), "a valid identifiers unit");
    DataSpan ids = {s_unit, size};
    uint16_t code = 0;
    uint8_t kind = 0;
    CHECK(mlocFindUnitNameId(ids, "day", 3, &code, &kind) && code == 0 && kind == unitIdUnit &&
              mlocFindUnitNameId(ids, "power2:feminine", 15, &code, &kind) && code == 3 &&
              kind == unitIdPower && mlocFindUnitNameId(ids, "per", 3, &code, &kind) && code == 2 &&
              kind == unitIdCompound,
          "identifiers found, with their codes and kinds");
    CHECK(!mlocFindUnitNameId(ids, "da", 2, &code, &kind) &&
              !mlocFindUnitNameId(ids, "days", 4, &code, &kind) &&
              !mlocFindUnitNameId(ids, "", 0, &code, &kind) &&
              !mlocFindUnitNameId(ids, "zz", 2, &code, &kind),
          "a prefix, a longer text, an empty one and one past the end are not");
    CHECK(!ValidIds(size - 1) && !ValidIds(unitIdsHeaderSize - 1), "a size that does not hold");
    s_unit[2] = 1;
    CHECK(!ValidIds(size), "a header byte not 0");
    size = BuildIds(s_ids, s_kinds, 4);
    s_unit[unitIdsHeaderSize + 4 * 5] = unitIdCompound + 1;
    CHECK(!ValidIds(size), "a kind out of range");
    static const char* const s_unsorted[] = {"meter", "day"};
    static const char* const s_twice[] = {"day", "day"};
    static const char* const s_upper[] = {"Day"};
    static const char* const s_long[] = {
        "a2345678901234567890123456789012345678901234567890123456789012345"};
    static const char* const s_empty[] = {"day", ""};
    CHECK(!ValidIds(BuildIds(s_unsorted, s_kinds, 2)) && !ValidIds(BuildIds(s_twice, s_kinds, 2)),
          "identifiers not strictly ascending");
    CHECK(!ValidIds(BuildIds(s_upper, s_kinds, 1)) && !ValidIds(BuildIds(s_long, s_kinds, 1)) &&
              !ValidIds(BuildIds(s_empty, s_kinds, 2)),
          "an identifier of another byte, too long or empty");
    size = BuildIds(s_ids, s_kinds, 4);
    Put(s_unit + unitIdsHeaderSize + 4 * 4, 30, 4);
    CHECK(!ValidIds(size), "the last offset not the text size");
}

// Builds a sparse unit of one entry, code 0, with two strings in its
// fields 0 and 6 (the second may be empty); its size.
static uint32_t BuildSparse(const char* first, const char* second)
{
    memset(s_unit, 0, sizeof(s_unit));
    uint32_t a = (uint32_t)strlen(first);
    uint32_t b = (uint32_t)strlen(second);
    Put(s_unit, 2, 2);
    Put(s_unit + 2, 1, 2);
    Put(s_unit + 4, a + b, 4);
    Put(s_unit + 8, 5, 4);
    uint8_t* at = s_unit + sparseHeaderSize;
    Put(at, 0, 2);
    Put(at + 2, a, 2);
    Put(at + 4, a + b, 2);
    at += 6;
    Put(at, 0, 2);
    Put(at + 2, 0, 2);
    Put(at + 4, 5, 2);
    at += 6;
    at[0] = 1u << 0 | 1u << 6;
    Put(at + 1, 0, 2);
    Put(at + 3, 1, 2);
    memcpy(at + 5, first, a);
    memcpy(at + 5 + a, second, b);
    return sparseHeaderSize + 6 + 6 + 5 + a + b;
}

static void TestNames(void)
{
    uint32_t size = BuildSparse("meters", "{0} meters");
    CHECK(mlocValidUnitNames(nullptr, (DataSpan){s_unit, size}) &&
              mlocValidUnitNameExtras(nullptr, (DataSpan){s_unit, size}),
          "a names unit, code 0 among its codes");
    size = BuildSparse("masculine", "");
    CHECK(!mlocValidUnitNames(nullptr, (DataSpan){s_unit, size}) &&
              mlocValidUnitNameExtras(nullptr, (DataSpan){s_unit, size}),
          "an empty string in the extras alone");
    size = BuildSparse("meters", "{0} meters");
    s_unit[sparseHeaderSize + 12] = 1u << 0 | 1u << 1 | 1u << 6;
    CHECK(!mlocValidUnitNames(nullptr, (DataSpan){s_unit, size}),
          "a row naming more fields than it holds");
    size = BuildSparse("meters", "{0} meters");
    s_unit[sparseHeaderSize + 12] = 1u << 0 | 1u << unitExtraFields;
    CHECK(!mlocValidUnitNameExtras(nullptr, (DataSpan){s_unit, size}),
          "a field past the extras' fields");
}

int main(void)
{
    TestIds();
    TestNames();
    return s_failures == 0 ? 0 : 1;
}
