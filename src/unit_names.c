// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names (unit_names.h).

#include "unit_names.h"

#include "bytes.h"
#include "currency_tables.h"
#include "data_format.h"
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
    UnitNameFields* fields;
} Gathering;

static bool Gather(void* context, DataSpan unit, const DataView* view, int depth)
{
    (void)depth;
    Gathering* g = context;
    UnitNameFields* f = g->fields;
    DataSpan ids;
    uint16_t code = 0;
    uint8_t kind = 0;
    if (!mlocFindDataUnit(view, kindUnitIds, dataNoLocale, &ids) ||
        !mlocFindUnitNameId(ids, g->id, g->length, &code, &kind))
    {
        return false;
    }
    SparseTable t = mlocReadSparseTable(unit);
    const uint8_t* row = mlocFindSparseRow(&t, code);
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

uint32_t mlocUnitPatternField(const UnitNameFields* fields, uint32_t category)
{
    uint32_t own = unitNameZero + category;
    if ((fields->found & 1u << own) != 0)
    {
        return own;
    }
    return (fields->found & 1u << unitNameOther) != 0 ? unitNameOther : unitNameFields;
}
