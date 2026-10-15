// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// test262's Temporal strings that must be refused (record mloc-0041):
// every string its tests expect a RangeError for because the string
// itself is malformed, in test262_ixdtf.inc, checked against the parser
// the reader called: mlocParseTemporalString with the goals given,
// mlocParseUTCOffset, mlocParseTimeZoneString, mlocParseCalendarString
// or mlocParseDuration. The strings test262 reads successfully are
// checked by the rows of the values made from them, and every string is
// compared with a second reading of the grammar by tools/check_ixdtf.py.
// The rows were recorded by running the tests on the library through a
// reader of their JavaScript, kept in the docs with the oracle programs.

#include "test_harness.h"

#include "maul-locale/ixdtf.h"

#include <stdio.h>
#include <string.h>

static const char* const s_rows[] = {
#include "test262_ixdtf.inc"
};

enum
{
    maxText = 512,
    rowCount = sizeof s_rows / sizeof s_rows[0],
};

static int HexDigit(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    return ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : -1;
}

// Reads x and the string's bytes in hexadecimal up to the next space;
// returns the length, or SIZE_MAX for a malformed token.
static size_t Unhex(const char** p, char* out)
{
    const char* s = *p;
    if (*s != 'x')
    {
        return SIZE_MAX;
    }
    s += 1;
    size_t n = 0;
    while (*s != ' ' && *s != '\0')
    {
        int high = HexDigit(s[0]);
        int low = s[1] == '\0' ? -1 : HexDigit(s[1]);
        if (high < 0 || low < 0 || n == maxText)
        {
            return SIZE_MAX;
        }
        out[n] = (char)(high * 16 + low);
        n += 1;
        s += 2;
    }
    *p = s;
    return n;
}

// Whether the row's parser refuses its string; *badOut is set for a row
// this test cannot read.
static bool Refused(const char* row, bool* badOut)
{
    static char text[maxText];
    *badOut = true;
    const char* p = strchr(row, ' ');
    if (p == nullptr)
    {
        return false;
    }
    size_t kindLength = (size_t)(p - row);
    p += 1;
    unsigned formats = 0;
    if (kindLength == 2 && memcmp(row, "dt", 2) == 0)
    {
        while (*p >= '0' && *p <= '9')
        {
            formats = formats * 10 + (unsigned)(*p - '0');
            p += 1;
        }
        if (*p != ' ')
        {
            return false;
        }
        p += 1;
    }
    size_t length = Unhex(&p, text);
    if (length == SIZE_MAX || strcmp(p, " = refused") != 0)
    {
        return false;
    }
    *badOut = false;
    mlocResult r = mloc_errorInvalid;
    if (kindLength == 2 && memcmp(row, "dt", 2) == 0)
    {
        mlocTemporalParse parse;
        r = mlocParseTemporalString(text, length, formats, &parse);
    }
    else if (kindLength == 6 && memcmp(row, "offset", 6) == 0)
    {
        int64_t ns = 0;
        r = mlocParseUTCOffset(text, length, &ns, nullptr);
    }
    else if (kindLength == 4 && memcmp(row, "zone", 4) == 0)
    {
        mlocTimeZoneParse zone;
        r = mlocParseTimeZoneString(text, length, &zone);
    }
    else if (kindLength == 8 && memcmp(row, "calendar", 8) == 0)
    {
        mlocTextSpan span;
        r = mlocParseCalendarString(text, length, &span);
    }
    else if (kindLength == 8 && memcmp(row, "duration", 8) == 0)
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

int main(void)
{
    for (size_t i = 0; i < rowCount; i += 1)
    {
        bool bad = false;
        bool refused = Refused(s_rows[i], &bad);
        if (bad || !refused)
        {
            printf("%s: %s\n", bad ? "bad row" : "accepted", s_rows[i]);
        }
        CHECK(!bad && refused, "test262's refused string");
    }
    CHECK(rowCount > 0, "rows");
    return s_failures == 0 ? 0 : 1;
}
