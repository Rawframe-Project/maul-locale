// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the calendars, through the public date calls. The
// input is a program over two dates: each step's first byte chooses a
// call (making a date from a day or from fields, reading its fields,
// adding, a difference plain or rounded, changing fields, a month-day),
// and the bytes after it its arguments. Numbers are drawn from each
// value's edges (Temporal's first and last days and years, 0, the limits
// of a duration's fields, the 32-bit extremes), small, anywhere in its
// range (days from mlocDateMinDay to mlocDateMaxDay, years over those
// days, so every calendar's years are reached, Chinese and Dangi's
// astronomical model among them) or as any 64 bits. Eras and month codes
// come from the calendars' own lists or as raw bytes.
//
// Every call must give success or a refusal, and the answers must agree:
// a date's fields describe a valid date that the same fields make again,
// a difference has one sign, and a difference in days or weeks added to
// the first date reaches the second.

#include "maul-locale/calendar.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

enum
{
    calendars = 18,
    maxSteps = 64,
};

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

typedef struct Reader
{
    const uint8_t* at;
    const uint8_t* end;
} Reader;

static uint8_t Byte(Reader* r)
{
    return r->at < r->end ? *r->at++ : 0;
}

static uint64_t Bits(Reader* r, int bytes)
{
    uint64_t v = 0;
    for (int i = 0; i < bytes; i++)
    {
        v = v << 8 | Byte(r);
    }
    return v;
}

// A number near an edge, small, anywhere in [low, high], or any 64 bits.
static int64_t Spread(Reader* r, const int64_t* edges, size_t edgeCount, int64_t low, int64_t high)
{
    uint8_t how = Byte(r);
    if (how < 0x80)
    {
        return edges[how % edgeCount] + (int8_t)Byte(r) % 4;
    }
    if (how < 0xA0)
    {
        return (int8_t)Byte(r);
    }
    if (how < 0xF0)
    {
        return low + (int64_t)(Bits(r, 4) % (uint64_t)(high - low + 1));
    }
    return (int64_t)Bits(r, 8);
}

static int64_t Number(Reader* r, const int64_t* edges, size_t edgeCount)
{
    return Spread(r, edges, edgeCount, 0, 0xFFFF);
}

static const int64_t s_days[] = {
    mlocDateMinDay,
    mlocDateMaxDay,
    0,
    1,
    -1,
    730120,
    693596,
    -693596,
    36524,
    146097,
    -100000000,
    100000000,
    INT32_MIN,
    INT32_MAX,
    mlocDateMinDay - 1,
    mlocDateMaxDay + 1,
};

static const int64_t s_years[] = {
    -271821, 275760, -271822, 275761, 0,         1,         -1,      1600,       2400, 1972,
    2035,    1900,   5500,    -5500,  1873,      1989,      2019,    622,        1444, 5785,
    4000,    6000,   -1000,   10000,  INT32_MIN, INT32_MAX, 1 << 30, -(1 << 30),
};

// A year: near an edge, small, anywhere in the years Temporal's days
// reach, or any 64 bits.
static int64_t Year(Reader* r)
{
    return Spread(r, s_years, sizeof(s_years) / sizeof(s_years[0]), -271822, 275761);
}

static const int64_t s_amounts[] = {
    0,
    1,
    -1,
    12,
    13,
    30,
    365,
    4294967295,
    -4294967295,
    4294967296,
    -4294967296,
    104249991374,
    -104249991374,
    104249991375,
    200000000,
    -200000000,
    600000,
    -600000,
};

static const int64_t s_small[] = {0, 1, 2, 5, 12, 13, 14, 29, 30, 31, 32, 255, 256};

static const char* const s_eras[] = {
    "ce", "bce", "be",    "roc",    "broc",  "am",     "aa",    "shaka", "ap",
    "ah", "bh",  "meiji", "taisho", "showa", "heisei", "reiwa", "",      "CE",
};

static const char* const s_codes[] = {"M01",  "M02", "M05",  "M05L", "M06",  "M06L",
                                      "M11L", "M12", "M13",  "M12L", "M13L", "M00",
                                      "M14",  "M1",  "M01L", "M001", "m01",  ""};

// A code from a list or raw bytes: its text and length.
static void Code(Reader* r, const char* const* list, size_t count, char* raw, const char** textOut,
                 size_t* lengthOut)
{
    uint8_t how = Byte(r);
    if (how < 0xE0)
    {
        *textOut = list[how % count];
        *lengthOut = strlen(*textOut);
        return;
    }
    size_t length = Byte(r) % 9;
    for (size_t i = 0; i < length; i++)
    {
        raw[i] = (char)Byte(r);
    }
    *textOut = raw;
    *lengthOut = length;
}

static mlocCalendar Calendar(Reader* r)
{
    uint8_t b = Byte(r);
    // Now and then one past the last.
    return (mlocCalendar)(b == 0xFF ? calendars : b % calendars);
}

static bool Expected(mlocResult result)
{
    return result == mloc_success || result == mloc_errorInvalid || result == mloc_errorSyntax;
}

// A date's fields hold together, and making the date from them gives it
// again.
static void CheckFields(const mlocDate* date)
{
    mlocDateFields f;
    Require(mlocGetDateFields(date, &f) == mloc_success);
    Require(f.month >= 1 && f.month <= f.monthsInYear && f.day >= 1 && f.day <= f.daysInMonth &&
            f.dayOfYear >= 1 && f.dayOfYear <= f.daysInYear && f.dayOfWeek >= 1 &&
            f.dayOfWeek <= 7 && f.monthCodeLength >= 3 && f.monthCodeLength <= 4 &&
            f.inLeapYear <= 1);
    mlocDateInput input = {.year = f.year,
                           .monthCode = f.monthCode,
                           .monthCodeLength = f.monthCodeLength,
                           .month = f.month,
                           .day = f.day};
    mlocDate again = {0};
    Require(mlocMakeDate((mlocCalendar)date->calendar, &input, mloc_overflowReject, &again) ==
                mloc_success &&
            again.day == date->day);
    if (f.eraLength > 0)
    {
        mlocDateInput byEra = {.era = f.era,
                               .eraLength = f.eraLength,
                               .year = f.eraYear,
                               .monthCode = f.monthCode,
                               .monthCodeLength = f.monthCodeLength,
                               .day = f.day};
        Require(mlocMakeDate((mlocCalendar)date->calendar, &byEra, mloc_overflowReject, &again) ==
                    mloc_success &&
                again.day == date->day);
    }
}

static mlocDateDuration Duration(Reader* r)
{
    size_t n = sizeof(s_amounts) / sizeof(s_amounts[0]);
    mlocDateDuration d = {0};
    uint8_t which = Byte(r);
    d.years = (which & 1) != 0 ? Number(r, s_amounts, n) : 0;
    d.months = (which & 2) != 0 ? Number(r, s_amounts, n) : 0;
    d.weeks = (which & 4) != 0 ? Number(r, s_amounts, n) : 0;
    d.days = (which & 8) != 0 ? Number(r, s_amounts, n) : 0;
    return d;
}

static bool OneSign(const mlocDateDuration* d)
{
    bool positive = d->years > 0 || d->months > 0 || d->weeks > 0 || d->days > 0;
    bool negative = d->years < 0 || d->months < 0 || d->weeks < 0 || d->days < 0;
    return !(positive && negative);
}

static void Step(Reader* r, mlocDate dates[2])
{
    uint8_t op = Byte(r);
    mlocDate* date = &dates[op >> 7];
    mlocDate* other = &dates[(op >> 7) ^ 1];
    mlocDate out = {0};
    char rawEra[9];
    char rawCode[9];
    mlocOverflow overflow = (mlocOverflow)(Byte(r) & 1);
    switch (op % 8)
    {
    case 0:
    {
        mlocCalendar c = Calendar(r);
        int64_t day =
            Spread(r, s_days, sizeof(s_days) / sizeof(s_days[0]), mlocDateMinDay, mlocDateMaxDay);
        mlocResult result = mlocMakeDateFromDay(c, (int32_t)day, &out);
        Require(Expected(result));
        if (result == mloc_success)
        {
            *date = out;
        }
        break;
    }
    case 1:
    {
        mlocCalendar c = Calendar(r);
        mlocDateInput input = {0};
        uint8_t parts = Byte(r);
        if ((parts & 1) != 0)
        {
            Code(r, s_eras, sizeof(s_eras) / sizeof(s_eras[0]), rawEra, &input.era,
                 &input.eraLength);
        }
        input.year = (int32_t)Year(r);
        if ((parts & 2) != 0)
        {
            Code(r, s_codes, sizeof(s_codes) / sizeof(s_codes[0]), rawCode, &input.monthCode,
                 &input.monthCodeLength);
        }
        input.month = (uint32_t)Number(r, s_small, sizeof(s_small) / sizeof(s_small[0]));
        input.day = (uint32_t)Number(r, s_small, sizeof(s_small) / sizeof(s_small[0]));
        mlocResult result = mlocMakeDate(c, &input, overflow, &out);
        Require(Expected(result));
        if (result == mloc_success)
        {
            *date = out;
            CheckFields(date);
        }
        break;
    }
    case 2:
    {
        // A date the library did not make now and then.
        mlocDate probe = *date;
        uint8_t spoil = Byte(r);
        if (spoil == 1)
        {
            probe.reserved[0] = 1;
        }
        else if (spoil == 2)
        {
            probe.day = mlocDateMaxDay + 1;
        }
        else if (spoil == 3)
        {
            probe.calendar = calendars;
        }
        mlocDateFields f;
        mlocResult result = mlocGetDateFields(&probe, &f);
        Require(spoil >= 1 && spoil <= 3 ? result == mloc_errorInvalid : result == mloc_success);
        if (result == mloc_success)
        {
            CheckFields(&probe);
        }
        break;
    }
    case 3:
    {
        mlocDateDuration d = Duration(r);
        mlocResult result = mlocAddToDate(date, &d, overflow, &out);
        Require(result == mloc_success || result == mloc_errorInvalid);
        if (result == mloc_success)
        {
            Require(out.calendar == date->calendar);
            *other = out;
        }
        break;
    }
    case 4:
    {
        mlocDateUnit unit = (mlocDateUnit)(Byte(r) % 5);
        mlocDateDuration d;
        mlocResult result = mlocDateUntil(date, other, unit, &d);
        Require(result == mloc_success || result == mloc_errorInvalid);
        if (result != mloc_success)
        {
            Require(unit > mloc_dateUnitDay || date->calendar != other->calendar);
            break;
        }
        Require(OneSign(&d));
        if (unit >= mloc_dateUnitWeek)
        {
            Require(d.years == 0 && d.months == 0 &&
                    (int64_t)date->day + 7 * d.weeks + d.days == other->day);
        }
        mlocDate reached = {0};
        if (unit >= mloc_dateUnitWeek)
        {
            Require(mlocAddToDate(date, &d, mloc_overflowConstrain, &reached) == mloc_success &&
                    reached.day == other->day);
        }
        break;
    }
    case 5:
    {
        mlocDateRounding rounding = {
            .largestUnit = (mlocDateUnit)(Byte(r) % 5),
            .smallestUnit = (mlocDateUnit)(Byte(r) % 5),
            .increment = (uint32_t)Number(r, s_amounts, sizeof(s_amounts) / sizeof(s_amounts[0])),
            .mode = (mlocRoundingMode)(Byte(r) % 10),
        };
        mlocDateDuration d;
        mlocResult result = mlocDateUntilRounded(date, other, &rounding, &d);
        Require(result == mloc_success || result == mloc_errorInvalid);
        if (result == mloc_success)
        {
            Require(OneSign(&d));
        }
        break;
    }
    case 6:
    {
        mlocDateChanges changes = {.given = Byte(r) & 63};
        Code(r, s_eras, sizeof(s_eras) / sizeof(s_eras[0]), rawEra, &changes.era,
             &changes.eraLength);
        changes.eraYear = (int32_t)Year(r);
        changes.year = (int32_t)Year(r);
        Code(r, s_codes, sizeof(s_codes) / sizeof(s_codes[0]), rawCode, &changes.monthCode,
             &changes.monthCodeLength);
        changes.month = (uint32_t)Number(r, s_small, sizeof(s_small) / sizeof(s_small[0]));
        changes.day = (uint32_t)Number(r, s_small, sizeof(s_small) / sizeof(s_small[0]));
        mlocResult result = mlocWithDate(date, &changes, overflow, &out);
        Require(Expected(result));
        if (result == mloc_success)
        {
            Require(out.calendar == date->calendar);
            *date = out;
            CheckFields(date);
        }
        break;
    }
    default:
    {
        mlocCalendar c = Calendar(r);
        mlocDateChanges fields = {.given = Byte(r) & 63};
        Code(r, s_eras, sizeof(s_eras) / sizeof(s_eras[0]), rawEra, &fields.era, &fields.eraLength);
        fields.eraYear = (int32_t)Year(r);
        fields.year = (int32_t)Year(r);
        Code(r, s_codes, sizeof(s_codes) / sizeof(s_codes[0]), rawCode, &fields.monthCode,
             &fields.monthCodeLength);
        fields.month = (uint32_t)Number(r, s_small, sizeof(s_small) / sizeof(s_small[0]));
        fields.day = (uint32_t)Number(r, s_small, sizeof(s_small) / sizeof(s_small[0]));
        mlocResult result = mlocMakeMonthDay(c, &fields, overflow, &out);
        Require(Expected(result));
        if (result == mloc_success)
        {
            Require(out.calendar == (uint8_t)c);
            CheckFields(&out);
        }
        break;
    }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Reader r = {data, data + size};
    mlocDate dates[2] = {{.day = 738000, .calendar = mloc_calendarGregorian},
                         {.day = 738000, .calendar = mloc_calendarGregorian}};
    for (int step = 0; step < maxSteps && r.at < r.end; step++)
    {
        Step(&r, dates);
    }
    return 0;
}
