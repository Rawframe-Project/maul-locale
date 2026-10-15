// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// test262's Temporal.PlainTime cases (record mloc-0041): every assertion
// its tests make about a time of day made from fields, a time plus a
// duration, a rounded time, or the rounded difference of two times, in
// test262_time.inc, checked against mlocMakeTime, mlocAddToTime,
// mlocRoundTime and mlocTimeUntil. since is until with the mode negated,
// as zone.h says; its rows hold the fields before negation. The rows
// were recorded by running the tests on the library through a reader of
// their JavaScript, kept in the docs with the oracle programs.

#include "test_harness.h"

#include "maul-locale/zone.h"

#include <string.h>

static const char* const s_rows[] = {
#include "test262_time.inc"
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

// A signed decimal of up to 19 digits within int64_t.
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

static const char* const s_modes[] = {"halfExpand", "halfEven",  "halfTrunc",
                                      "halfCeil",   "halfFloor", "ceil",
                                      "floor",      "expand",    "trunc"};

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

// A time: hour, minute, second, nanoseconds past the second.
static bool TimeOf(const Token* tokens, mlocTime* out)
{
    int64_t v[4];
    if (!Numbers(tokens, 4, v) || v[0] < 0 || v[0] > 23 || v[1] < 0 || v[1] > 59 || v[2] < 0 ||
        v[2] > 59 || v[3] < 0 || v[3] > 999999999)
    {
        return false;
    }
    *out = (mlocTime){(uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2], 0, (uint32_t)v[3]};
    return true;
}

typedef struct Result
{
    bool refused;
    mlocTime time;
    mlocTimeDurationFields fields;
} Result;

static const char* const s_keys[] = {"h",  "mi",  "s",  "ms",  "us",  "ns",
                                     "dh", "dmi", "ds", "dms", "dus", "dns"};

static int64_t ValueOf(const Result* r, size_t key)
{
    const mlocTime* t = &r->time;
    const mlocTimeDurationFields* f = &r->fields;
    int64_t values[12] = {t->hour,
                          t->minute,
                          t->second,
                          t->nanosecond / 1000000,
                          t->nanosecond / 1000 % 1000,
                          t->nanosecond % 1000,
                          f->hours,
                          f->minutes,
                          f->seconds,
                          f->milliseconds,
                          f->microseconds,
                          f->nanoseconds};
    return values[key];
}

// Checks one "key=value" against the result.
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
    for (size_t k = 0; k < sizeof s_keys / sizeof s_keys[0]; k += 1)
    {
        if (Is(key, s_keys[k]))
        {
            return Number(value, &want) && ValueOf(r, k) == want;
        }
    }
    return false;
}

// Runs a row's operation, the tokens before '='; false for a bad row.
static bool Run(const Token* t, size_t count, Result* r)
{
    int64_t v[12];
    mlocResult result = mloc_errorInvalid;
    if (count == 8 && Is(t[0], "make") && (Is(t[1], "r") || Is(t[1], "c")) && Numbers(t + 2, 6, v))
    {
        mlocTimeInput input = {v[0], v[1], v[2], v[3], v[4], v[5]};
        result = mlocMakeTime(&input, Is(t[1], "r") ? mloc_overflowReject : mloc_overflowConstrain,
                              &r->time);
    }
    else if (count == 7 && Is(t[0], "add") && Numbers(t + 1, 6, v))
    {
        mlocTime time = {0};
        mlocTimeDuration d = {v[4], (int32_t)v[5]};
        if (!TimeOf(t + 1, &time))
        {
            return false;
        }
        result = mlocAddToTime(&time, &d, &r->time, nullptr);
    }
    else if (count == 8 && Is(t[0], "round") && Numbers(t + 5, 2, v))
    {
        mlocTime time = {0};
        mlocRoundingMode mode = mloc_roundTrunc;
        if (!TimeOf(t + 1, &time) || !ModeOf(t[7], false, &mode))
        {
            return false;
        }
        result = mlocRoundTime(&time, (mlocTimeUnit)v[0], (uint32_t)v[1], mode, &r->time, nullptr);
    }
    else if (count == 13 && (Is(t[0], "until") || Is(t[0], "since")) && Numbers(t + 9, 3, v))
    {
        mlocTime one = {0};
        mlocTime two = {0};
        mlocTimeRounding rounding = {(mlocTimeUnit)v[0], (mlocTimeUnit)v[1], (uint32_t)v[2],
                                     mloc_roundTrunc};
        if (!TimeOf(t + 1, &one) || !TimeOf(t + 5, &two) ||
            !ModeOf(t[12], Is(t[0], "since"), &rounding.mode))
        {
            return false;
        }
        result = mlocTimeUntil(&one, &two, &rounding, &r->fields);
    }
    else
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
