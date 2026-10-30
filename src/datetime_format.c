// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date-time formatter (maul-locale/datetime.h): a date formatter's
// state, a time formatter's, and the calendar's atTime glue between them,
// as ICU4C's DateTimePatternGenerator composes a skeleton it has no single
// item for; or a weekday with a time, which one item covers, matched whole
// unless a zone follows the time.

#include "bytes.h"
#include "date_format.h"
#include "date_range.h"
#include "date_time.h"
#include "invariant.h"
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
    // A range's glues: the calendar's time set and the glue's kind
    // (dateTimeGlue or dateTimeAtGlue), whose length a prefixed date's
    // style picks, and the Gregorian medium standard glue that holds a
    // time's range on one day (ICU's DateTimePatterns[kDateTime]).
    const uint8_t* timeSet;
    uint32_t glueKind;
    const uint8_t* dayGlue;
    uint32_t dayGlueLength;
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
        // A valid pattern's fields are whole (mlocValidDatePattern).
        MLOC_ASSERT(at + 3 <= length);
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

// A range's glues: each length's of the formatter's kind, checked, and
// the Gregorian medium standard glue.
static bool FindRangeGlues(DateTimeFormat* f, const DateLocale* shared, const TimeChoice* c,
                           uint32_t kind)
{
    const DatePatterns* gregorian = &shared->units.patterns;
    const uint8_t* set = nullptr;
    DataSpan glue;
    for (uint32_t length = 0; length < dateStandardCount; length++)
    {
        if (!mlocDatePattern(&c->patterns, mlocLoad16(c->set + 2 * (kind + length)), &glue) ||
            !mlocValidGlue(glue.bytes, glue.size))
        {
            return false;
        }
    }
    if (!mlocTimeSetOf(&shared->units, mloc_calendarGregorian, &set, nullptr) ||
        !mlocDatePattern(gregorian, mlocLoad16(set + 2 * (dateTimeGlue + 2)), &glue) ||
        !mlocValidGlue(glue.bytes, glue.size))
    {
        return false;
    }
    f->timeSet = c->set;
    f->glueKind = kind;
    f->dayGlue = glue.bytes;
    f->dayGlueLength = glue.size;
    return true;
}

static mlocResult Resolve(DateTimeFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                          const mlocDateTimeOptions* o)
{
    TimeChoice choice;
    // A weekday alone at the short length is narrow, with a time
    // abbreviated (UTS #35's semantic skeletons): its medium patterns.
    mlocDateOptions dateOptions = o->date;
    if (dateOptions.style == mloc_dateStyleNone && dateOptions.fields == mloc_dateFieldSetWeekday &&
        dateOptions.length == mloc_dateLengthShort)
    {
        dateOptions.length = mloc_dateLengthMedium;
    }
    // The date and the time read the locale's preferences, digits and
    // names once.
    DateLocale shared;
    mlocResult read = mlocReadDateLocale(&shared, data, locale);
    mlocResult date =
        read < 0 ? read : mlocMakeDateFormat(&f->date, data, locale, &shared, &dateOptions);
    mlocResult time =
        date < 0 ? date : mlocMakeTimeFormat(&f->time, data, locale, &shared, &o->time, &choice);
    if (time < 0)
    {
        return time;
    }
    uint32_t style = mlocDateGlueStyle(&o->date, mlocLoad16(choice.set + 2 * dateTimeFlags));
    // A standard format's length (ValidOptions), or a glue's.
    MLOC_ASSERT(style < dateStandardCount);
    uint32_t kind = o->glue == mloc_dateTimeGlueStandard ? dateTimeGlue : dateTimeAtGlue;
    // The range's glues hold every length's of the kind, checked.
    DataSpan glue;
    if (!FindRangeGlues(f, &shared, &choice, kind) ||
        !mlocDatePattern(&choice.patterns, mlocLoad16(choice.set + 2 * (kind + style)), &glue))
    {
        return mloc_errorFormat;
    }
    f->glue = glue.bytes;
    f->glueLength = glue.size;
    if (o->date.style == mloc_dateStyleNone && o->date.fields == mloc_dateFieldSetWeekday &&
        !f->time.zoned && !FindWhole(f, o, &choice))
    {
        return mloc_errorFormat;
    }
    // The date, the time and the zone read the locale's digits alike, so
    // their results agree.
    mlocZoneStyle zoneStyle =
        o->time.zone != mloc_zoneStyleNone ? o->time.zone : mloc_zoneStyleSpecificShort;
    return f->time.zoned ? mlocMakeZoneFormat(&f->zone, data, locale, zoneStyle) : time;
}

mlocResult mlocInitDateTimeFormatter(const mlocDataSet* data, const mlocLocale* locale,
                                     const mlocDateTimeOptions* options,
                                     mlocDateTimeFormatter* formatterOut)
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
           mlocTimeFormatSound(&f->time) && f->glue != nullptr && f->dayGlue != nullptr &&
           f->timeSet != nullptr && (f->whole[0] == nullptr) == (f->whole[1] == nullptr) &&
           f->time.zoned == zoned &&
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

mlocResult mlocDateTimeFormatter_Format(const mlocDateTimeFormatter* formatter,
                                        const mlocDateTime* dateTime, mlocOutput* textOut)
{
    DateTimeFormat f;
    DateValue date;
    if (!ReadFormatter(formatter, false, &f) || dateTime == nullptr ||
        !mlocDateTimeValid(dateTime) || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocReadDateValue(&f.date, &dateTime->date, &date))
    {
        return mloc_errorInvalid;
    }
    Writing g = {&f, &date, &dateTime->time, dateTime->time.minute == 0 ? 1 : 0, nullptr};
    return WriteOut(&g, textOut);
}

mlocResult mlocDateTimeFormatter_FormatInZone(const mlocDateTimeFormatter* formatter,
                                              const mlocTimeZone* zone, const mlocInstant* instant,
                                              mlocOutput* textOut)
{
    DateTimeFormat f;
    DateValue date;
    ZoneAt at;
    mlocDateTime local;
    if (!ReadFormatter(formatter, true, &f) || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocReadZonedLocal(&f.zone, zone, instant, &at, &local) ||
        !mlocReadDateValue(&f.date, &local.date, &date))
    {
        return mloc_errorInvalid;
    }
    ZoneWriting writing = {&f.zone, &at};
    Writing g = {&f, &date, &local.time, local.time.minute == 0 ? 1 : 0, &writing};
    return WriteOut(&g, textOut);
}

// A range of date-times: the formatter, both dates, times and zones
// (null without one), the time's pattern both minutes pick.
typedef struct RangeWriting
{
    const DateTimeFormat* f;
    const DateValue* dates[2];
    const mlocTime* times[2];
    const ZoneWriting* zones[2];
    uint32_t pattern;
    // What it writes: one date-time (one), the time's range in the day's
    // glue (day), or each whole around the fallback (join).
    const Writing* one;
    const RangeChoice* day;
    const RangeJoin* join;
    DataSpan date;
    DataSpan fallback;
} RangeWriting;

static void WriteRangeDate(const void* context, uint32_t value, const uint8_t* pattern,
                           uint32_t length, const PatternSpan* span, NumberWriter* w)
{
    const RangeWriting* g = context;
    const DateValue* v = g->dates[value];
    mlocWriteDateFieldsIn(&g->f->date, pattern, length, &v->fields, v->related, span, w);
}

static void WriteRangeTime(const void* context, uint32_t value, const uint8_t* pattern,
                           uint32_t length, const PatternSpan* span, NumberWriter* w)
{
    const RangeWriting* g = context;
    mlocWriteTimeFieldsIn(&g->f->time, pattern, length, g->times[value], mlocWriteZoneField,
                          (void*)g->zones[value], span, w);
}

// The day's glue: {1} the date once, both values', {0} the time's range.
static void WriteDaySlot(void* context, uint32_t slot, NumberWriter* w)
{
    const RangeWriting* g = context;
    if (slot == 0)
    {
        mlocWriteRange(g->day, w);
        return;
    }
    PatternSpan none = {0, 0, 0};
    WriteRangeDate(g, 0, g->date.bytes, g->date.size, &none, w);
}

static void WriteDateTimeRange(const RangeWriting* g, NumberWriter* w)
{
    if (g->join != nullptr)
    {
        mlocWriteJoinedFallback(g->join, g->fallback, w);
    }
    else if (g->day != nullptr)
    {
        mlocWriteGlue(g->f->dayGlue, g->f->dayGlueLength, WriteDaySlot, (void*)g, w);
    }
    else
    {
        Write(g->one, w);
    }
}

static mlocResult WriteRangeOut(const RangeWriting* g, mlocOutput* output)
{
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    WriteDateTimeRange(g, &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        WriteDateTimeRange(g, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}

// Writes a range of date-times read already: on one day the time's range
// inside the day's glue, the date once (one date-time where the time
// shows no difference); on two days each whole around the fallback, the
// date the prefixed pattern where it lacks the greatest difference's
// field, joined by the formatter's glue of that pattern's length.
static mlocResult WriteRangeOf(RangeWriting* g, Writing* one, bool sameDay, mlocOutput* output)
{
    const DateTimeFormat* f = g->f;
    const DateRange* r = &f->date.range;
    const TimeRange* t = &f->time.range;
    uint32_t p =
        g->dates[0]->pattern > g->dates[1]->pattern ? g->dates[0]->pattern : g->dates[1]->pattern;
    DataSpan time = t->own != nullptr ? (DataSpan){t->own, t->ownLength}
                                      : (DataSpan){f->time.pattern[g->pattern],
                                                   f->time.patternLength[g->pattern]};
    g->date = r->own != nullptr ? (DataSpan){r->own, r->ownLength}
                                : (DataSpan){f->date.pattern[p], f->date.patternLength[p]};
    g->fallback = (DataSpan){r->fallback, r->fallbackLength};
    g->one = one;
    RangeChoice day = {.write = WriteRangeTime,
                       .context = g,
                       .table = &t->table,
                       .pair = t->pairs[g->pattern],
                       .difference =
                           mlocTimeDifference(g->times[0], g->times[1], f->time.fractionDigits),
                       .own = time,
                       .fallback = g->fallback};
    if (sameDay)
    {
        g->day = mlocRangeWritesTwo(&day) ? &day : nullptr;
        mlocResult result = WriteRangeOut(g, output);
        g->day = nullptr;
        return result;
    }
    RangeJoin join = {.glue = {f->glue, f->glueLength},
                      .date = g->date,
                      .time = time,
                      .writeDate = WriteRangeDate,
                      .writeTime = WriteRangeTime,
                      .context = g};
    uint32_t style = 0;
    DataSpan prefixed;
    if (mlocRangePrefix(&r->table, r->prefixes[p],
                        mlocDateDifference(&g->dates[0]->fields, &g->dates[1]->fields), &prefixed,
                        &style))
    {
        join.date = prefixed;
        if (!mlocTablePattern(&r->table, mlocLoad16(f->timeSet + 2 * (f->glueKind + style)),
                              &join.glue))
        {
            return mloc_errorFormat;
        }
    }
    // The writing's pointers to this frame are cleared before it returns.
    g->join = &join;
    mlocResult result = WriteRangeOut(g, output);
    g->join = nullptr;
    return result;
}

mlocResult mlocDateTimeFormatter_FormatRange(const mlocDateTimeFormatter* formatter,
                                             const mlocDateTime* start, const mlocDateTime* end,
                                             mlocOutput* textOut)
{
    DateTimeFormat f;
    DateValue dates[2];
    if (!ReadFormatter(formatter, false, &f) || start == nullptr || end == nullptr ||
        !mlocDateTimeValid(start) || !mlocDateTimeValid(end) || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocReadDateValue(&f.date, &start->date, &dates[0]) ||
        !mlocReadDateValue(&f.date, &end->date, &dates[1]))
    {
        return mloc_errorInvalid;
    }
    uint32_t pattern = start->time.minute == 0 && end->time.minute == 0 ? 1 : 0;
    Writing one = {&f, &dates[0], &start->time, start->time.minute == 0 ? 1 : 0, nullptr};
    RangeWriting g = {.f = &f,
                      .dates = {&dates[0], &dates[1]},
                      .times = {&start->time, &end->time},
                      .pattern = pattern};
    return WriteRangeOf(&g, &one, start->date.day == end->date.day, textOut);
}

mlocResult mlocDateTimeFormatter_FormatRangeInZone(const mlocDateTimeFormatter* formatter,
                                                   const mlocTimeZone* zone,
                                                   const mlocInstant* start, const mlocInstant* end,
                                                   mlocOutput* textOut)
{
    DateTimeFormat f;
    DateValue dates[2];
    ZoneAt at[2];
    mlocDateTime local[2];
    if (!ReadFormatter(formatter, true, &f) || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocReadZonedLocal(&f.zone, zone, start, &at[0], &local[0]) ||
        !mlocReadZonedLocal(&f.zone, zone, end, &at[1], &local[1]) ||
        !mlocReadDateValue(&f.date, &local[0].date, &dates[0]) ||
        !mlocReadDateValue(&f.date, &local[1].date, &dates[1]))
    {
        return mloc_errorInvalid;
    }
    ZoneWriting zones[2] = {{&f.zone, &at[0]}, {&f.zone, &at[1]}};
    uint32_t pattern = local[0].time.minute == 0 && local[1].time.minute == 0 ? 1 : 0;
    Writing one = {&f, &dates[0], &local[0].time, local[0].time.minute == 0 ? 1 : 0, &zones[0]};
    RangeWriting g = {.f = &f,
                      .dates = {&dates[0], &dates[1]},
                      .times = {&local[0].time, &local[1].time},
                      .zones = {&zones[0], &zones[1]},
                      .pattern = pattern};
    return WriteRangeOf(&g, &one, local[0].date.day == local[1].date.day, textOut);
}
