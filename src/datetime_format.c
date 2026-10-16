// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date-time formatter (maul-locale/datetime.h, record mloc-0042): a
// date formatter's state, a time formatter's, and the calendar's atTime
// glue between them, as ICU4C's DateTimePatternGenerator composes a
// skeleton it has no single item for; or a weekday with a time, which
// one item covers, matched whole unless a zone follows the time.

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
};

typedef struct DateTimeFormat
{
    DateFormat date;
    TimeFormat time;
    // The zone's state when the time's patterns hold a zone.
    ZoneFormat zone;
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
           o->date.zone == mloc_zoneStyleNone && (uint32_t)o->glue <= mloc_dateTimeGlueStandard &&
           standard == (o->time.style != mloc_timeStyleNone) &&
           (standard || o->date.fields <= mloc_dateFieldSetWeekday);
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
    uint32_t style = mlocDateGlueStyle(&o->date, mlocLoad16(choice.set + 2 * dateTimeFlags));
    uint32_t kind = o->glue == mloc_dateTimeGlueStandard ? dateTimeGlue : dateTimeAtGlue;
    DataSpan glue;
    if (!mlocDatePattern(&choice.patterns, mlocLoad16(choice.set + 2 * (kind + style)), &glue) ||
        !mlocValidGlue(glue.bytes, glue.size))
    {
        return mloc_errorData;
    }
    f->glue = glue.bytes;
    f->glueLength = glue.size;
    if (o->date.style == mloc_dateStyleNone && o->date.fields == mloc_dateFieldSetWeekday &&
        !f->time.zoned && !FindWhole(f, o, &choice))
    {
        return mloc_errorData;
    }
    mlocZoneStyle zoneStyle =
        o->time.zone != mloc_zoneStyleNone ? o->time.zone : mloc_zoneStyleSpecificShort;
    mlocResult zone =
        f->time.zoned ? mlocMakeZoneFormat(&f->zone, data, locale, zoneStyle) : mloc_success;
    if (zone < 0)
    {
        return zone;
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

// A formatter the library made, copied out, with a zone or without as
// asked; false for any other.
static bool ReadFormatter(const mlocDateTimeFormatter* opaque, bool zoned, DateTimeFormat* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    return f->stamp == dateTimeStamp && mlocDateFormatSound(&f->date) &&
           mlocTimeFormatSound(&f->time) && f->glue != nullptr &&
           (f->whole[0] == nullptr) == (f->whole[1] == nullptr) && f->time.zoned == zoned &&
           (!zoned || (mlocZoneFormatSound(&f->zone) && f->whole[0] == nullptr));
}

typedef struct Writing
{
    const DateTimeFormat* f;
    const DateValue* date;
    const mlocTime* time;
    // The time's pattern: 1 for a minute of zero.
    uint32_t pattern;
    // The zone's, or null for a date-time without one.
    const ZoneWriting* zone;
} Writing;

// A field of a pattern matched whole other than the time's: its weekday.
static void WriteWeekday(void* context, const uint8_t* item, NumberWriter* w)
{
    const Writing* g = context;
    mlocWriteDateFields(&g->f->date, item, 3, &g->date->fields, g->date->related, w);
}

// The glue's slots: the time, with its zone, and the date.
static void WriteSlot(void* context, uint32_t slot, NumberWriter* w)
{
    const Writing* g = context;
    if (slot == 0)
    {
        const TimeFormat* t = &g->f->time;
        mlocWriteTimeFieldsWith(t, t->pattern[g->pattern], t->patternLength[g->pattern], g->time,
                                mlocWriteZoneField, (void*)g->zone, w);
        return;
    }
    const DateFormat* d = &g->f->date;
    uint32_t p = g->date->pattern;
    mlocWriteDateFields(d, d->pattern[p], d->patternLength[p], &g->date->fields, g->date->related,
                        w);
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
    mlocWriteGlue(f->glue, f->glueLength, WriteSlot, (void*)g, w);
}

static mlocResult WriteOut(const Writing* g, mlocOutput* output)
{
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    Write(g, &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        Write(g, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}

mlocResult mlocFormatDateTime(const mlocDateTimeFormatter* formatter, const mlocDateTime* dateTime,
                              mlocOutput* output)
{
    DateTimeFormat f;
    DateValue date;
    if (!ReadFormatter(formatter, false, &f) || dateTime == nullptr ||
        !mlocDateTimeValid(dateTime) || output == nullptr ||
        (output->buffer == nullptr && output->capacity != 0) ||
        !mlocReadDateValue(&f.date, &dateTime->date, &date))
    {
        return mloc_errorInvalid;
    }
    Writing g = {&f, &date, &dateTime->time, dateTime->time.minute == 0 ? 1 : 0, nullptr};
    return WriteOut(&g, output);
}

mlocResult mlocFormatDateTimeInZone(const mlocDateTimeFormatter* formatter,
                                    const mlocTimeZone* zone, const mlocInstant* instant,
                                    mlocOutput* output)
{
    DateTimeFormat f;
    DateValue date;
    mlocDateTime local;
    if (!ReadFormatter(formatter, true, &f) || zone == nullptr || instant == nullptr ||
        output == nullptr || (output->buffer == nullptr && output->capacity != 0) ||
        mlocGetDateTimeFor(zone, instant, mloc_calendarIso8601, &local) != mloc_success ||
        !mlocReadDateValue(&f.date, &local.date, &date))
    {
        return mloc_errorInvalid;
    }
    ZoneWriting writing = {&f.zone, zone, instant};
    Writing g = {&f, &date, &local.time, local.time.minute == 0 ? 1 : 0, &writing};
    return WriteOut(&g, output);
}
