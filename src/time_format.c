// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The time formatter (maul-locale/datetime.h, record mloc-0042): the
// locale's time set in its calendar, the clock and hour letter its
// options and keywords ask for, its digits, decimal separator, day period
// names and rules.

#include "bytes.h"
#include "date_time.h"
#include "locale_keyword.h"
#include "time_write.h"

#include "maul-locale/datetime.h"

#include <string.h>

enum
{
    // Marks a time formatter the library made.
    timeStamp = 0x54,
};

static_assert(sizeof(TimeFormat) <= sizeof(mlocTimeFormatter), "a time formatter fits its value");

static bool ValidOptions(const mlocTimeOptions* o)
{
    return (uint32_t)o->hourCycle <= mloc_hourCycleClock24 &&
           (uint32_t)o->precision <= mloc_timePrecisionFraction9 &&
           (uint32_t)o->style <= mloc_timeStyleShort && o->style != mloc_timeStyleFull &&
           o->style != mloc_timeStyleLong;
}

// Whether a pattern holds only fields the formatter writes.
static bool Writable(const uint8_t* pattern, uint32_t length)
{
    for (uint32_t at = 0; at < length;)
    {
        if (pattern[at] >= 0x20)
        {
            at++;
            continue;
        }
        if (pattern[at] != datePatternField || strchr("hHKkmsabB", pattern[at + 1]) == nullptr)
        {
            return false;
        }
        at += 3;
    }
    return true;
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
static bool FindPatterns(TimeFormat* f, const DatePatterns* p, const uint8_t* set,
                         const mlocTimeOptions* o, mlocHourCycle cycle, bool asked)
{
    bool twelve =
        cycle == mloc_hourCycleH11 || cycle == mloc_hourCycleH12 || cycle == mloc_hourCycleClock12;
    uint32_t clock = twelve ? dateTimeClock12 : dateTimeClock24;
    uint32_t entries[timeFormatPatterns] = {clock + 2, clock + 2};
    if (o->style != mloc_timeStyleNone)
    {
        uint32_t standard = dateTimeStandard + (uint32_t)o->style - 1;
        bool other = (mlocLoad16(set + 2 * dateTimeFlags) == dateTimeOwnTwelve) != twelve;
        uint32_t own = (twelve ? dateTimeStyle12 : dateTimeStyle24) +
                       (o->style == mloc_timeStyleShort ? 0 : 1);
        entries[0] = asked || other ? own : standard;
        entries[1] = entries[0];
    }
    else if (o->precision >= mloc_timePrecisionHour &&
             o->precision <= mloc_timePrecisionMinuteOptional)
    {
        entries[0] = clock + (o->precision == mloc_timePrecisionHour ? 0 : 1);
        entries[1] = o->precision == mloc_timePrecisionMinuteOptional ? clock : entries[0];
    }
    for (uint32_t i = 0; i < timeFormatPatterns; i++)
    {
        DataSpan pattern;
        if (!mlocDatePattern(p, mlocLoad16(set + 2 * (size_t)entries[i]), &pattern) ||
            !Writable(pattern.bytes, pattern.size))
        {
            return false;
        }
        f->pattern[i] = pattern.bytes;
        f->patternLength[i] = pattern.size;
    }
    return true;
}

static mlocResult Resolve(TimeFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                          const mlocTimeOptions* o)
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

mlocResult mlocInitTimeFormatter(mlocTimeFormatter* formatterOut, const mlocDataSet* data,
                                 const mlocLocale* locale, const mlocTimeOptions* options)
{
    mlocTimeOptions o = options != nullptr ? *options : (mlocTimeOptions){0};
    if (formatterOut == nullptr || data == nullptr || locale == nullptr || !ValidOptions(&o))
    {
        return mloc_errorInvalid;
    }
    TimeFormat f;
    memset(&f, 0, sizeof(f));
    mlocResult result = Resolve(&f, data, locale, &o);
    if (result < 0)
    {
        return result;
    }
    f.stamp = timeStamp;
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

// A formatter the library made, copied out; false for any other.
static bool ReadFormatter(const mlocTimeFormatter* opaque, TimeFormat* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    return f->stamp == timeStamp && f->pattern[0] != nullptr && f->pattern[1] != nullptr &&
           f->digitLength > 0 && f->digitLength <= numberDigitSize &&
           f->decimalLength <= sizeof(f->decimal) && f->fractionDigits <= 9;
}

mlocResult mlocFormatTime(const mlocTimeFormatter* formatter, const mlocTime* time,
                          mlocOutput* output)
{
    TimeFormat f;
    if (!ReadFormatter(formatter, &f) || time == nullptr || !mlocTimeValid(time) ||
        output == nullptr || (output->buffer == nullptr && output->capacity != 0))
    {
        return mloc_errorInvalid;
    }
    uint32_t p = time->minute == 0 ? 1 : 0;
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    mlocWriteTimeFields(&f, f.pattern[p], f.patternLength[p], time, &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        mlocWriteTimeFields(&f, f.pattern[p], f.patternLength[p], time, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}
