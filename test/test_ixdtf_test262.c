// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// test262's Temporal strings, in test262_ixdtf.inc: every string its tests
// expect a parser to refuse, checked against the parser the reader called
// (mlocParseTemporalString with the goals given, mlocParseUTCOffset,
// mlocParseTimeZoneString, mlocParseCalendarString or mlocParseDuration);
// and every string they expect a toString to write, checked against the
// writer with the value and options the reader gave it. The strings
// test262 reads successfully are checked by the rows of the values made
// from them, and every string is compared with a second reading of the
// grammar by tools/check_ixdtf.py. The rows were recorded by running the
// tests on the library through a reader of their JavaScript, in
// tools/oracle/test262/.

#include "test_harness.h"

#include "maul-locale/ixdtf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* const s_rows[] = {
#include "test262_ixdtf.inc"
};

enum
{
    maxText = 512,
    maxTokens = 24,
    rowCount = sizeof s_rows / sizeof s_rows[0],
};

typedef struct Token
{
    const char* text;
    size_t length;
} Token;

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
    if (i == token.length || token.length - i > 18)
    {
        return false;
    }
    int64_t value = 0;
    for (; i < token.length; i += 1)
    {
        if (token.text[i] < '0' || token.text[i] > '9')
        {
            return false;
        }
        value = value * 10 + (token.text[i] - '0');
    }
    *valueOut = negative ? -value : value;
    return true;
}

static bool Numbers(const Token* tokens, size_t count, int64_t* values)
{
    for (size_t i = 0; i < count; i += 1)
    {
        if (!Number(tokens[i], &values[i]))
        {
            return false;
        }
    }
    return true;
}

static int HexDigit(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    return ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1;
}

// x and bytes in hexadecimal; SIZE_MAX for a malformed token.
static size_t Unhex(Token token, char* out)
{
    if (token.length == 0 || token.text[0] != 'x' || token.length % 2 == 0 ||
        token.length / 2 > maxText)
    {
        return SIZE_MAX;
    }
    size_t n = 0;
    for (size_t i = 1; i < token.length; i += 2)
    {
        int high = HexDigit(token.text[i]);
        int low = HexDigit(token.text[i + 1]);
        if (high < 0 || low < 0)
        {
            return SIZE_MAX;
        }
        out[n] = (char)(high * 16 + low);
        n += 1;
    }
    return n;
}

// Whether a refusal row's parser refuses its string.
static bool Refused(const Token* t, size_t count, bool* badOut)
{
    char text[maxText];
    bool dt = Is(t[0], "dt");
    int64_t formats = 0;
    size_t length = count == (dt ? 5u : 4u) ? Unhex(t[dt ? 2 : 1], text) : SIZE_MAX;
    *badOut = length == SIZE_MAX || !Is(t[count - 2], "=") || !Is(t[count - 1], "refused") ||
              (dt && (!Number(t[1], &formats) || formats <= 0 || formats > 63));
    if (*badOut)
    {
        return false;
    }
    mlocResult r = mloc_errorInvalid;
    if (dt)
    {
        mlocTemporalParse parse;
        r = mlocParseTemporalString(text, length, (uint32_t)formats, &parse);
    }
    else if (Is(t[0], "offset"))
    {
        int64_t ns = 0;
        r = mlocParseUTCOffset(text, length, &ns, nullptr);
    }
    else if (Is(t[0], "zone"))
    {
        mlocTimeZoneParse zone;
        r = mlocParseTimeZoneString(text, length, &zone);
    }
    else if (Is(t[0], "calendar"))
    {
        mlocTextSpan span;
        r = mlocParseCalendarString(text, length, &span);
    }
    else if (Is(t[0], "duration"))
    {
        // A duration past the limits is mloc_errorInvalid; test262 sees
        // it as a RangeError like a malformed one.
        mlocDurationValues values;
        r = mlocParseDuration(text, length, &values);
        return r == mloc_errorSyntax || r == mloc_errorInvalid;
    }
    else
    {
        *badOut = true;
    }
    return r == mloc_errorSyntax;
}

// The five options after a writer's value: the calendar, the precision
// (0 to 9 digits, -1 for auto, -2 for minutes), and what is shown.
static bool Options(const Token* t, mlocTemporalStringOptions* out)
{
    int64_t v[4];
    if (!Numbers(t + 1, 4, v) || v[0] < -2 || v[0] > 9)
    {
        return false;
    }
    *out = (mlocTemporalStringOptions){
        .calendar = t[0].text,
        .calendarLength = t[0].length,
        .secondsPrecision = v[0] == -1   ? mloc_secondsAuto
                            : v[0] == -2 ? mloc_secondsMinute
                                         : mloc_secondsDigits,
        .fractionDigits = v[0] >= 0 ? (uint8_t)v[0] : 0,
        .showCalendar = (uint8_t)v[1],
        .showTimeZone = (uint8_t)v[2],
        .hideOffset = v[3] != 0,
    };
    return true;
}

static bool Day(Token token, mlocDate* out)
{
    int64_t day = 0;
    return Number(token, &day) && day >= INT32_MIN && day <= INT32_MAX &&
           mlocMakeDateFromDay(mloc_calendarGregorian, (int32_t)day, out) == mloc_success;
}

static bool TimeOf(const Token* t, mlocTime* out)
{
    int64_t v[4];
    if (!Numbers(t, 4, v) || v[0] < 0 || v[0] > 23 || v[1] < 0 || v[1] > 59 || v[2] < 0 ||
        v[2] > 59 || v[3] < 0 || v[3] > 999999999)
    {
        return false;
    }
    *out = (mlocTime){(uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2], (uint32_t)v[3]};
    return true;
}

static bool InstantOf(const Token* t, mlocInstant* out)
{
    int64_t v[2];
    if (!Numbers(t, 2, v) || v[1] < 0 || v[1] > 999999999)
    {
        return false;
    }
    *out = (mlocInstant){v[0], (int32_t)v[1]};
    return true;
}

static bool ZoneOf(Token token, mlocTimeZone* out)
{
    return mlocFindTimeZone(&s_data, token.text, token.length, out) == mloc_success;
}

// Writes a writer row's string; false for a row this test cannot read.
static bool Write(const Token* t, size_t count, mlocOutput* out, mlocResult* resultOut)
{
    mlocTemporalStringOptions o;
    mlocDate date;
    mlocDateTime dt;
    mlocInstant instant;
    mlocTimeZone zone;
    // The value's tokens, the options' five, '=' and the string.
    static const struct
    {
        const char* name;
        size_t values;
    } s_writers[] = {{"wdate", 1}, {"wym", 1},  {"wmd", 1},   {"wtime", 4},  {"wdt", 5},
                     {"winst", 3}, {"wzdt", 3}, {"wdur", 10}, {"woffset", 1}};
    size_t w = 0;
    while (w < sizeof s_writers / sizeof s_writers[0] && !Is(t[0], s_writers[w].name))
    {
        w += 1;
    }
    bool offset = w == 8;
    if (w == sizeof s_writers / sizeof s_writers[0] ||
        count != 1 + s_writers[w].values + (offset ? 0 : 5) + 2 ||
        (!offset && !Options(t + 1 + s_writers[w].values, &o)))
    {
        return false;
    }
    const Token* v = t + 1;
    switch (w)
    {
    case 0:
    case 1:
    case 2:
        if (!Day(v[0], &date))
        {
            return false;
        }
        *resultOut = w == 0   ? mlocFormatTemporalDate(&date, &o, out)
                     : w == 1 ? mlocFormatYearMonth(&date, &o, out)
                              : mlocFormatMonthDay(&date, &o, out);
        return true;
    case 3:
        if (!TimeOf(v, &dt.time))
        {
            return false;
        }
        *resultOut = mlocFormatTemporalTime(&dt.time, &o, out);
        return true;
    case 4:
        if (!Day(v[0], &dt.date) || !TimeOf(v + 1, &dt.time))
        {
            return false;
        }
        *resultOut = mlocFormatTemporalDateTime(&dt, &o, out);
        return true;
    case 5:
        if (!InstantOf(v, &instant) || (!Is(v[2], "-") && !ZoneOf(v[2], &zone)))
        {
            return false;
        }
        *resultOut = mlocFormatInstant(&instant, Is(v[2], "-") ? nullptr : &zone, &o, out);
        return true;
    case 6:
        if (!ZoneOf(v[0], &zone) || !InstantOf(v + 1, &instant))
        {
            return false;
        }
        *resultOut = mlocFormatZonedDateTime(&zone, &instant, &o, out);
        return true;
    case 7:
    {
        double f[10];
        for (int i = 0; i < 10; i += 1)
        {
            char number[48];
            if (v[i].length >= sizeof number)
            {
                return false;
            }
            memcpy(number, v[i].text, v[i].length);
            number[v[i].length] = '\0';
            f[i] = strtod(number, nullptr);
        }
        mlocDurationValues values = {f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]};
        *resultOut = mlocFormatTemporalDuration(&values, &o, out);
        return true;
    }
    default:
    {
        int64_t ns = 0;
        if (!Number(v[0], &ns))
        {
            return false;
        }
        *resultOut = mlocFormatUTCOffset(ns, out);
        return true;
    }
    }
}

static bool Written(const Token* t, size_t count, bool* badOut)
{
    char want[maxText];
    char got[maxText];
    mlocOutput out = {.buffer = got, .capacity = sizeof got};
    mlocResult r = mloc_errorInvalid;
    size_t length = count > 2 && Is(t[count - 2], "=") ? Unhex(t[count - 1], want) : SIZE_MAX;
    *badOut = length == SIZE_MAX || !Write(t, count, &out, &r);
    return !*badOut && r == mloc_success && out.length == length && memcmp(got, want, length) == 0;
}

int main(void)
{
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    size_t refusals = 0;
    for (size_t i = 0; i < rowCount; i += 1)
    {
        Token t[maxTokens];
        size_t count = Split(s_rows[i], t);
        bool bad = count < 4;
        bool writer = !bad && t[0].text[0] == 'w';
        bool ok = !bad && (writer ? Written(t, count, &bad) : Refused(t, count, &bad));
        refusals += writer ? 0 : 1;
        if (bad || !ok)
        {
            printf("%s: %s\n", bad ? "bad row" : "differs", s_rows[i]);
        }
        CHECK(!bad && ok, writer ? "test262's string written" : "test262's refused string");
    }
    CHECK(refusals > 0 && refusals < rowCount, "both kinds of rows");
    return s_failures == 0 ? 0 : 1;
}
