// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// test262's Temporal.PlainDateTime cases: every assertion its tests make
// about a date-time made from ISO fields, a date-time plus a duration, a
// rounded date-time, or the rounded difference of two, in
// test262_datetime.inc, checked against mlocMakeDateTime,
// mlocAddToDateTime, mlocRoundDateTime and mlocDateTimeUntil. A difference
// is given as a host gives it: its time part balanced by
// mlocBalanceTimeDuration to the largest unit (days for a date unit, added
// to the date part's), each field as the float64 Temporal keeps; since is
// until with the mode negated. The rows were recorded by running the tests
// on the library through a reader of their JavaScript, in
// tools/oracle/test262/.

#include "test_harness.h"

#include "maul-locale/zone.h"

#include <string.h>

static const char* const s_rows[] = {
#include "test262_datetime.inc"
};

enum
{
    maxTokens = 40,
    rowCount = sizeof s_rows / sizeof s_rows[0],
};

typedef struct Token
{
    const char* text;
    size_t length;
} Token;

typedef struct CalendarName
{
    const char* name;
    mlocCalendar calendar;
} CalendarName;

static const CalendarName s_calendars[] = {
    {"iso8601", mloc_calendarGregorian},
    {"gregory", mloc_calendarGregorian},
    {"buddhist", mloc_calendarBuddhist},
    {"roc", mloc_calendarRoc},
    {"japanese", mloc_calendarJapanese},
    {"coptic", mloc_calendarCoptic},
    {"ethiopic", mloc_calendarEthiopic},
    {"ethioaa", mloc_calendarEthiopicAmeteAlem},
    {"indian", mloc_calendarIndian},
    {"persian", mloc_calendarPersian},
    {"hebrew", mloc_calendarHebrew},
    {"islamic-civil", mloc_calendarIslamicCivil},
    {"islamic-tbla", mloc_calendarIslamicTbla},
    {"islamic-umalqura", mloc_calendarIslamicUmalqura},
    {"chinese", mloc_calendarChinese},
    {"dangi", mloc_calendarDangi},
};

static const char* const s_modes[] = {"halfExpand", "halfEven",  "halfTrunc",
                                      "halfCeil",   "halfFloor", "ceil",
                                      "floor",      "expand",    "trunc"};

static size_t Split(const char* row, Token* tokens)
{
    size_t count = 0;
    const char* p = row;
    while (*p != '\0' && count < maxTokens)
    {
        while (*p == ' ')
        {
            p += 1;
        }
        const char* start = p;
        while (*p != ' ' && *p != '\0')
        {
            p += 1;
        }
        if (p > start)
        {
            tokens[count] = (Token){start, (size_t)(p - start)};
            count += 1;
        }
    }
    return *p == '\0' ? count : 0;
}

static bool Is(Token token, const char* text)
{
    return token.length == strlen(text) && memcmp(token.text, text, token.length) == 0;
}

static bool Number(Token token, int64_t* valueOut)
{
    bool negative = token.length > 1 && token.text[0] == '-';
    size_t i = negative ? 1 : 0;
    if (i == token.length || token.length - i > 19)
    {
        return false;
    }
    uint64_t value = 0;
    for (; i < token.length; i += 1)
    {
        char c = token.text[i];
        if (c < '0' || c > '9')
        {
            return false;
        }
        value = value * 10 + (uint64_t)(c - '0');
    }
    if (value > (negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX))
    {
        return false;
    }
    *valueOut = negative ? (int64_t)(0 - value) : (int64_t)value;
    return true;
}

static bool Numbers(const Token* tokens, size_t count, int64_t* out)
{
    for (size_t i = 0; i < count; i += 1)
    {
        if (!Number(tokens[i], &out[i]))
        {
            return false;
        }
    }
    return true;
}

static bool CalendarOf(Token token, mlocCalendar* out)
{
    for (size_t i = 0; i < sizeof s_calendars / sizeof s_calendars[0]; i += 1)
    {
        if (Is(token, s_calendars[i].name))
        {
            *out = s_calendars[i].calendar;
            return true;
        }
    }
    return false;
}

static bool ModeOf(Token token, bool negate, mlocRoundingMode* out)
{
    for (int m = 0; m <= mloc_roundTrunc; m += 1)
    {
        if (Is(token, s_modes[m]))
        {
            mlocRoundingMode mode = (mlocRoundingMode)m;
            if (negate)
            {
                mode = mode == mloc_roundCeil        ? mloc_roundFloor
                       : mode == mloc_roundFloor     ? mloc_roundCeil
                       : mode == mloc_roundHalfCeil  ? mloc_roundHalfFloor
                       : mode == mloc_roundHalfFloor ? mloc_roundHalfCeil
                                                     : mode;
            }
            *out = mode;
            return true;
        }
    }
    return false;
}

// A date-time: mlocDate's day, hour, minute, second, nanoseconds past
// the second.
static bool DateTimeOf(mlocCalendar calendar, const Token* tokens, mlocDateTime* out)
{
    int64_t v[5];
    mlocTimeInput input = {0};
    mlocTime time = {0};
    mlocDate date = {0};
    if (!Numbers(tokens, 5, v) || v[4] < 0 || v[4] > 999999999)
    {
        return false;
    }
    input = (mlocTimeInput){v[1], v[2], v[3], v[4] / 1000000, v[4] / 1000 % 1000, v[4] % 1000};
    return mlocMakeDateFromDay(calendar, (int32_t)v[0], &date) == mloc_success &&
           mlocMakeTime(&input, mloc_overflowReject, &time) == mloc_success &&
           mlocMakeDateTime(&date, &time, out) == mloc_success;
}

typedef struct Result
{
    bool refused;
    bool isDuration;
    mlocDateTime dateTime;
    int64_t duration[10];
} Result;

static const char* const s_durationKeys[] = {"dy",  "dm", "dw",  "dd",  "dh",
                                             "dmi", "ds", "dms", "dus", "dns"};
static const char* const s_timeKeys[] = {"h", "mi", "s", "ms", "us", "ns"};

static bool SameText(Token value, const char* text, size_t length)
{
    return value.length == length && memcmp(value.text, text, length) == 0;
}

// A date's field: y, m, mc, d, era, ey.
static bool DateField(Token key, Token value, const mlocDateFields* f)
{
    bool none = Is(value, "-");
    int64_t n = 0;
    if (Is(key, "era"))
    {
        return none ? f->eraLength == 0 : SameText(value, f->era, f->eraLength);
    }
    if (Is(key, "mc"))
    {
        return SameText(value, f->monthCode, f->monthCodeLength);
    }
    if (Is(key, "ey") && none)
    {
        return f->eraLength == 0;
    }
    if (!Number(value, &n))
    {
        return false;
    }
    return Is(key, "y")    ? f->year == n
           : Is(key, "m")  ? f->month == n
           : Is(key, "d")  ? f->day == n
           : Is(key, "ey") ? f->eraLength > 0 && f->eraYear == n
                           : false;
}

static bool Expected(Token expectation, const Result* r)
{
    const char* eq = memchr(expectation.text, '=', expectation.length);
    if (eq == nullptr)
    {
        return false;
    }
    Token key = {expectation.text, (size_t)(eq - expectation.text)};
    Token value = {eq + 1, expectation.length - key.length - 1};
    int64_t want = 0;
    for (size_t k = 0; k < 10; k += 1)
    {
        if (Is(key, s_durationKeys[k]))
        {
            return r->isDuration && Number(value, &want) && r->duration[k] == want;
        }
    }
    if (r->isDuration)
    {
        return false;
    }
    const mlocTime* t = &r->dateTime.time;
    int64_t time[6] = {t->hour,
                       t->minute,
                       t->second,
                       t->nanosecond / 1000000,
                       t->nanosecond / 1000 % 1000,
                       t->nanosecond % 1000};
    for (size_t k = 0; k < 6; k += 1)
    {
        if (Is(key, s_timeKeys[k]))
        {
            return Number(value, &want) && time[k] == want;
        }
    }
    mlocDateFields f = {0};
    return mlocGetDateFields(&r->dateTime.date, &f) == mloc_success && DateField(key, value, &f);
}

static mlocResult Make(mlocCalendar calendar, const int64_t* v, mlocDateTime* out)
{
    mlocDateInput input = {.year = (int32_t)v[0], .month = (uint32_t)v[1], .day = (uint32_t)v[2]};
    mlocTimeInput t = {v[3], v[4], v[5], v[6], v[7], v[8]};
    mlocDate iso = {0};
    mlocDate date = {0};
    mlocTime time = {0};
    if (v[0] < INT32_MIN || v[0] > INT32_MAX || v[1] < 1 || v[1] > UINT32_MAX || v[2] < 1 ||
        v[2] > UINT32_MAX ||
        mlocMakeDate(mloc_calendarGregorian, &input, mloc_overflowReject, &iso) != mloc_success ||
        mlocMakeDateFromDay(calendar, iso.day, &date) != mloc_success ||
        mlocMakeTime(&t, mloc_overflowReject, &time) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    return mlocMakeDateTime(&date, &time, out);
}

// The difference as the host gives it: the time part balanced, days of a
// date unit joining the date part's, each field through a float64.
static mlocResult Until(mlocCalendar calendar, const Token* t, bool since, Result* r)
{
    mlocDateTime one = {0};
    mlocDateTime two = {0};
    int64_t v[3];
    mlocDateTimeRounding rounding = {0};
    if (!DateTimeOf(calendar, t, &one) || !DateTimeOf(calendar, t + 5, &two) ||
        !Numbers(t + 10, 3, v) || !ModeOf(t[13], since, &rounding.mode))
    {
        return (mlocResult)-100;
    }
    rounding.largestUnit = (mlocUnit)v[0];
    rounding.smallestUnit = (mlocUnit)v[1];
    rounding.increment = (uint32_t)v[2];
    mlocDateTimeDuration d = {0};
    mlocTimeDurationFields f = {0};
    mlocResult result = mlocDateTimeUntil(&one, &two, &rounding, &d);
    if (result != mloc_success)
    {
        return result;
    }
    mlocTimeUnit unit = (mlocTimeUnit)(v[0] < mloc_unitDay ? 0 : v[0] - mloc_unitDay);
    if (mlocBalanceTimeDuration(&d.time, unit, &f) != mloc_success)
    {
        return (mlocResult)-100;
    }
    int64_t values[10] = {d.date.years,   d.date.months, d.date.weeks, d.date.days + f.days,
                          f.hours,        f.minutes,     f.seconds,    f.milliseconds,
                          f.microseconds, f.nanoseconds};
    for (size_t i = 0; i < 10; i += 1)
    {
        r->duration[i] = (int64_t)(double)values[i];
    }
    r->isDuration = true;
    return mloc_success;
}

// Runs a row's operation, the tokens before '='; false for a bad row.
static bool Run(const Token* t, size_t count, Result* r)
{
    mlocCalendar calendar = mloc_calendarGregorian;
    int64_t v[12];
    mlocResult result = (mlocResult)-100;
    if (count < 2 || !CalendarOf(t[1], &calendar))
    {
        return false;
    }
    if (count == 11 && Is(t[0], "make") && Numbers(t + 2, 9, v))
    {
        result = Make(calendar, v, &r->dateTime);
    }
    else if (count == 14 && Is(t[0], "add") && Numbers(t + 7, 6, v) &&
             (Is(t[13], "c") || Is(t[13], "r")))
    {
        mlocDateTime start = {0};
        mlocDateTimeDuration d = {{v[0], v[1], v[2], v[3]}, {v[4], (int32_t)v[5]}};
        if (DateTimeOf(calendar, t + 2, &start))
        {
            result = mlocAddToDateTime(
                &start, &d, Is(t[13], "r") ? mloc_overflowReject : mloc_overflowConstrain,
                &r->dateTime);
        }
    }
    else if (count == 10 && Is(t[0], "round") && Numbers(t + 7, 2, v))
    {
        mlocDateTime start = {0};
        mlocRoundingMode mode = mloc_roundTrunc;
        if (DateTimeOf(calendar, t + 2, &start) && ModeOf(t[9], false, &mode))
        {
            result =
                mlocRoundDateTime(&start, (mlocTimeUnit)v[0], (uint32_t)v[1], mode, &r->dateTime);
        }
    }
    else if (count == 16 && (Is(t[0], "until") || Is(t[0], "since")))
    {
        result = Until(calendar, t + 2, Is(t[0], "since"), r);
    }
    if (result == (mlocResult)-100)
    {
        return false;
    }
    r->refused = result != mloc_success;
    return true;
}

static bool CheckRow(const char* row, size_t* checks)
{
    Token t[maxTokens];
    size_t count = Split(row, t);
    size_t eq = 0;
    while (eq < count && !Is(t[eq], "="))
    {
        eq += 1;
    }
    Result r = {0};
    if (eq == count || !Run(t, eq, &r))
    {
        printf("bad row: %s\n", row);
        return false;
    }
    if (count == eq + 2 && Is(t[eq + 1], "refused"))
    {
        *checks += 1;
        return r.refused;
    }
    if (r.refused)
    {
        return false;
    }
    for (size_t i = eq + 1; i < count; i += 1)
    {
        *checks += 1;
        if (!Expected(t[i], &r))
        {
            return false;
        }
    }
    return true;
}

int main(void)
{
    size_t checks = 0;
    for (size_t i = 0; i < rowCount; i += 1)
    {
        if (!CheckRow(s_rows[i], &checks))
        {
            printf("differs: %s\n", s_rows[i]);
            s_failures += 1;
        }
    }
    printf("%zu rows, %zu checks\n", (size_t)rowCount, checks);
    CHECK(rowCount > 0 && checks >= rowCount, "the table is read");
    return s_failures == 0 ? 0 : 1;
}
