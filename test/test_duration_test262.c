// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// test262's Temporal.Duration arithmetic: every assertion its tests make
// about rounding a duration, its total, the sum of two, or which of two is
// longer, in test262_duration.inc, checked by doing what a host does: from
// no start, mlocRoundTimeDuration, mlocTotalTimeDuration and
// mlocAddTimeDurations; from a plain date's midnight, mlocAddToDateTime
// and then mlocDateTimeUntil or mlocDateTimeTotal; from a zoned date-time,
// mlocAddToZonedDateTime and then mlocZonedDateTimeUntil or
// mlocZonedDateTimeTotal; comparisons through mlocAddToDate,
// mlocAddTimeDurations and the zoned sums. Each result's fields come from
// mlocGetDurationValues. The rows were recorded by running the tests on
// the library through a reader of their JavaScript, in
// tools/oracle/test262/.

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/zone.h"

#include <stdlib.h>
#include <string.h>

static const char* const s_rows[] = {
#include "test262_duration.inc"
};

enum
{
    maxTokens = 32,
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
static const char* const s_keys[] = {"dy",  "dm", "dw",  "dd",  "dh",
                                     "dmi", "ds", "dms", "dus", "dns"};

static mlocData s_builtin;
static mlocDataSet s_data;

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

// Where a duration starts.
typedef enum StartKind
{
    startNone,
    startPlain,
    startZoned,
} StartKind;

typedef struct Start
{
    StartKind kind;
    mlocCalendar calendar;
    mlocDateTime midnight;
    mlocTimeZone zone;
    mlocInstant instant;
} Start;

// Reads a start, returning the tokens it takes, or 0.
static size_t StartOf(const Token* t, size_t count, Start* out)
{
    int64_t v[2];
    *out = (Start){0};
    if (count >= 1 && Is(t[0], "none"))
    {
        out->kind = startNone;
        return 1;
    }
    mlocDate date;
    if (count >= 3 && Is(t[0], "plain") && CalendarOf(t[1], &out->calendar) &&
        Number(t[2], &v[0]) && v[0] >= INT32_MIN && v[0] <= INT32_MAX &&
        mlocMakeDateFromDay(out->calendar, (int32_t)v[0], &date) == mloc_success &&
        mlocMakeDateTime(&date, &(mlocTime){0}, &out->midnight) == mloc_success)
    {
        out->kind = startPlain;
        return 3;
    }
    if (count >= 5 && Is(t[0], "zoned") &&
        mlocFindTimeZone(&s_data, t[1].text, t[1].length, &out->zone) == mloc_success &&
        CalendarOf(t[2], &out->calendar) && Numbers(t + 3, 2, v) && v[1] >= 0 && v[1] < 1000000000)
    {
        out->kind = startZoned;
        out->instant = (mlocInstant){v[0], (int32_t)v[1]};
        return 5;
    }
    return 0;
}

static bool DurationOf(const Token* t, mlocDateTimeDuration* out)
{
    int64_t v[6];
    if (!Numbers(t, 6, v) || v[5] < INT32_MIN || v[5] > INT32_MAX)
    {
        return false;
    }
    *out = (mlocDateTimeDuration){{v[0], v[1], v[2], v[3]}, {v[4], (int32_t)v[5]}};
    return true;
}

typedef struct Result
{
    bool refused;
    int kind; // 0 fields, 1 total, 2 comparison
    double fields[10];
    double total;
    int comparison;
} Result;

static mlocResult Values(const mlocDateTimeDuration* d, mlocUnit largest, Result* r)
{
    mlocDurationValues v;
    mlocResult result = mlocGetDurationValues(d, largest, &v);
    if (result == mloc_success)
    {
        double f[10] = {v.years,   v.months,  v.weeks,        v.days,         v.hours,
                        v.minutes, v.seconds, v.milliseconds, v.microseconds, v.nanoseconds};
        memcpy(r->fields, f, sizeof f);
    }
    return result;
}

// The plain date-time or instant the duration reaches from its start.
static mlocResult Target(const Start* s, const mlocDateTimeDuration* d, mlocDateTime* plainOut,
                         mlocInstant* instantOut)
{
    if (s->kind == startPlain)
    {
        return mlocAddToDateTime(&s->midnight, d, mloc_overflowConstrain, plainOut);
    }
    return mlocAddToZonedDateTime(&s->zone, s->calendar, &s->instant, d, mloc_overflowConstrain,
                                  instantOut);
}

static mlocResult Round(const Start* s, const mlocDateTimeDuration* d, const Token* t, Result* r)
{
    int64_t v[3];
    mlocDateTimeRounding rounding = {0};
    if (!Numbers(t, 3, v) || !ModeOf(t[3], false, &rounding.mode) || v[0] < 0 || v[0] > 9 ||
        v[1] < 0 || v[1] > 9)
    {
        return (mlocResult)-100;
    }
    rounding.largestUnit = (mlocUnit)v[0];
    rounding.smallestUnit = (mlocUnit)v[1];
    rounding.increment = (uint32_t)v[2];
    mlocDateTimeDuration out = {0};
    mlocResult result = mloc_success;
    if (s->kind == startNone)
    {
        result = mlocRoundTimeDuration(&d->time, (mlocTimeUnit)(rounding.smallestUnit - 3),
                                       rounding.increment, rounding.mode, &out.time);
    }
    else
    {
        mlocDateTime plain;
        mlocInstant instant;
        result = Target(s, d, &plain, &instant);
        if (result == mloc_success)
        {
            result = s->kind == startPlain
                         ? mlocDateTimeUntil(&s->midnight, &plain, &rounding, &out)
                         : mlocZonedDateTimeUntil(&s->zone, s->calendar, &s->instant, &instant,
                                                  &rounding, &out);
        }
    }
    // From a zoned start, a date unit balances the time to hours.
    mlocUnit largest = s->kind == startZoned && rounding.largestUnit < mloc_unitHour
                           ? mloc_unitHour
                           : rounding.largestUnit;
    return result == mloc_success ? Values(&out, largest, r) : result;
}

static mlocResult Total(const Start* s, const mlocDateTimeDuration* d, Token unitToken, Result* r)
{
    int64_t unit = 0;
    if (!Number(unitToken, &unit) || unit < 0 || unit > 9)
    {
        return (mlocResult)-100;
    }
    r->kind = 1;
    if (s->kind == startNone)
    {
        return mlocTotalTimeDuration(&d->time, (mlocTimeUnit)(unit - 3), &r->total);
    }
    mlocDateTime plain;
    mlocInstant instant;
    mlocResult result = Target(s, d, &plain, &instant);
    if (result != mloc_success)
    {
        return result;
    }
    return s->kind == startPlain
               ? mlocDateTimeTotal(&s->midnight, &plain, (mlocUnit)unit, &r->total)
               : mlocZonedDateTimeTotal(&s->zone, s->calendar, &s->instant, &instant,
                                        (mlocUnit)unit, &r->total);
}

// A duration from a plain start as a time duration: the years, months
// and weeks as the days they reach, every day as 24 hours.
static mlocResult PlainLength(const Start* s, const mlocDateTimeDuration* d, mlocTimeDuration* out)
{
    int64_t days = d->date.days;
    if (d->date.years != 0 || d->date.months != 0 || d->date.weeks != 0)
    {
        mlocDateDuration ymw = {d->date.years, d->date.months, d->date.weeks, 0};
        mlocDate later;
        if (mlocAddToDate(&s->midnight.date, &ymw, mloc_overflowConstrain, &later) != mloc_success)
        {
            return mloc_errorInvalid;
        }
        days += (int64_t)later.day - s->midnight.date.day;
    }
    if (days > INT64_C(106751991167) || days < -INT64_C(106751991167))
    {
        return mloc_errorInvalid;
    }
    mlocTimeDuration dayTime = {days * 86400, 0};
    return mlocAddTimeDurations(&d->time, &dayTime, out);
}

static int CompareInstants(const mlocInstant* a, const mlocInstant* b)
{
    return a->seconds != b->seconds
               ? (a->seconds > b->seconds) - (a->seconds < b->seconds)
               : (a->nanoseconds > b->nanoseconds) - (a->nanoseconds < b->nanoseconds);
}

static mlocResult Compare(const Start* s, const mlocDateTimeDuration* one,
                          const mlocDateTimeDuration* two, Result* r)
{
    r->kind = 2;
    if (s->kind == startZoned)
    {
        mlocInstant a;
        mlocInstant b;
        mlocResult result = Target(s, one, nullptr, &a);
        if (result == mloc_success)
        {
            result = Target(s, two, nullptr, &b);
        }
        r->comparison = result == mloc_success ? CompareInstants(&a, &b) : 0;
        return result;
    }
    if (s->kind != startPlain)
    {
        return (mlocResult)-100;
    }
    mlocTimeDuration a;
    mlocTimeDuration b;
    mlocResult result = PlainLength(s, one, &a);
    if (result == mloc_success)
    {
        result = PlainLength(s, two, &b);
    }
    if (result == mloc_success)
    {
        mlocInstant x = {a.seconds, a.nanoseconds};
        mlocInstant y = {b.seconds, b.nanoseconds};
        r->comparison = CompareInstants(&x, &y);
    }
    return result;
}

static bool Run(const Token* t, size_t count, Result* r)
{
    mlocResult result = (mlocResult)-100;
    Start s;
    mlocDateTimeDuration one;
    mlocDateTimeDuration two;
    if (count == 6 && Is(t[0], "add"))
    {
        int64_t v[5];
        if (Numbers(t + 1, 5, v) && v[1] >= INT32_MIN && v[1] <= INT32_MAX && v[3] >= INT32_MIN &&
            v[3] <= INT32_MAX && v[4] >= 0 && v[4] <= 9)
        {
            mlocTimeDuration a = {v[0], (int32_t)v[1]};
            mlocTimeDuration b = {v[2], (int32_t)v[3]};
            mlocDateTimeDuration sum = {0};
            result = mlocAddTimeDurations(&a, &b, &sum.time);
            if (result == mloc_success)
            {
                result = Values(&sum, (mlocUnit)v[4], r);
            }
        }
    }
    else if (count > 1)
    {
        size_t used = StartOf(t + 1, count - 1, &s);
        size_t rest = count - 1 - used;
        const Token* u = t + 1 + used;
        if (used == 0)
        {
            result = (mlocResult)-100;
        }
        else if (Is(t[0], "round") && rest == 10 && DurationOf(u, &one))
        {
            result = Round(&s, &one, u + 6, r);
        }
        else if (Is(t[0], "total") && rest == 7 && DurationOf(u, &one))
        {
            result = Total(&s, &one, u[6], r);
        }
        else if (Is(t[0], "compare") && rest == 12 && DurationOf(u, &one) &&
                 DurationOf(u + 6, &two))
        {
            result = Compare(&s, &one, &two, r);
        }
    }
    if (result == (mlocResult)-100)
    {
        return false;
    }
    r->refused = result != mloc_success;
    return true;
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
    char text[64];
    if (value.length >= sizeof text)
    {
        return false;
    }
    memcpy(text, value.text, value.length);
    text[value.length] = '\0';
    if (Is(key, "total"))
    {
        return r->kind == 1 && r->total == strtod(text, nullptr);
    }
    if (Is(key, "c"))
    {
        return r->kind == 2 && r->comparison == atoi(text);
    }
    for (size_t k = 0; k < 10; k += 1)
    {
        if (Is(key, s_keys[k]))
        {
            return r->kind == 0 && r->fields[k] == strtod(text, nullptr);
        }
    }
    return false;
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
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
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
