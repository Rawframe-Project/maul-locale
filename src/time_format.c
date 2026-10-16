// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The time formatter (maul-locale/datetime.h, record mloc-0042): the
// locale's time set in its calendar, the clock and hour letter its
// options and keywords ask for, its digits, decimal separator, day period
// names and rules.

#include "time_format.h"

#include "bytes.h"
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
        mlocWriteZone(z->zone, style, z->timeZone, z->instant, w);
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
                         const mlocTimeOptions* o, mlocHourCycle cycle, bool asked)
{
    bool twelve =
        cycle == mloc_hourCycleH11 || cycle == mloc_hourCycleH12 || cycle == mloc_hourCycleClock12;
    uint32_t base = twelve ? dateTimeClock12 : dateTimeClock24;
    if (o->zone != mloc_zoneStyleNone)
    {
        base = dateTimeZoned + 6 * ((uint32_t)o->zone - 1) + (twelve ? 0 : 3);
    }
    uint32_t entries[timeFormatPatterns] = {base + 2, base + 2};
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

static mlocResult Resolve(TimeFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                          const mlocTimeOptions* o, TimeChoice* choice)
{
    mlocLocalePreferences preferences;
    NumberCore core;
    DatePatterns patterns;
    const uint8_t* set = nullptr;
    const uint8_t* nameSet = nullptr;
    const uint8_t* rules = nullptr;
    if (mlocGetLocalePreferences(data, locale, &preferences) != mloc_success)
    {
        return mloc_errorNotFound;
    }
    mlocResult digits = mlocInitNumberCore(&core, data, locale, mloc_numberDecimal);
    // The ISO calendar's time set and names are the Gregorian ones.
    uint32_t calendar = preferences.calendar;
    if (digits < 0 || !mlocFindTimeSet(data, locale, calendar, &patterns, &set) ||
        !mlocFindDateNames(data, locale, calendar, &f->names, &nameSet) ||
        !mlocFindDayPeriodRules(data, locale, &rules))
    {
        return mloc_errorNotFound;
    }
    bool asked = false;
    mlocHourCycle cycle = CycleOf(o, locale, &preferences, &asked);
    if (!FindPatterns(f, &patterns, set, o, cycle, asked))
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
        mlocResult zone = mlocMakeZoneFormat(&f.zone, data, locale, style);
        result = zone < 0 ? zone : result;
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
           f->digitLength > 0 && f->digitLength <= numberDigitSize &&
           f->decimalLength <= sizeof(f->decimal) && f->fractionDigits <= 9 && f->zoned <= 1;
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
    mlocDateTime local;
    if (!ReadFormatter(formatter, true, &f) || zone == nullptr || instant == nullptr ||
        output == nullptr || (output->buffer == nullptr && output->capacity != 0) ||
        mlocGetDateTimeFor(zone, instant, mloc_calendarIso8601, &local) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    ZoneWriting writing = {&f.zone, zone, instant};
    return Write(&f.time, &local.time, &writing, output);
}
