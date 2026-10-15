// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date pattern units (date_data.h).

#include "date_data.h"

#include "bytes.h"
#include "data_format.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "number_data.h"

#include <string.h>

// A field item's length at text, or 0 when it is malformed.
static uint32_t FieldLength(const uint8_t* text, uint32_t left)
{
    if (left < 3 || text[1] == '\0' || strchr(datePatternLetters, text[1]) == nullptr ||
        text[2] < 1 || text[2] > dateMaxWidth)
    {
        return 0;
    }
    if (text[0] == datePatternField)
    {
        return 3;
    }
    uint32_t id = left > 3 ? text[3] : 0;
    if (id < 1 || id > dateMaxSystemId || 4 + id > left)
    {
        return 0;
    }
    for (uint32_t i = 0; i < id; i++)
    {
        if (text[4 + i] < 'a' || text[4 + i] > 'z')
        {
            return 0;
        }
    }
    return 4 + id;
}

bool mlocValidDatePattern(const uint8_t* text, uint32_t length)
{
    uint32_t at = 0;
    while (at < length)
    {
        if (text[at] == datePatternField || text[at] == datePatternOverride)
        {
            uint32_t n = FieldLength(text + at, length - at);
            if (n == 0)
            {
                return false;
            }
            at += n;
            continue;
        }
        uint32_t end = at;
        while (end < length && text[end] >= 0x20)
        {
            end++;
        }
        if (end == at || !mlocValidDataText(text + at, end - at, 0))
        {
            return false;
        }
        at = end;
    }
    return length > 0;
}

// The section offsets of a unit of these counts, or false when they
// overflow or pass its size.
static bool Layout(uint32_t patterns, uint32_t rows, uint32_t sets, uint32_t text, uint32_t size,
                   uint32_t out[3])
{
    uint64_t rowsAt = ((uint64_t)datePatternsHeaderSize + 4 * ((uint64_t)patterns + 1) + 7) & ~7ULL;
    uint64_t setsAt = (rowsAt + (uint64_t)dateRowSize * rows + 7) & ~7ULL;
    uint64_t textAt = (setsAt + (uint64_t)dateSetSize * sets + 7) & ~7ULL;
    if (textAt + text != size)
    {
        return false;
    }
    out[0] = (uint32_t)rowsAt;
    out[1] = (uint32_t)setsAt;
    out[2] = (uint32_t)textAt;
    return true;
}

DatePatterns mlocReadDatePatterns(DataSpan unit)
{
    DatePatterns p = {0};
    p.patternCount = mlocLoad32(unit.bytes);
    p.rowCount = mlocLoad32(unit.bytes + 4);
    p.setCount = mlocLoad32(unit.bytes + 8);
    // A valid unit's layout holds; the offsets stay 0 for any other.
    uint32_t at[3] = {0};
    (void)Layout(p.patternCount, p.rowCount, p.setCount, mlocLoad32(unit.bytes + 12), unit.size,
                 at);
    p.offsets = unit.bytes + datePatternsHeaderSize;
    p.rows = unit.bytes + at[0];
    p.sets = unit.bytes + at[1];
    p.text = unit.bytes + at[2];
    return p;
}

static bool ValidIndexes(const uint8_t* bytes, uint32_t count, uint32_t limit)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (mlocLoad16(bytes + 2 * (size_t)i) >= limit)
        {
            return false;
        }
    }
    return true;
}

bool mlocValidDatePatterns(const DataView* view, DataSpan payload)
{
    (void)view;
    uint32_t at[3] = {0};
    if (payload.size < datePatternsHeaderSize ||
        !Layout(mlocLoad32(payload.bytes), mlocLoad32(payload.bytes + 4),
                mlocLoad32(payload.bytes + 8), mlocLoad32(payload.bytes + 12), payload.size, at))
    {
        return false;
    }
    DatePatterns p = mlocReadDatePatterns(payload);
    uint32_t text = mlocLoad32(payload.bytes + 12);
    if (mlocLoad32(p.offsets) != 0 || mlocLoad32(p.offsets + 4 * (size_t)p.patternCount) != text)
    {
        return false;
    }
    for (uint32_t i = 0; i < p.patternCount; i++)
    {
        uint32_t start = mlocLoad32(p.offsets + 4 * (size_t)i);
        uint32_t end = mlocLoad32(p.offsets + 4 * (size_t)i + 4);
        if (end < start || end > text || !mlocValidDatePattern(p.text + start, end - start))
        {
            return false;
        }
    }
    if (!ValidIndexes(p.rows, p.rowCount * dateLengthCount, p.patternCount))
    {
        return false;
    }
    for (uint32_t s = 0; s < p.setCount; s++)
    {
        const uint8_t* set = p.sets + (size_t)s * dateSetSize;
        if (!ValidIndexes(set, dateEntryCount, p.rowCount) ||
            !ValidIndexes(set + 2 * dateEntryCount, dateStandardCount, p.patternCount))
        {
            return false;
        }
    }
    return true;
}

bool mlocValidDates(const DataView* view, DataSpan payload)
{
    (void)view;
    return payload.size == dateLocaleSize;
}

bool mlocFindDateSet(const mlocDataSet* data, const mlocLocale* locale, uint32_t calendar,
                     DatePatterns* patterns, const uint8_t** set)
{
    DataSpan dates;
    DataSpan unit;
    bool fellBack = false;
    DataView view;
    if (calendar >= dateCalendarCount ||
        !mlocFindLocaleUnit(data, mloc_componentMain, kindDates, locale, &dates, &fellBack,
                            &view) ||
        !mlocFindDataUnit(&view, kindDatePatterns, dataNoLocale, &unit))
    {
        return false;
    }
    *patterns = mlocReadDatePatterns(unit);
    uint32_t index = mlocLoad16(dates.bytes + 2 * (size_t)calendar);
    if (index >= patterns->setCount)
    {
        return false;
    }
    *set = patterns->sets + (size_t)index * dateSetSize;
    return true;
}

bool mlocDatePattern(const DatePatterns* p, uint32_t index, DataSpan* out)
{
    if (index >= p->patternCount)
    {
        return false;
    }
    uint32_t start = mlocLoad32(p->offsets + 4 * (size_t)index);
    out->bytes = p->text + start;
    out->size = mlocLoad32(p->offsets + 4 * (size_t)index + 4) - start;
    return true;
}
