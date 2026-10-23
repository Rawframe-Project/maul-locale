// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// test262's Temporal.ZonedDateTime time zone cases: every assertion its
// tests make about a zone's identifier, equality, offset or transitions,
// in test262_zone.inc, checked against mlocFindTimeZone,
// mlocFormatTimeZoneId, mlocTimeZonesEqual, mlocGetTimeZoneOffset and
// mlocGetTimeZoneTransition. The rows were recorded by running the tests
// on the library through a reader of their JavaScript, in
// tools/oracle/test262/. The tests that loop over
// Intl.supportedValuesOf("timeZone") run here over mlocGetTimeZone's list:
// each primary keeps its identifier (supported-values-of.js), no two are
// equal (canonical-not-equal.js), and none has a transition past
// Temporal's range (transition-at-instant-boundaries.js).

#include "test_harness.h"

#include "maul-locale/zone.h"

#include <string.h>

static const char* const s_rows[] = {
#include "test262_zone.inc"
};

enum
{
    maxTokens = 6,
    rowCount = sizeof s_rows / sizeof s_rows[0],
    nanosecondsPerSecond = 1000000000,
};

static const int64_t s_rangeSeconds = INT64_C(8640000000000);

static mlocData s_builtin;
static mlocDataSet s_data;

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

// A signed decimal of up to 18 digits.
static bool Digits(const char* text, size_t length, int64_t* valueOut)
{
    if (length == 0 || length > 18)
    {
        return false;
    }
    int64_t value = 0;
    for (size_t i = 0; i < length; i += 1)
    {
        if (text[i] < '0' || text[i] > '9')
        {
            return false;
        }
        value = value * 10 + (text[i] - '0');
    }
    *valueOut = value;
    return true;
}

static bool Number(Token token, int64_t* valueOut)
{
    bool negative = token.length > 1 && token.text[0] == '-';
    size_t sign = negative ? 1 : 0;
    int64_t value = 0;
    if (!Digits(token.text + sign, token.length - sign, &value))
    {
        return false;
    }
    *valueOut = negative ? -value : value;
    return true;
}

// Nanoseconds since 1970, which may pass int64_t's range, as an instant:
// the last nine digits are the nanoseconds, the rest the seconds.
static bool InstantOf(Token token, mlocInstant* out)
{
    bool negative = token.length > 1 && token.text[0] == '-';
    size_t sign = negative ? 1 : 0;
    size_t digits = token.length - sign;
    size_t split = digits > 9 ? digits - 9 : 0;
    int64_t seconds = 0;
    int64_t nanoseconds = 0;
    if ((split > 0 && !Digits(token.text + sign, split, &seconds)) ||
        !Digits(token.text + sign + split, digits - split, &nanoseconds))
    {
        return false;
    }
    if (negative && nanoseconds > 0)
    {
        seconds += 1;
        nanoseconds = nanosecondsPerSecond - nanoseconds;
    }
    *out = (mlocInstant){negative ? -seconds : seconds, (int32_t)nanoseconds};
    return true;
}

static bool Find(Token id, mlocTimeZone* zoneOut)
{
    return mlocFindTimeZone(&s_data, id.text, id.length, zoneOut) == mloc_success;
}

static bool IdIs(const mlocTimeZone* zone, const char* want, size_t length)
{
    char buffer[64];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    return mlocFormatTimeZoneId(zone, &output) == mloc_success && output.length == length &&
           memcmp(buffer, want, length) == 0;
}

static bool EqualRow(const Token* t)
{
    mlocTimeZone one = {0};
    mlocTimeZone two = {0};
    bool equal = false;
    return Find(t[1], &one) && Find(t[2], &two) &&
           mlocTimeZonesEqual(&one, &two, &equal) == mloc_success && equal == Is(t[4], "1");
}

static bool OffsetRow(const Token* t)
{
    mlocTimeZone zone = {0};
    mlocInstant instant = {0};
    int64_t want = 0;
    int32_t offset = 0;
    return Find(t[1], &zone) && InstantOf(t[2], &instant) && Number(t[4], &want) &&
           mlocGetTimeZoneOffset(&zone, &instant, &offset) == mloc_success &&
           (int64_t)offset * nanosecondsPerSecond == want;
}

static bool TransitionRow(const Token* t, mlocTransitionDirection direction)
{
    mlocTimeZone zone = {0};
    mlocInstant instant = {0};
    mlocInstant want = {0};
    mlocInstant found = {0};
    if (!Find(t[1], &zone) || !InstantOf(t[2], &instant))
    {
        return false;
    }
    mlocResult r = mlocGetTimeZoneTransition(&zone, &instant, direction, &found);
    if (Is(t[4], "none"))
    {
        return r == mloc_errorNotFound;
    }
    return InstantOf(t[4], &want) && r == mloc_success && found.seconds == want.seconds &&
           found.nanoseconds == want.nanoseconds;
}

static bool CheckRow(const char* row)
{
    Token t[maxTokens];
    size_t count = Split(row, t);
    mlocTimeZone zone = {0};
    if (count == 2 && Is(t[0], "refused"))
    {
        return !Find(t[1], &zone);
    }
    if (count == 4 && Is(t[0], "id") && Is(t[2], "="))
    {
        return Find(t[1], &zone) && IdIs(&zone, t[3].text, t[3].length);
    }
    if (count != 5 || !Is(t[3], "="))
    {
        return false;
    }
    if (Is(t[0], "equal"))
    {
        return EqualRow(t);
    }
    if (Is(t[0], "offset"))
    {
        return OffsetRow(t);
    }
    if (Is(t[0], "next") || Is(t[0], "previous"))
    {
        return TransitionRow(t, Is(t[0], "next") ? mloc_transitionNext : mloc_transitionPrevious);
    }
    return false;
}

// supported-values-of.js: each primary found by its identifier keeps it.
static void TestListIdentifiers(size_t count)
{
    for (size_t i = 0; i < count; i += 1)
    {
        mlocTimeZone zone = {0};
        mlocTimeZone found = {0};
        char buffer[64];
        mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
        bool ok = mlocGetTimeZone(&s_data, i, &zone) == mloc_success &&
                  mlocFormatTimeZoneId(&zone, &output) == mloc_success &&
                  Find((Token){buffer, output.length}, &found) &&
                  IdIs(&found, buffer, output.length);
        CHECK(ok, "a primary keeps its identifier");
    }
}

// canonical-not-equal.js: no two primaries are equal.
static void TestListUnequal(size_t count)
{
    size_t equalPairs = 0;
    for (size_t i = 0; i < count; i += 1)
    {
        mlocTimeZone one = {0};
        CHECK(mlocGetTimeZone(&s_data, i, &one) == mloc_success, "a primary");
        for (size_t j = i + 1; j < count; j += 1)
        {
            mlocTimeZone two = {0};
            bool equal = true;
            if (mlocGetTimeZone(&s_data, j, &two) != mloc_success ||
                mlocTimeZonesEqual(&one, &two, &equal) != mloc_success || equal)
            {
                equalPairs += 1;
            }
        }
    }
    CHECK(equalPairs == 0, "no two primaries are equal");
}

// transition-at-instant-boundaries.js: a transition from the range's
// first instant is after it, one from its last before it, and none lies
// beyond either.
static void TestListBoundaries(size_t count)
{
    mlocInstant first = {-s_rangeSeconds, 0};
    mlocInstant last = {s_rangeSeconds, 0};
    for (size_t i = 0; i < count; i += 1)
    {
        mlocTimeZone zone = {0};
        mlocInstant found = {0};
        CHECK(mlocGetTimeZone(&s_data, i, &zone) == mloc_success, "a primary");
        mlocResult r = mlocGetTimeZoneTransition(&zone, &first, mloc_transitionNext, &found);
        CHECK(r == mloc_errorNotFound || (r == mloc_success && found.seconds > first.seconds),
              "the next transition from the first instant is after it");
        r = mlocGetTimeZoneTransition(&zone, &last, mloc_transitionPrevious, &found);
        CHECK(r == mloc_errorNotFound || (r == mloc_success && found.seconds < last.seconds),
              "the previous transition from the last instant is before it");
        CHECK(mlocGetTimeZoneTransition(&zone, &last, mloc_transitionNext, &found) ==
                      mloc_errorNotFound &&
                  mlocGetTimeZoneTransition(&zone, &first, mloc_transitionPrevious, &found) ==
                      mloc_errorNotFound,
              "no transition beyond the range");
    }
}

int main(void)
{
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    for (size_t i = 0; i < rowCount; i += 1)
    {
        if (!CheckRow(s_rows[i]))
        {
            printf("differs: %s\n", s_rows[i]);
            s_failures += 1;
        }
    }
    size_t count = mlocGetTimeZoneCount(&s_data);
    TestListIdentifiers(count);
    TestListUnequal(count);
    TestListBoundaries(count);
    printf("%zu rows, %zu primaries\n", (size_t)rowCount, count);
    CHECK(rowCount > 0 && count > 0, "the table and the list are read");
    return s_failures == 0 ? 0 : 1;
}
