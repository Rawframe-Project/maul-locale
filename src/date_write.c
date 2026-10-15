// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A date's fields written by a compiled pattern (date_format.h): numbers
// in the formatter's digits, or a field's own system; names from the
// formatter's lists by context and width; months by code, so that a
// Hebrew leap year's Adar I and Adar II and a Chinese leap month find
// their names.

#include "date_write.h"

#include "bytes.h"

#include <string.h>

enum
{
    maxNumber = 64,
};

static uint32_t Width(uint32_t width, uint32_t abbreviated)
{
    // Names: abbreviated up to three letters (or the given), wide four,
    // narrow five, short six.
    if (width <= abbreviated)
    {
        return 0;
    }
    return width == 4 ? 1 : width == 5 ? 2 : 3;
}

static void WriteName(const DateFormat* f, uint32_t kind, const uint8_t* set, uint32_t item,
                      NumberWriter* w)
{
    DateNameList list = mlocDateNameList(&f->names, mlocLoad16(set + 2 * (size_t)kind));
    DataSpan name = mlocDateName(&f->names, list, item);
    mlocWritePart(w,
                  kind == dateNamesEras       ? mloc_datePartEra
                  : kind == dateNamesWeekdays ? mloc_datePartWeekday
                  : kind == dateNamesCyclic   ? mloc_datePartYearName
                                              : mloc_datePartMonth,
                  name.bytes, name.size);
}

// A number in digits (each of digitLength bytes), at least width of them.
static size_t Digits(const uint8_t* digits, uint8_t digitLength, uint64_t value, uint32_t width,
                     char* out)
{
    char reversed[24];
    uint32_t n = 0;
    do
    {
        reversed[n++] = (char)(value % 10);
        value /= 10;
    } while (value > 0 && n < sizeof(reversed));
    while (n < width && n < sizeof(reversed))
    {
        reversed[n++] = 0;
    }
    size_t length = 0;
    while (n > 0)
    {
        memcpy(out + length, digits + numberDigitSize * (size_t)reversed[--n], digitLength);
        length += digitLength;
    }
    return length;
}

// A number field: in the formatter's digits, or by the system the
// pattern names for it (its digits, or its spell-out rules).
static void WriteNumber(const DateFormat* f, bool overridden, uint32_t kind, uint64_t value,
                        uint32_t width, NumberWriter* w)
{
    char text[maxNumber * numberDigitSize];
    size_t length = 0;
    if (overridden && f->hasSpellout)
    {
        mlocDecimal decimal;
        mlocOutput out = {.buffer = text, .capacity = sizeof(text)};
        if (mlocMakeUnsignedDecimal(value, &decimal) == mloc_success &&
            mlocFormatSpellout(&f->spellout, &decimal, &out) == mloc_success)
        {
            mlocWritePart(w, kind, (const uint8_t*)text, out.length);
            return;
        }
    }
    if (overridden && f->overrideDigitLength > 0)
    {
        length = Digits(f->overrideDigits, f->overrideDigitLength, value, width, text);
    }
    else
    {
        length = Digits(f->digits, f->digitLength, value, width, text);
    }
    mlocWritePart(w, kind, (const uint8_t*)text, length);
}

// The era's place among the calendar's, or -1 (a Japanese date before
// Meiji, whose era is the Gregorian calendar's).
static int32_t EraIndex(uint32_t calendar, const mlocDateFields* fields)
{
    static const char* const s_codes[dateCalendarCount] = dateEraCodes;
    const char* codes = s_codes[calendar];
    int32_t index = 0;
    for (const char* at = codes; *at != '\0'; index++)
    {
        size_t n = strcspn(at, " ");
        if (n == fields->eraLength && memcmp(at, fields->era, n) == 0)
        {
            return index;
        }
        at += n + (at[n] == ' ');
    }
    return -1;
}

static void WriteEra(const DateFormat* f, const mlocDateFields* fields, uint32_t width,
                     NumberWriter* w)
{
    const uint8_t* set = f->nameSet;
    uint32_t calendar = f->calendar;
    int32_t index = EraIndex(calendar, fields);
    if (index < 0)
    {
        set = f->gregorianSet;
        calendar = mloc_calendarGregorian;
        index = EraIndex(calendar, fields);
    }
    DateNameList list = mlocDateNameList(&f->names, mlocLoad16(set + 2 * dateNamesEras));
    uint32_t eras = list.count / 3;
    if (index < 0 || (uint32_t)index >= eras)
    {
        return;
    }
    WriteName(f, dateNamesEras, set, Width(width, 3) % 3 * eras + (uint32_t)index, w);
}

// The month's slot in the month lists: its code's number, Hebrew months
// by CLDR's numbering (Adar I is month 6, a leap year's Adar II the leap
// slot).
static uint32_t MonthSlot(uint32_t calendar, const mlocDateFields* fields)
{
    uint32_t number =
        (uint32_t)(fields->monthCode[1] - '0') * 10 + (uint32_t)(fields->monthCode[2] - '0');
    bool leap = fields->monthCodeLength == 4;
    if (calendar != mloc_calendarHebrew)
    {
        return number - 1;
    }
    if (leap)
    {
        return 5;
    }
    if (number == 6 && fields->inLeapYear)
    {
        return dateMonthCount - 1;
    }
    return number < 6 ? number - 1 : number;
}

// A leap month's pattern around text ("闰{0}"); the text alone when the
// calendar has none.
static void WriteLeap(const DateFormat* f, uint32_t context, uint32_t width, const uint8_t* text,
                      size_t length, NumberWriter* w)
{
    DateNameList list = mlocDateNameList(&f->names, mlocLoad16(f->nameSet + 2 * dateNamesLeap));
    DataSpan pattern = mlocDateName(&f->names, list, context * 3 + width);
    const uint8_t* at = nullptr;
    for (uint32_t i = 0; i + 3 <= pattern.size && at == nullptr; i++)
    {
        at = memcmp(pattern.bytes + i, "{0}", 3) == 0 ? pattern.bytes + i : nullptr;
    }
    if (at == nullptr)
    {
        mlocWritePart(w, mloc_datePartMonth, text, length);
        return;
    }
    mlocWritePart(w, mloc_datePartMonth, pattern.bytes, (size_t)(at - pattern.bytes));
    mlocWritePart(w, mloc_datePartMonth, text, length);
    mlocWritePart(w, mloc_datePartMonth, at + 3, pattern.size - (size_t)(at + 3 - pattern.bytes));
}

static void WriteMonth(const DateFormat* f, const mlocDateFields* fields, char letter,
                       uint32_t width, bool overridden, NumberWriter* w)
{
    bool leap = fields->monthCodeLength == 4 && f->calendar != mloc_calendarHebrew;
    uint32_t context = letter == 'L' ? 1 : 0;
    if (width <= 2)
    {
        // A Hebrew month's number is its place in its year; the others'
        // their code's, a leap month's in the numeric leap pattern.
        uint32_t number =
            f->calendar == mloc_calendarHebrew ? fields->month : MonthSlot(f->calendar, fields) + 1;
        char text[maxNumber];
        NumberWriter local = {.buffer = text, .capacity = sizeof(text)};
        WriteNumber(f, overridden, mloc_datePartMonth, number, width, &local);
        size_t length = local.length < sizeof(text) ? local.length : sizeof(text);
        if (leap)
        {
            WriteLeap(f, 2, 0, (const uint8_t*)text, length, w);
        }
        else
        {
            mlocWritePart(w, mloc_datePartMonth, (const uint8_t*)text, length);
        }
        return;
    }
    uint32_t slot = MonthSlot(f->calendar, fields);
    uint32_t item = (context * 3 + Width(width, 3) % 3) * dateMonthCount + slot;
    DateNameList list = mlocDateNameList(&f->names, mlocLoad16(f->nameSet + 2 * dateNamesMonths));
    DataSpan name = mlocDateName(&f->names, list, item);
    if (leap)
    {
        WriteLeap(f, context, Width(width, 3) % 3, name.bytes, name.size, w);
        return;
    }
    mlocWritePart(w, mloc_datePartMonth, name.bytes, name.size);
}

static void WriteWeekday(const DateFormat* f, const mlocDateFields* fields, char letter,
                         uint32_t width, NumberWriter* w)
{
    uint32_t context = letter == 'c' ? 1 : 0;
    uint32_t day = fields->dayOfWeek % 7;
    WriteName(f, dateNamesWeekdays, f->nameSet, (context * 4 + Width(width, 3)) * 7 + day, w);
}

// The year of the week a date is in, by the locale's first day and
// minimal days (UTS #35's week of year).
static int64_t WeekYear(const DateFormat* f, const mlocDateFields* fields, int64_t year)
{
    int32_t relative = (fields->dayOfWeek - f->firstDay + 7) % 7;
    int32_t firstRelative = ((relative - (fields->dayOfYear - 1)) % 7 + 7) % 7;
    int32_t firstDays = 7 - firstRelative;
    int32_t start = firstDays >= f->minimalDays ? 1 - firstRelative : 1 + firstDays;
    if (fields->dayOfYear < start)
    {
        return year - 1;
    }
    int32_t nextRelative = (relative + (fields->daysInYear - fields->dayOfYear) + 1) % 7;
    int32_t nextDays = 7 - nextRelative;
    int32_t nextStart =
        fields->daysInYear + 1 + (nextDays >= f->minimalDays ? -nextRelative : nextDays);
    return fields->dayOfYear >= nextStart ? year + 1 : year;
}

// A year field: two digits for a width of 2, else at least width. A
// Hebrew year of the current millennium written in Hebrew letters leaves
// its thousands out ("תשפ״ז" for 5787), as ICU4C writes it.
static void WriteYear(const DateFormat* f, int64_t year, uint32_t width, bool overridden,
                      uint32_t kind, NumberWriter* w)
{
    uint64_t value = year < 0 ? (uint64_t)-year : (uint64_t)year;
    if (width == 2)
    {
        value %= 100;
    }
    if (overridden && f->hasSpellout && f->calendar == mloc_calendarHebrew && value > 5000 &&
        value < 6000)
    {
        value -= 5000;
    }
    if (year < 0)
    {
        mlocWritePart(w, kind, f->minus, f->minusLength);
    }
    WriteNumber(f, overridden, kind, value, width, w);
}

// The year in the calendar's cycle of 60 (Chinese and Dangi), from 0.
static uint32_t CycleIndex(int32_t relatedYear)
{
    return (uint32_t)((((int64_t)relatedYear - 4) % 60 + 60) % 60);
}

static void WriteField(const DateFormat* f, const mlocDateFields* fields, int32_t relatedYear,
                       const uint8_t* item, NumberWriter* w)
{
    char letter = (char)item[1];
    uint32_t width = item[2];
    bool overridden = item[0] == datePatternOverride;
    bool cyclic = f->calendar == mloc_calendarChinese || f->calendar == mloc_calendarDangi;
    int64_t year = cyclic ? (int64_t)CycleIndex(relatedYear) + 1 : (int64_t)fields->eraYear;
    switch (letter)
    {
    case 'G':
        WriteEra(f, fields, width, w);
        break;
    case 'y':
        WriteYear(f, year, width, overridden, mloc_datePartYear, w);
        break;
    case 'Y':
        WriteYear(f, WeekYear(f, fields, year), width, overridden, mloc_datePartYear, w);
        break;
    case 'r':
        WriteYear(f, relatedYear, width, overridden, mloc_datePartRelatedYear, w);
        break;
    case 'U':
        WriteName(f, dateNamesCyclic, f->nameSet,
                  Width(width, 3) % 3 * 60 + CycleIndex(relatedYear), w);
        break;
    case 'M':
    case 'L':
        WriteMonth(f, fields, letter, width, overridden, w);
        break;
    case 'd':
        WriteNumber(f, overridden, mloc_datePartDay, fields->day, width, w);
        break;
    default:
        WriteWeekday(f, fields, letter, width, w);
        break;
    }
}

void mlocWriteDateFields(const DateFormat* f, const uint8_t* pattern, uint32_t length,
                         const mlocDateFields* fields, int32_t relatedYear, NumberWriter* w)
{
    uint32_t at = 0;
    while (at < length)
    {
        if (pattern[at] == datePatternField || pattern[at] == datePatternOverride)
        {
            WriteField(f, fields, relatedYear, pattern + at, w);
            at += pattern[at] == datePatternField ? 3 : 4 + pattern[at + 3];
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
