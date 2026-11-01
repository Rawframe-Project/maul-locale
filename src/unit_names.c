// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names (unit_names.h).

#include "unit_names.h"

#include "bytes.h"
#include "currency_tables.h"
#include "data_format.h"
#include "invariant.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "number_data.h"

#include <string.h>

// Whether a byte may be in an identifier.
static bool IdByte(uint8_t c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == ':';
}

// Compares an identifier with a text, by bytes then by length.
static int CompareId(const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    size_t shorter = aLength < bLength ? aLength : bLength;
    int order = shorter == 0 ? 0 : memcmp(a, b, shorter);
    if (order != 0)
    {
        return order;
    }
    return aLength == bLength ? 0 : (aLength < bLength ? -1 : 1);
}

bool mlocValidUnitIds(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < unitIdsHeaderSize || mlocLoad16(payload.bytes + 2) != 0)
    {
        return false;
    }
    uint64_t count = mlocLoad16(payload.bytes);
    uint64_t textSize = mlocLoad32(payload.bytes + 4);
    if (payload.size != unitIdsHeaderSize + 4 * (count + 1) + count + textSize)
    {
        return false;
    }
    const uint8_t* offsets = payload.bytes + unitIdsHeaderSize;
    const uint8_t* kinds = offsets + 4 * (count + 1);
    const uint8_t* text = kinds + count;
    if (mlocLoad32(offsets) != 0 || mlocLoad32(offsets + 4 * count) != textSize)
    {
        return false;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t start = mlocLoad32(offsets + (size_t)i * 4);
        uint32_t end = mlocLoad32(offsets + (size_t)i * 4 + 4);
        // Each identifier ends within the text before any of it is read.
        if (end <= start || end > textSize || end - start > unitMaxNameId ||
            kinds[i] > unitIdCompound)
        {
            return false;
        }
        for (uint32_t at = start; at < end; at++)
        {
            if (!IdByte(text[at]))
            {
                return false;
            }
        }
        uint32_t before = i == 0 ? 0 : mlocLoad32(offsets + (size_t)i * 4 - 4);
        if (i > 0 && CompareId(text + before, start - before, text + start, end - start) >= 0)
        {
            return false;
        }
    }
    return true;
}

bool mlocValidUnitNames(const DataView* view, DataSpan payload)
{
    (void)view;
    static const SparseShape s_shape = {
        .fieldCount = unitNameFields,
        .stringFields = (1u << unitNameFields) - 1,
        .maxCode = UINT16_MAX,
        .empty = true,
    };
    return mlocValidSparse(payload, &s_shape);
}

bool mlocValidUnitNameExtras(const DataView* view, DataSpan payload)
{
    (void)view;
    static const SparseShape s_shape = {
        .fieldCount = unitExtraFields,
        .stringFields = (1u << unitExtraFields) - 1,
        .maxCode = UINT16_MAX,
        .empty = true,
    };
    return mlocValidSparse(payload, &s_shape);
}

bool mlocValidUnitCases(const DataView* view, DataSpan payload)
{
    (void)view;
    static const SparseShape s_shape = {
        .fieldCount = unitCaseFields,
        .stringFields = (1u << unitCaseFields) - 1,
        .maxCode = UINT16_MAX,
        .empty = true,
    };
    return mlocValidSparse(payload, &s_shape);
}

// Whether a derivation value is one its place allows.
static bool ValidDerived(uint32_t at, uint8_t value)
{
    if (value == unitDerivedNone)
    {
        return true;
    }
    if (at >= unitDerivationGender)
    {
        return value == unitGenderFirst || value == unitGenderSecond ||
               (value >= unitGenderAnimate && value < unitGenderCount);
    }
    uint32_t count = at >= unitDerivationCase ? unitCaseCount : unitNameOther - unitNameZero + 1;
    return value < count || value == unitDerivedCompound;
}

// Whether a derivation's language is lowercase letters then NULs, and
// its values those of their places.
static bool ValidDerivation(const uint8_t* record)
{
    uint32_t length = 0;
    while (length < unitDerivationLanguage && record[length] != 0)
    {
        if (record[length] < 'a' || record[length] > 'z')
        {
            return false;
        }
        length++;
    }
    for (uint32_t i = length; i < unitDerivationSize; i++)
    {
        if (i < unitDerivationLanguage ? record[i] != 0 : !ValidDerived(i, record[i]))
        {
            return false;
        }
    }
    return length > 0;
}

bool mlocValidUnitDerivations(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < unitDerivationsHeaderSize || mlocLoad16(payload.bytes + 2) != 0 ||
        payload.size !=
            unitDerivationsHeaderSize + (size_t)mlocLoad16(payload.bytes) * unitDerivationSize)
    {
        return false;
    }
    const uint8_t* records = payload.bytes + unitDerivationsHeaderSize;
    uint32_t count = mlocLoad16(payload.bytes);
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* record = records + (size_t)i * unitDerivationSize;
        if (!ValidDerivation(record) ||
            (i > 0 && memcmp(record - unitDerivationSize, record, unitDerivationLanguage) >= 0))
        {
            return false;
        }
    }
    return true;
}

bool mlocFindUnitNameId(DataSpan ids, const char* id, size_t length, uint16_t* codeOut,
                        uint8_t* kindOut)
{
    uint32_t count = mlocLoad16(ids.bytes);
    const uint8_t* offsets = ids.bytes + unitIdsHeaderSize;
    const uint8_t* kinds = offsets + 4 * ((size_t)count + 1);
    const uint8_t* text = kinds + count;
    uint32_t low = 0;
    uint32_t high = count;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint32_t start = mlocLoad32(offsets + (size_t)middle * 4);
        uint32_t end = mlocLoad32(offsets + (size_t)middle * 4 + 4);
        int order = CompareId(text + start, end - start, (const uint8_t*)id, length);
        if (order == 0)
        {
            *codeOut = (uint16_t)middle;
            *kindOut = kinds[middle];
            return true;
        }
        if (order < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return false;
}

bool mlocUnitNameKind(const mlocDataSet* data, const char* id, size_t length, uint8_t* kindOut)
{
    DataSpan ids;
    uint16_t code = 0;
    return mlocFindDataSetUnit(data, kindUnitIds, nullptr, 0, &ids) &&
           mlocFindUnitNameId(ids, id, length, &code, kindOut);
}

typedef struct Gathering
{
    const char* id;
    size_t length;
    uint32_t fieldCount;
    // A case's index past the nominative, for the case rows; else 0.
    uint32_t caseIndex;
    UnitNameFields* fields;
    // The identifier's code in the source last visited, whose ids table
    // the walk's later steps would look up again.
    const uint8_t* source;
    DataSpan ids;
    uint16_t code;
} Gathering;

static bool Gather(void* context, DataSpan unit, const DataView* view, int depth)
{
    (void)depth;
    Gathering* g = context;
    UnitNameFields* f = g->fields;
    uint8_t kind = 0;
    if (g->source != view->bytes &&
        (!mlocFindDataUnit(view, kindUnitIds, dataNoLocale, &g->ids) ||
         !mlocFindUnitNameId(g->ids, g->id, g->length, &g->code, &kind)))
    {
        g->source = nullptr;
        return false;
    }
    g->source = view->bytes;
    DataSpan ids = g->ids;
    uint16_t code = g->code;
    // A case row's code: the identifier's index past those of the cases
    // before.
    uint32_t full = code + (g->caseIndex > 0 ? (g->caseIndex - 1) * mlocLoad16(ids.bytes) : 0);
    if (full > UINT16_MAX)
    {
        return false;
    }
    // The gatherings ask for the names', extras' or cases' fields.
    MLOC_ASSERT(g->fieldCount <= unitNameFields);
    SparseTable t = mlocReadSparseTable(unit);
    const uint8_t* row = mlocFindSparseRow(&t, (uint16_t)full);
    for (uint32_t field = 0; row != nullptr && field < g->fieldCount; field++)
    {
        uint16_t value = 0;
        if ((f->found & 1u << field) != 0 || !mlocSparseField(row, field, &value))
        {
            continue;
        }
        uint16_t start = mlocLoad16(t.offsets + (size_t)value * 2);
        f->found |= 1u << field;
        f->text[field] = t.text + start;
        f->length[field] = (uint16_t)(mlocLoad16(t.offsets + (size_t)value * 2 + 2) - start);
    }
    return f->found == (1u << g->fieldCount) - 1;
}

bool mlocGatherUnitCases(const mlocDataSet* data, const mlocLocale* locale, uint32_t kind,
                         uint32_t caseIndex, const char* id, size_t length, UnitNameFields* out)
{
    *out = (UnitNameFields){0};
    Gathering g = {
        .id = id,
        .length = length,
        .fieldCount = unitCaseFields,
        .caseIndex = caseIndex,
        .fields = out,
    };
    return caseIndex > 0 && caseIndex < unitCaseCount &&
           mlocVisitLocaleUnits(data, mloc_componentMain, kind, locale, Gather, &g);
}

bool mlocGatherUnitNames(const mlocDataSet* data, const mlocLocale* locale, uint32_t kind,
                         const char* id, size_t length, UnitNameFields* out)
{
    *out = (UnitNameFields){0};
    Gathering g = {
        .id = id,
        .length = length,
        .fieldCount = kind == kindUnitNameExtras ? unitExtraFields : unitNameFields,
        .fields = out,
    };
    return mlocVisitLocaleUnits(data, mloc_componentMain, kind, locale, Gather, &g);
}

// The record of a language in a valid derivations unit, or null.
static const uint8_t* FindDerivation(DataSpan unit, const char* language)
{
    uint8_t key[unitDerivationLanguage] = {0};
    for (size_t i = 0; i < unitDerivationLanguage && language[i] != '\0'; i++)
    {
        key[i] = (uint8_t)language[i];
    }
    const uint8_t* records = unit.bytes + unitDerivationsHeaderSize;
    uint32_t low = 0;
    uint32_t high = mlocLoad16(unit.bytes);
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* record = records + (size_t)middle * unitDerivationSize;
        int order = memcmp(record, key, unitDerivationLanguage);
        if (order == 0)
        {
            return record;
        }
        if (order < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return nullptr;
}

bool mlocUnitDerivation(const mlocDataSet* data, const mlocLocale* locale, uint32_t feature,
                        uint32_t structure, uint8_t* value0Out, uint8_t* value1Out)
{
    DataSpan unit;
    if (!mlocFindDataSetUnit(data, kindUnitDerivations, nullptr, 0, &unit))
    {
        return false;
    }
    const uint8_t* record = FindDerivation(unit, locale->language);
    record = record != nullptr ? record : FindDerivation(unit, "root");
    uint32_t at =
        feature == unitFeatureGender ? unitDerivationGender + structure : feature + 2 * structure;
    // The callers' features and structures are within the record.
    MLOC_ASSERT(at + (feature == unitFeatureGender ? 0 : 1) < unitDerivationSize);
    *value0Out = record != nullptr ? record[at] : unitDerivedNone;
    *value1Out =
        record != nullptr && feature != unitFeatureGender ? record[at + 1] : unitDerivedNone;
    return true;
}

// Whether text holds "{0}" exactly once.
static bool OnePlaceholder(const uint8_t* text, uint32_t length)
{
    uint32_t count = 0;
    for (uint32_t at = 0; at + 3 <= length; at++)
    {
        count += memcmp(text + at, "{0}", 3) == 0;
    }
    return count == 1;
}

bool mlocValidUnitPatterns(const DataView* view, DataSpan payload)
{
    (void)view;
    if (!mlocValidNumberTable(payload, 2 * unitPatternCount, 0))
    {
        return false;
    }
    NumberTable t = mlocReadNumberTable(payload, 2 * unitPatternCount);
    if (t.recordCount != 1)
    {
        return false;
    }
    for (uint32_t i = 0; i < unitPatternCount; i++)
    {
        uint32_t s = mlocLoad16(t.records + 2 * (size_t)i);
        if (s >= t.stringCount)
        {
            return false;
        }
        uint32_t start = mlocLoad16(t.offsets + (size_t)s * 2);
        uint32_t end = mlocLoad16(t.offsets + (size_t)s * 2 + 2);
        if (i >= unitPatternCoordinates && !OnePlaceholder(t.text + start, end - start))
        {
            return false;
        }
    }
    return true;
}

bool mlocFindUnitPatterns(const mlocDataSet* data, const mlocLocale* locale, UnitPatternTexts* out)
{
    DataSpan unit;
    bool fellBack = false;
    if (!mlocFindLocaleUnit(data, mloc_componentMain, kindUnitPatterns, locale, &unit, &fellBack,
                            nullptr))
    {
        return false;
    }
    NumberTable t = mlocReadNumberTable(unit, 2 * unitPatternCount);
    for (uint32_t i = 0; i < unitPatternCount; i++)
    {
        uint32_t s = mlocLoad16(t.records + 2 * (size_t)i);
        uint32_t start = mlocLoad16(t.offsets + (size_t)s * 2);
        out->text[i] = (const char*)t.text + start;
        out->length[i] = (uint16_t)(mlocLoad16(t.offsets + (size_t)s * 2 + 2) - start);
    }
    return true;
}
