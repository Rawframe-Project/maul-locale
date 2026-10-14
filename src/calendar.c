// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Dates (maul-locale/calendar.h, record mloc-0040): fields made into a
// day and read back, with Temporal's era codes, month codes and
// overflow.

#include "maul-locale/calendar.h"

#include "calendar_arith.h"
#include "calendar_eras.h"
#include "calendar_hebrew.h"
#include "calendar_lunar.h"
#include "calendar_umalqura.h"

#include <string.h>

enum
{
    calendarCount = 18,
    // Years past this are refused before any arithmetic; Temporal's range
    // lies well within.
    maxYear = 1000000,
    // The Chinese and Dangi years the astronomy is computed for: those
    // holding Temporal's first and last days, and one more each side.
    lunisolarMinYear = -271822,
    lunisolarMaxYear = 275761,
    // January 1 of 1873 (Meiji 6), when Japan took the Gregorian
    // calendar: earlier days read in ce and bce, as Temporal's era table
    // and ICU4X have it.
    japaneseGregorianDay = 683735,
};

// An era: year = base + sign * eraYear.
typedef struct Era
{
    char code[8];
    int32_t base;
    int8_t sign;
} Era;

// A calendar's eras: the first for years from start, the second before;
// the rest are aliases accepted in input.
typedef struct Eras
{
    Era eras[4];
    uint8_t count;
    int32_t start;
} Eras;

static const Era s_ce = {"ce", 0, 1};
static const Era s_bce = {"bce", 1, -1};

static Eras ErasOf(mlocCalendar calendar)
{
    switch (calendar)
    {
    case mloc_calendarGregorian:
    case mloc_calendarJapanese:
        return (Eras){{s_ce, s_bce, {"ad", 0, 1}, {"bc", 1, -1}}, 4, 1};
    case mloc_calendarBuddhist:
        return (Eras){{{"be", 0, 1}}, 1, INT32_MIN};
    case mloc_calendarRoc:
        return (Eras){{{"roc", 0, 1}, {"broc", 1, -1}}, 2, 1};
    case mloc_calendarCoptic:
    case mloc_calendarHebrew:
        return (Eras){{{"am", 0, 1}}, 1, INT32_MIN};
    case mloc_calendarEthiopic:
        return (Eras){{{"am", 0, 1}, {"aa", -5500, 1}}, 2, 1};
    case mloc_calendarEthiopicAmeteAlem:
        return (Eras){{{"aa", 0, 1}}, 1, INT32_MIN};
    case mloc_calendarIndian:
        return (Eras){{{"shaka", 0, 1}}, 1, INT32_MIN};
    case mloc_calendarPersian:
        return (Eras){{{"ap", 0, 1}}, 1, INT32_MIN};
    case mloc_calendarIslamicCivil:
    case mloc_calendarIslamicTbla:
    case mloc_calendarIslamicUmalqura:
    case mloc_calendarIslamic:
    case mloc_calendarIslamicRgsa:
        return (Eras){{{"ah", 0, 1}, {"bh", 1, -1}}, 2, 1};
    default:
        return (Eras){0};
    }
}

// The year an era and a year in it name; false for an era not of the
// calendar.
static bool YearInEra(mlocCalendar calendar, const char* era, size_t length, int64_t eraYear,
                      int64_t* yearOut)
{
    Eras eras = ErasOf(calendar);
    for (uint32_t i = 0; i < eras.count; i++)
    {
        const Era* e = &eras.eras[i];
        if (strlen(e->code) == length && memcmp(e->code, era, length) == 0)
        {
            *yearOut = e->base + e->sign * eraYear;
            return true;
        }
    }
    for (uint32_t i = 0; calendar == mloc_calendarJapanese && i < mlocJapaneseEraCount; i++)
    {
        const JapaneseEra* e = &mlocJapaneseEras[i];
        if (strlen(e->code) == length && memcmp(e->code, era, length) == 0)
        {
            *yearOut = e->year + eraYear - 1;
            return true;
        }
    }
    return false;
}

// The Japanese era a day falls in from Meiji 6, or null.
static const JapaneseEra* JapaneseEraOf(int32_t day)
{
    const JapaneseEra* found = nullptr;
    for (uint32_t i = 0; day >= japaneseGregorianDay && i < mlocJapaneseEraCount; i++)
    {
        if (mlocJapaneseEras[i].day <= day)
        {
            found = &mlocJapaneseEras[i];
        }
    }
    return found;
}

// A month code's number, 1 to 13, and whether it names a leap month;
// false when it is not of the form "Mnn" or "MnnL".
static bool ParseMonthCode(const char* code, size_t length, uint32_t* numberOut, bool* leapOut)
{
    if ((length != 3 && length != 4) || code[0] != 'M' || code[1] < '0' || code[1] > '9' ||
        code[2] < '0' || code[2] > '9' || (length == 4 && code[3] != 'L'))
    {
        return false;
    }
    *numberOut = (uint32_t)(code[1] - '0') * 10 + (uint32_t)(code[2] - '0');
    *leapOut = length == 4;
    return *numberOut >= 1 && *numberOut <= 13;
}

// The file that has a calendar's arithmetic.
typedef enum Source
{
    sourceArithmetic,
    sourceHebrew,
    sourceUmalqura,
    sourceChinese,
    sourceDangi,
} Source;

static Source SourceOf(mlocCalendar calendar)
{
    switch (calendar)
    {
    case mloc_calendarHebrew:
        return sourceHebrew;
    case mloc_calendarIslamicUmalqura:
    case mloc_calendarIslamic:
    case mloc_calendarIslamicRgsa:
        return sourceUmalqura;
    case mloc_calendarChinese:
        return sourceChinese;
    case mloc_calendarDangi:
        return sourceDangi;
    default:
        return sourceArithmetic;
    }
}

static bool Lunisolar(Source source)
{
    return source == sourceChinese || source == sourceDangi;
}

// A year of a calendar, with the Chinese and Dangi months computed once.
typedef struct Year
{
    mlocCalendar calendar;
    Source source;
    int64_t year;
    LunisolarYear lunar;
} Year;

static void OpenYear(Year* y, mlocCalendar calendar, int64_t year)
{
    y->calendar = calendar;
    y->source = SourceOf(calendar);
    y->year = year;
    if (Lunisolar(y->source))
    {
        mlocLunarYear(y->source == sourceDangi, year, &y->lunar);
    }
}

static uint32_t MonthsIn(const Year* y)
{
    switch (y->source)
    {
    case sourceHebrew:
        return mlocHebrewMonths(y->year);
    case sourceChinese:
    case sourceDangi:
        return y->lunar.count;
    default:
        return mlocArithmeticMonths(y->calendar);
    }
}

static uint32_t DaysIn(const Year* y, uint32_t month)
{
    switch (y->source)
    {
    case sourceHebrew:
        return mlocHebrewDaysInMonth(y->year, month);
    case sourceUmalqura:
        return mlocUmalquraDaysInMonth(y->year, month);
    case sourceChinese:
    case sourceDangi:
        return (uint32_t)(y->lunar.start[month] - y->lunar.start[month - 1]);
    default:
        return mlocArithmeticDaysInMonth(y->calendar, y->year, month);
    }
}

static int64_t DayOf(const Year* y, uint32_t month, uint32_t day)
{
    switch (y->source)
    {
    case sourceHebrew:
        return mlocHebrewToDay(y->year, month, day);
    case sourceUmalqura:
        return mlocUmalquraToDay(y->year, month, day);
    case sourceChinese:
    case sourceDangi:
        return (int64_t)y->lunar.start[month - 1] + day - 1;
    default:
        return mlocArithmeticToDay(y->calendar, y->year, month, day);
    }
}

static bool LeapYear(const Year* y)
{
    switch (y->source)
    {
    case sourceHebrew:
        return mlocHebrewLeap(y->year);
    case sourceUmalqura:
        return mlocUmalquraLeap(y->year);
    case sourceChinese:
    case sourceDangi:
        return y->lunar.count == 13;
    default:
        return mlocArithmeticLeap(y->calendar, y->year);
    }
}

// The leap month's place in the year, 0 for none: Hebrew's Adar I, the
// sixth month of a leap year, and the Chinese or Dangi leap month.
static uint32_t LeapMonth(const Year* y)
{
    if (y->source == sourceHebrew)
    {
        return mlocHebrewLeap(y->year) ? 6 : 0;
    }
    return Lunisolar(y->source) ? y->lunar.leap : 0;
}

static int64_t DaysInYear(const Year* y)
{
    if (Lunisolar(y->source))
    {
        return y->lunar.start[y->lunar.count] - y->lunar.start[0];
    }
    Year next;
    OpenYear(&next, y->calendar, y->year + 1);
    return DayOf(&next, 1, 1) - DayOf(y, 1, 1);
}

// The year holding a day, and the day's month and day in it.
static CalendarDate OpenYearOf(Year* y, mlocCalendar calendar, int32_t day)
{
    Source source = SourceOf(calendar);
    if (!Lunisolar(source))
    {
        CalendarDate d = source == sourceHebrew     ? mlocHebrewFromDay(day)
                         : source == sourceUmalqura ? mlocUmalquraFromDay(day)
                                                    : mlocArithmeticFromDay(calendar, day);
        OpenYear(y, calendar, d.year);
        return d;
    }
    y->calendar = calendar;
    y->source = source;
    mlocLunarYearOf(source == sourceDangi, day, &y->lunar);
    y->year = y->lunar.year;
    uint32_t month = 1;
    while (y->lunar.start[month] <= day)
    {
        month++;
    }
    return (CalendarDate){y->lunar.year, (uint8_t)month,
                          (uint8_t)(day - y->lunar.start[month - 1] + 1)};
}

// A month's code: its number, and whether it is the leap month, which
// takes the number of the month before it ('M05L' after 'M05'); the
// months after a leap month take the code of the place before theirs.
static void CodeOf(const Year* y, uint32_t month, uint32_t* numberOut, bool* leapOut)
{
    uint32_t leap = LeapMonth(y);
    *leapOut = leap != 0 && month == leap;
    *numberOut = leap != 0 && month >= leap ? month - 1 : month;
}

// Whether a code is one some year of the calendar has.
static bool CodeExists(const Year* y, uint32_t number, bool leap)
{
    switch (y->source)
    {
    case sourceHebrew:
        return number <= 12 && (!leap || number == 5);
    case sourceChinese:
    case sourceDangi:
        return number <= 12;
    default:
        return !leap && number <= mlocArithmeticMonths(y->calendar);
    }
}

// The month a code names in a year: mloc_success; for a leap month the
// year lacks, the month Temporal puts in its place (Hebrew's Adar,
// 'M06'; the Chinese month of the same number) or mloc_errorInvalid
// (reject); mloc_errorSyntax for a code no year of the calendar has.
static mlocResult MonthOfCode(const Year* y, uint32_t number, bool leap, mlocOverflow overflow,
                              uint32_t* monthOut)
{
    if (!CodeExists(y, number, leap))
    {
        return mloc_errorSyntax;
    }
    uint32_t leapMonth = LeapMonth(y);
    if (leap && leapMonth != 0 && number == leapMonth - 1)
    {
        *monthOut = leapMonth;
        return mloc_success;
    }
    if (leap && overflow == mloc_overflowReject)
    {
        return mloc_errorInvalid;
    }
    if (leap && y->source == sourceHebrew)
    {
        number++;
    }
    *monthOut = leapMonth != 0 && number >= leapMonth ? number + 1 : number;
    return mloc_success;
}

// The month's place the input names, by code, by place, or by both
// when they agree; 0 when it names none.
static mlocResult MonthOf(const Year* y, const mlocDateInput* input, mlocOverflow overflow,
                          uint32_t* monthOut)
{
    *monthOut = input->month;
    if (input->monthCode == nullptr)
    {
        return mloc_success;
    }
    uint32_t number = 0;
    bool leap = false;
    uint32_t coded = 0;
    if (!ParseMonthCode(input->monthCode, input->monthCodeLength, &number, &leap))
    {
        return mloc_errorSyntax;
    }
    mlocResult r = MonthOfCode(y, number, leap, overflow, &coded);
    if (r != mloc_success)
    {
        return r;
    }
    if (input->month != 0 && input->month != coded)
    {
        return mloc_errorInvalid;
    }
    *monthOut = coded;
    return mloc_success;
}

// The year an input names, checked; mloc_success or the refusal.
static mlocResult YearOfInput(mlocCalendar calendar, const mlocDateInput* input, int64_t* yearOut)
{
    *yearOut = input->year;
    if (input->era != nullptr &&
        !YearInEra(calendar, input->era, input->eraLength, input->year, yearOut))
    {
        return mloc_errorSyntax;
    }
    bool lunisolar = Lunisolar(SourceOf(calendar));
    if (*yearOut < (lunisolar ? lunisolarMinYear : -maxYear) ||
        *yearOut > (lunisolar ? lunisolarMaxYear : maxYear))
    {
        return mloc_errorInvalid;
    }
    return mloc_success;
}

mlocResult mlocMakeDate(mlocCalendar calendar, const mlocDateInput* input, mlocOverflow overflow,
                        mlocDate* dateOut)
{
    if (input == nullptr || dateOut == nullptr || (uint32_t)calendar >= calendarCount ||
        (uint32_t)overflow > mloc_overflowReject ||
        (input->era == nullptr && input->eraLength != 0) ||
        (input->monthCode == nullptr && input->monthCodeLength != 0))
    {
        return mloc_errorInvalid;
    }
    int64_t year = 0;
    mlocResult r = YearOfInput(calendar, input, &year);
    if (r != mloc_success)
    {
        return r;
    }
    Year y;
    OpenYear(&y, calendar, year);
    uint32_t months = MonthsIn(&y);
    uint32_t month = 0;
    r = MonthOf(&y, input, overflow, &month);
    if (r != mloc_success)
    {
        return r;
    }
    if (month == 0 || input->day == 0 || (overflow == mloc_overflowReject && month > months))
    {
        return mloc_errorInvalid;
    }
    month = month < months ? month : months;
    uint32_t last = DaysIn(&y, month);
    if (overflow == mloc_overflowReject && input->day > last)
    {
        return mloc_errorInvalid;
    }
    uint32_t day = input->day < last ? input->day : last;
    int64_t fixed = DayOf(&y, month, day);
    if (fixed < mlocDateMinDay || fixed > mlocDateMaxDay)
    {
        return mloc_errorInvalid;
    }
    *dateOut = (mlocDate){.day = (int32_t)fixed, .calendar = (uint8_t)calendar};
    return mloc_success;
}

mlocResult mlocMakeDateFromDay(mlocCalendar calendar, int32_t day, mlocDate* dateOut)
{
    if (dateOut == nullptr || (uint32_t)calendar >= calendarCount || day < mlocDateMinDay ||
        day > mlocDateMaxDay)
    {
        return mloc_errorInvalid;
    }
    *dateOut = (mlocDate){.day = day, .calendar = (uint8_t)calendar};
    return mloc_success;
}

// The era and the year in it of a date.
static void SetEra(mlocCalendar calendar, int32_t day, int32_t year, mlocDateFields* f)
{
    f->eraYear = year;
    Eras eras = ErasOf(calendar);
    if (eras.count > 0)
    {
        const Era* e = year >= eras.start || eras.count == 1 ? &eras.eras[0] : &eras.eras[1];
        f->eraYear = e->sign * (year - e->base);
        f->eraLength = (uint8_t)strlen(e->code);
        memcpy(f->era, e->code, f->eraLength);
    }
    const JapaneseEra* japanese = calendar == mloc_calendarJapanese ? JapaneseEraOf(day) : nullptr;
    if (japanese != nullptr)
    {
        f->eraYear = year - japanese->year + 1;
        f->eraLength = (uint8_t)strlen(japanese->code);
        memset(f->era, 0, sizeof(f->era));
        memcpy(f->era, japanese->code, f->eraLength);
    }
}

mlocResult mlocGetDateFields(const mlocDate* date, mlocDateFields* fieldsOut)
{
    if (date == nullptr || fieldsOut == nullptr || date->calendar >= calendarCount ||
        date->day < mlocDateMinDay || date->day > mlocDateMaxDay || date->reserved[0] != 0 ||
        date->reserved[1] != 0 || date->reserved[2] != 0)
    {
        return mloc_errorInvalid;
    }
    mlocCalendar calendar = (mlocCalendar)date->calendar;
    Year y;
    CalendarDate d = OpenYearOf(&y, calendar, date->day);
    mlocDateFields f = {
        .year = d.year,
        .month = d.month,
        .day = d.day,
        .monthsInYear = (uint8_t)MonthsIn(&y),
        .daysInMonth = (uint8_t)DaysIn(&y, d.month),
        .inLeapYear = LeapYear(&y) ? 1 : 0,
    };
    SetEra(calendar, date->day, d.year, &f);
    uint32_t number = 0;
    bool leapMonth = false;
    CodeOf(&y, d.month, &number, &leapMonth);
    f.monthCode[0] = 'M';
    f.monthCode[1] = (char)('0' + number / 10);
    f.monthCode[2] = (char)('0' + number % 10);
    f.monthCode[3] = leapMonth ? 'L' : '\0';
    f.monthCodeLength = leapMonth ? 4 : 3;
    // January 1 of year 1, day 1, was a Monday.
    int64_t weekday = ((int64_t)date->day - 1) % 7;
    f.dayOfWeek = (uint8_t)((weekday < 0 ? weekday + 7 : weekday) + 1);
    f.dayOfYear = (uint16_t)(date->day - DayOf(&y, 1, 1) + 1);
    f.daysInYear = (uint16_t)DaysInYear(&y);
    *fieldsOut = f;
    return mloc_success;
}
