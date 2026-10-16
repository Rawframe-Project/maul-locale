// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date-time formatter (maul-locale/datetime.h, record mloc-0042): a
// date formatter's state, a time formatter's, and the calendar's atTime
// glue between them, as ICU4C's DateTimePatternGenerator composes a
// skeleton it has no single item for; or a weekday with a time, which
// one item covers, matched whole.

#include "bytes.h"
#include "date_format.h"
#include "date_time.h"
#include "time_format.h"

#include "maul-locale/datetime.h"

#include <string.h>

enum
{
    // Marks a date-time formatter the library made.
    dateTimeStamp = 0x4D,
    // The glue's styles, as the standard date formats' lengths.
    glueFull = 0,
    glueLong = 1,
    glueMedium = 2,
    glueShort = 3,
};

typedef struct DateTimeFormat
{
    DateFormat date;
    TimeFormat time;
    // The glue, literal text with {0} for the time and {1} for the date.
    const uint8_t* glue;
    uint32_t glueLength;
    // A weekday with a time matched whole, as the time's patterns are
    // kept (the second for minute-optional precision is the hour's).
    const uint8_t* whole[timeFormatPatterns];
    uint32_t wholeLength[timeFormatPatterns];
    uint8_t stamp;
} DateTimeFormat;

static_assert(sizeof(DateTimeFormat) <= sizeof(mlocDateTimeFormatter),
              "a date-time formatter fits its value");

static bool ValidOptions(const mlocDateTimeOptions* o)
{
    bool standard = o->date.style != mloc_dateStyleNone;
    return mlocValidDateOptions(&o->date) && mlocValidTimeOptions(&o->time) &&
           (uint32_t)o->glue <= mloc_dateTimeGlueStandard &&
           standard == (o->time.style != mloc_timeStyleNone) &&
           (standard || o->date.fields <= mloc_dateFieldSetWeekday);
}

// The glue's style: a standard date format's length, or by the field
// set's month at its length (four letters with a weekday full, four
// letters long, three medium, else short), as ICU4C chooses it.
static uint32_t GlueStyle(const mlocDateOptions* o, uint32_t flags)
{
    if (o->style != mloc_dateStyleNone)
    {
        return (uint32_t)o->style - 1;
    }
    mlocDateFieldSet s = o->fields;
    bool month = s == mloc_dateFieldSetYearMonthDay || s == mloc_dateFieldSetYearMonthDayWeekday ||
                 s == mloc_dateFieldSetMonthDay || s == mloc_dateFieldSetMonthDayWeekday;
    bool weekday = s == mloc_dateFieldSetYearMonthDayWeekday ||
                   s == mloc_dateFieldSetMonthDayWeekday || s == mloc_dateFieldSetDayWeekday;
    uint32_t code = flags >> (dateTimeMonthShift + 2 * (uint32_t)o->length) & 3;
    if (!month || code == 0)
    {
        return glueShort;
    }
    return code == 1 ? glueMedium : weekday ? glueFull : glueLong;
}

// Whether glue holds {0} and {1}.
static bool ValidGlue(const uint8_t* glue, uint32_t length)
{
    bool time = false;
    bool date = false;
    for (uint32_t at = 0; at + 3 <= length; at++)
    {
        time = time || memcmp(glue + at, "{0}", 3) == 0;
        date = date || memcmp(glue + at, "{1}", 3) == 0;
    }
    return time && date;
}

// Whether a weekday-with-time pattern holds only a weekday and the
// fields the time formatter writes.
static bool WritableWhole(const uint8_t* pattern, uint32_t length)
{
    for (uint32_t at = 0; at < length;)
    {
        if (pattern[at] >= 0x20)
        {
            at++;
            continue;
        }
        if (pattern[at] != datePatternField || strchr("EcehHKkmsabB", pattern[at + 1]) == nullptr)
        {
            return false;
        }
        at += 3;
    }
    return true;
}

// The weekday-with-time patterns of the options: the time's clock, the
// weekday's width (long EEEE, else EEE) and the time's precision.
static bool FindWhole(DateTimeFormat* f, const mlocDateTimeOptions* o, const TimeChoice* c)
{
    uint32_t base =
        dateTimeWeekday + (c->twelve ? 0 : 6) + (o->date.length == mloc_dateLengthLong ? 0 : 3);
    mlocTimePrecision p = o->time.precision;
    uint32_t entries[timeFormatPatterns] = {base + 2, base + 2};
    if (p == mloc_timePrecisionHour || p == mloc_timePrecisionMinute ||
        p == mloc_timePrecisionMinuteOptional)
    {
        entries[0] = base + (p == mloc_timePrecisionHour ? 0 : 1);
        entries[1] = p == mloc_timePrecisionMinuteOptional ? base : entries[0];
    }
    for (uint32_t i = 0; i < timeFormatPatterns; i++)
    {
        DataSpan pattern;
        if (!mlocDatePattern(&c->patterns, mlocLoad16(c->set + 2 * (size_t)entries[i]), &pattern) ||
            !WritableWhole(pattern.bytes, pattern.size))
        {
            return false;
        }
        f->whole[i] = pattern.bytes;
        f->wholeLength[i] = pattern.size;
    }
    return true;
}

static mlocResult Resolve(DateTimeFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                          const mlocDateTimeOptions* o)
{
    TimeChoice choice;
    mlocResult date = mlocMakeDateFormat(&f->date, data, locale, &o->date);
    mlocResult time =
        date < 0 ? date : mlocMakeTimeFormat(&f->time, data, locale, &o->time, &choice);
    if (time < 0)
    {
        return time;
    }
    uint32_t style = GlueStyle(&o->date, mlocLoad16(choice.set + 2 * dateTimeFlags));
    uint32_t kind = o->glue == mloc_dateTimeGlueStandard ? dateTimeGlue : dateTimeAtGlue;
    DataSpan glue;
    if (!mlocDatePattern(&choice.patterns, mlocLoad16(choice.set + 2 * (kind + style)), &glue) ||
        !ValidGlue(glue.bytes, glue.size))
    {
        return mloc_errorData;
    }
    f->glue = glue.bytes;
    f->glueLength = glue.size;
    if (o->date.style == mloc_dateStyleNone && o->date.fields == mloc_dateFieldSetWeekday &&
        !FindWhole(f, o, &choice))
    {
        return mloc_errorData;
    }
    return date < time ? date : time;
}

mlocResult mlocInitDateTimeFormatter(mlocDateTimeFormatter* formatterOut, const mlocDataSet* data,
                                     const mlocLocale* locale, const mlocDateTimeOptions* options)
{
    mlocDateTimeOptions o = options != nullptr ? *options : (mlocDateTimeOptions){0};
    if (formatterOut == nullptr || data == nullptr || locale == nullptr || !ValidOptions(&o))
    {
        return mloc_errorInvalid;
    }
    DateTimeFormat f;
    memset(&f, 0, sizeof(f));
    mlocResult result = Resolve(&f, data, locale, &o);
    if (result < 0)
    {
        return result;
    }
    f.stamp = dateTimeStamp;
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

// A formatter the library made, copied out; false for any other.
static bool ReadFormatter(const mlocDateTimeFormatter* opaque, DateTimeFormat* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    return f->stamp == dateTimeStamp && mlocDateFormatSound(&f->date) &&
           mlocTimeFormatSound(&f->time) && f->glue != nullptr &&
           (f->whole[0] == nullptr) == (f->whole[1] == nullptr);
}

typedef struct Writing
{
    const DateTimeFormat* f;
    const DateValue* date;
    const mlocTime* time;
    // The time's pattern: 1 for a minute of zero.
    uint32_t pattern;
} Writing;

// A weekday field of a pattern matched whole.
static void WriteWeekday(void* context, const uint8_t* item, NumberWriter* w)
{
    const Writing* g = context;
    mlocWriteDateFields(&g->f->date, item, 3, &g->date->fields, g->date->related, w);
}

static void WriteDate(const Writing* g, NumberWriter* w)
{
    const DateFormat* d = &g->f->date;
    uint32_t p = g->date->pattern;
    mlocWriteDateFields(d, d->pattern[p], d->patternLength[p], &g->date->fields, g->date->related,
                        w);
}

static void WriteTime(const Writing* g, NumberWriter* w)
{
    const TimeFormat* t = &g->f->time;
    mlocWriteTimeFields(t, t->pattern[g->pattern], t->patternLength[g->pattern], g->time, w);
}

// The date and time by the glue, or the whole pattern.
static void Write(const Writing* g, NumberWriter* w)
{
    const DateTimeFormat* f = g->f;
    if (f->whole[0] != nullptr)
    {
        mlocWriteTimeFieldsWith(&f->time, f->whole[g->pattern], f->wholeLength[g->pattern], g->time,
                                WriteWeekday, (void*)g, w);
        return;
    }
    uint32_t at = 0;
    while (at < f->glueLength)
    {
        bool slot = at + 3 <= f->glueLength && f->glue[at] == '{' && f->glue[at + 2] == '}' &&
                    (f->glue[at + 1] == '0' || f->glue[at + 1] == '1');
        if (slot)
        {
            if (f->glue[at + 1] == '0')
            {
                WriteTime(g, w);
            }
            else
            {
                WriteDate(g, w);
            }
            at += 3;
            continue;
        }
        uint32_t end = at + 1;
        while (end < f->glueLength && f->glue[end] != '{')
        {
            end++;
        }
        mlocWritePart(w, mloc_datePartLiteral, f->glue + at, end - at);
        at = end;
    }
}

mlocResult mlocFormatDateTime(const mlocDateTimeFormatter* formatter, const mlocDateTime* dateTime,
                              mlocOutput* output)
{
    DateTimeFormat f;
    DateValue date;
    if (!ReadFormatter(formatter, &f) || dateTime == nullptr || !mlocDateTimeValid(dateTime) ||
        output == nullptr || (output->buffer == nullptr && output->capacity != 0) ||
        !mlocReadDateValue(&f.date, &dateTime->date, &date))
    {
        return mloc_errorInvalid;
    }
    Writing g = {&f, &date, &dateTime->time, dateTime->time.minute == 0 ? 1 : 0};
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    Write(&g, &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        Write(&g, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}
