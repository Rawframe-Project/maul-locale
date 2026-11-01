// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_date.h"

#include "invariant.h"
#include "locale_keyword.h"

#include "maul-locale/calendar.h"
#include "maul-locale/ixdtf.h"
#include "maul-locale/zone.h"

#include <stdio.h>
#include <string.h>

// Keeps a formatter made in a call (a date-time formatter is 1.4 KB) in
// its own frame, not in every write's.
#if defined(_MSC_VER) && !defined(__clang__)
#define MLOC_NOINLINE __declspec(noinline)
#else
#define MLOC_NOINLINE __attribute__((noinline))
#endif

static_assert(sizeof(DateKey) == offsetof(DateKey, reserved) + 1, "a date key has no padding");
static_assert(offsetof(PreparedDate, formatter) % 8 == 0, "a prepared date's formatter is aligned");

// The first day of 1970, the date of a time alone.
enum
{
    epochDay = 719163,
};

// Nanoseconds a minute.
static const int64_t minuteNanoseconds = 60000000000;

// UTC's zone in the data, or an offset zone of 0 when it has none.
static mlocTimeZone Utc(const mlocDataSet* data)
{
    mlocTimeZone zone;
    memset(&zone, 0, sizeof zone);
    if (data == NULL || mlocFindTimeZone(data, "UTC", 3, &zone) != mloc_success)
    {
        memset(&zone, 0, sizeof zone);
    }
    return zone;
}

// Whether a span of text is "iso8601", in any case.
static bool IsIso(const char* text, mlocTextSpan span)
{
    static const char iso[] = "iso8601";
    if (span.length != sizeof iso - 1)
    {
        return false;
    }
    for (size_t i = 0; i < span.length; i++)
    {
        char c = text[span.start + i];
        if ((c >= 'A' && c <= 'Z' ? (char)(c + 32) : c) != iso[i])
        {
            return false;
        }
    }
    return true;
}

// The instant and zone of a literal with Z, an offset or a zone, its
// local date-time already read; out's local becomes the zone's.
static bool ZonedLiteral(const mlocTemporalParse* p, const char* text, const mlocDataSet* data,
                         DateMoment* out)
{
    // Z or the offset as a zone of its own.
    mlocTimeZone fixed;
    memset(&fixed, 0, sizeof fixed);
    mlocResult result = mloc_success;
    if (p->timeZone.length > 0)
    {
        if (data == NULL || mlocFindTimeZone(data, text + p->timeZone.start, p->timeZone.length,
                                             &out->zone) != mloc_success)
        {
            return false;
        }
        result = p->hasOffset
                     ? mlocGetInstantForOffset(&out->zone, &out->local, p->offsetNanoseconds,
                                               mloc_offsetReject, mloc_disambiguationCompatible,
                                               !p->offsetHasSeconds, &out->instant)
                 : p->utc ? mlocGetInstantFor(&fixed, &out->local, mloc_disambiguationCompatible,
                                              &out->instant)
                          : mlocGetInstantFor(&out->zone, &out->local,
                                              mloc_disambiguationCompatible, &out->instant);
    }
    else
    {
        if (p->offsetNanoseconds % minuteNanoseconds != 0)
        {
            // No zone holds an offset of seconds.
            return false;
        }
        fixed.offsetMinutes = (int16_t)(p->offsetNanoseconds / minuteNanoseconds);
        out->zone = p->utc ? Utc(data) : fixed;
        result =
            mlocGetInstantFor(&fixed, &out->local, mloc_disambiguationCompatible, &out->instant);
    }
    out->has |= dateHasZone;
    return result == mloc_success &&
           mlocGetDateTimeFor(&out->zone, &out->instant, mloc_calendarIso8601, &out->local) ==
               mloc_success;
}

bool mlocDateLiteral(const uint8_t* text, size_t length, const mlocDataSet* data, DateMoment* out)
{
    memset(out, 0, sizeof *out);
    mlocTemporalParse p;
    uint32_t goals = mloc_formatDateTime | mloc_formatInstant | mloc_formatZonedDateTime;
    const char* chars = (const char*)text;
    if (mlocParseTemporalString(chars, length, goals, &p) != mloc_success || !p.hasYear ||
        (p.calendar.length > 0 && !IsIso(chars, p.calendar)))
    {
        return false;
    }
    mlocDateInput input = {NULL, 0, p.year, NULL, 0, p.month, p.day};
    if (mlocMakeDate(mloc_calendarIso8601, &input, mloc_overflowReject, &out->local.date) !=
        mloc_success)
    {
        return false;
    }
    if (p.hasTime)
    {
        out->local.time = p.time;
    }
    out->has = (uint8_t)(dateHasDate | (p.hasTime ? dateHasTime : 0));
    bool zoned = p.utc || p.hasOffset || p.timeZone.length > 0;
    return !zoned || ZonedLiteral(&p, chars, data, out);
}

bool mlocGoodDate(const mlocDate* date)
{
    return (uint32_t)date->calendar <= mloc_calendarDangi && date->day >= mlocDateMinDay &&
           date->day <= mlocDateMaxDay;
}

bool mlocGoodTime(const mlocTime* time)
{
    return time->hour < 24 && time->minute < 60 && time->second < 60 &&
           time->nanosecond < 1000000000;
}

bool mlocGoodInstant(const mlocInstant* instant)
{
    static const int64_t most = 8640000000000;
    return instant->seconds >= -most && instant->nanoseconds >= 0 &&
           instant->nanoseconds < 1000000000 &&
           (instant->seconds < most || (instant->seconds == most && instant->nanoseconds == 0));
}

bool mlocGoodZoned(const mlocMessageValue* argument)
{
    int32_t offset = 0;
    return argument->as.zoned.zone != NULL && mlocGoodInstant(&argument->as.zoned.instant) &&
           mlocGetTimeZoneOffset(argument->as.zoned.zone, &argument->as.zoned.instant, &offset) ==
               mloc_success;
}

// An exact time in a zone: its local date-time there.
static bool AtInstant(const mlocInstant* instant, const mlocTimeZone* zone, DateMoment* out)
{
    out->instant = *instant;
    out->zone = *zone;
    out->has = dateHasDate | dateHasTime | dateHasZone;
    return mlocGetDateTimeFor(zone, instant, mloc_calendarIso8601, &out->local) == mloc_success;
}

bool mlocArgumentDateTime(const mlocMessageValue* argument, const mlocDataSet* data,
                          DateMoment* out)
{
    memset(out, 0, sizeof *out);
    switch (argument->kind)
    {
    case mloc_messageValueDate:
        out->local.date = argument->as.date;
        out->has = dateHasDate;
        return true;
    case mloc_messageValueTime:
        out->local.date.day = epochDay;
        out->local.date.calendar = mloc_calendarIso8601;
        out->local.time = argument->as.time;
        out->has = dateHasTime;
        return true;
    case mloc_messageValueDateTime:
        out->local = argument->as.dateTime;
        out->has = dateHasDate | dateHasTime;
        return true;
    case mloc_messageValueInstant:
    {
        mlocTimeZone utc = Utc(data);
        return AtInstant(&argument->as.instant, &utc, out);
    }
    case mloc_messageValueZonedDateTime:
        return AtInstant(&argument->as.zoned.instant, argument->as.zoned.zone, out);
    default:
        return false;
    }
}

DateValue mlocMakeDateValue(DateFunction function, const DateMoment* moment)
{
    DateValue v;
    memset(&v, 0, sizeof v);
    v.value = moment->local;
    v.instant = moment->instant;
    v.zone = moment->zone;
    v.function = (uint8_t)function;
    v.has = moment->has;
    v.fields = mloc_dateFieldSetYearMonthDay;
    v.length = mloc_dateLengthMedium;
    v.precision = mloc_timePrecisionMinute;
    v.hourCycle = mloc_hourCycleAuto;
    return v;
}

DateValue mlocDateFromValue(DateFunction function, const DateValue* operand)
{
    DateMoment moment = {operand->value, operand->instant, operand->zone, operand->has};
    DateValue v = mlocMakeDateValue(function, &moment);
    // The override options alone pass on.
    v.given = operand->given & (dateGivenHour12 | dateGivenCalendar | dateGivenTimeZone);
    v.hourCycle = operand->hourCycle;
    v.calendar = operand->calendar;
    v.zoneMode = operand->zoneMode;
    v.timeZone = operand->timeZone;
    return v;
}

bool mlocDateTakes(DateFunction function, uint8_t has)
{
    return function == dateFunctionTime ? has != 0 : (has & dateHasDate) != 0;
}

// The names of the options whose values are a public enumeration, each
// keyed by its value, so that the index a name is found at is the value
// it gives; null for a value no name gives. Each table covers its
// enumeration to its last value an option names.
// The day alone, and the month, year and year-month sets, have none.
static const char* const s_fields[] = {
    [mloc_dateFieldSetYearMonthDay] = "year-month-day",
    [mloc_dateFieldSetYearMonthDayWeekday] = "year-month-day-weekday",
    [mloc_dateFieldSetMonthDay] = "month-day",
    [mloc_dateFieldSetMonthDayWeekday] = "month-day-weekday",
    [mloc_dateFieldSetDayWeekday] = "day-weekday",
    [mloc_dateFieldSetWeekday] = "weekday",
};
static const char* const s_lengths[] = {
    [mloc_dateLengthLong] = "long",
    [mloc_dateLengthMedium] = "medium",
    [mloc_dateLengthShort] = "short",
};
// As far as the specification names them.
static const char* const s_precisions[] = {
    [mloc_timePrecisionSecond] = "second",
    [mloc_timePrecisionHour] = "hour",
    [mloc_timePrecisionMinute] = "minute",
};
// The BCP 47 calendar types.
static const char* const s_calendars[] = {
    [mloc_calendarGregorian] = "gregory",
    [mloc_calendarIso8601] = "iso8601",
    [mloc_calendarBuddhist] = "buddhist",
    [mloc_calendarRoc] = "roc",
    [mloc_calendarJapanese] = "japanese",
    [mloc_calendarCoptic] = "coptic",
    [mloc_calendarEthiopic] = "ethiopic",
    [mloc_calendarEthiopicAmeteAlem] = "ethioaa",
    [mloc_calendarIndian] = "indian",
    [mloc_calendarPersian] = "persian",
    [mloc_calendarHebrew] = "hebrew",
    [mloc_calendarIslamicCivil] = "islamic-civil",
    [mloc_calendarIslamicTbla] = "islamic-tbla",
    [mloc_calendarIslamicUmalqura] = "islamic-umalqura",
    [mloc_calendarIslamic] = "islamic",
    [mloc_calendarIslamicRgsa] = "islamic-rgsa",
    [mloc_calendarChinese] = "chinese",
    [mloc_calendarDangi] = "dangi",
};
// Shown as none has no name.
static const char* const s_zoneStyles[] = {
    [mloc_zoneStyleSpecificLong] = "long",
    [mloc_zoneStyleSpecificShort] = "short",
};

static_assert(sizeof(s_fields) / sizeof(s_fields[0]) == mloc_dateFieldSetWeekday + 1,
              "s_fields covers mlocDateFieldSet to its last named set");
static_assert(sizeof(s_lengths) / sizeof(s_lengths[0]) == mloc_dateLengthShort + 1,
              "a name for each mlocDateLength");
static_assert(sizeof(s_precisions) / sizeof(s_precisions[0]) == mloc_timePrecisionMinute + 1,
              "s_precisions covers mlocTimePrecision to its last named precision");
static_assert(sizeof(s_calendars) / sizeof(s_calendars[0]) == mloc_calendarDangi + 1,
              "a name for each mlocCalendar");
static_assert(sizeof(s_zoneStyles) / sizeof(s_zoneStyles[0]) == mloc_zoneStyleSpecificShort + 1,
              "s_zoneStyles covers mlocZoneStyle to its last named style");

#define NAMES(table) table, (int)(sizeof(table) / sizeof((table)[0]))

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
    if (time && NameIs(name, length, "timeZoneStyle"))
    {
        return dateGivenZoneStyle;
    }
    if (NameIs(name, length, "timeZone"))
    {
        return dateGivenTimeZone;
    }
    return NameIs(name, length, "calendar") ? dateGivenCalendar : 0;
}

// Sets the timeZone option: input (the operand's zone), or a zone the
// data has by identifier or offset ("UTC" one of them).
static mlocMessageFaultKind SetTimeZone(DateValue* value, const NumberOption* option,
                                        const mlocDataSet* data)
{
    if (!option->hasText)
    {
        return mloc_messageFaultBadOption;
    }
    if (NameIs(option->text, option->length, "input"))
    {
        if ((value->has & dateHasZone) == 0)
        {
            return mloc_messageFaultBadOperand;
        }
        value->zoneMode = dateZoneInput;
    }
    else
    {
        mlocTimeZone zone;
        if (data == NULL || mlocFindTimeZone(data, (const char*)option->text, option->length,
                                             &zone) != mloc_success)
        {
            return mloc_messageFaultBadOption;
        }
        value->zoneMode = dateZoneGiven;
        value->timeZone = zone;
    }
    value->given |= dateGivenTimeZone;
    return mloc_messageFaultNone;
}

mlocMessageFaultKind mlocSetDateOption(DateValue* value, DateFunction function, const uint8_t* name,
                                       size_t length, const NumberOption* option,
                                       const mlocDataSet* data)
{
    uint32_t bit = OptionBit(function, name, length);
    if (bit == 0)
    {
        return mloc_messageFaultNone;
    }
    if ((bit & (dateGivenHour12 | dateGivenCalendar | dateGivenTimeZone)) == 0 && !option->literal)
    {
        // Only the override options may come from a variable.
        return mloc_messageFaultBadOption;
    }
    if (bit == dateGivenTimeZone)
    {
        return SetTimeZone(value, option, data);
    }
    static const char* const s_hour12[] = {"true", "false"};
    int index = bit == dateGivenFields      ? Choice(option, NAMES(s_fields))
                : bit == dateGivenLength    ? Choice(option, NAMES(s_lengths))
                : bit == dateGivenPrecision ? Choice(option, NAMES(s_precisions))
                : bit == dateGivenHour12    ? Choice(option, NAMES(s_hour12))
                : bit == dateGivenZoneStyle ? Choice(option, NAMES(s_zoneStyles))
                                            : Choice(option, NAMES(s_calendars));
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
    case dateGivenZoneStyle:
        value->zoneStyle = chosen;
        break;
    default:
        value->calendar = chosen;
        break;
    }
    value->given |= bit;
    return mloc_messageFaultNone;
}

mlocMessageFaultKind mlocCheckDateZone(DateValue* value, bool hasDefault)
{
    bool zoned = (value->has & dateHasZone) != 0;
    bool shown = zoned || value->zoneMode == dateZoneGiven || hasDefault;
    if ((value->given & dateGivenZoneStyle) == 0 ||
        (shown && (zoned || (value->has & dateHasDate) != 0)))
    {
        return mloc_messageFaultNone;
    }
    value->given &= ~(uint32_t)dateGivenZoneStyle;
    value->zoneStyle = mloc_zoneStyleNone;
    return mloc_messageFaultBadOption;
}

bool mlocDateZone(const DateValue* value, const mlocDataSet* data, const mlocTimeZone* defaultZone,
                  mlocTimeZone* zoneOut)
{
    bool zoned = (value->has & dateHasZone) != 0;
    if (value->zoneMode == dateZoneGiven)
    {
        *zoneOut = value->timeZone;
    }
    else if (value->zoneMode == dateZoneInput && zoned)
    {
        *zoneOut = value->zone;
    }
    else if (defaultZone != NULL)
    {
        *zoneOut = *defaultZone;
    }
    else if (zoned)
    {
        *zoneOut = Utc(data);
    }
    else
    {
        return false;
    }
    return true;
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
    // A calendar is set only to its index among s_calendars.
    MLOC_ASSERT(value->calendar < sizeof s_calendars / sizeof s_calendars[0]);
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
    options.zone = (mlocZoneStyle)value->zoneStyle;
    return options;
}

// A value's options as a prepared value keeps them: those its formatter
// is made with (the zone it is shown in is not one).
static DateKey KeyOf(const DateValue* value)
{
    DateKey key = {value->given & ~(uint32_t)dateGivenTimeZone,
                   value->function,
                   value->fields,
                   value->length,
                   value->precision,
                   value->hourCycle,
                   value->calendar,
                   value->zoneStyle,
                   0};
    return key;
}

size_t mlocPreparedDateSize(DateFunction function)
{
    static const size_t sizes[] = {sizeof(mlocDateFormatter), sizeof(mlocTimeFormatter),
                                   sizeof(mlocDateTimeFormatter)};
    MLOC_ASSERT((uint32_t)function <= dateFunctionDateTime);
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
        return mlocInitDateFormatter(data, &l, &options, &prepared->formatter.date);
    }
    if (value->function == dateFunctionTime)
    {
        mlocTimeOptions options = TimeOptions(value);
        return mlocInitTimeFormatter(data, &l, &options, &prepared->formatter.time);
    }
    mlocDateTimeOptions options;
    memset(&options, 0, sizeof options);
    options.date = DateOptions(value);
    options.time = TimeOptions(value);
    options.glue = mloc_dateTimeGlueAtTime;
    return mlocInitDateTimeFormatter(data, &l, &options, &prepared->formatter.dateTime);
}

bool mlocPrepareDate(const DateValue* value, const mlocDataSet* data, const mlocLocale* locale,
                     PreparedDate* out)
{
    out->key = KeyOf(value);
    return Make(value, data, locale, out) >= 0;
}

// What a value is written as: its local date-time in the zone it is
// shown in, and for a zone style that zone and the instant.
typedef struct Shown
{
    mlocDateTime local;
    mlocInstant instant;
    mlocTimeZone zone;
} Shown;

// Writes a value as shown with a formatter that has its options.
static mlocResult WriteWith(const DateValue* value, const Shown* shown,
                            const PreparedDate* prepared, mlocOutput* output)
{
    bool inZone = value->zoneStyle != mloc_zoneStyleNone;
    if (value->function == dateFunctionDate)
    {
        return mlocDateFormatter_Format(&prepared->formatter.date, &shown->local.date, output);
    }
    if (value->function == dateFunctionTime)
    {
        return inZone ? mlocTimeFormatter_FormatInZone(&prepared->formatter.time, &shown->zone,
                                                       &shown->instant, output)
                      : mlocTimeFormatter_Format(&prepared->formatter.time, &shown->local.time,
                                                 output);
    }
    return inZone
               ? mlocDateTimeFormatter_FormatInZone(&prepared->formatter.dateTime, &shown->zone,
                                                    &shown->instant, output)
               : mlocDateTimeFormatter_Format(&prepared->formatter.dateTime, &shown->local, output);
}

// Writes a value with a formatter made here, off the resolver's frames.
MLOC_NOINLINE static mlocResult WriteMade(const DateValue* value, const Shown* shown,
                                          const mlocDataSet* data, const mlocLocale* locale,
                                          mlocOutput* output)
{
    PreparedDate made;
    mlocResult result = Make(value, data, locale, &made);
    return result < 0 ? result : WriteWith(value, shown, &made, output);
}

// How a value is shown: a zoned one at its instant in the zone it is
// shown in; a floating one as it is, at the instant its date-time names
// in the zone for a zone style.
static bool ShownOf(const DateValue* value, const mlocDataSet* data,
                    const mlocTimeZone* defaultZone, Shown* out)
{
    out->local = value->value;
    out->instant = value->instant;
    bool zoned = mlocDateZone(value, data, defaultZone, &out->zone);
    if ((value->has & dateHasZone) != 0)
    {
        return mlocGetDateTimeFor(&out->zone, &out->instant, mloc_calendarIso8601, &out->local) ==
               mloc_success;
    }
    return value->zoneStyle == mloc_zoneStyleNone ||
           (zoned && mlocGetInstantFor(&out->zone, &out->local, mloc_disambiguationCompatible,
                                       &out->instant) == mloc_success);
}

mlocMessageFaultKind mlocWriteDateValue(const DateValue* value, const PreparedDate* prepared,
                                        const mlocDataSet* data, const mlocLocale* locale,
                                        const mlocTimeZone* defaultZone, mlocOutput* output)
{
    Shown shown;
    if (!ShownOf(value, data, defaultZone, &shown))
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    DateKey key = KeyOf(value);
    mlocResult result = prepared != NULL && memcmp(&key, &prepared->key, sizeof key) == 0
                            ? WriteWith(value, &shown, prepared, output)
                            : WriteMade(value, &shown, data, locale, output);
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

// Writes a date-time: its date, T and its time.
static int WriteIsoDateTimeOf(const mlocDateTime* value, char* text, size_t capacity)
{
    int n = WriteIsoDate(&value->date, text, capacity);
    if (n < 0 || (size_t)n + 1 >= capacity)
    {
        return -1;
    }
    text[n] = 'T';
    int time = WriteIsoTime(&value->time, text + n + 1, capacity - (size_t)n - 1);
    return time < 0 ? time : n + 1 + time;
}

// Writes an instant's local date-time in a zone: with Z for UTC, else
// with the zone's offset then (its seconds where it has any, as Temporal
// writes it) and the zone's identifier in brackets.
static int WriteZoned(const mlocInstant* instant, const mlocTimeZone* zone, bool utc, char* text,
                      size_t capacity)
{
    mlocDateTime local;
    int32_t offset = 0;
    if (mlocGetDateTimeFor(zone, instant, mloc_calendarIso8601, &local) != mloc_success ||
        mlocGetTimeZoneOffset(zone, instant, &offset) != mloc_success)
    {
        return -1;
    }
    int n = WriteIsoDateTimeOf(&local, text, capacity);
    if (n < 0 || (size_t)n >= capacity)
    {
        return -1;
    }
    size_t left = capacity - (size_t)n;
    if (utc)
    {
        int z = snprintf(text + n, left, "Z");
        return z < 0 ? z : n + z;
    }
    unsigned size = (unsigned)(offset < 0 ? -(int64_t)offset : offset);
    char sign = offset < 0 ? '-' : '+';
    int more = size % 60 != 0
                   ? snprintf(text + n, left, "%c%02u:%02u:%02u[", sign, size / 3600,
                              size / 60 % 60, size % 60)
                   : snprintf(text + n, left, "%c%02u:%02u[", sign, size / 3600, size / 60 % 60);
    if (more < 0 || (size_t)more + 1 >= left)
    {
        return -1;
    }
    n += more;
    // Room for the identifier and the closing bracket.
    mlocOutput out = {text + n, capacity - (size_t)n - 1, 0, NULL, NULL};
    if (mlocFormatTimeZoneId(zone, &out) != mloc_success)
    {
        return -1;
    }
    n += (int)out.length;
    text[n] = ']';
    return n + 1;
}

size_t mlocWriteIsoDateTime(const mlocMessageValue* argument, char* text, size_t capacity)
{
    int n = -1;
    mlocTimeZone utc;
    memset(&utc, 0, sizeof utc);
    switch (argument->kind)
    {
    case mloc_messageValueDate:
        n = WriteIsoDate(&argument->as.date, text, capacity);
        break;
    case mloc_messageValueTime:
        n = WriteIsoTime(&argument->as.time, text, capacity);
        break;
    case mloc_messageValueDateTime:
        n = WriteIsoDateTimeOf(&argument->as.dateTime, text, capacity);
        break;
    case mloc_messageValueInstant:
        n = WriteZoned(&argument->as.instant, &utc, true, text, capacity);
        break;
    case mloc_messageValueZonedDateTime:
        n = WriteZoned(&argument->as.zoned.instant, argument->as.zoned.zone, false, text, capacity);
        break;
    default:
        break;
    }
    return n < 0 || (size_t)n >= capacity ? 0 : (size_t)n;
}
