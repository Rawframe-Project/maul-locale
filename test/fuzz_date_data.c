// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the date patterns and date names units'
// validators (record mloc-0042). The input is a payload of either; a
// patterns unit the validator accepts must give every pattern from inside
// its text, well formed, and every row's, set's and time set's pattern
// by index; a names unit, every name of every set's lists from inside
// its text and a period of every hour from its rules.

#include "bytes.h"
#include "date_data.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static void CheckNames(DataSpan span)
{
    DateNameTables t = mlocReadDateNames(span);
    for (uint32_t s = 0; s < t.setCount; s++)
    {
        for (uint32_t k = 0; k < dateNameKindCount; k++)
        {
            uint32_t index = mlocLoad16(t.sets + (size_t)s * dateNameSetSize + 2 * k);
            Require(index < t.listCount);
            DateNameList list = mlocDateNameList(&t, index);
            for (uint32_t i = 0; i < list.count; i++)
            {
                DataSpan name = mlocDateName(&t, list, i);
                Require(name.bytes >= span.bytes &&
                        name.bytes + name.size <= span.bytes + span.size);
            }
        }
    }
    for (uint32_t r = 0; r < t.ruleCount; r++)
    {
        const uint8_t* rule = t.rules + (size_t)r * dateRuleSize;
        Require(rule + dateRuleSize <= span.bytes + span.size);
        for (uint32_t hour = 0; hour < 24; hour++)
        {
            Require(rule[hour] < dateDayPeriodCount);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Aligned, as a payload in a data file is.
    static uint64_t s_bytes[1024];
    if (size > sizeof(s_bytes))
    {
        return 0;
    }
    memcpy(s_bytes, data, size);
    DataSpan span = {(const uint8_t*)s_bytes, (uint32_t)size};
    DataView view = {0};
    if (mlocValidDateNames(&view, span))
    {
        CheckNames(span);
    }
    if (!mlocValidDatePatterns(&view, span))
    {
        return 0;
    }
    DatePatterns p = mlocReadDatePatterns(span);
    for (uint32_t i = 0; i < p.patternCount; i++)
    {
        DataSpan pattern;
        Require(mlocDatePattern(&p, i, &pattern));
        Require(pattern.bytes >= span.bytes && pattern.bytes + pattern.size <= span.bytes + size);
        Require(mlocValidDatePattern(pattern.bytes, pattern.size));
    }
    for (uint32_t r = 0; r < p.rowCount * dateLengthCount; r++)
    {
        DataSpan pattern;
        Require(mlocDatePattern(&p, mlocLoad16(p.rows + 2 * (size_t)r), &pattern));
    }
    for (uint32_t s = 0; s < p.setCount; s++)
    {
        const uint8_t* set = p.sets + (size_t)s * dateSetSize;
        for (uint32_t e = 0; e < dateEntryCount; e++)
        {
            Require(mlocLoad16(set + 2 * (size_t)e) < p.rowCount);
        }
        for (uint32_t i = 0; i < dateStandardCount; i++)
        {
            DataSpan pattern;
            Require(
                mlocDatePattern(&p, mlocLoad16(set + 2 * (dateEntryCount + (size_t)i)), &pattern));
        }
    }
    for (uint32_t s = 0; s < p.timeSetCount; s++)
    {
        const uint8_t* set = p.timeSets + (size_t)s * dateTimeSetSize;
        for (uint32_t i = 0; i < dateTimePatternCount; i++)
        {
            DataSpan pattern;
            Require(mlocDatePattern(&p, mlocLoad16(set + 2 * (size_t)i), &pattern));
        }
        uint32_t flags = mlocLoad16(set + 2 * dateTimeFlags);
        Require(flags <= dateTimeFlagsMax);
        for (uint32_t length = 0; length < dateLengthCount; length++)
        {
            Require((flags >> (dateTimeMonthShift + 2 * length) & 3) != 3);
        }
    }
    return 0;
}
