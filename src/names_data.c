// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The names part's units (names_data.h).

#include "names_data.h"

#include "bytes.h"
#include "data_format.h"
#include "invariant.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "number_data.h"

#include <string.h>

// Whether a byte may be in a code.
static bool CodeByte(uint8_t c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
           c == ':' || c == '%';
}

// Compares a code with a text, by bytes then by length.
static int CompareCode(const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    size_t shorter = aLength < bLength ? aLength : bLength;
    int order = shorter == 0 ? 0 : memcmp(a, b, shorter);
    if (order != 0)
    {
        return order;
    }
    return aLength == bLength ? 0 : (aLength < bLength ? -1 : 1);
}

bool mlocValidNameIds(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < nameIdsHeaderSize || mlocLoad16(payload.bytes + 2) != 0)
    {
        return false;
    }
    uint64_t count = mlocLoad16(payload.bytes);
    uint64_t textSize = mlocLoad32(payload.bytes + 4);
    if (count > nameMaxCodes || payload.size != nameIdsHeaderSize + 4 * (count + 1) + textSize)
    {
        return false;
    }
    const uint8_t* offsets = payload.bytes + nameIdsHeaderSize;
    const uint8_t* text = offsets + 4 * (count + 1);
    if (mlocLoad32(offsets) != 0 || mlocLoad32(offsets + 4 * count) != textSize)
    {
        return false;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t start = mlocLoad32(offsets + (size_t)i * 4);
        uint32_t end = mlocLoad32(offsets + (size_t)i * 4 + 4);
        // Each code ends within the text before any of it is read.
        if (end <= start || end > textSize || end - start > nameMaxId)
        {
            return false;
        }
        for (uint32_t at = start; at < end; at++)
        {
            if (!CodeByte(text[at]))
            {
                return false;
            }
        }
        uint32_t before = i == 0 ? 0 : mlocLoad32(offsets + (size_t)i * 4 - 4);
        if (i > 0 && CompareCode(text + before, start - before, text + start, end - start) >= 0)
        {
            return false;
        }
    }
    return true;
}

bool mlocValidNames(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < nameUnitHeaderSize)
    {
        return false;
    }
    uint32_t count = mlocLoad16(payload.bytes);
    uint32_t textSize = mlocLoad16(payload.bytes + 2);
    if (payload.size != nameUnitHeaderSize + 2 * (size_t)count + 2 * ((size_t)count + 1) + textSize)
    {
        return false;
    }
    const uint8_t* keys = payload.bytes + nameUnitHeaderSize;
    const uint8_t* offsets = keys + 2 * (size_t)count;
    const uint8_t* text = offsets + 2 * ((size_t)count + 1);
    if (mlocLoad16(offsets) != 0 || mlocLoad16(offsets + 2 * (size_t)count) != textSize)
    {
        return false;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t start = mlocLoad16(offsets + (size_t)i * 2);
        uint32_t end = mlocLoad16(offsets + (size_t)i * 2 + 2);
        if ((i > 0 && mlocLoad16(keys + (size_t)i * 2) <= mlocLoad16(keys + (size_t)i * 2 - 2)) ||
            end <= start || end > textSize || !mlocValidDataText(text + start, end - start, 0))
        {
            return false;
        }
    }
    return true;
}

bool mlocFindNameId(DataSpan ids, const char* code, size_t length, uint16_t* indexOut)
{
    uint32_t count = mlocLoad16(ids.bytes);
    const uint8_t* offsets = ids.bytes + nameIdsHeaderSize;
    const uint8_t* text = offsets + 4 * ((size_t)count + 1);
    uint32_t low = 0;
    uint32_t high = count;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint32_t start = mlocLoad32(offsets + (size_t)middle * 4);
        uint32_t end = mlocLoad32(offsets + (size_t)middle * 4 + 4);
        int order = CompareCode(text + start, end - start, (const uint8_t*)code, length);
        if (order == 0)
        {
            *indexOut = (uint16_t)middle;
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

typedef struct Finding
{
    const char* code;
    size_t length;
    NameField field;
    // The name met first, for a field the chain lacks.
    NameText name;
    NameText* out;
    // The code's index in the source last visited, whose codes unit the
    // walk's later steps would look up again.
    const uint8_t* source;
    uint16_t index;
} Finding;

// The entry text of a valid names unit.
static NameText EntryText(const uint8_t* offsets, const uint8_t* text, uint32_t entry)
{
    uint16_t start = mlocLoad16(offsets + (size_t)entry * 2);
    return (NameText){text + start,
                      (uint16_t)(mlocLoad16(offsets + (size_t)entry * 2 + 2) - start)};
}

// Reads a code's field and name from a valid names unit; true when the
// field is there.
static bool UnitField(DataSpan unit, uint16_t index, NameField field, NameText* fieldOut,
                      NameText* nameOut)
{
    // Both callers refuse a field past the last, and the index is one
    // mlocFindNameId found in an ids unit of at most nameMaxCodes codes,
    // so the code's keys fit in 16 bits.
    MLOC_ASSERT((uint32_t)field < nameFieldCount);
    MLOC_ASSERT(index < nameMaxCodes);
    uint32_t count = mlocLoad16(unit.bytes);
    const uint8_t* keys = unit.bytes + nameUnitHeaderSize;
    const uint8_t* offsets = keys + 2 * (size_t)count;
    const uint8_t* text = offsets + 2 * ((size_t)count + 1);
    // The first entry of the code: the least key not below its name's.
    uint32_t first = (uint32_t)index * nameFieldCount;
    uint32_t low = 0;
    uint32_t high = count;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        if (mlocLoad16(keys + (size_t)middle * 2) < first)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    for (uint32_t e = low; e < count; e++)
    {
        uint32_t key = mlocLoad16(keys + (size_t)e * 2);
        if (key >= first + nameFieldCount)
        {
            break;
        }
        if (key == first + field)
        {
            *fieldOut = EntryText(offsets, text, e);
            return true;
        }
        if (key == first && nameOut->text == nullptr)
        {
            *nameOut = EntryText(offsets, text, e);
        }
    }
    return false;
}

static bool Find(void* context, DataSpan unit, const DataView* view, int depth)
{
    (void)depth;
    Finding* f = context;
    DataSpan ids;
    if (f->source != view->bytes && (!mlocFindDataUnit(view, kindNameIds, dataNoLocale, &ids) ||
                                     !mlocFindNameId(ids, f->code, f->length, &f->index)))
    {
        f->source = nullptr;
        return false;
    }
    f->source = view->bytes;
    return UnitField(unit, f->index, f->field, f->out, &f->name);
}

bool mlocFindName(const mlocDataSet* data, const mlocLocale* locale, const char* code,
                  size_t length, NameField field, NameText* out)
{
    *out = (NameText){0};
    uint32_t kind = mlocNameKind(code, length);
    Finding f = {.code = code, .length = length, .field = field, .out = out};
    if (kind == 0 || (uint32_t)field >= nameFieldCount ||
        !mlocVisitLocaleUnits(data, mloc_componentMain, kind, locale, Find, &f))
    {
        return false;
    }
    if (out->text == nullptr)
    {
        *out = f.name;
    }
    return out->text != nullptr;
}

static bool TakeStep(void* context, const DataView* view, uint32_t locale, int depth)
{
    (void)depth;
    NameChain* chain = context;
    DataSpan ids;
    if (!mlocFindDataUnit(view, kindNameIds, dataNoLocale, &ids))
    {
        return false;
    }
    if (chain->count == nameChainMost)
    {
        chain->count = nameChainMost + 1;
        return true;
    }
    chain->steps[chain->count++] = (NameStep){*view, ids, locale};
    return false;
}

bool mlocOpenNameChain(const mlocDataSet* data, const mlocLocale* locale, NameChain* out)
{
    out->data = data;
    out->locale = locale;
    out->count = 0;
    return mlocVisitLocaleChain(data, mloc_componentMain, locale, TakeStep, out);
}

bool mlocFindChainName(const NameChain* chain, const char* code, size_t length, NameField field,
                       NameText* out)
{
    if (chain->count > nameChainMost)
    {
        return mlocFindName(chain->data, chain->locale, code, length, field, out);
    }
    *out = (NameText){0};
    uint32_t kind = mlocNameKind(code, length);
    if (kind == 0 || (uint32_t)field >= nameFieldCount)
    {
        return false;
    }
    NameText name = {0};
    // The code's index in the source last looked up.
    const uint8_t* source = nullptr;
    bool known = false;
    uint16_t index = 0;
    for (uint32_t i = 0; i < chain->count; i++)
    {
        const NameStep* step = &chain->steps[i];
        if (step->view.bytes != source)
        {
            source = step->view.bytes;
            known = mlocFindNameId(step->ids, code, length, &index);
        }
        DataSpan unit;
        bool none = false;
        if (known && mlocFindDataUnitOrKind(&step->view, kind, step->locale, &unit, &none) &&
            UnitField(unit, index, field, out, &name))
        {
            return true;
        }
    }
    *out = name;
    return out->text != nullptr;
}
