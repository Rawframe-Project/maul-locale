// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A relative day and a time joined (maul-locale/relative.h): the time
// formatter's relative date-time glue around the relative formatter's day
// and the time formatter's time.

#include "date_data.h"
#include "date_time.h"
#include "date_write.h"
#include "invariant.h"
#include "number_core.h"
#include "number_output.h"
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
    DataSpan glue;
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

// The glue with its slots, as one writing: the day's failure, if any.
static mlocResult WriteDayTime(const void* context, NumberWriter* w)
{
    DayTimeWriting d = *(const DayTimeWriting*)context;
    mlocWriteGlue(d.glue.bytes, d.glue.size, WriteSlot, &d, w);
    return d.result;
}

mlocResult mlocRelativeFormatter_FormatDayTime(const mlocRelativeFormatter* relative,
                                               const mlocTimeFormatter* time,
                                               const mlocDecimal* days, const mlocTime* timeOfDay,
                                               mlocOutput* textOut)
{
    RelativeFormat r;
    TimeFormat t;
    if (!mlocReadRelativeFormat(relative, &r) || !mlocReadTimeFormat(time, &t) ||
        !mlocValidRelativeAmount(days, relativeDay) || timeOfDay == nullptr ||
        !mlocTimeValid(timeOfDay) || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0))
    {
        return mloc_errorInvalid;
    }
    // Long, short and narrow take the long, medium and short glue. A
    // sound relative formatter's width is at most narrow.
    MLOC_ASSERT(r.width + 1 < dateStandardCount);
    DataSpan glue;
    if (!mlocTablePattern(&t.range.table, t.range.relativeGlue[r.width + 1], &glue))
    {
        return mloc_errorInvalid;
    }
    r.capitalize = glue.size >= 3 && memcmp(glue.bytes, "{1}", 3) == 0 ? r.capitalize : 0;
    // The day, the one slot that can fail, may come after text the glue
    // has written.
    DayTimeWriting d = {&r, &t, days, timeOfDay, glue, mloc_success};
    return mlocWriteOutput(WriteDayTime, &d, textOut);
}
