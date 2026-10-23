// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// test262's Temporal.Instant cases: every assertion its tests make about
// an instant plus a duration, a rounded instant, or the rounded difference
// of two, in test262_instant.inc, checked against mlocAddToInstant,
// mlocRoundInstant and mlocInstantUntil. A difference is given as a host
// gives it: balanced by mlocBalanceTimeDuration to the largest unit, or
// where a field passes int64_t, that field formed from the duration's
// exact decimal; each field as the float64 Temporal keeps. since is until
// with the mode negated. The rows were recorded by running the tests on
// the library through a reader of their JavaScript, kept in the docs with
// the oracle programs.

#include "test_harness.h"

#include "maul-locale/zone.h"

#include <stdlib.h>
#include <string.h>

static const char* const s_rows[] = {
#include "test262_instant.inc"
};

enum
{
    maxTokens = 24,
    rowCount = sizeof s_rows / sizeof s_rows[0],
};

typedef struct Token
{
    const char* text;
    size_t length;
} Token;

static const char* const s_modes[] = {"halfExpand", "halfEven",  "halfTrunc",
                                      "halfCeil",   "halfFloor", "ceil",
                                      "floor",      "expand",    "trunc"};
static const char* const s_keys[] = {"dh", "dmi", "ds", "dms", "dus", "dns"};

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

// Seconds and nanoseconds of one sign, or an instant's floored ones, as
// the exact decimal of their nanoseconds.
static void Decimal(int64_t seconds, int64_t nanoseconds, char* out, size_t size)
{
    bool negative = seconds < 0 || (seconds == 0 && nanoseconds < 0);
    if (negative && nanoseconds > 0)
    {
        seconds += 1;
        nanoseconds -= 1000000000;
    }
    uint64_t s = negative ? (uint64_t)0 - (uint64_t)seconds : (uint64_t)seconds;
    int64_t n = nanoseconds < 0 ? -nanoseconds : nanoseconds;
    if (s == 0)
    {
        snprintf(out, size, "%s%lld", negative && n > 0 ? "-" : "", (long long)n);
    }
    else
    {
        snprintf(out, size, "%s%llu%09lld", negative ? "-" : "", (unsigned long long)s,
                 (long long)n);
    }
}

typedef struct Result
{
    bool refused;
    bool isDuration;
    mlocInstant instant;
    double fields[6];
} Result;

static bool Instant(const Token* t, mlocInstant* out)
{
    int64_t v[2];
    if (!Numbers(t, 2, v) || v[1] < 0 || v[1] > 999999999)
    {
        return false;
    }
    *out = (mlocInstant){v[0], (int32_t)v[1]};
    return true;
}

// The host's fields of a duration: balanced, or past int64_t formed from
// its decimal, each a float64.
static bool Fields(const mlocTimeDuration* d, mlocTimeUnit largest, double* out)
{
    mlocTimeDurationFields f = {0};
    if (mlocBalanceTimeDuration(d, largest, &f) == mloc_success)
    {
        int64_t v[6] = {f.hours,        f.minutes,      f.seconds,
                        f.milliseconds, f.microseconds, f.nanoseconds};
        for (size_t i = 0; i < 6; i += 1)
        {
            out[i] = (double)v[i];
        }
        return true;
    }
    char text[48];
    Decimal(d->seconds, d->nanoseconds, text, sizeof text);
    memset(out, 0, 6 * sizeof(double));
    if (largest == mloc_timeUnitNanosecond)
    {
        out[5] = strtod(text, nullptr);
        return true;
    }
    if (largest != mloc_timeUnitMicrosecond)
    {
        return false;
    }
    // Microseconds: the decimal without its last three digits.
    size_t length = strlen(text);
    int64_t below = (d->nanoseconds < 0 ? -d->nanoseconds : d->nanoseconds) % 1000;
    text[length - 3] = '\0';
    out[4] = strtod(text, nullptr);
    out[5] = (double)(d->seconds < 0 || d->nanoseconds < 0 ? -below : below);
    return true;
}

static mlocResult Until(const Token* t, bool since, Result* r)
{
    mlocInstant one = {0};
    mlocInstant two = {0};
    int64_t v[3];
    mlocTimeRounding rounding = {0};
    if (!Instant(t, &one) || !Instant(t + 2, &two) || !Numbers(t + 4, 3, v) ||
        !ModeOf(t[7], since, &rounding.mode))
    {
        return (mlocResult)-100;
    }
    rounding.largestUnit = (mlocTimeUnit)v[0];
    rounding.smallestUnit = (mlocTimeUnit)v[1];
    rounding.increment = (uint32_t)v[2];
    mlocTimeDuration d = {0};
    mlocResult result = mlocInstantUntil(&one, &two, &rounding, &d);
    if (result == mloc_success && !Fields(&d, rounding.largestUnit, r->fields))
    {
        return (mlocResult)-100;
    }
    r->isDuration = true;
    return result;
}

static bool Run(const Token* t, size_t count, Result* r)
{
    int64_t v[4];
    mlocResult result = (mlocResult)-100;
    mlocInstant start = {0};
    if (count == 5 && Is(t[0], "add") && Instant(t + 1, &start) && Numbers(t + 3, 2, v))
    {
        mlocTimeDuration d = {v[0], (int32_t)v[1]};
        result = mlocAddToInstant(&start, &d, &r->instant);
    }
    else if (count == 6 && Is(t[0], "round") && Instant(t + 1, &start) && Numbers(t + 3, 2, v))
    {
        mlocRoundingMode mode = mloc_roundTrunc;
        if (ModeOf(t[5], false, &mode))
        {
            result =
                mlocRoundInstant(&start, (mlocTimeUnit)v[0], (uint32_t)v[1], mode, &r->instant);
        }
    }
    else if (count == 9 && (Is(t[0], "until") || Is(t[0], "since")))
    {
        result = Until(t + 1, Is(t[0], "since"), r);
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
    char text[48];
    if (Is(key, "ns"))
    {
        Decimal(r->instant.seconds, r->instant.nanoseconds, text, sizeof text);
        return !r->isDuration && Is(value, text);
    }
    if (value.length >= sizeof text)
    {
        return false;
    }
    memcpy(text, value.text, value.length);
    text[value.length] = '\0';
    for (size_t k = 0; k < 6; k += 1)
    {
        if (Is(key, s_keys[k]))
        {
            return r->isDuration && r->fields[k] == strtod(text, nullptr);
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
