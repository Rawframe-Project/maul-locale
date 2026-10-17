// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The time formatter (maul-locale/datetime.h, record mloc-0042): the
// locale's time set in its calendar, the clock and hour letter its
// options and keywords ask for, its digits, decimal separator, day period
// names and rules.

#include "time_format.h"

#include "bytes.h"
#include "date_format.h"
#include "date_range.h"
#include "date_time.h"
#include "locale_keyword.h"

#include "maul-locale/datetime.h"

#include <string.h>

enum
{
    // Marks a time formatter the library made.
    timeStamp = 0x54,
};

// A time formatter: its state, and the zone's when a pattern holds one.
typedef struct ZonedTimeFormat
{
    TimeFormat time;
    ZoneFormat zone;
} ZonedTimeFormat;

static_assert(sizeof(ZonedTimeFormat) <= sizeof(mlocTimeFormatter),
              "a time formatter fits its value");

bool mlocValidTimeOptions(const mlocTimeOptions* o)
{
    return (uint32_t)o->hourCycle <= mloc_hourCycleClock24 &&
           (uint32_t)o->precision <= mloc_timePrecisionFraction9 &&
           (uint32_t)o->style <= mloc_timeStyleShort &&
           (uint32_t)o->zone <= mloc_zoneStyleOffsetShort &&
           (o->zone == mloc_zoneStyleNone || o->style == mloc_timeStyleNone);
}

bool mlocWritableTime(const uint8_t* pattern, uint32_t length, bool* zonedOut)
{
    *zonedOut = false;
    for (uint32_t at = 0; at < length;)
    {
        if (pattern[at] >= 0x20)
        {
            at++;
            continue;
        }
        bool zone = pattern[at] == datePatternField &&
                    mlocZoneFieldStyle(pattern[at + 1], pattern[at + 2]) != mloc_zoneStyleNone;
        if (pattern[at] != datePatternField ||
            (!zone && strchr("hHKkmsabB", pattern[at + 1]) == nullptr))
        {
            return false;
        }
        *zonedOut = *zonedOut || zone;
        at += 3;
    }
    return true;
}

void mlocWriteZoneField(void* context, const uint8_t* item, NumberWriter* w)
{
    const ZoneWriting* z = context;
    mlocZoneStyle style = mlocZoneFieldStyle(item[1], item[2]);
    if (z != nullptr && style != mloc_zoneStyleNone)
    {
        mlocWriteZone(z->zone, style, z->at, w);
    }
}

static bool KeywordIs(const char* value, size_t length, const char* want)
{
    return length == strlen(want) && memcmp(value, want, length) == 0;
}

// The cycle asked for: the options', else -u-hc's h11 to h24 exactly (the
// preferences hold it), else the clock of the preferences' cycle (-u-hc's
// c12 and c24, or the region's). Asked is whether the options, -u-hc or
// -u-rg ask for anything, which a standard format then follows.
static mlocHourCycle CycleOf(const mlocTimeOptions* o, const mlocLocale* locale,
                             const mlocLocalePreferences* p, bool* asked)
{
    const char* value = nullptr;
    const char* region = nullptr;
    size_t length = mlocFindKeyword(locale, "hc", &value);
    *asked = o->hourCycle != mloc_hourCycleAuto || length > 0 ||
             mlocFindKeyword(locale, "rg", &region) > 0;
    if (o->hourCycle != mloc_hourCycleAuto)
    {
        return o->hourCycle;
    }
    if (KeywordIs(value, length, "h11") || KeywordIs(value, length, "h12") ||
        KeywordIs(value, length, "h23") || KeywordIs(value, length, "h24"))
    {
        return (mlocHourCycle)(mloc_hourCycleH11 + p->hourCycle);
    }
    bool twelve = p->hourCycle == mloc_hourFormatH11 || p->hourCycle == mloc_hourFormatH12;
    return twelve ? mloc_hourCycleClock12 : mloc_hourCycleClock24;
}

// The patterns of the options from a time set: a standard format's, or
// the precision's; the hour's beside the minutes under minute-optional.
// A standard format is written for the clock of the locale's own region;
// when a cycle or region is asked for, or the locale's region (as for
// de-US, de's data) keeps the other clock, the cycle's clock's minutes
// (short) or seconds (medium) stand instead, as ICU4C matches them.
// A standard format's entry in a time set, else the clock's match.
static uint32_t StandardEntry(const uint8_t* set, mlocTimeStyle style, bool twelve, bool asked)
{
    uint32_t standard = dateTimeStandard + (uint32_t)style - 1;
    bool other = ((mlocLoad16(set + 2 * dateTimeFlags) & dateTimeOwnTwelve) != 0) != twelve;
    if (!asked && !other)
    {
        return standard;
    }
    if (style == mloc_timeStyleFull || style == mloc_timeStyleLong)
    {
        return (twelve ? dateTimeZoneStyle12 : dateTimeZoneStyle24) +
               (style == mloc_timeStyleFull ? 0 : 1);
    }
    return (twelve ? dateTimeStyle12 : dateTimeStyle24) + (style == mloc_timeStyleShort ? 0 : 1);
}

static bool FindPatterns(TimeFormat* f, const DatePatterns* p, const uint8_t* set,
                         const mlocTimeOptions* o, mlocHourCycle cycle, bool asked,
                         uint32_t entries[timeFormatPatterns])
{
    bool twelve =
        cycle == mloc_hourCycleH11 || cycle == mloc_hourCycleH12 || cycle == mloc_hourCycleClock12;
    uint32_t base = twelve ? dateTimeClock12 : dateTimeClock24;
    if (o->zone != mloc_zoneStyleNone)
    {
        base = dateTimeZoned + 6 * ((uint32_t)o->zone - 1) + (twelve ? 0 : 3);
    }
    entries[0] = base + 2;
    entries[1] = base + 2;
    if (o->style != mloc_timeStyleNone)
    {
        entries[0] = StandardEntry(set, o->style, twelve, asked);
        entries[1] = entries[0];
    }
    else if (o->precision >= mloc_timePrecisionHour &&
             o->precision <= mloc_timePrecisionMinuteOptional)
    {
        entries[0] = base + (o->precision == mloc_timePrecisionHour ? 0 : 1);
        entries[1] = o->precision == mloc_timePrecisionMinuteOptional ? base : entries[0];
    }
    for (uint32_t i = 0; i < timeFormatPatterns; i++)
    {
        DataSpan pattern;
        bool zoned = false;
        if (!mlocDatePattern(p, mlocLoad16(set + 2 * (size_t)entries[i]), &pattern) ||
            !mlocWritableTime(pattern.bytes, pattern.size, &zoned))
        {
            return false;
        }
        f->zoned = (uint8_t)(f->zoned || zoned);
        f->pattern[i] = pattern.bytes;
        f->patternLength[i] = pattern.size;
    }
    return true;
}

// The interval entry a time set's entry was matched for (research 40):
// a clock's hour, minute or second, alone or with a zone form; a
// standard format's own (dateIntervalTimeStyle on), or one matched anew
// for another clock, its minutes or seconds, the full and long with
// their zones.
static uint32_t IntervalEntry(uint32_t entry)
{
    if (entry < dateTimeStandard)
    {
        return dateIntervalTime + entry;
    }
    if (entry < dateTimeGlue)
    {
        return dateIntervalTimeStyle + entry - dateTimeStandard;
    }
    if (entry < dateTimeWeekday)
    {
        uint32_t k = entry - dateTimeStyle12;
        return dateIntervalTime + k / 2 * 3 + 1 + k % 2;
    }
    if (entry < dateTimeZoneStyle12)
    {
        return dateIntervalTime + 6 + entry - dateTimeZoned;
    }
    uint32_t k = entry - dateTimeZoneStyle12;
    return dateIntervalTime + 6 * (1 + k % 2) + k / 2 * 3 + 2;
}

static bool WritableHalf(const uint8_t* pattern, uint32_t length)
{
    bool zoned = false;
    return mlocWritableTime(pattern, length, &zoned);
}

// The relative date-time glue of each length, each holding {0} and {1}.
static bool FindRelativeGlues(TimeRange* r, const uint8_t* set)
{
    for (uint32_t i = 0; i < dateStandardCount; i++)
    {
        DataSpan glue;
        r->relativeGlue[i] = (uint16_t)mlocLoad16(set + 2 * (dateTimeRelativeGlue + i));
        if (!mlocTablePattern(&r->table, r->relativeGlue[i], &glue) ||
            !mlocValidGlue(glue.bytes, glue.size))
        {
            return false;
        }
    }
    return true;
}

// The formatter's interval patterns: each kept pattern's pair, a
// standard format's own pattern, the fallback, and what a range in a
// zone across dates joins.
static bool FindRange(TimeFormat* f, const DatePatterns* p, const uint8_t* set,
                      const uint8_t* intervals, const uint32_t entries[timeFormatPatterns])
{
    enum
    {
        times = dateIntervalTimeStyle - dateIntervalTime,
    };
    TimeRange* r = &f->range;
    r->table = (PatternTable){p->offsets, p->text, p->patternCount};
    const uint8_t* part = p->timeParts + (size_t)mlocLoad16(intervals + 2) * dateIntervalTimeSize;
    DataSpan fallback;
    DataSpan date;
    DataSpan era;
    r->shortDate = (uint16_t)mlocLoad16(intervals + 6);
    r->shortEraDate = (uint16_t)mlocLoad16(intervals + 8);
    r->glue = (uint16_t)mlocLoad16(set + 2 * (dateTimeAtGlue + dateStandardCount - 1));
    if (!mlocTablePattern(&r->table, mlocLoad16(intervals + 4), &fallback) ||
        !mlocTablePattern(&r->table, r->shortDate, &date) ||
        !mlocWritableDate(date.bytes, date.size) ||
        !mlocTablePattern(&r->table, r->shortEraDate, &era) ||
        !mlocWritableDate(era.bytes, era.size))
    {
        return false;
    }
    r->fallback = fallback.bytes;
    r->fallbackLength = fallback.size;
    for (uint32_t i = 0; i < timeFormatPatterns; i++)
    {
        uint32_t entry = IntervalEntry(entries[i]);
        const uint8_t* at = part + 2 * (size_t)(entry - dateIntervalTime);
        if (entry >= dateIntervalTimeStyle)
        {
            at = part + 2 * times + 4 * (size_t)(entry - dateIntervalTimeStyle);
            DataSpan own;
            if (!mlocTablePattern(&r->table, mlocLoad16(at + 2), &own) ||
                !WritableHalf(own.bytes, own.size))
            {
                return false;
            }
            r->own = own.bytes;
            r->ownLength = own.size;
        }
        r->pairs[i] = p->pairs + (size_t)mlocLoad16(at) * dateIntervalPairSize;
        if (!mlocValidRangePair(&r->table, r->pairs[i], WritableHalf))
        {
            return false;
        }
    }
    return true;
}

static mlocResult Resolve(TimeFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                          const mlocTimeOptions* o, TimeChoice* choice)
{
    mlocLocalePreferences preferences;
    NumberCore core;
    DatePatterns patterns;
    const uint8_t* set = nullptr;
    const uint8_t* intervals = nullptr;
    const uint8_t* nameSet = nullptr;
    const uint8_t* rules = nullptr;
    uint32_t entries[timeFormatPatterns];
    if (mlocGetLocalePreferences(data, locale, &preferences) != mloc_success)
    {
        return mloc_errorNotFound;
    }
    mlocResult digits = mlocInitNumberCore(&core, data, locale, mloc_numberDecimal);
    // The ISO calendar's time set and names are the Gregorian ones.
    uint32_t calendar = preferences.calendar;
    if (digits < 0 || !mlocFindTimeSet(data, locale, calendar, &patterns, &set, &intervals) ||
        !mlocFindDateNames(data, locale, calendar, &f->names, &nameSet) ||
        !mlocFindDayPeriodRules(data, locale, &rules))
    {
        return mloc_errorNotFound;
    }
    bool asked = false;
    mlocHourCycle cycle = CycleOf(o, locale, &preferences, &asked);
    if (!FindPatterns(f, &patterns, set, o, cycle, asked, entries) ||
        !FindRange(f, &patterns, set, intervals, entries) || !FindRelativeGlues(&f->range, set))
    {
        return mloc_errorData;
    }
    *choice = (TimeChoice){patterns, set,
                           cycle == mloc_hourCycleH11 || cycle == mloc_hourCycleH12 ||
                               cycle == mloc_hourCycleClock12};
    static const uint8_t s_letters[] = {0, 'K', 'h', 'H', 'k', 0, 0};
    f->hourLetter = s_letters[cycle];
    f->dayPeriods = mlocDateNameList(&f->names, mlocLoad16(nameSet + 2 * dateNamesDayPeriods));
    memcpy(f->rules, rules, dateRuleSize);
    memcpy(f->digits, core.digits, sizeof(f->digits));
    f->digitLength = core.digitLength;
    uint32_t decimal = core.symbolLength[numberDecimal];
    f->decimalLength = (uint8_t)(decimal <= sizeof(f->decimal) ? decimal : 0);
    memcpy(f->decimal, core.symbolText + core.symbolStart[numberDecimal], f->decimalLength);
    f->fractionDigits =
        o->style == mloc_timeStyleNone && o->precision >= mloc_timePrecisionFraction1
            ? (uint8_t)(o->precision - mloc_timePrecisionFraction1 + 1)
            : 0;
    return digits;
}

mlocResult mlocMakeTimeFormat(TimeFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                              const mlocTimeOptions* options, TimeChoice* choice)
{
    TimeChoice unused;
    memset(f, 0, sizeof(*f));
    mlocResult result = Resolve(f, data, locale, options, choice != nullptr ? choice : &unused);
    f->stamp = result < 0 ? 0 : timeStamp;
    return result;
}

mlocResult mlocInitTimeFormatter(mlocTimeFormatter* formatterOut, const mlocDataSet* data,
                                 const mlocLocale* locale, const mlocTimeOptions* options)
{
    mlocTimeOptions o = options != nullptr ? *options : (mlocTimeOptions){0};
    if (formatterOut == nullptr || data == nullptr || locale == nullptr ||
        !mlocValidTimeOptions(&o))
    {
        return mloc_errorInvalid;
    }
    ZonedTimeFormat f;
    memset(&f, 0, sizeof(f));
    mlocResult result = mlocMakeTimeFormat(&f.time, data, locale, &o, nullptr);
    if (result >= 0 && f.time.zoned)
    {
        mlocZoneStyle style = o.zone != mloc_zoneStyleNone ? o.zone : mloc_zoneStyleSpecificShort;
        // The zone reads the locale's digits as the time does: one result.
        result = mlocMakeZoneFormat(&f.zone, data, locale, style);
    }
    if (result < 0)
    {
        return result;
    }
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

bool mlocTimeFormatSound(const TimeFormat* f)
{
    return f->stamp == timeStamp && f->pattern[0] != nullptr && f->pattern[1] != nullptr &&
           f->range.pairs[0] != nullptr && f->range.pairs[1] != nullptr &&
           f->range.fallback != nullptr && f->digitLength > 0 &&
           f->digitLength <= numberDigitSize && f->decimalLength <= sizeof(f->decimal) &&
           f->fractionDigits <= 9 && f->zoned <= 1;
}

bool mlocReadTimeFormat(const mlocTimeFormatter* opaque, TimeFormat* format)
{
    ZonedTimeFormat f;
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(&f, opaque, sizeof(f));
    *format = f.time;
    return mlocTimeFormatSound(&f.time) && f.time.zoned == 0;
}

// A formatter the library made, copied out, with a zone or without as
// asked; false for any other.
static bool ReadFormatter(const mlocTimeFormatter* opaque, bool zoned, ZonedTimeFormat* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    return mlocTimeFormatSound(&f->time) && f->time.zoned == zoned &&
           (!zoned || mlocZoneFormatSound(&f->zone));
}

// Writes a time by the pattern its minute picks, its zone field by the
// zone writing (null for none).
static mlocResult Write(const TimeFormat* f, const mlocTime* time, const ZoneWriting* zone,
                        mlocOutput* output)
{
    uint32_t p = time->minute == 0 ? 1 : 0;
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    mlocWriteTimeFieldsWith(f, f->pattern[p], f->patternLength[p], time, mlocWriteZoneField,
                            (void*)zone, &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        mlocWriteTimeFieldsWith(f, f->pattern[p], f->patternLength[p], time, mlocWriteZoneField,
                                (void*)zone, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}

mlocResult mlocFormatTime(const mlocTimeFormatter* formatter, const mlocTime* time,
                          mlocOutput* output)
{
    ZonedTimeFormat f;
    if (!ReadFormatter(formatter, false, &f) || time == nullptr || !mlocTimeValid(time) ||
        output == nullptr || (output->buffer == nullptr && output->capacity != 0))
    {
        return mloc_errorInvalid;
    }
    return Write(&f.time, time, nullptr, output);
}

mlocResult mlocFormatTimeInZone(const mlocTimeFormatter* formatter, const mlocTimeZone* zone,
                                const mlocInstant* instant, mlocOutput* output)
{
    ZonedTimeFormat f;
    ZoneAt at;
    mlocDateTime local;
    if (!ReadFormatter(formatter, true, &f) || output == nullptr ||
        (output->buffer == nullptr && output->capacity != 0) ||
        !mlocReadZonedLocal(&f.zone, zone, instant, &at, &local))
    {
        return mloc_errorInvalid;
    }
    ZoneWriting writing = {&f.zone, &at};
    return Write(&f.time, &local.time, &writing, output);
}

// A range of times: the formatter, both values and their zones' writing
// (null without a zone), and with dates that differ the date state and
// both dates.
typedef struct TimeRangeWriting
{
    const ZonedTimeFormat* f;
    const mlocTime* times[2];
    const ZoneWriting* zones[2];
    const DateFormat* date;
    const DateValue* dates[2];
    const RangeChoice* choice;
    const RangeJoin* join;
    DataSpan fallback;
} TimeRangeWriting;

static void WriteTimeValue(const void* context, uint32_t value, const uint8_t* pattern,
                           uint32_t length, const PatternSpan* span, NumberWriter* w)
{
    const TimeRangeWriting* g = context;
    mlocWriteTimeFieldsIn(&g->f->time, pattern, length, g->times[value], mlocWriteZoneField,
                          (void*)g->zones[value], span, w);
}

static void WriteDateValue(const void* context, uint32_t value, const uint8_t* pattern,
                           uint32_t length, const PatternSpan* span, NumberWriter* w)
{
    const TimeRangeWriting* g = context;
    const DateValue* v = g->dates[value];
    mlocWriteDateFieldsIn(g->date, pattern, length, &v->fields, v->related, span, w);
}

static void WriteRange(const TimeRangeWriting* g, NumberWriter* w)
{
    if (g->join != nullptr)
    {
        mlocWriteJoinedFallback(g->join, g->fallback, w);
        return;
    }
    mlocWriteRange(g->choice, w);
}

static mlocResult WriteRangeOut(const TimeRangeWriting* g, mlocOutput* output)
{
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    WriteRange(g, &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        WriteRange(g, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}

// What a range of times writes: the pattern both times' minutes pick
// (the hour's when both are zero under minute-optional precision), its
// pair for their difference.
static RangeChoice TimeChoiceOf(const TimeRangeWriting* g, const TimeRange* r)
{
    const TimeFormat* t = &g->f->time;
    uint32_t p = g->times[0]->minute == 0 && g->times[1]->minute == 0 ? 1 : 0;
    return (RangeChoice){
        .write = WriteTimeValue,
        .context = g,
        .table = &r->table,
        .pair = r->pairs[p],
        .difference = mlocTimeDifference(g->times[0], g->times[1], t->fractionDigits),
        .own = r->own != nullptr ? (DataSpan){r->own, r->ownLength}
                                 : (DataSpan){t->pattern[p], t->patternLength[p]},
        .fallback = {r->fallback, r->fallbackLength},
    };
}

mlocResult mlocFormatTimeRange(const mlocTimeFormatter* formatter, const mlocTime* start,
                               const mlocTime* end, mlocOutput* output)
{
    ZonedTimeFormat f;
    if (!ReadFormatter(formatter, false, &f) || start == nullptr || end == nullptr ||
        !mlocTimeValid(start) || !mlocTimeValid(end) || output == nullptr ||
        (output->buffer == nullptr && output->capacity != 0))
    {
        return mloc_errorInvalid;
    }
    TimeRangeWriting g = {.f = &f, .times = {start, end}};
    RangeChoice choice = TimeChoiceOf(&g, &f.time.range);
    g.choice = &choice;
    return WriteRangeOut(&g, output);
}

// The date state a range in a zone across dates writes its dates by: a
// short date in the locale's calendar, made where the zone's state keeps
// the data and the locale.
static mlocResult MakeRangeDate(const ZonedTimeFormat* f, DateFormat* date)
{
    mlocDateOptions options = {.length = mloc_dateLengthShort, .yearStyle = mloc_yearStyleFull};
    return mlocMakeDateFormat(date, f->zone.data, &f->zone.locale, &options);
}

mlocResult mlocFormatTimeRangeInZone(const mlocTimeFormatter* formatter, const mlocTimeZone* zone,
                                     const mlocInstant* start, const mlocInstant* end,
                                     mlocOutput* output)
{
    ZonedTimeFormat f;
    ZoneAt at[2];
    mlocDateTime local[2];
    if (!ReadFormatter(formatter, true, &f) || output == nullptr ||
        (output->buffer == nullptr && output->capacity != 0) ||
        !mlocReadZonedLocal(&f.zone, zone, start, &at[0], &local[0]) ||
        !mlocReadZonedLocal(&f.zone, zone, end, &at[1], &local[1]))
    {
        return mloc_errorInvalid;
    }
    ZoneWriting zones[2] = {{&f.zone, &at[0]}, {&f.zone, &at[1]}};
    TimeRangeWriting g = {
        .f = &f, .times = {&local[0].time, &local[1].time}, .zones = {&zones[0], &zones[1]}};
    const TimeRange* r = &f.time.range;
    RangeChoice choice = TimeChoiceOf(&g, r);
    g.choice = &choice;
    if (local[0].date.day == local[1].date.day)
    {
        return WriteRangeOut(&g, output);
    }
    DateFormat date;
    DateValue dates[2];
    mlocResult made = MakeRangeDate(&f, &date);
    if (made < 0)
    {
        return made;
    }
    if (!mlocReadDateValue(&date, &local[0].date, &dates[0]) ||
        !mlocReadDateValue(&date, &local[1].date, &dates[1]))
    {
        return mloc_errorInvalid;
    }
    // The fallback around each whole: yMd (GyMd across eras), the short
    // atTime glue, the time.
    uint32_t shortDate = mlocDateDifference(&dates[0].fields, &dates[1].fields) == rangeEra
                             ? r->shortEraDate
                             : r->shortDate;
    RangeJoin join = {.writeDate = WriteDateValue, .writeTime = WriteTimeValue, .context = &g};
    if (!mlocTablePattern(&r->table, r->glue, &join.glue) ||
        !mlocTablePattern(&r->table, shortDate, &join.date))
    {
        return mloc_errorData;
    }
    join.time = choice.own;
    g.date = &date;
    g.dates[0] = &dates[0];
    g.dates[1] = &dates[1];
    g.join = &join;
    g.fallback = choice.fallback;
    return WriteRangeOut(&g, output);
}
