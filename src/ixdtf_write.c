// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Temporal's strings out (maul-locale/ixdtf.h): each type's toString as
// ECMA-262's formatting operations write it, into a caller's buffer, with
// no floating-point arithmetic: a duration's fields are read from their
// bits and its seconds summed exactly.

#include "binary64.h"
#include "calendar_arith.h"
#include "calendar_year.h"
#include "date_time.h"

#include "maul-locale/ixdtf.h"

#include <string.h>

enum
{
    nanosecondsPerSecond = 1000000000,
    // A precision as the writers take it: 0 to 9 digits, or these.
    precisionAuto = -1,
    precisionMinute = -2,
    // The longest zone identifier the library writes.
    maxZoneId = 64,
};

static const int64_t s_nanosecondsPerDay = INT64_C(86400000000000);

// Appends to an output, measuring once the buffer is full.
typedef struct Writer
{
    mlocOutput* output;
    size_t length;
} Writer;

static void Text(Writer* w, const char* text, size_t length)
{
    if (w->length + length <= w->output->capacity && length > 0)
    {
        memcpy(w->output->buffer + w->length, text, length);
    }
    w->length += length;
}

static void Char(Writer* w, char ch)
{
    Text(w, &ch, 1);
}

// Writes value's decimal digits, at least width of them.
static void Digits(Writer* w, uint64_t value, int width)
{
    char digits[20];
    int n = 0;
    do
    {
        digits[sizeof digits - 1 - n] = (char)('0' + value % 10);
        value /= 10;
        n += 1;
    } while (value != 0);
    for (; n < width; n += 1)
    {
        digits[sizeof digits - 1 - n] = '0';
    }
    Text(w, digits + sizeof digits - n, (size_t)n);
}

static mlocResult Finish(Writer* w)
{
    w->output->length = w->length;
    return w->length > w->output->capacity ? mloc_errorCapacity : mloc_success;
}

static bool OutputValid(const mlocOutput* output)
{
    return output != nullptr && (output->buffer != nullptr || output->capacity == 0);
}

static bool IsAlphaNumeric(char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

// Whether a calendar id is the grammar's AnnotationValue: components of
// one to eight letters or digits joined by hyphens.
static bool CalendarIdValid(const char* id, size_t length)
{
    size_t run = 0;
    for (size_t i = 0; i < length; i += 1)
    {
        if (id[i] == '-' && run > 0)
        {
            run = 0;
        }
        else if (IsAlphaNumeric(id[i]) && run < 8)
        {
            run += 1;
        }
        else
        {
            return false;
        }
    }
    return run > 0;
}

// The options with their defaults, checked, and the precision as 0 to 9
// digits, precisionAuto or precisionMinute; false when out of range.
static bool ReadOptions(const mlocTemporalStringOptions* given, mlocTemporalStringOptions* out,
                        int* precisionOut)
{
    static const char s_iso[7] = {'i', 's', 'o', '8', '6', '0', '1'};
    *out = given != nullptr ? *given : (mlocTemporalStringOptions){0};
    if (out->calendar == nullptr)
    {
        if (out->calendarLength != 0)
        {
            return false;
        }
        out->calendar = s_iso;
        out->calendarLength = sizeof s_iso;
    }
    *precisionOut = out->secondsPrecision == mloc_secondsAuto     ? precisionAuto
                    : out->secondsPrecision == mloc_secondsMinute ? precisionMinute
                                                                  : out->fractionDigits;
    return CalendarIdValid(out->calendar, out->calendarLength) &&
           out->secondsPrecision <= mloc_secondsDigits &&
           (out->secondsPrecision != mloc_secondsDigits || out->fractionDigits <= 9) &&
           out->showCalendar <= mloc_showCalendarCritical &&
           out->showTimeZone <= mloc_showTimeZoneCritical;
}

static bool IsIso(const mlocTemporalStringOptions* o)
{
    return o->calendarLength == 7 && memcmp(o->calendar, "iso8601", 7) == 0;
}

// PadISOYear.
static void Year(Writer* w, int32_t year)
{
    if (year >= 0 && year <= 9999)
    {
        Digits(w, (uint64_t)year, 4);
        return;
    }
    Char(w, year > 0 ? '+' : '-');
    Digits(w, year > 0 ? (uint64_t)year : (uint64_t)0 - (uint64_t)year, 6);
}

// FormatCalendarAnnotation.
static void CalendarAnnotation(Writer* w, const mlocTemporalStringOptions* o)
{
    if (o->showCalendar == mloc_showCalendarNever ||
        (o->showCalendar == mloc_showCalendarAuto && IsIso(o)))
    {
        return;
    }
    Text(w, o->showCalendar == mloc_showCalendarCritical ? "[!u-ca=" : "[u-ca=",
         o->showCalendar == mloc_showCalendarCritical ? 7 : 6);
    Text(w, o->calendar, o->calendarLength);
    Char(w, ']');
}

// FormatFractionalSeconds.
static void Fraction(Writer* w, uint32_t nanoseconds, int precision)
{
    int digits = precision;
    if (precision == precisionAuto)
    {
        digits = 9;
        for (uint32_t v = nanoseconds; digits > 0 && v % 10 == 0; v /= 10)
        {
            digits -= 1;
        }
    }
    if (digits <= 0)
    {
        return;
    }
    char text[10] = {'.'};
    for (int i = 9; i >= 1; i -= 1)
    {
        text[i] = (char)('0' + nanoseconds % 10);
        nanoseconds /= 10;
    }
    Text(w, text, (size_t)digits + 1);
}

// FormatTimeString, separated.
static void Time(Writer* w, uint32_t hour, uint32_t minute, uint32_t second, uint32_t nanoseconds,
                 int precision)
{
    Digits(w, hour, 2);
    Char(w, ':');
    Digits(w, minute, 2);
    if (precision == precisionMinute)
    {
        return;
    }
    Char(w, ':');
    Digits(w, second, 2);
    Fraction(w, nanoseconds, precision);
}

static void Date(Writer* w, CalendarDate d)
{
    Year(w, d.year);
    Char(w, '-');
    Digits(w, d.month, 2);
    Char(w, '-');
    Digits(w, d.day, 2);
}

static void DateTime(Writer* w, const mlocDateTime* dt, int precision)
{
    Date(w, mlocArithmeticFromDay(mloc_calendarGregorian, dt->date.day));
    Char(w, 'T');
    Time(w, dt->time.hour, dt->time.minute, dt->time.second, dt->time.nanosecond, precision);
}

// The writer, the options and their precision.
typedef struct Start
{
    Writer w;
    mlocTemporalStringOptions o;
    int precision;
} Start;

static bool Begin(const mlocTemporalStringOptions* given, mlocOutput* output, Start* s)
{
    s->w = (Writer){output, 0};
    return OutputValid(output) && ReadOptions(given, &s->o, &s->precision);
}

mlocResult mlocFormatTemporalDate(const mlocDate* date, const mlocTemporalStringOptions* options,
                                  mlocOutput* textOut)
{
    Start s;
    if (!Begin(options, textOut, &s) || !mlocYearDateValid(date))
    {
        return mloc_errorInvalid;
    }
    Date(&s.w, mlocArithmeticFromDay(mloc_calendarGregorian, date->day));
    CalendarAnnotation(&s.w, &s.o);
    return Finish(&s.w);
}

// A year-month or month-day: the part always shown, then the reference
// part and the annotation when the calendar is not ISO or is shown.
static mlocResult Partial(const mlocDate* date, bool yearMonth,
                          const mlocTemporalStringOptions* options, mlocOutput* output)
{
    Start s;
    if (!Begin(options, output, &s) || !mlocYearDateValid(date))
    {
        return mloc_errorInvalid;
    }
    CalendarDate d = mlocArithmeticFromDay(mloc_calendarGregorian, date->day);
    bool full = s.o.showCalendar == mloc_showCalendarAlways ||
                s.o.showCalendar == mloc_showCalendarCritical || !IsIso(&s.o);
    if (yearMonth || full)
    {
        Year(&s.w, d.year);
        Char(&s.w, '-');
    }
    Digits(&s.w, d.month, 2);
    if (!yearMonth || full)
    {
        Char(&s.w, '-');
        Digits(&s.w, d.day, 2);
    }
    if (full)
    {
        CalendarAnnotation(&s.w, &s.o);
    }
    return Finish(&s.w);
}

mlocResult mlocFormatYearMonth(const mlocDate* date, const mlocTemporalStringOptions* options,
                               mlocOutput* textOut)
{
    return Partial(date, true, options, textOut);
}

mlocResult mlocFormatMonthDay(const mlocDate* date, const mlocTemporalStringOptions* options,
                              mlocOutput* textOut)
{
    return Partial(date, false, options, textOut);
}

mlocResult mlocFormatTemporalTime(const mlocTime* time, const mlocTemporalStringOptions* options,
                                  mlocOutput* textOut)
{
    Start s;
    if (!Begin(options, textOut, &s) || time == nullptr || !mlocTimeValid(time))
    {
        return mloc_errorInvalid;
    }
    Time(&s.w, time->hour, time->minute, time->second, time->nanosecond, s.precision);
    return Finish(&s.w);
}

mlocResult mlocFormatTemporalDateTime(const mlocDateTime* dateTime,
                                      const mlocTemporalStringOptions* options, mlocOutput* textOut)
{
    Start s;
    if (!Begin(options, textOut, &s) || dateTime == nullptr || !mlocDateTimeValid(dateTime))
    {
        return mloc_errorInvalid;
    }
    DateTime(&s.w, dateTime, s.precision);
    CalendarAnnotation(&s.w, &s.o);
    return Finish(&s.w);
}

// FormatDateTimeUTCOffsetRounded of an offset in whole seconds: to the
// minute, half away from zero.
static void OffsetRounded(Writer* w, int32_t seconds)
{
    uint32_t magnitude = seconds < 0 ? (uint32_t)0 - (uint32_t)seconds : (uint32_t)seconds;
    uint32_t minutes = (magnitude + 30) / 60;
    Char(w, seconds < 0 && minutes != 0 ? '-' : '+');
    Time(w, minutes / 60, minutes % 60, 0, 0, precisionMinute);
}

// The local date-time and offset of an instant, in universal time
// without a zone.
static bool Local(const mlocTimeZone* zone, const mlocInstant* instant, mlocDateTime* dateTimeOut,
                  int32_t* offsetOut)
{
    static const mlocTimeZone s_utc = {0};
    const mlocTimeZone* z = zone != nullptr ? zone : &s_utc;
    return instant != nullptr &&
           mlocGetDateTimeFor(z, instant, mloc_calendarIso8601, dateTimeOut) == mloc_success &&
           mlocGetTimeZoneOffset(z, instant, offsetOut) == mloc_success;
}

mlocResult mlocFormatInstant(const mlocInstant* instant, const mlocTimeZone* zone,
                             const mlocTemporalStringOptions* options, mlocOutput* textOut)
{
    Start s;
    mlocDateTime local;
    int32_t offset = 0;
    if (!Begin(options, textOut, &s) || !Local(zone, instant, &local, &offset))
    {
        return mloc_errorInvalid;
    }
    DateTime(&s.w, &local, s.precision);
    if (zone == nullptr)
    {
        Char(&s.w, 'Z');
    }
    else
    {
        OffsetRounded(&s.w, offset);
    }
    return Finish(&s.w);
}

mlocResult mlocFormatZonedDateTime(const mlocTimeZone* zone, const mlocInstant* instant,
                                   const mlocTemporalStringOptions* options, mlocOutput* textOut)
{
    Start s;
    mlocDateTime local;
    int32_t offset = 0;
    char id[maxZoneId];
    mlocOutput idOutput = {.buffer = id, .capacity = sizeof id};
    if (!Begin(options, textOut, &s) || zone == nullptr || !Local(zone, instant, &local, &offset) ||
        mlocFormatTimeZoneId(zone, &idOutput) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    DateTime(&s.w, &local, s.precision);
    if (!s.o.hideOffset)
    {
        OffsetRounded(&s.w, offset);
    }
    if (s.o.showTimeZone != mloc_showTimeZoneNever)
    {
        Text(&s.w, s.o.showTimeZone == mloc_showTimeZoneCritical ? "[!" : "[",
             s.o.showTimeZone == mloc_showTimeZoneCritical ? 2 : 1);
        Text(&s.w, id, idOutput.length);
        Char(&s.w, ']');
    }
    CalendarAnnotation(&s.w, &s.o);
    return Finish(&s.w);
}

// A duration's field magnitudes, sign and seconds part, read from the
// float64 fields of a valid duration.
typedef struct Fields
{
    uint64_t magnitude[10];
    int sign;
    Wide subMinute;
} Fields;

static bool ReadDuration(const mlocDurationValues* v, Fields* f)
{
    mlocDateTimeDuration checked;
    if (v == nullptr || mlocMakeDuration(v, &checked) != mloc_success)
    {
        return false;
    }
    const double given[10] = {v->years,        v->months,     v->weeks,   v->days,
                              v->hours,        v->minutes,    v->seconds, v->milliseconds,
                              v->microseconds, v->nanoseconds};
    Wide wide[10];
    f->sign = 0;
    for (int i = 0; i < 10; i += 1)
    {
        bool negative = false;
        (void)mlocWideOfDouble(given[i], &negative, &wide[i]);
        f->magnitude[i] = wide[i].low;
        if (f->sign == 0 && (wide[i].high != 0 || wide[i].low != 0))
        {
            f->sign = negative ? -1 : 1;
        }
    }
    // Valid, the seconds and milliseconds fit 64 bits and the sum of the
    // sub-minute fields in nanoseconds stays below 2^84.
    f->subMinute = mlocWideProduct(wide[6].low, nanosecondsPerSecond);
    f->subMinute = mlocWideSum(f->subMinute, mlocWideProduct(wide[7].low, 1000000));
    f->subMinute = mlocWideSum(f->subMinute, mlocWideScaled(wide[8], 1000));
    f->subMinute = mlocWideSum(f->subMinute, wide[9]);
    return true;
}

mlocResult mlocFormatTemporalDuration(const mlocDurationValues* values,
                                      const mlocTemporalStringOptions* options, mlocOutput* textOut)
{
    static const char s_designators[6] = {'Y', 'M', 'W', 'D', 'H', 'M'};
    Start s;
    Fields f;
    if (!Begin(options, textOut, &s) || s.precision == precisionMinute || !ReadDuration(values, &f))
    {
        return mloc_errorInvalid;
    }
    if (f.sign < 0)
    {
        Char(&s.w, '-');
    }
    Char(&s.w, 'P');
    bool time = false;
    bool minutesAndHigher = false;
    for (int i = 0; i < 6; i += 1)
    {
        if (f.magnitude[i] == 0)
        {
            continue;
        }
        if (i >= 4 && !time)
        {
            Char(&s.w, 'T');
            time = true;
        }
        Digits(&s.w, f.magnitude[i], 1);
        Char(&s.w, s_designators[i]);
        minutesAndHigher = true;
    }
    bool seconds = f.subMinute.high != 0 || f.subMinute.low != 0;
    if (seconds || !minutesAndHigher || s.precision != precisionAuto)
    {
        if (!time)
        {
            Char(&s.w, 'T');
        }
        Wide remainder;
        Digits(&s.w, mlocWideQuotient(f.subMinute, (Wide){0, nanosecondsPerSecond}, &remainder), 1);
        Fraction(&s.w, (uint32_t)remainder.low, s.precision);
        Char(&s.w, 'S');
    }
    return Finish(&s.w);
}

mlocResult mlocFormatUTCOffset(int64_t offsetNanoseconds, mlocOutput* textOut)
{
    Writer w = {textOut, 0};
    if (!OutputValid(textOut) || offsetNanoseconds <= -s_nanosecondsPerDay ||
        offsetNanoseconds >= s_nanosecondsPerDay)
    {
        return mloc_errorInvalid;
    }
    uint64_t magnitude = offsetNanoseconds < 0 ? (uint64_t)0 - (uint64_t)offsetNanoseconds
                                               : (uint64_t)offsetNanoseconds;
    uint64_t seconds = magnitude / nanosecondsPerSecond;
    uint32_t fraction = (uint32_t)(magnitude % nanosecondsPerSecond);
    Char(&w, offsetNanoseconds < 0 ? '-' : '+');
    Time(&w, (uint32_t)(seconds / 3600), (uint32_t)(seconds / 60 % 60), (uint32_t)(seconds % 60),
         fraction, seconds % 60 == 0 && fraction == 0 ? precisionMinute : precisionAuto);
    return Finish(&w);
}
