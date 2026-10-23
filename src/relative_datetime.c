// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A relative day and a time joined (maul-locale/relative.h): the time
// formatter's relative date-time glue around the relative formatter's day
// and the time formatter's time.

#include "date_data.h"
#include "date_time.h"
#include "date_write.h"
#include "number_core.h"
#include "relative_format.h"
#include "relative_tables.h"
#include "time_format.h"
#include "time_write.h"

#include <string.h>

typedef struct DayTimeWriting
{
    const RelativeFormat* relative;
    const TimeFormat* time;
    const mlocDecimal* days;
    const mlocTime* timeOfDay;
    mlocResult result;
} DayTimeWriting;

// Writes the glue's {0}, the time, or its {1}, the day.
static void WriteSlot(void* context, uint32_t slot, NumberWriter* w)
{
    DayTimeWriting* d = context;
    if (slot == 1)
    {
        mlocResult r =
            mlocWriteRelative(d->relative, d->days, relativeDay, mloc_datePartRelativeDay, w);
        d->result = r != mloc_success ? r : d->result;
        return;
    }
    uint32_t p = d->timeOfDay->minute == 0 ? 1 : 0;
    mlocWriteTimeFields(d->time, d->time->pattern[p], d->time->patternLength[p], d->timeOfDay, w);
}

mlocResult mlocFormatRelativeDayTime(const mlocRelativeFormatter* relative,
                                     const mlocTimeFormatter* time, const mlocDecimal* days,
                                     const mlocTime* timeOfDay, mlocOutput* output)
{
    RelativeFormat r;
    TimeFormat t;
    if (!mlocReadRelativeFormat(relative, &r) || !mlocReadTimeFormat(time, &t) ||
        !mlocValidRelativeAmount(days, relativeDay) || timeOfDay == nullptr ||
        !mlocTimeValid(timeOfDay) || output == nullptr ||
        (output->buffer == nullptr && output->capacity != 0))
    {
        return mloc_errorInvalid;
    }
    // Long, short and narrow take the long, medium and short glue.
    DataSpan glue;
    if (!mlocTablePattern(&t.range.table, t.range.relativeGlue[r.width + 1], &glue))
    {
        return mloc_errorInvalid;
    }
    r.capitalize = glue.size >= 3 && memcmp(glue.bytes, "{1}", 3) == 0 ? r.capitalize : 0;
    DayTimeWriting d = {&r, &t, days, timeOfDay, mloc_success};
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    mlocWriteGlue(glue.bytes, glue.size, WriteSlot, &d, &w);
    output->length = w.length;
    if (d.result != mloc_success)
    {
        return d.result;
    }
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        mlocWriteGlue(glue.bytes, glue.size, WriteSlot, &d, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}
