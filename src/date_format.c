// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date formatter (maul-locale/datetime.h, record mloc-0042): the
// locale's calendar from its preferences, the set of its options'
// patterns from dates.mloc, its digits and those its patterns name; a
// date read in that calendar and written by the pattern of the year
// style its year needs.

#include "date_format.h"

#include "bytes.h"

#include "maul-locale/datetime.h"

#include <string.h>

enum
{
    // Marks a date formatter the library made.
    dateStamp = 0x44,
    dateStandard = 3,
};

static_assert(sizeof(DateFormat) <= sizeof(mlocDateFormatter), "a date formatter fits its value");

// The set entry of each field set (mlocDateFieldSet).
static const uint8_t s_entries[] = {
    dateYearMonthDay, dateYearMonthDayWeekday, dateMonthDay, dateMonthDayWeekday,
    dateDay,          dateDayWeekday,          dateWeekday,  dateMonth,
    dateYear,         dateYearMonth,
};

bool mlocValidDateOptions(const mlocDateOptions* o)
{
    return (uint32_t)o->fields <= mloc_dateFieldSetYearMonth &&
           (uint32_t)o->length <= mloc_dateLengthShort &&
           (uint32_t)o->yearStyle <= mloc_yearStyleWithEra &&
           (uint32_t)o->style <= mloc_dateStyleShort;
}

// The numbering system a pattern names for its fields, or none.
static bool OverrideOf(const uint8_t* pattern, uint32_t length, const char** id, size_t* idLength)
{
    for (uint32_t at = 0; at < length;)
    {
        if (pattern[at] == datePatternOverride)
        {
            *id = (const char*)pattern + at + 4;
            *idLength = pattern[at + 3];
            return true;
        }
        at += pattern[at] == datePatternField ? 3 : pattern[at] < 0x20 ? 4u + pattern[at + 3] : 1;
    }
    return false;
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
        if (strchr("GyYrUMLdEc", pattern[at + 1]) == nullptr)
        {
            return false;
        }
        at += pattern[at] == datePatternField ? 3 : 4u + pattern[at + 3];
    }
    return true;
}

// The formatter's patterns: the standard format's, or the field set's at
// its length in the three year styles (one for a set without a year).
static bool FindPatterns(DateFormat* f, const DatePatterns* p, const uint8_t* set,
                         const mlocDateOptions* o)
{
    for (uint32_t i = 0; i < dateFormatPatterns; i++)
    {
        uint32_t index = 0;
        if (o->style != mloc_dateStyleNone)
        {
            index = mlocLoad16(set + 2 * (dateEntryCount + (size_t)o->style - 1));
        }
        else
        {
            uint32_t entry = s_entries[o->fields];
            bool year = entry >= dateYearMonthDay;
            uint32_t row = mlocLoad16(set + 2 * (size_t)(entry + (year ? i : 0)));
            index = mlocLoad16(p->rows + (size_t)row * dateRowSize + 2 * (size_t)o->length);
        }
        DataSpan pattern;
        if (!mlocDatePattern(p, index, &pattern) || !Writable(pattern.bytes, pattern.size))
        {
            return false;
        }
        f->pattern[i] = pattern.bytes;
        f->patternLength[i] = pattern.size;
    }
    return true;
}

// The digits of the system the patterns name, or its spell-out rules.
static mlocResult FindOverride(DateFormat* f, const mlocDataSet* data, const mlocLocale* locale)
{
    for (uint32_t i = 0; i < dateFormatPatterns; i++)
    {
        const char* id = nullptr;
        size_t length = 0;
        if (!OverrideOf(f->pattern[i], f->patternLength[i], &id, &length))
        {
            continue;
        }
        if (mlocNumberSystemDigits(data, id, length, f->overrideDigits, &f->overrideDigitLength))
        {
            return mloc_success;
        }
        mlocResult result =
            mlocInitNumberingSystemFormatter(&f->spellout, data, locale, id, length, nullptr);
        if (result < 0)
        {
            return mloc_errorNotFound;
        }
        f->hasSpellout = 1;
        return mloc_success;
    }
    return mloc_success;
}

static mlocResult Resolve(DateFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                          const mlocDateOptions* o)
{
    mlocLocalePreferences preferences;
    NumberCore core;
    DatePatterns patterns;
    const uint8_t* set = nullptr;
    if (mlocGetLocalePreferences(data, locale, &preferences) != mloc_success)
    {
        return mloc_errorNotFound;
    }
    mlocResult digits = mlocInitNumberCore(&core, data, locale, mloc_numberDecimal);
    uint32_t calendar = preferences.calendar;
    uint32_t names = calendar == mloc_calendarIso8601 ? mloc_calendarGregorian : calendar;
    if (digits < 0 || !mlocFindDateSet(data, locale, names, &patterns, &set) ||
        !mlocFindDateNames(data, locale, names, &f->names, &f->nameSet) ||
        !mlocFindDateNames(data, locale, mloc_calendarGregorian, &f->names, &f->gregorianSet))
    {
        return mloc_errorNotFound;
    }
    if (!FindPatterns(f, &patterns, set, o))
    {
        return mloc_errorData;
    }
    memcpy(f->digits, core.digits, sizeof(f->digits));
    f->digitLength = core.digitLength;
    uint32_t minus = core.symbolLength[numberMinus];
    f->minusLength = (uint8_t)(minus <= sizeof(f->minus) ? minus : 0);
    memcpy(f->minus, core.symbolText + core.symbolStart[numberMinus], f->minusLength);
    f->calendar = (uint8_t)names;
    f->firstDay = preferences.firstDay;
    f->minimalDays = preferences.minimalDays;
    f->yearStyle = o->style != mloc_dateStyleNone ? dateStandard : (uint8_t)o->yearStyle;
    mlocResult override = FindOverride(f, data, locale);
    return override < 0 ? override : digits;
}

mlocResult mlocMakeDateFormat(DateFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                              const mlocDateOptions* o)
{
    static_assert(sizeof(s_entries) == mloc_dateFieldSetYearMonth + 1, "an entry per field set");
    memset(f, 0, sizeof(*f));
    mlocResult result = Resolve(f, data, locale, o);
    f->stamp = result < 0 ? 0 : dateStamp;
    return result;
}

mlocResult mlocInitDateFormatter(mlocDateFormatter* formatterOut, const mlocDataSet* data,
                                 const mlocLocale* locale, const mlocDateOptions* options)
{
    mlocDateOptions o = options != nullptr ? *options : (mlocDateOptions){0};
    if (formatterOut == nullptr || data == nullptr || locale == nullptr ||
        !mlocValidDateOptions(&o))
    {
        return mloc_errorInvalid;
    }
    DateFormat f;
    mlocResult result = mlocMakeDateFormat(&f, data, locale, &o);
    if (result < 0)
    {
        return result;
    }
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

// Whether a year needs its era shown beside it: a year of an era before
// the calendar's main one (before the common era, before the Republic of
// China, before the Hijra, before the Incarnation), or a Japanese date
// before Meiji.
static bool NeedsEra(uint32_t calendar, const mlocDateFields* fields)
{
    static const char* const s_main[dateCalendarCount] = {"ce", "ce", "be",    "roc", "",   "am",
                                                          "am", "aa", "shaka", "ap",  "am", "ah",
                                                          "ah", "ah", "ah",    "ah",  "",   ""};
    if (calendar == mloc_calendarJapanese)
    {
        return fields->eraLength == 2 || fields->eraLength == 3;
    }
    const char* main = s_main[calendar];
    return fields->eraLength != 0 &&
           (strlen(main) != fields->eraLength || memcmp(main, fields->era, fields->eraLength) != 0);
}

// The pattern for a date by the formatter's year style. A year of a
// calendar counted from the Gregorian (Buddhist, ROC, Japanese) is clear
// in its pattern's digits when its Gregorian year is from 1950 to 2049,
// as CLDR's datetime conformance data has it; any other year needs its
// century, and one before its calendar's main era or a Gregorian year
// before 1000 its era too.
static uint32_t PatternFor(const DateFormat* f, const mlocDateFields* fields, int32_t gregorianYear)
{
    if (f->yearStyle == dateStandard)
    {
        return 0;
    }
    uint32_t c = f->calendar;
    bool counted = c == mloc_calendarGregorian || c == mloc_calendarBuddhist ||
                   c == mloc_calendarRoc || c == mloc_calendarJapanese;
    bool era = NeedsEra(c, fields) || (c == mloc_calendarGregorian && fields->eraYear < 1000);
    if (f->yearStyle == mloc_yearStyleWithEra || era)
    {
        return dateStyleEra;
    }
    bool clear = counted && gregorianYear >= 1950 && gregorianYear <= 2049;
    return f->yearStyle == mloc_yearStyleAuto && clear ? 0 : dateStyleFull;
}

// The Gregorian year of a day.
static int32_t GregorianYear(int32_t day, int32_t otherwise)
{
    mlocDate date;
    mlocDateFields gregorian;
    if (mlocMakeDateFromDay(mloc_calendarGregorian, day, &date) != mloc_success ||
        mlocGetDateFields(&date, &gregorian) != mloc_success)
    {
        return otherwise;
    }
    return gregorian.year;
}

bool mlocDateFormatSound(const DateFormat* f)
{
    return f->stamp == dateStamp && f->calendar < dateCalendarCount && f->pattern[0] != nullptr &&
           f->pattern[1] != nullptr && f->pattern[2] != nullptr && f->digitLength > 0 &&
           f->digitLength <= numberDigitSize && f->overrideDigitLength <= numberDigitSize &&
           f->minusLength <= sizeof(f->minus);
}

bool mlocReadDateValue(const DateFormat* f, const mlocDate* date, DateValue* value)
{
    mlocDate in;
    if (mlocMakeDateFromDay((mlocCalendar)f->calendar, date->day, &in) != mloc_success ||
        mlocGetDateFields(&in, &value->fields) != mloc_success)
    {
        return false;
    }
    // The Gregorian year the date's year begins in, and its own.
    value->related = GregorianYear(in.day - value->fields.dayOfYear + 1, value->fields.year);
    value->pattern = PatternFor(f, &value->fields, GregorianYear(in.day, value->fields.year));
    return true;
}

// A formatter the library made, copied out; false for any other.
static bool ReadFormatter(const mlocDateFormatter* opaque, DateFormat* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    return mlocDateFormatSound(f);
}

mlocResult mlocFormatDate(const mlocDateFormatter* formatter, const mlocDate* date,
                          mlocOutput* output)
{
    DateFormat f;
    DateValue v;
    if (!ReadFormatter(formatter, &f) || date == nullptr || output == nullptr ||
        (output->buffer == nullptr && output->capacity != 0) || !mlocReadDateValue(&f, date, &v))
    {
        return mloc_errorInvalid;
    }
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    mlocWriteDateFields(&f, f.pattern[v.pattern], f.patternLength[v.pattern], &v.fields, v.related,
                        &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        mlocWriteDateFields(&f, f.pattern[v.pattern], f.patternLength[v.pattern], &v.fields,
                            v.related, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}
