// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Temporal's date-time, offset, time zone and calendar strings
// (maul-locale/ixdtf.h, record mloc-0041): ECMA-262's grammar for them,
// read by recursive descent with a cursor that each rule restores when it
// does not match. A goal matches syntactically first; the annotation
// rules, which make ParseISODateTime throw rather than try another goal,
// are applied after.

#include "binary64.h"

#include "maul-locale/ixdtf.h"

enum
{
    nanosecondsPerSecond = 1000000000,
};

static const int64_t s_nanosecondsPerMinute = INT64_C(60000000000);

typedef struct Cursor
{
    const char* text;
    size_t length;
    size_t at;
} Cursor;

static bool AtEnd(const Cursor* c)
{
    return c->at == c->length;
}

static char Next(const Cursor* c)
{
    return c->at < c->length ? c->text[c->at] : '\0';
}

static bool Take(Cursor* c, char ch)
{
    if (c->at < c->length && c->text[c->at] == ch)
    {
        c->at += 1;
        return true;
    }
    return false;
}

// One of two characters, as the grammar's designators take either case.
static bool TakeEither(Cursor* c, char upper, char lower)
{
    return Take(c, upper) || Take(c, lower);
}

static bool IsDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static bool IsAlpha(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

// count digits as a number.
static bool Digits(Cursor* c, int count, int32_t* out)
{
    if (c->length - c->at < (size_t)count)
    {
        return false;
    }
    int32_t value = 0;
    for (int i = 0; i < count; i += 1)
    {
        char ch = c->text[c->at + (size_t)i];
        if (!IsDigit(ch))
        {
            return false;
        }
        value = value * 10 + (ch - '0');
    }
    c->at += (size_t)count;
    *out = value;
    return true;
}

// Two digits from 0 to a limit.
static bool Bounded(Cursor* c, int32_t limit, int32_t* out)
{
    size_t start = c->at;
    if (!Digits(c, 2, out) || *out > limit)
    {
        c->at = start;
        return false;
    }
    return true;
}

// DateYear: four digits, or a sign and six that are not -000000.
static bool Year(Cursor* c, int32_t* out)
{
    size_t start = c->at;
    bool negative = Take(c, '-');
    if (negative || Take(c, '+'))
    {
        if (!Digits(c, 6, out) || (negative && *out == 0))
        {
            c->at = start;
            return false;
        }
        *out = negative ? -*out : *out;
        return true;
    }
    return Digits(c, 4, out);
}

static bool Month(Cursor* c, int32_t* out)
{
    size_t start = c->at;
    if (!Bounded(c, 12, out) || *out == 0)
    {
        c->at = start;
        return false;
    }
    return true;
}

static bool Day(Cursor* c, int32_t* out)
{
    size_t start = c->at;
    if (!Bounded(c, 31, out) || *out == 0)
    {
        c->at = start;
        return false;
    }
    return true;
}

static bool IsLeap(int32_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

// IsValidMonthDay, and IsValidDate with a year.
static bool ValidMonthDay(int32_t month, int32_t day)
{
    bool thirty = month == 4 || month == 6 || month == 9 || month == 11;
    return !(day == 31 && (thirty || month == 2)) && !(month == 2 && day == 30);
}

typedef struct Fields
{
    int32_t year;
    int32_t month;
    int32_t day;
    bool hasYear;
    bool hasTime;
    int32_t hour;
    int32_t minute;
    int32_t second;
    int32_t nanosecond;
    bool utc;
    bool hasOffset;
    bool offsetHasSeconds;
    int64_t offsetNanoseconds;
    mlocTextSpan timeZone;
    // The annotations' facts: the first calendar, whether it was
    // critical, and whether a later calendar or another annotation makes
    // the string an error.
    mlocTextSpan calendar;
    bool calendarCritical;
    bool refused;
    // Matched without a date-time (DateSpecYearMonth, DateSpecMonthDay).
    bool dateSpecOnly;
} Fields;

// DateSpec[Extended]: the separators both present or both absent.
static bool DateSpec(Cursor* c, Fields* f)
{
    size_t start = c->at;
    if (!Year(c, &f->year))
    {
        return false;
    }
    bool extended = Take(c, '-');
    if (!Month(c, &f->month) || (extended ? !Take(c, '-') : Next(c) == '-') || !Day(c, &f->day) ||
        !ValidMonthDay(f->month, f->day) || (f->month == 2 && f->day == 29 && !IsLeap(f->year)))
    {
        c->at = start;
        return false;
    }
    f->hasYear = true;
    return true;
}

// TemporalDecimalFraction: a separator and 1 to 9 digits, as nanoseconds.
static bool Fraction(Cursor* c, int32_t* out)
{
    size_t start = c->at;
    if (!Take(c, '.') && !Take(c, ','))
    {
        return false;
    }
    int32_t value = 0;
    int count = 0;
    while (IsDigit(Next(c)) && count < 9)
    {
        value = value * 10 + (Next(c) - '0');
        c->at += 1;
        count += 1;
    }
    if (count == 0 || IsDigit(Next(c)))
    {
        c->at = start;
        return false;
    }
    for (; count < 9; count += 1)
    {
        value *= 10;
    }
    *out = value;
    return true;
}

// TimeSpec[Extended]: an hour, then minutes, then seconds (60 allowed)
// with a fraction; colons throughout or none.
static bool TimeSpec(Cursor* c, Fields* f)
{
    size_t start = c->at;
    f->minute = f->second = f->nanosecond = 0;
    if (!Bounded(c, 23, &f->hour))
    {
        return false;
    }
    bool extended = Next(c) == ':';
    if (!extended && !IsDigit(Next(c)))
    {
        f->hasTime = true;
        return true;
    }
    if ((extended && !Take(c, ':')) || !Bounded(c, 59, &f->minute))
    {
        c->at = start;
        return false;
    }
    if ((extended && Next(c) != ':') || (!extended && !IsDigit(Next(c))))
    {
        f->hasTime = true;
        return true;
    }
    if ((extended && !Take(c, ':')) || !Bounded(c, 60, &f->second))
    {
        c->at = start;
        return false;
    }
    (void)Fraction(c, &f->nanosecond);
    f->hasTime = true;
    return true;
}

// UTCOffset[SubMinutePrecision]: a sign and an hour, then minutes, then
// (with sub-minute precision) seconds and a fraction; colons throughout
// or none.
static bool UTCOffset(Cursor* c, bool subMinute, int64_t* nanosecondsOut, bool* hasSecondsOut)
{
    size_t start = c->at;
    bool negative = Next(c) == '-';
    int32_t hour = 0;
    int32_t minute = 0;
    int32_t second = 0;
    int32_t fraction = 0;
    *hasSecondsOut = false;
    if ((!Take(c, '+') && !Take(c, '-')) || !Bounded(c, 23, &hour))
    {
        c->at = start;
        return false;
    }
    bool extended = Next(c) == ':';
    size_t mark = c->at;
    if ((!extended || Take(c, ':')) && Bounded(c, 59, &minute))
    {
        mark = c->at;
        if (subMinute && (!extended || Take(c, ':')) && Bounded(c, 59, &second))
        {
            *hasSecondsOut = true;
            (void)Fraction(c, &fraction);
        }
        else
        {
            c->at = mark;
            second = 0;
        }
    }
    else
    {
        c->at = mark;
        minute = 0;
    }
    int64_t total = ((int64_t)hour * 3600 + minute * 60 + second) * nanosecondsPerSecond + fraction;
    *nanosecondsOut = negative ? -total : total;
    return true;
}

// DateTimeUTCOffset[Z].
static bool DateTimeOffset(Cursor* c, bool z, Fields* f)
{
    if (z && TakeEither(c, 'Z', 'z'))
    {
        f->utc = true;
        return true;
    }
    if (Next(c) != '+' && Next(c) != '-')
    {
        return false;
    }
    f->hasOffset = UTCOffset(c, true, &f->offsetNanoseconds, &f->offsetHasSeconds);
    return f->hasOffset;
}

static bool IsZoneLeading(char ch)
{
    return IsAlpha(ch) || ch == '.' || ch == '_';
}

static bool IsZoneChar(char ch)
{
    return IsZoneLeading(ch) || IsDigit(ch) || ch == '-' || ch == '+';
}

// TimeZoneIANAName: components of a leading character and more, joined
// by slashes.
static bool ZoneName(Cursor* c)
{
    size_t start = c->at;
    do
    {
        if (!IsZoneLeading(Next(c)))
        {
            c->at = start;
            return false;
        }
        while (IsZoneChar(Next(c)))
        {
            c->at += 1;
        }
    } while (Take(c, '/'));
    return true;
}

// TimeZoneIdentifier: an offset to the minute, or a name.
static bool ZoneIdentifier(Cursor* c, bool* isOffsetOut, int64_t* offsetOut)
{
    bool seconds = false;
    *isOffsetOut = Next(c) == '+' || Next(c) == '-';
    return *isOffsetOut ? UTCOffset(c, false, offsetOut, &seconds) : ZoneName(c);
}

// TimeZoneAnnotation: [ !? TimeZoneIdentifier ].
static bool ZoneAnnotation(Cursor* c, Fields* f)
{
    size_t start = c->at;
    bool isOffset = false;
    int64_t offset = 0;
    if (!Take(c, '['))
    {
        return false;
    }
    (void)Take(c, '!');
    size_t idStart = c->at;
    if (!ZoneIdentifier(c, &isOffset, &offset) || !Take(c, ']'))
    {
        c->at = start;
        return false;
    }
    f->timeZone = (mlocTextSpan){idStart, c->at - 1 - idStart};
    return true;
}

static bool IsKeyLeading(char ch)
{
    return (ch >= 'a' && ch <= 'z') || ch == '_';
}

// AnnotationValue: components of letters and digits joined by hyphens.
static bool AnnotationValue(Cursor* c)
{
    size_t start = c->at;
    do
    {
        size_t component = c->at;
        while (IsAlpha(Next(c)) || IsDigit(Next(c)))
        {
            c->at += 1;
        }
        if (c->at == component)
        {
            c->at = start;
            return false;
        }
    } while (Take(c, '-'));
    return true;
}

// Annotation: [ !? key = value ], with u-ca's rules noted.
static bool Annotation(Cursor* c, Fields* f)
{
    size_t start = c->at;
    if (!Take(c, '['))
    {
        return false;
    }
    bool critical = Take(c, '!');
    size_t keyStart = c->at;
    if (!IsKeyLeading(Next(c)))
    {
        c->at = start;
        return false;
    }
    while (IsKeyLeading(Next(c)) || IsDigit(Next(c)) || Next(c) == '-')
    {
        c->at += 1;
    }
    size_t keyLength = c->at - keyStart;
    size_t valueStart = c->at + 1;
    if (!Take(c, '=') || !AnnotationValue(c) || !Take(c, ']'))
    {
        c->at = start;
        return false;
    }
    bool calendar = keyLength == 4 && c->text[keyStart] == 'u' && c->text[keyStart + 1] == '-' &&
                    c->text[keyStart + 2] == 'c' && c->text[keyStart + 3] == 'a';
    if (!calendar)
    {
        f->refused = f->refused || critical;
    }
    else if (f->calendar.length == 0)
    {
        f->calendar = (mlocTextSpan){valueStart, c->at - 1 - valueStart};
        f->calendarCritical = critical;
    }
    else
    {
        f->refused = f->refused || critical || f->calendarCritical;
    }
    return true;
}

// TimeZoneAnnotation? Annotations? to the end of the text.
static bool Suffix(Cursor* c, bool zoneRequired, Fields* f)
{
    if (!ZoneAnnotation(c, f) && zoneRequired)
    {
        return false;
    }
    while (Annotation(c, f))
    {
    }
    return AtEnd(c);
}

// AnnotatedDateTime[Zoned, TimeRequired].
static bool AnnotatedDateTime(Cursor* c, bool zoned, bool timeRequired, Fields* f)
{
    if (!DateSpec(c, f))
    {
        return false;
    }
    if (Take(c, ' ') || TakeEither(c, 'T', 't'))
    {
        if (!TimeSpec(c, f))
        {
            return false;
        }
        (void)DateTimeOffset(c, zoned, f);
    }
    else if (timeRequired)
    {
        return false;
    }
    return Suffix(c, zoned, f);
}

static bool Instant(Cursor* c, Fields* f)
{
    return DateSpec(c, f) && (Take(c, ' ') || TakeEither(c, 'T', 't')) && TimeSpec(c, f) &&
           DateTimeOffset(c, true, f) && Suffix(c, false, f);
}

// DateSpecYearMonth: a year and a month.
static bool YearMonthSpec(Cursor* c, Fields* f)
{
    size_t start = c->at;
    if (!Year(c, &f->year))
    {
        return false;
    }
    (void)Take(c, '-');
    if (!Month(c, &f->month))
    {
        c->at = start;
        return false;
    }
    f->hasYear = true;
    f->day = 1;
    return true;
}

// DateSpecMonthDay: an optional --, a month and a day.
static bool MonthDaySpec(Cursor* c, Fields* f)
{
    size_t start = c->at;
    if (Next(c) == '-')
    {
        // Both hyphens of --, or neither.
        (void)Take(c, '-');
        if (!Take(c, '-'))
        {
            c->at = start;
            return false;
        }
    }
    if (!Month(c, &f->month))
    {
        c->at = start;
        return false;
    }
    (void)Take(c, '-');
    if (!Day(c, &f->day) || !ValidMonthDay(f->month, f->day))
    {
        c->at = start;
        return false;
    }
    return true;
}

// Whether a text is exactly a DateSpecMonthDay or a DateSpecYearMonth,
// AnnotatedTime's early error.
static bool ReadsAsDate(const char* text, size_t length)
{
    Fields scratch = {0};
    Cursor c = {text, length, 0};
    if (MonthDaySpec(&c, &scratch) && AtEnd(&c))
    {
        return true;
    }
    c.at = 0;
    return YearMonthSpec(&c, &scratch) && AtEnd(&c);
}

// AnnotatedTime: T and a time, or a time that does not read as a date.
static bool AnnotatedTime(Cursor* c, Fields* f)
{
    size_t start = c->at;
    bool designated = TakeEither(c, 'T', 't');
    if (!TimeSpec(c, f))
    {
        c->at = start;
        return false;
    }
    (void)DateTimeOffset(c, false, f);
    if (!designated && ReadsAsDate(c->text + start, c->at - start))
    {
        c->at = start;
        return false;
    }
    return Suffix(c, false, f);
}

static bool Goal(const char* text, size_t length, uint32_t format, Fields* f)
{
    Cursor c = {text, length, 0};
    *f = (Fields){.month = 1, .day = 1};
    switch (format)
    {
    case mloc_formatZonedDateTime:
        return AnnotatedDateTime(&c, true, false, f);
    case mloc_formatDateTime:
        return AnnotatedDateTime(&c, false, false, f);
    case mloc_formatInstant:
        return Instant(&c, f);
    case mloc_formatTime:
        if (AnnotatedTime(&c, f))
        {
            return true;
        }
        *f = (Fields){.month = 1, .day = 1};
        c.at = 0;
        return AnnotatedDateTime(&c, false, true, f);
    default:
        if (format == mloc_formatMonthDay ? MonthDaySpec(&c, f) : YearMonthSpec(&c, f))
        {
            f->dateSpecOnly = true;
            if (Suffix(&c, false, f))
            {
                return true;
            }
        }
        *f = (Fields){.month = 1, .day = 1};
        c.at = 0;
        return AnnotatedDateTime(&c, false, false, f);
    }
}

// Whether a span reads iso8601, ignoring ASCII case.
static bool IsIso(const char* text, mlocTextSpan span)
{
    static const char s_iso[] = "iso8601";
    if (span.length != sizeof s_iso - 1)
    {
        return false;
    }
    for (size_t i = 0; i < span.length; i += 1)
    {
        char ch = text[span.start + i];
        if ((ch >= 'A' && ch <= 'Z' ? (char)(ch + 32) : ch) != s_iso[i])
        {
            return false;
        }
    }
    return true;
}

// ParseISODateTime's matching: the first goal allowed that matches, then
// the annotation rules.
static bool Match(const char* text, size_t length, uint32_t formats, Fields* f, uint32_t* formatOut)
{
    for (uint32_t format = 1; format <= mloc_formatYearMonth; format <<= 1)
    {
        if ((formats & format) == 0 || !Goal(text, length, format, f))
        {
            continue;
        }
        *formatOut = format;
        return !f->refused &&
               !(f->dateSpecOnly && f->calendar.length != 0 && !IsIso(text, f->calendar));
    }
    return false;
}

mlocResult mlocParseTemporalString(const char* text, size_t length, uint32_t formats,
                                   mlocTemporalParse* parseOut)
{
    Fields f;
    uint32_t format = 0;
    if (text == nullptr || parseOut == nullptr || (formats & 63) == 0)
    {
        return mloc_errorInvalid;
    }
    if (!Match(text, length, formats, &f, &format))
    {
        return mloc_errorSyntax;
    }
    bool monthDay = format == mloc_formatMonthDay && f.dateSpecOnly;
    *parseOut = (mlocTemporalParse){
        .format = format,
        .year = monthDay ? 0 : f.year,
        .month = (uint8_t)f.month,
        .day = (uint8_t)f.day,
        .hasYear = !monthDay,
        .hasTime = f.hasTime,
        .time = {(uint8_t)f.hour, (uint8_t)f.minute, (uint8_t)(f.second == 60 ? 59 : f.second), 0,
                 (uint32_t)f.nanosecond},
        .utc = f.utc,
        .hasOffset = f.hasOffset,
        .offsetHasSeconds = f.offsetHasSeconds,
        .offsetNanoseconds = f.offsetNanoseconds,
        .timeZone = f.timeZone,
        .calendar = f.calendar,
    };
    return mloc_success;
}

mlocResult mlocParseUTCOffset(const char* text, size_t length, int64_t* nanosecondsOut,
                              bool* hasSecondsOut)
{
    Cursor c = {text, length, 0};
    int64_t offset = 0;
    bool seconds = false;
    if (text == nullptr || nanosecondsOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    if (!UTCOffset(&c, true, &offset, &seconds) || !AtEnd(&c))
    {
        return mloc_errorSyntax;
    }
    *nanosecondsOut = offset;
    if (hasSecondsOut != nullptr)
    {
        *hasSecondsOut = seconds;
    }
    return mloc_success;
}

// ParseTimeZoneIdentifier of a whole span.
static bool Identifier(const char* text, mlocTextSpan span, mlocTimeZoneParse* out)
{
    Cursor c = {text + span.start, span.length, 0};
    bool isOffset = false;
    int64_t offset = 0;
    if (!ZoneIdentifier(&c, &isOffset, &offset) || !AtEnd(&c))
    {
        return false;
    }
    *out = (mlocTimeZoneParse){0};
    if (isOffset)
    {
        out->hasOffset = true;
        out->offsetMinutes = (int16_t)(offset / s_nanosecondsPerMinute);
    }
    else
    {
        out->name = span;
    }
    return true;
}

mlocResult mlocParseTimeZoneString(const char* text, size_t length, mlocTimeZoneParse* parseOut)
{
    if (text == nullptr || parseOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    mlocTimeZoneParse out;
    if (Identifier(text, (mlocTextSpan){0, length}, &out))
    {
        *parseOut = out;
        return mloc_success;
    }
    Fields f;
    uint32_t format = 0;
    if (!Match(text, length, 63, &f, &format))
    {
        return mloc_errorSyntax;
    }
    if (f.timeZone.length != 0)
    {
        // The annotation's identifier, which the grammar already read.
        if (!Identifier(text, f.timeZone, &out))
        {
            return mloc_errorSyntax;
        }
    }
    else if (f.utc)
    {
        out = (mlocTimeZoneParse){.utc = true};
    }
    else if (f.hasOffset)
    {
        // ParseTimeZoneIdentifier refuses an offset with seconds.
        if (f.offsetHasSeconds)
        {
            return mloc_errorSyntax;
        }
        out = (mlocTimeZoneParse){.hasOffset = true,
                                  .offsetMinutes =
                                      (int16_t)(f.offsetNanoseconds / s_nanosecondsPerMinute)};
    }
    else
    {
        return mloc_errorSyntax;
    }
    *parseOut = out;
    return mloc_success;
}

mlocResult mlocParseCalendarString(const char* text, size_t length, mlocTextSpan* calendarOut)
{
    Fields f;
    uint32_t format = 0;
    if (text == nullptr || calendarOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    if (Match(text, length, 63, &f, &format))
    {
        *calendarOut = f.calendar;
        return mloc_success;
    }
    Cursor c = {text, length, 0};
    if (!AnnotationValue(&c) || !AtEnd(&c))
    {
        return mloc_errorSyntax;
    }
    *calendarOut = (mlocTextSpan){0, length};
    return mloc_success;
}

// A run of digits, at least one, as a number; saturated past 2^120,
// which every field limit is far below.
static Wide Number(Cursor* c)
{
    Wide out = {0, 0};
    while (IsDigit(Next(c)))
    {
        if (out.high >> 56 == 0)
        {
            out = mlocWideSum(mlocWideScaled(out, 10), (Wide){0, (uint64_t)(Next(c) - '0')});
        }
        c->at += 1;
    }
    return out;
}

// Which of the designators a character is, from the first given, or -1.
static int Designator(char ch, const char* designators, int count)
{
    char upper = ch >= 'a' && ch <= 'z' ? (char)(ch - 32) : ch;
    for (int i = 0; i < count; i += 1)
    {
        if (designators[i] == upper)
        {
            return i;
        }
    }
    return -1;
}

// DurationDate's parts, years to days, each once and in order.
static bool DateParts(Cursor* c, Wide* f, bool* anyOut)
{
    int next = 0;
    while (IsDigit(Next(c)))
    {
        Wide value = Number(c);
        int unit = Designator(Next(c), "YMWD", 4);
        if (unit < next)
        {
            return false;
        }
        c->at += 1;
        f[unit] = value;
        next = unit + 1;
        *anyOut = true;
    }
    return true;
}

// DurationTime's parts, hours to seconds, each once and in order; only
// the last may have a fraction, which carries down into the fields below
// it exactly (its nanoseconds are a whole number for 1 to 9 digits).
static bool TimeParts(Cursor* c, Wide* f)
{
    static const uint64_t s_unitNanoseconds[3] = {UINT64_C(3600000000000), 60000000000, 1000000000};
    int next = 0;
    while (IsDigit(Next(c)))
    {
        Wide value = Number(c);
        int32_t fraction = 0;
        bool fractional = Fraction(c, &fraction);
        int unit = Designator(Next(c), "HMS", 3);
        if (unit < next)
        {
            return false;
        }
        c->at += 1;
        f[4 + unit] = value;
        next = unit + 1;
        if (fractional)
        {
            // fraction is the digits scaled to nine places.
            uint64_t below = (uint64_t)fraction * (s_unitNanoseconds[unit] / nanosecondsPerSecond);
            uint64_t seconds = below / nanosecondsPerSecond;
            uint64_t rest = below % nanosecondsPerSecond;
            if (unit == 0)
            {
                f[5] = (Wide){0, seconds / 60};
                f[6] = (Wide){0, seconds % 60};
            }
            else if (unit == 1)
            {
                f[6] = (Wide){0, seconds};
            }
            f[7] = (Wide){0, rest / 1000000};
            f[8] = (Wide){0, rest / 1000 % 1000};
            f[9] = (Wide){0, rest % 1000};
            return AtEnd(c);
        }
    }
    return next > 0;
}

mlocResult mlocParseDuration(const char* text, size_t length, mlocDurationValues* valuesOut)
{
    if (text == nullptr || valuesOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    Cursor c = {text, length, 0};
    Wide f[10] = {{0, 0}};
    bool negative = Take(&c, '-');
    bool any = false;
    if (!negative)
    {
        (void)Take(&c, '+');
    }
    if (!TakeEither(&c, 'P', 'p') || !DateParts(&c, f, &any))
    {
        return mloc_errorSyntax;
    }
    if (TakeEither(&c, 'T', 't'))
    {
        if (!TimeParts(&c, f))
        {
            return mloc_errorSyntax;
        }
        any = true;
    }
    if (!any || !AtEnd(&c))
    {
        return mloc_errorSyntax;
    }
    double v[10];
    for (int i = 0; i < 10; i += 1)
    {
        v[i] = mlocBinary64Of(negative, f[i], (Wide){0, 1});
    }
    mlocDurationValues values = {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]};
    mlocDateTimeDuration check;
    if (mlocMakeDuration(&values, &check) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    *valuesOut = values;
    return mloc_success;
}
