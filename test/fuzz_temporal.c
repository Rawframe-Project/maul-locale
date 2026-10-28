// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the arithmetic of instants and zoned date-times
// over the compiled-in zones. The input gives a zone (one of the data's,
// or an offset), a calendar, two instants, a time and a date duration, a
// rounding and a disambiguation. Every call succeeds or refuses as invalid
// alone, and the results agree as ECMA-262 makes them: an instant's local
// date-time names the instant again; a difference of exact time, added to
// its start, reaches its end, for instants and zoned date-times alike;
// rounding an instant twice gives what rounding it once gave.

#include "maul-locale/calendar.h"
#include "maul-locale/data.h"
#include "maul-locale/zone.h"

#include <stdlib.h>
#include <string.h>

enum
{
    inputBytes = 64,
};

// Temporal's range: 10^8 days either side of 1970, in seconds.
static const int64_t s_maxSeconds = INT64_C(8640000000000);

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static bool SuccessOrInvalid(mlocResult r)
{
    return r == mloc_success || r == mloc_errorInvalid;
}

static uint64_t Load64(const uint8_t* p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
    {
        v |= (uint64_t)p[i] << (8 * i);
    }
    return v;
}

static uint32_t Load32(const uint8_t* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

// An instant mostly within Temporal's range, at its edges when the
// fuzzer finds them, and now and then past them.
static mlocInstant InstantOf(const uint8_t* p)
{
    int64_t raw = (int64_t)Load64(p);
    int64_t seconds = (p[11] & 0x80) != 0 ? raw : raw % (s_maxSeconds + 1);
    int32_t nanoseconds = (int32_t)(Load32(p + 8) % 1000000000u);
    return (mlocInstant){seconds, (p[11] & 0x40) != 0 ? -nanoseconds : nanoseconds};
}

static bool SameInstant(const mlocInstant* a, const mlocInstant* b)
{
    return a->seconds == b->seconds && a->nanoseconds == b->nanoseconds;
}

static bool Zone(const mlocDataSet* set, const uint8_t* p, mlocTimeZone* zone)
{
    if ((p[0] & 0x80) != 0)
    {
        // An offset zone, as the identifier "+hhmm" or "-hhmm".
        uint32_t minutes = Load32(p) % (24 * 60);
        char id[6] = {(p[1] & 1) != 0 ? '-' : '+',     (char)('0' + minutes / 600),
                      (char)('0' + minutes / 60 % 10), (char)('0' + minutes % 60 / 10),
                      (char)('0' + minutes % 10),      0};
        return mlocFindTimeZone(set, id, 5, zone) == mloc_success;
    }
    size_t count = mlocGetTimeZoneCount(set);
    return count > 0 && mlocGetTimeZone(set, Load32(p) % count, zone) == mloc_success;
}

// An instant's local date-time names it again: the possible instants of
// that date-time hold it.
static void CheckLocal(const mlocTimeZone* zone, mlocCalendar calendar, const mlocInstant* instant)
{
    mlocDateTime local;
    mlocResult r = mlocGetDateTimeFor(zone, instant, calendar, &local);
    Require(SuccessOrInvalid(r));
    if (r != mloc_success)
    {
        return;
    }
    mlocPossibleInstants possible;
    r = mlocGetPossibleInstants(zone, &local, &possible);
    Require(SuccessOrInvalid(r));
    Require(r != mloc_success ||
            (possible.count >= 1 &&
             (SameInstant(&possible.earliest, instant) || SameInstant(&possible.latest, instant))));
    for (int d = 0; d <= mloc_disambiguationReject; d++)
    {
        mlocInstant found;
        Require(SuccessOrInvalid(mlocGetInstantFor(zone, &local, (mlocDisambiguation)d, &found)));
    }
    mlocInstant start;
    Require(SuccessOrInvalid(mlocGetStartOfDay(zone, &local.date, &start)));
}

// Instants: a sum and the exact time back, rounding twice.
static void CheckInstants(const mlocInstant* one, const mlocInstant* two,
                          const mlocTimeDuration* duration, const uint8_t* p)
{
    mlocInstant sum;
    mlocResult r = mlocAddToInstant(one, duration, &sum);
    Require(SuccessOrInvalid(r));
    if (r == mloc_success)
    {
        mlocTimeRounding exact = {mloc_timeUnitHour, mloc_timeUnitNanosecond, 1, mloc_roundTrunc};
        mlocTimeDuration back;
        Require(mlocInstantUntil(one, &sum, &exact, &back) == mloc_success);
        mlocInstant again;
        Require(mlocAddToInstant(one, &back, &again) == mloc_success && SameInstant(&again, &sum));
    }
    mlocTimeRounding rounding = {(mlocTimeUnit)(p[0] % 8), (mlocTimeUnit)(p[1] % 8), p[2] % 64u,
                                 (mlocRoundingMode)(p[3] % 10)};
    mlocTimeDuration until;
    Require(SuccessOrInvalid(mlocInstantUntil(one, two, &rounding, &until)));
    mlocInstant rounded;
    r = mlocRoundInstant(one, rounding.smallestUnit, rounding.increment, rounding.mode, &rounded);
    Require(SuccessOrInvalid(r));
    if (r == mloc_success)
    {
        mlocInstant twice;
        Require(mlocRoundInstant(&rounded, rounding.smallestUnit, rounding.increment, rounding.mode,
                                 &twice) == mloc_success &&
                SameInstant(&twice, &rounded));
    }
}

// Zoned date-times: a sum, the exact time from one to another added
// back, a rounded difference, a total and a rounding.
static void CheckZoned(const mlocTimeZone* zone, mlocCalendar calendar, const mlocInstant* one,
                       const mlocInstant* two, const mlocDateTimeDuration* duration,
                       const uint8_t* p)
{
    mlocInstant sum;
    Require(SuccessOrInvalid(
        mlocAddToZonedDateTime(zone, calendar, one, duration, (mlocOverflow)(p[0] % 3), &sum)));
    mlocDateTimeRounding exact = {mloc_unitHour, mloc_unitNanosecond, 1, mloc_roundTrunc};
    mlocDateTimeDuration difference;
    mlocResult r = mlocZonedDateTimeUntil(zone, calendar, one, two, &exact, &difference);
    Require(SuccessOrInvalid(r));
    if (r == mloc_success)
    {
        mlocInstant reached;
        Require(mlocAddToZonedDateTime(zone, calendar, one, &difference, mloc_overflowConstrain,
                                       &reached) == mloc_success &&
                SameInstant(&reached, two));
    }
    mlocDateTimeRounding rounding = {(mlocUnit)(p[1] % 11), (mlocUnit)(p[2] % 11), p[3] % 64u,
                                     (mlocRoundingMode)(p[4] % 10)};
    Require(
        SuccessOrInvalid(mlocZonedDateTimeUntil(zone, calendar, one, two, &rounding, &difference)));
    double total = 0;
    Require(SuccessOrInvalid(
        mlocZonedDateTimeTotal(zone, calendar, one, two, (mlocUnit)(p[5] % 11), &total)));
    mlocInstant rounded;
    Require(SuccessOrInvalid(mlocRoundZonedDateTime(zone, one, (mlocTimeUnit)(p[6] % 8), p[7] % 64u,
                                                    (mlocRoundingMode)(p[8] % 10), &rounded)));
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static mlocData s_builtin;
    static mlocDataSet s_set;
    static bool s_opened;
    if (!s_opened)
    {
        mlocOpenBuiltinData(&s_builtin);
        s_set = (mlocDataSet){&s_builtin, 1};
        s_opened = true;
    }
    if (size < inputBytes)
    {
        return 0;
    }
    mlocTimeZone zone;
    if (!Zone(&s_set, data, &zone))
    {
        return 0;
    }
    mlocCalendar calendar = (mlocCalendar)(data[4] % 19);
    mlocInstant one = InstantOf(data + 5);
    mlocInstant two = InstantOf(data + 17);
    int64_t durationSeconds = (int64_t)Load64(data + 29) % (2 * s_maxSeconds + 1);
    int32_t durationNanoseconds = (int32_t)(Load32(data + 37) % 1000000000u);
    mlocTimeDuration time = {durationSeconds,
                             durationSeconds < 0 ? -durationNanoseconds : durationNanoseconds};
    mlocDateTimeDuration duration = {
        {(int8_t)data[41], (int8_t)data[42], (int8_t)data[43], (int16_t)Load32(data + 44)},
        {(int64_t)(int16_t)Load32(data + 48), 0}};
    CheckLocal(&zone, calendar, &one);
    CheckInstants(&one, &two, &time, data + 52);
    CheckZoned(&zone, calendar, &one, &two, &duration, data + 52);
    return 0;
}
