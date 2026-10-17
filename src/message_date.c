// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_date.h"

#include "locale_keyword.h"

#include "maul-locale/calendar.h"
#include "maul-locale/ixdtf.h"

#include <stdio.h>
#include <string.h>

// Keeps a formatter made in a call (a date-time formatter is 1.4 KB) in
// its own frame, not in every write's.
#if defined(_MSC_VER) && !defined(__clang__)
#define MLOC_NOINLINE __declspec(noinline)
#else
#define MLOC_NOINLINE __attribute__((noinline))
#endif

static_assert(sizeof(DateValue) == offsetof(DateValue, reserved) + 1,
              "a date value has no padding");
static_assert(offsetof(PreparedDate, formatter) % 8 == 0, "a prepared date's formatter is aligned");

// The first day of 1970, the date of a time alone.
enum
{
    epochDay = 719163,
};

bool mlocDateLiteral(const uint8_t* text, size_t length, mlocDateTime* valueOut, uint8_t* hasOut)
{
    mlocTemporalParse p;
    uint32_t goals = mloc_formatDateTime | mloc_formatInstant | mloc_formatZonedDateTime;
    if (mlocParseTemporalString((const char*)text, length, goals, &p) != mloc_success || !p.hasYear)
    {
        return false;
    }
    mlocDateInput input = {NULL, 0, p.year, NULL, 0, p.month, p.day};
    if (mlocMakeDate(mloc_calendarIso8601, &input, mloc_overflowReject, &valueOut->date) !=
        mloc_success)
    {
        return false;
    }
    memset(&valueOut->time, 0, sizeof valueOut->time);
    if (p.hasTime)
    {
        valueOut->time = p.time;
    }
    *hasOut = (uint8_t)(dateHasDate | (p.hasTime ? dateHasTime : 0));
    return true;
}

bool mlocGoodDate(const mlocDate* date)
{
    return date->calendar <= mloc_calendarDangi && date->day >= mlocDateMinDay &&
           date->day <= mlocDateMaxDay && date->reserved[0] == 0 && date->reserved[1] == 0 &&
           date->reserved[2] == 0;
}

bool mlocGoodTime(const mlocTime* time)
{
    return time->hour < 24 && time->minute < 60 && time->second < 60 && time->reserved == 0 &&
           time->nanosecond < 1000000000;
}

bool mlocArgumentDateTime(const mlocMessageValue* argument, mlocDateTime* valueOut, uint8_t* hasOut)
{
    memset(valueOut, 0, sizeof *valueOut);
    switch (argument->kind)
    {
    case mloc_messageValueDate:
        valueOut->date = argument->as.date;
        *hasOut = dateHasDate;
        return true;
    case mloc_messageValueTime:
        valueOut->date.day = epochDay;
        valueOut->date.calendar = mloc_calendarIso8601;
        valueOut->time = argument->as.time;
        *hasOut = dateHasTime;
        return true;
    case mloc_messageValueDateTime:
        *valueOut = argument->as.dateTime;
        *hasOut = dateHasDate | dateHasTime;
        return true;
    default:
        return false;
    }
}

DateValue mlocMakeDateValue(DateFunction function, const mlocDateTime* value, uint8_t has)
{
    DateValue v;
    memset(&v, 0, sizeof v);
    v.value = *value;
    v.function = (uint8_t)function;
    v.has = has;
    v.fields = mloc_dateFieldSetYearMonthDay;
    v.length = mloc_dateLengthMedium;
    v.precision = mloc_timePrecisionMinute;
    v.hourCycle = mloc_hourCycleAuto;
    return v;
}

DateValue mlocDateFromValue(DateFunction function, const DateValue* operand)
{
    DateValue v = mlocMakeDateValue(function, &operand->value, operand->has);
    // The override options alone pass on.
    v.given = operand->given & (dateGivenHour12 | dateGivenCalendar);
    v.hourCycle = operand->hourCycle;
    v.calendar = operand->calendar;
    return v;
}

bool mlocDateTakes(DateFunction function, uint8_t has)
{
    return function == dateFunctionTime ? has != 0 : (has & dateHasDate) != 0;
}

static const char* const s_fields[] = {"year-month-day",
                                       "year-month-day-weekday",
                                       "month-day",
                                       "month-day-weekday",
                                       NULL,
                                       "day-weekday",
                                       "weekday"};
// In mlocDateLength's order.
static const char* const s_lengths[] = {"long", "medium", "short"};
// In mlocTimePrecision's order, as far as the specification names them.
static const char* const s_precisions[] = {"second", "hour", "minute"};
// In mlocCalendar's order: the BCP 47 calendar types.
static const char* const s_calendars[] = {
    "gregory",      "iso8601",          "buddhist", "roc",          "japanese", "coptic",
    "ethiopic",     "ethioaa",          "indian",   "persian",      "hebrew",   "islamic-civil",
    "islamic-tbla", "islamic-umalqura", "islamic",  "islamic-rgsa", "chinese",  "dangi"};

// The index of an option's text among names (null entries skipped), or
// -1.
static int Choice(const NumberOption* option, const char* const* names, int count)
{
    for (int i = 0; option->hasText && i < count; i++)
    {
        if (names[i] != NULL && option->length == strlen(names[i]) &&
            memcmp(option->text, names[i], option->length) == 0)
        {
            return i;
        }
    }
    return -1;
}

static bool NameIs(const uint8_t* name, size_t length, const char* text)
{
    return length == strlen(text) && memcmp(name, text, length) == 0;
}

// The option a name is for a function, as its given bit, or 0.
static uint32_t OptionBit(DateFunction function, const uint8_t* name, size_t length)
{
    bool date = function != dateFunctionTime;
    bool time = function != dateFunctionDate;
    bool both = function == dateFunctionDateTime;
    if (date && NameIs(name, length, both ? "dateFields" : "fields"))
    {
        return dateGivenFields;
    }
    if (date && NameIs(name, length, both ? "dateLength" : "length"))
    {
        return dateGivenLength;
    }
    if (time && NameIs(name, length, both ? "timePrecision" : "precision"))
    {
        return dateGivenPrecision;
    }
    if (time && NameIs(name, length, "hour12"))
    {
        return dateGivenHour12;
    }
    return NameIs(name, length, "calendar") ? dateGivenCalendar : 0;
}

mlocMessageFaultKind mlocSetDateOption(DateValue* value, DateFunction function, const uint8_t* name,
                                       size_t length, const NumberOption* option)
{
    uint32_t bit = OptionBit(function, name, length);
    if (bit == 0)
    {
        return mloc_messageFaultNone;
    }
    if ((bit & (dateGivenHour12 | dateGivenCalendar)) == 0 && !option->literal)
    {
        // Only the override options may come from a variable.
        return mloc_messageFaultBadOption;
    }
    static const char* const s_hour12[] = {"true", "false"};
    int index = bit == dateGivenFields      ? Choice(option, s_fields, 7)
                : bit == dateGivenLength    ? Choice(option, s_lengths, 3)
                : bit == dateGivenPrecision ? Choice(option, s_precisions, 3)
                : bit == dateGivenHour12    ? Choice(option, s_hour12, 2)
                                            : Choice(option, s_calendars, 18);
    if (index < 0)
    {
        return mloc_messageFaultBadOption;
    }
    uint8_t chosen = (uint8_t)index;
    switch (bit)
    {
    case dateGivenFields:
        value->fields = chosen;
        break;
    case dateGivenLength:
        value->length = chosen;
        break;
    case dateGivenPrecision:
        value->precision = chosen;
        break;
    case dateGivenHour12:
        value->hourCycle = index == 0 ? mloc_hourCycleClock12 : mloc_hourCycleClock24;
        break;
    default:
        value->calendar = chosen;
        break;
    }
    value->given |= bit;
    return mloc_messageFaultNone;
}

// The locale a value is written in: the formatter's, with the calendar
// option's -u-ca; false when that makes no locale.
static bool LocaleOf(const DateValue* value, const mlocLocale* locale, mlocLocale* out)
{
    *out = *locale;
    if ((value->given & dateGivenCalendar) == 0)
    {
        return true;
    }
    const char* calendar = s_calendars[value->calendar];
    return mlocSetKeyword(out, "ca", calendar, strlen(calendar));
}

static mlocDateOptions DateOptions(const DateValue* value)
{
    mlocDateOptions options;
    memset(&options, 0, sizeof options);
    options.fields = (mlocDateFieldSet)value->fields;
    options.length = (mlocDateLength)value->length;
    return options;
}

static mlocTimeOptions TimeOptions(const DateValue* value)
{
    mlocTimeOptions options;
    memset(&options, 0, sizeof options);
    options.hourCycle = (mlocHourCycle)value->hourCycle;
    options.precision = (mlocTimePrecision)value->precision;
    return options;
}

// A value's options as a prepared value keeps them.
static DateValue KeyOf(const DateValue* value)
{
    DateValue key = *value;
    memset(&key.value, 0, sizeof key.value);
    key.origin = 0;
    key.has = 0;
    return key;
}

size_t mlocPreparedDateSize(DateFunction function)
{
    static const size_t sizes[] = {sizeof(mlocDateFormatter), sizeof(mlocTimeFormatter),
                                   sizeof(mlocDateTimeFormatter)};
    return offsetof(PreparedDate, formatter) + sizes[function];
}

// Makes the formatter of a value's function into formatter.
static mlocResult Make(const DateValue* value, const mlocDataSet* data, const mlocLocale* locale,
                       PreparedDate* prepared)
{
    mlocLocale l;
    if (!LocaleOf(value, locale, &l))
    {
        return mloc_errorInvalid;
    }
    if (value->function == dateFunctionDate)
    {
        mlocDateOptions options = DateOptions(value);
        return mlocInitDateFormatter(&prepared->formatter.date, data, &l, &options);
    }
    if (value->function == dateFunctionTime)
    {
        mlocTimeOptions options = TimeOptions(value);
        return mlocInitTimeFormatter(&prepared->formatter.time, data, &l, &options);
    }
    mlocDateTimeOptions options;
    memset(&options, 0, sizeof options);
    options.date = DateOptions(value);
    options.time = TimeOptions(value);
    options.glue = mloc_dateTimeGlueAtTime;
    return mlocInitDateTimeFormatter(&prepared->formatter.dateTime, data, &l, &options);
}

bool mlocPrepareDate(const DateValue* value, const mlocDataSet* data, const mlocLocale* locale,
                     PreparedDate* out)
{
    out->key = KeyOf(value);
    return Make(value, data, locale, out) >= 0;
}

// Writes a value with a formatter that has its options.
static mlocResult WriteWith(const DateValue* value, const PreparedDate* prepared,
                            mlocOutput* output)
{
    if (value->function == dateFunctionDate)
    {
        return mlocFormatDate(&prepared->formatter.date, &value->value.date, output);
    }
    if (value->function == dateFunctionTime)
    {
        return mlocFormatTime(&prepared->formatter.time, &value->value.time, output);
    }
    return mlocFormatDateTime(&prepared->formatter.dateTime, &value->value, output);
}

// Writes a value with a formatter made here, off the resolver's frames.
MLOC_NOINLINE static mlocResult WriteMade(const DateValue* value, const mlocDataSet* data,
                                          const mlocLocale* locale, mlocOutput* output)
{
    PreparedDate made;
    mlocResult result = Make(value, data, locale, &made);
    return result < 0 ? result : WriteWith(value, &made, output);
}

mlocMessageFaultKind mlocWriteDateValue(const DateValue* value, const PreparedDate* prepared,
                                        const mlocDataSet* data, const mlocLocale* locale,
                                        mlocOutput* output)
{
    DateValue key = KeyOf(value);
    mlocResult result = prepared != NULL && memcmp(&key, &prepared->key, sizeof key) == 0
                            ? WriteWith(value, prepared, output)
                            : WriteMade(value, data, locale, output);
    // A text past the room reports the length it needs.
    return result == mloc_success || (result == mloc_errorCapacity && output->length > 0)
               ? mloc_messageFaultNone
               : mloc_messageFaultUnsupportedOperation;
}

// Writes an ISO date (a year past four digits with its sign and six).
static int WriteIsoDate(const mlocDate* date, char* text, size_t capacity)
{
    mlocDate iso;
    mlocDateFields f;
    if (mlocMakeDateFromDay(mloc_calendarIso8601, date->day, &iso) != mloc_success ||
        mlocGetDateFields(&iso, &f) != mloc_success)
    {
        return -1;
    }
    if (f.year >= 0 && f.year <= 9999)
    {
        return snprintf(text, capacity, "%04d-%02u-%02u", (int)f.year, (unsigned)f.month,
                        (unsigned)f.day);
    }
    return snprintf(text, capacity, "%c%06d-%02u-%02u", f.year < 0 ? '-' : '+',
                    f.year < 0 ? -(int)f.year : (int)f.year, (unsigned)f.month, (unsigned)f.day);
}

// Writes an ISO time, its fraction without trailing zeros.
static int WriteIsoTime(const mlocTime* time, char* text, size_t capacity)
{
    int n = snprintf(text, capacity, "%02u:%02u:%02u", (unsigned)time->hour, (unsigned)time->minute,
                     (unsigned)time->second);
    if (time->nanosecond == 0 || n < 0)
    {
        return n;
    }
    char fraction[16];
    snprintf(fraction, sizeof fraction, "%09u", (unsigned)time->nanosecond);
    size_t digits = 9;
    while (fraction[digits - 1] == '0')
    {
        digits--;
    }
    int more = snprintf(text + n, (size_t)n < capacity ? capacity - (size_t)n : 0, ".%.*s",
                        (int)digits, fraction);
    return more < 0 ? more : n + more;
}

size_t mlocWriteIsoDateTime(const mlocMessageValue* argument, char* text, size_t capacity)
{
    int n = -1;
    if (argument->kind == mloc_messageValueDate)
    {
        n = WriteIsoDate(&argument->as.date, text, capacity);
    }
    else if (argument->kind == mloc_messageValueTime)
    {
        n = WriteIsoTime(&argument->as.time, text, capacity);
    }
    else if (argument->kind == mloc_messageValueDateTime)
    {
        n = WriteIsoDate(&argument->as.dateTime.date, text, capacity);
        if (n >= 0 && (size_t)n + 1 < capacity)
        {
            text[n] = 'T';
            int time =
                WriteIsoTime(&argument->as.dateTime.time, text + n + 1, capacity - (size_t)n - 1);
            n = time < 0 ? time : n + 1 + time;
        }
        else
        {
            n = -1;
        }
    }
    return n < 0 || (size_t)n >= capacity ? 0 : (size_t)n;
}
