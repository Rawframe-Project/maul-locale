// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names' validators and identifier lookup (unit_names.h,
// unit_tables.h) on units built here: valid ones pass and their
// identifiers are found; copies with one field spoiled are refused. Then
// the fields gathered along a chain from the unit names data file, a
// source without the identifiers skipped, and the trim of a unit's name
// (number_core.h).

#include "bytes.h"
#include "currency_tables.h"
#include "data_view.h"
#include "kinds.h"
#include "number_core.h"
#include "test_harness.h"
#include "test_pack.h"
#include "unit_names.h"
#include "unit_tables.h"

#include <stdio.h>
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
    static const char* const s_emptyFirst[] = {"", "day"};
    static const char* const s_third[] = {"a", "c", "b"};
    CHECK(!ValidIds(BuildIds(s_unsorted, s_kinds, 2)) && !ValidIds(BuildIds(s_twice, s_kinds, 2)) &&
              !ValidIds(BuildIds(s_third, s_kinds, 3)),
          "identifiers not strictly ascending, each against the one before");
    CHECK(!ValidIds(BuildIds(s_upper, s_kinds, 1)) && !ValidIds(BuildIds(s_long, s_kinds, 1)) &&
              !ValidIds(BuildIds(s_empty, s_kinds, 2)) &&
              !ValidIds(BuildIds(s_emptyFirst, s_kinds, 2)),
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

static mlocData s_sources[3];
static mlocDataSet s_data;

static bool Text(const UnitNameFields* f, uint32_t field, const char* expected)
{
    return (f->found & 1u << field) != 0 && f->length[field] == strlen(expected) &&
           memcmp(f->text[field], expected, f->length[field]) == 0;
}

static void TestGather(void)
{
    mlocLocale en;
    mlocLocale de;
    UnitNameFields f;
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocParseLocale("de", 2, &de).status == mloc_success,
          "the locales");
    CHECK(mlocGatherUnitNames(&s_data, &en, kindUnitNamesLong, "meter", 5, &f) &&
              Text(&f, unitNameDisplay, "meters") && Text(&f, unitNameOther, "{0} meters") &&
              Text(&f, unitNamePer, "{0} per meter"),
          "a width's fields, the per-unit pattern among them");
    CHECK(mlocGatherUnitNames(&s_data, &de, kindUnitNameExtras, "meter", 5, &f) &&
              Text(&f, unitExtraGender, "masculine") && Text(&f, unitNameOther, "{0} meter") &&
              (f.found & 1u << unitNamePer) == 0,
          "the extras: the gender and a pattern lowercased");
    CHECK(mlocGatherUnitNames(&s_data, &en, kindUnitNamesShort, "meter", 5, &f) &&
              Text(&f, unitNameDisplay, "m"),
          "a field from root's unit, past a source without the identifiers");
    uint8_t kind = 0;
    CHECK(mlocUnitNameKind(&s_data, "10p3", 4, &kind) && kind == unitIdPrefix &&
              !mlocUnitNameKind(&s_data, "kilometers", 10, &kind),
          "an identifier's kind");
}

typedef struct Parts
{
    uint32_t kinds[8];
    size_t lengths[8];
    int count;
} Parts;

static void Collect(void* context, uint32_t kind, size_t start, size_t length)
{
    (void)start;
    Parts* p = context;
    if (p->count < 8)
    {
        p->kinds[p->count] = kind;
        p->lengths[p->count] = length;
    }
    p->count++;
}

// The parts of a text written trimmed as a unit part.
static Parts Trimmed(const char* text)
{
    Parts parts = {0};
    mlocOutput output = {.part = Collect, .context = &parts};
    NumberWriter w = {.parts = &output};
    mlocWriteTrimmed(&w, mloc_numberPartUnit, (const uint8_t*)text, strlen(text));
    mlocEndParts(&w);
    return parts;
}

static void TestTrim(void)
{
    Parts p = Trimmed(" m ");
    CHECK(p.count == 3 && p.kinds[0] == mloc_numberPartLiteral &&
              p.kinds[1] == mloc_numberPartUnit && p.lengths[1] == 1 &&
              p.kinds[2] == mloc_numberPartLiteral,
          "spaces at both ends literal");
    // U+180B (a Mongolian variation selector) is trimmed, U+180E (once a
    // space) is not; U+E01EF (the last variation selector) is trimmed.
    p = Trimmed("\xE1\xA0\x8Bm\xF3\xA0\x87\xAF");
    CHECK(p.count == 3 && p.lengths[0] == 3 && p.lengths[1] == 1 && p.lengths[2] == 4,
          "variation selectors at the ends literal");
    p = Trimmed("\xE1\xA0\x8Em");
    CHECK(p.count == 1 && p.kinds[0] == mloc_numberPartUnit && p.lengths[0] == 4,
          "U+180E part of the name");
    p = Trimmed("\xC2\xB0\xE2\x80\x8F");
    CHECK(p.count == 2 && p.lengths[0] == 2 && p.kinds[1] == mloc_numberPartLiteral &&
              p.lengths[1] == 3,
          "a right-to-left mark ending the name literal");
}

// The compiled-in data, a pack of one names unit without identifiers,
// then the unit names data file at the path the build gives.
static bool OpenData(void)
{
    alignas(8) static uint8_t s_names[8 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    uint32_t size = BuildSparse("mm", "{0} mm");
    static uint8_t s_alone[64];
    memcpy(s_alone, s_unit, size);
    PackUnit unit = {kindUnitNamesShort, {s_alone, size}, true};
    FILE* file = fopen(MLOC_UNITS_FILE, "rb");
    size_t read = file == nullptr ? 0 : fread(s_names, 1, sizeof(s_names), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_data = (mlocDataSet){s_sources, 3};
    return OpenRootPack(&unit, 1, &s_sources[1]) && read > 0 && read < sizeof(s_names) &&
           mlocOpenData(&s_sources[2], s_names, read) == mloc_success;
}

int main(void)
{
    TestIds();
    TestNames();
    TestTrim();
    if (!OpenData())
    {
        fputs("test_unit_names: cannot open " MLOC_UNITS_FILE "\n", stderr);
        return 2;
    }
    TestGather();
    return s_failures == 0 ? 0 : 1;
}
