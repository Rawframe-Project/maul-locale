// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names' validators and identifier lookup (unit_names.h,
// unit_tables.h) on units built here: valid ones pass and their
// identifiers are found; copies with one field spoiled are refused, the
// derivations' and the duration and coordinate patterns' as well. Then
// the fields gathered along a chain from the unit names data file, a
// source without the identifiers skipped, the derivations looked up by
// language, identifiers normalized as ICU normalizes them
// (unit_normal.h), the trim of a unit's name (number_core.h), and a
// locale's patterns found.

#include "bytes.h"
#include "currency_tables.h"
#include "data_view.h"
#include "kinds.h"
#include "number_core.h"
#include "test_harness.h"
#include "test_pack.h"
#include "unit_compound.h"
#include "unit_id.h"
#include "unit_names.h"
#include "unit_normal.h"
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
    CHECK(mlocValidUnitNames(nullptr, (DataSpan){s_unit, size}) &&
              mlocValidUnitNameExtras(nullptr, (DataSpan){s_unit, size}),
          "an empty string in the names and the extras");
    size = BuildSparse("meters", "{0} meters");
    s_unit[sparseHeaderSize + 12] = 1u << 0 | 1u << 1 | 1u << 6;
    CHECK(!mlocValidUnitNames(nullptr, (DataSpan){s_unit, size}),
          "a row naming more fields than it holds");
    size = BuildSparse("meters", "{0} meters");
    s_unit[sparseHeaderSize + 12] = 1u << 0 | 1u << unitExtraFields;
    CHECK(!mlocValidUnitNameExtras(nullptr, (DataSpan){s_unit, size}),
          "a field past the extras' fields");
    size = BuildSparse("{0} meter", "{0} meters");
    s_unit[sparseHeaderSize + 12] = 1u << 0 | 1u << (unitCaseFields - 1);
    bool cases = mlocValidUnitCases(nullptr, (DataSpan){s_unit, size});
    s_unit[sparseHeaderSize + 12] = 1u << 0 | 1u << unitCaseFields;
    CHECK(cases && !mlocValidUnitCases(nullptr, (DataSpan){s_unit, size}),
          "case rows: their six fields, none past");
}

static mlocData s_sources[3];
static mlocDataSet s_data;

static bool Text(const UnitNameFields* f, uint32_t field, const char* expected)
{
    return (f->found & 1u << field) != 0 && f->length[field] == strlen(expected) &&
           memcmp(f->text[field], expected, f->length[field]) == 0;
}

// Builds a derivations unit of two records, "de" and "fa"; its size.
static uint32_t BuildDerivations(void)
{
    memset(s_unit, 0, sizeof(s_unit));
    Put(s_unit, 2, 2);
    uint8_t* at = s_unit + unitDerivationsHeaderSize;
    memcpy(at, "de", 2);
    memset(at + unitDerivationPlural, unitDerivedCompound, 16);
    memset(at + unitDerivationGender, unitGenderFirst, 4);
    at[unitDerivationPlural + 1] = 1;
    at[unitDerivationCase] = 7;
    at[unitDerivationCase + 1] = unitCaseCount - 1;
    at[unitDerivationGender + 1] = unitGenderSecond;
    at += unitDerivationSize;
    memcpy(at, "fa", 2);
    memset(at + unitDerivationPlural, unitDerivedNone, 20);
    at[unitDerivationPlural] = 5;
    at[unitDerivationGender] = unitGenderCount - 1;
    at[unitDerivationGender + 1] = unitGenderAnimate;
    return unitDerivationsHeaderSize + 2 * unitDerivationSize;
}

static bool ValidDerivations(uint32_t size)
{
    return mlocValidUnitDerivations(nullptr, (DataSpan){s_unit, size});
}

static void TestDerivations(void)
{
    uint32_t size = BuildDerivations();
    CHECK(ValidDerivations(size),
          "derivations: categories, cases, genders, components, compound and none");
    CHECK(!ValidDerivations(size - 1) && !ValidDerivations(size + unitDerivationSize) &&
              !ValidDerivations(2),
          "a size that is not the count's");
    s_unit[2] = 1;
    CHECK(!ValidDerivations(size), "a header's padding set");
    uint8_t* second = s_unit + unitDerivationsHeaderSize + unitDerivationSize;
    struct
    {
        uint32_t offset;
        uint8_t value;
        const char* what;
    } spoils[] = {
        {1, 'A', "a language not in lowercase"},
        {0, 0, "an empty language"},
        {3, 'x', "a language's padding set"},
        {unitDerivationPlural + 1, 6, "a value past the categories"},
        {unitDerivationPlural + 7, unitDerivedCompound - 1, "a value below compound"},
        {unitDerivationCase, unitCaseCount, "a value past the cases"},
        {unitDerivationGender, unitDerivedCompound, "a gender of compound"},
        {unitDerivationGender + 1, 0, "a gender of 0"},
        {unitDerivationGender + 3, unitGenderCount, "a value past the genders"},
        {unitDerivationGender + 2, unitGenderFirst - 1, "a value below the first component"},
        {0, 'a', "a language not after the one before"},
    };
    for (size_t i = 0; i < sizeof(spoils) / sizeof(spoils[0]); i++)
    {
        size = BuildDerivations();
        second[spoils[i].offset] = spoils[i].value;
        CHECK(!ValidDerivations(size), spoils[i].what);
    }
    size = BuildDerivations();
    memcpy(second, "de", 2);
    CHECK(!ValidDerivations(size), "a language twice");
}

// Builds a patterns unit of records records: the durations "h:mm", each
// direction the text given; its size.
static uint32_t BuildPatterns(uint32_t records, const char* direction)
{
    memset(s_unit, 0, sizeof(s_unit));
    uint32_t length = (uint32_t)strlen(direction);
    Put(s_unit, 2, 2);
    Put(s_unit + 2, records, 2);
    Put(s_unit + 4, 4 + length, 4);
    uint8_t* offsets = s_unit + numberTableHeaderSize;
    Put(offsets + 2, 4, 2);
    Put(offsets + 4, 4 + length, 2);
    uint8_t* at = offsets + 6;
    for (uint32_t r = 0; r < records; r++)
    {
        for (uint32_t i = 0; i < unitPatternCount; i++)
        {
            Put(at, i >= unitPatternCoordinates, 2);
            at += 2;
        }
    }
    memcpy(at, "h:mm", 4);
    memcpy(at + 4, direction, length);
    return (uint32_t)(at - s_unit) + 4 + length;
}

static bool ValidPatterns(uint32_t size)
{
    return mlocValidUnitPatterns(nullptr, (DataSpan){s_unit, size});
}

static void TestCaseRows(void)
{
    mlocLocale de;
    UnitNameFields f;
    CHECK(mlocParseLocale("de", 2, &de).status == mloc_success &&
              mlocGatherUnitCases(&s_data, &de, kindUnitCases, 8, "meter", 5, &f) &&
              (f.found & 1u << 5) != 0 && f.length[5] == 10 &&
              memcmp(f.text[5], "{0} Metern", 10) == 0,
          "German's dative of meter, other");
    CHECK(mlocGatherUnitCases(&s_data, &de, kindUnitCasesLowered, 8, "meter", 5, &f) &&
              (f.found & 1u << 5) != 0 && memcmp(f.text[5], "{0} metern", 10) == 0,
          "and lowercased");
    CHECK(!mlocGatherUnitCases(&s_data, &de, kindUnitCases, unitCaseNominative, "meter", 5, &f) &&
              !mlocGatherUnitCases(&s_data, &de, kindUnitCases, unitCaseCount, "meter", 5, &f),
          "no rows of the nominative or past the cases");
    mlocDataSet builtin = {s_sources, 1};
    CHECK(mlocGatherUnitCases(&builtin, &de, kindUnitCases, 8, "meter", 5, &f) && f.found == 0,
          "data without them gives none");
}

static void TestPatterns(void)
{
    uint32_t size = BuildPatterns(1, "{0}N");
    CHECK(ValidPatterns(size), "patterns: durations and directions");
    CHECK(!ValidPatterns(size - 1), "a table of the wrong size");
    CHECK(!ValidPatterns(BuildPatterns(2, "{0}N")), "two records");
    CHECK(!ValidPatterns(BuildPatterns(1, "{0}{0}")) && !ValidPatterns(BuildPatterns(1, "N")),
          "a direction whose number is not there once");
    uint8_t* records = s_unit + numberTableHeaderSize + 6;
    size = BuildPatterns(1, "{0}N");
    Put(records + 2 * unitPatternHms, 2, 2);
    CHECK(!ValidPatterns(size), "a string past the table's");
    size = BuildPatterns(1, "{0}N");
    Put(records + 2 * unitPatternCoordinates, 0, 2);
    CHECK(!ValidPatterns(size), "the first direction without its number");
    UnitPatternTexts texts;
    mlocLocale en;
    mlocDataSet builtin = {s_sources, 1};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocFindUnitPatterns(&s_data, &en, &texts) && texts.length[unitPatternHm] == 4 &&
              memcmp(texts.text[unitPatternHm], "h:mm", 4) == 0 &&
              !mlocFindUnitPatterns(&builtin, &en, &texts),
          "a locale's patterns found, none in data without them");
}

static void TestDerivationLookup(void)
{
    mlocLocale en;
    mlocLocale fr;
    mlocLocale de;
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocParseLocale("fr_CA", 5, &fr).status == mloc_success &&
              mlocParseLocale("de", 2, &de).status == mloc_success,
          "the locales");
    uint8_t value0 = 0;
    uint8_t value1 = 0;
    CHECK(mlocUnitDerivation(&s_data, &en, unitFeaturePlural, unitDeriveTimes, &value0, &value1) &&
              value0 == 1 && value1 == unitDerivedCompound,
          "a language the derivations do not list takes root's");
    CHECK(mlocUnitDerivation(&s_data, &fr, unitFeaturePlural, unitDeriveTimes, &value0, &value1) &&
              value0 == unitDerivedCompound && value1 == unitDerivedCompound,
          "a locale takes its language's");
    CHECK(mlocUnitDerivation(&s_data, &de, unitFeaturePlural, unitDerivePer, &value0, &value1) &&
              value0 == unitDerivedCompound && value1 == 1,
          "a structure's two values");
    CHECK(
        mlocUnitDerivation(&s_data, &de, unitFeatureCase, unitDerivePer, &value0, &value1) &&
            value0 == unitDerivedCompound && value1 == 3 &&
            mlocUnitDerivation(&s_data, &en, unitFeatureCase, unitDerivePower, &value0, &value1) &&
            value0 == unitCaseNominative && value1 == unitDerivedCompound &&
            mlocUnitDerivation(&s_data, &fr, unitFeatureCase, unitDerivePer, &value0, &value1) &&
            value0 == unitDerivedNone && value1 == unitDerivedNone,
        "cases: de's accusative denominator, root's nominative power, none for French");
    CHECK(mlocUnitDerivation(&s_data, &en, unitFeatureGender, unitDeriveTimes, &value0, &value1) &&
              value0 == unitGenderSecond && value1 == unitDerivedNone &&
              mlocUnitDerivation(&s_data, &fr, unitFeatureGender, unitDeriveTimes, &value0,
                                 &value1) &&
              value0 == unitGenderFirst &&
              mlocUnitDerivation(&s_data, &de, unitFeatureGender, unitDerivePrefix, &value0,
                                 &value1) &&
              value0 == unitGenderFirst,
          "genders: a product's second component's, French's first");
    mlocDataSet builtin = {s_sources, 1};
    CHECK(!mlocUnitDerivation(&builtin, &en, unitFeaturePlural, unitDeriveTimes, &value0, &value1),
          "data without the derivations");
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
    // English files its own display name, "m", and takes root's patterns.
    CHECK(mlocGatherUnitNames(&s_data, &en, kindUnitNamesShort, "meter", 5, &f) &&
              Text(&f, unitNameDisplay, "m") && Text(&f, unitNameOther, "{0} m") &&
              Text(&f, unitNamePer, "{0}/m"),
          "fields from root's unit, past a source without the identifiers");
    uint8_t kind = 0;
    CHECK(mlocUnitNameKind(&s_data, "10p3", 4, &kind) && kind == unitIdPrefix &&
              !mlocUnitNameKind(&s_data, "kilometers", 10, &kind),
          "an identifier's kind");
}

static bool Normal(const char* unit, const char* expected)
{
    char out[unitMaxIdLength];
    size_t length = 0;
    return mlocNormalizeUnitName(&s_data, unit, strlen(unit), out, sizeof(out), &length) ==
               mloc_success &&
           length == strlen(expected) && memcmp(out, expected, length) == 0;
}

static void TestNormalize(void)
{
    CHECK(Normal("hour-meter", "meter-hour") && Normal("meter-foot", "foot-meter"),
          "by quantity, then by simple unit");
    CHECK(Normal("meter-kilometer", "kilometer-meter") &&
              Normal("kibibyte-megabyte", "megabyte-kibibyte") &&
              Normal("kilobyte-kibibyte", "kibibyte-kilobyte"),
          "a larger prefix first, a binary one at a tie");
    CHECK(Normal("meter-per-second-kilogram", "meter-per-kilogram-second") &&
              Normal("per-second-meter", "per-meter-second") &&
              Normal("second-per-meter", "second-per-meter"),
          "the numerator first, per before the first denominator unit");
    CHECK(Normal("kilogram-per-1000000-meter", "kilogram-per-1e6-meter") &&
              Normal("meter-per-1000-second", "meter-per-1000-second"),
          "a constant past 1,000 written with its exponent");
    CHECK(Normal("meter-square-meter", "cubic-meter") && Normal("metric-ton", "tonne"),
          "powers of one unit merged, an alias replaced");
    char out[12];
    size_t length = 0;
    CHECK(mlocNormalizeUnitName(&s_data, "square-meter", 12, out, 12, &length) == mloc_success &&
              length == 12 &&
              mlocNormalizeUnitName(&s_data, "square-meter", 12, out, 11, &length) ==
                  mloc_errorCapacity &&
              mlocNormalizeUnitName(&s_data, "square-meter", 12, out, 6, &length) ==
                  mloc_errorCapacity,
          "an identifier that fills the room, one past it");
}

// Each unit the names data names normalizes to itself, or normalizing
// does not find it: mlocNormalUnitName takes a named unit as it is.
static void TestNamedAreNormal(void)
{
    DataSpan ids;
    CHECK(mlocFindDataSetUnit(&s_data, kindUnitIds, nullptr, 0, &ids), "the identifiers");
    uint32_t count = mlocLoad16(ids.bytes);
    const uint8_t* offsets = ids.bytes + unitIdsHeaderSize;
    const uint8_t* kinds = offsets + 4 * ((size_t)count + 1);
    const uint8_t* text = kinds + count;
    uint32_t units = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t start = mlocLoad32(offsets + (size_t)i * 4);
        size_t length = mlocLoad32(offsets + (size_t)i * 4 + 4) - start;
        char out[unitMaxIdLength];
        size_t outLength = 0;
        mlocResult r = kinds[i] != unitIdUnit
                           ? mloc_success
                           : mlocNormalizeUnitName(&s_data, (const char*)text + start, length, out,
                                                   sizeof(out), &outLength);
        units += kinds[i] == unitIdUnit;
        if (kinds[i] == unitIdUnit && r != mloc_errorNotFound &&
            (r != mloc_success || outLength != length || memcmp(out, text + start, length) != 0))
        {
            fprintf(stderr, "%.*s\n", (int)length, (const char*)text + start);
            CHECK(false, "a named unit normalizes to another");
        }
    }
    CHECK(units > 200, "the named units");
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
    TestDerivations();
    TestTrim();
    if (!OpenData())
    {
        fputs("test_unit_names: cannot open " MLOC_UNITS_FILE "\n", stderr);
        return 2;
    }
    TestGather();
    TestDerivationLookup();
    TestNormalize();
    TestNamedAreNormal();
    TestPatterns();
    TestCaseRows();
    return s_failures == 0 ? 0 : 1;
}
