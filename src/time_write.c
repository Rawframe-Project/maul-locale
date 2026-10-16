// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A time's fields written by a compiled pattern (time_write.h): hours on
// the formatter's cycle, minutes, seconds and their fraction in its
// digits, and day periods as ICU4C writes them: a by the half of the
// day; b as noon at 12:00 exactly as shown; B by the locale's rules,
// noon at its instant where the rules name it, never midnight (ICU
// leaves it out), and a where the period is am or pm or has no name.

#include "time_write.h"

#include "date_write.h"

enum
{
    maxNumber = 24,
};

static void WriteNumber(const TimeFormat* f, uint32_t kind, uint64_t value, uint32_t width,
                        NumberWriter* w)
{
    char text[maxNumber * numberDigitSize];
    size_t length = mlocDateDigits(f->digits, f->digitLength, value, width, text);
    mlocWritePart(w, kind, (const uint8_t*)text, length);
}

static uint32_t Hour(char letter, uint32_t hour)
{
    switch (letter)
    {
    case 'h':
        return hour % 12 == 0 ? 12 : hour % 12;
    case 'K':
        return hour % 12;
    case 'k':
        return hour == 0 ? 24 : hour;
    default:
        return hour;
    }
}

// The fraction's digits after the seconds, truncated.
static void WriteFraction(const TimeFormat* f, uint32_t nanosecond, NumberWriter* w)
{
    uint32_t value = nanosecond;
    for (uint32_t i = f->fractionDigits; i < 9; i++)
    {
        value /= 10;
    }
    mlocWritePart(w, mloc_datePartLiteral, f->decimal, f->decimalLength);
    WriteNumber(f, mloc_datePartFraction, value, f->fractionDigits, w);
}

// A day period's name at a pattern width: abbreviated up to three
// letters, wide at four or past five, narrow at five.
static DataSpan PeriodName(const TimeFormat* f, uint32_t width, uint32_t period)
{
    uint32_t form = width <= 3 ? 0 : width == 5 ? 2 : 1;
    return mlocDateName(&f->names, f->dayPeriods, form * dateDayPeriodCount + period);
}

static void WritePeriod(const TimeFormat* f, uint32_t width, uint32_t period, uint32_t hour,
                        NumberWriter* w)
{
    DataSpan name = PeriodName(f, width, period);
    if (period <= dateDayPeriodPm || name.size == 0)
    {
        name = PeriodName(f, width, hour < 12 ? dateDayPeriodAm : dateDayPeriodPm);
    }
    mlocWritePart(w, mloc_datePartDayPeriod, name.bytes, name.size);
}

// Whether a pattern holds a field of a letter.
static bool HasField(const uint8_t* pattern, uint32_t length, char letter)
{
    for (uint32_t at = 0; at < length; at++)
    {
        if (pattern[at] == datePatternField && pattern[at + 1] == (uint8_t)letter)
        {
            return true;
        }
        at += pattern[at] == datePatternField ? 2 : 0;
    }
    return false;
}

static void WriteDayPeriod(const TimeFormat* f, const uint8_t* pattern, uint32_t length,
                           const mlocTime* time, char letter, uint32_t width, NumberWriter* w)
{
    // Noon is the time as shown: its minutes and seconds, where the
    // pattern shows them, zero.
    bool noon = time->hour == 12 && (time->minute == 0 || !HasField(pattern, length, 'm')) &&
                (time->second == 0 || !HasField(pattern, length, 's'));
    // Am stands for am or pm, which WritePeriod tells by the hour.
    uint32_t period = dateDayPeriodAm;
    if (letter == 'b')
    {
        period = noon ? dateDayPeriodNoon : period;
    }
    else if (letter == 'B')
    {
        bool named = (f->rules[24] & dateRuleNoon) != 0;
        period = noon && named && PeriodName(f, width, dateDayPeriodNoon).size > 0
                     ? dateDayPeriodNoon
                     : f->rules[time->hour];
    }
    WritePeriod(f, width, period, time->hour, w);
}

static void WriteField(const TimeFormat* f, const uint8_t* pattern, uint32_t length,
                       const mlocTime* time, const uint8_t* item, NumberWriter* w)
{
    char letter = (char)item[1];
    uint32_t width = item[2];
    switch (letter)
    {
    case 'h':
    case 'H':
    case 'K':
    case 'k':
        letter = f->hourLetter != 0 ? (char)f->hourLetter : letter;
        WriteNumber(f, mloc_datePartHour, Hour(letter, time->hour), width, w);
        break;
    case 'm':
        WriteNumber(f, mloc_datePartMinute, time->minute, width, w);
        break;
    case 's':
        WriteNumber(f, mloc_datePartSecond, time->second, width, w);
        if (f->fractionDigits > 0)
        {
            WriteFraction(f, time->nanosecond, w);
        }
        break;
    default:
        WriteDayPeriod(f, pattern, length, time, letter, width, w);
        break;
    }
}

void mlocWriteTimeFields(const TimeFormat* f, const uint8_t* pattern, uint32_t length,
                         const mlocTime* time, NumberWriter* w)
{
    uint32_t at = 0;
    while (at < length)
    {
        if (pattern[at] == datePatternField)
        {
            WriteField(f, pattern, length, time, pattern + at, w);
            at += 3;
            continue;
        }
        uint32_t end = at;
        while (end < length && pattern[end] >= 0x20)
        {
            end++;
        }
        mlocWritePart(w, mloc_datePartLiteral, pattern + at, end - at);
        at = end;
    }
}
