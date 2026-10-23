// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// test262's Temporal calendar cases: every assertion its PlainDate tests
// make about a date made from fields or by adding durations or by changing
// fields, about the difference of two dates, or about a month and day's
// reference date, in test262_calendar.inc, checked against mlocMakeDate,
// mlocAddToDate, mlocDateUntil, mlocDateUntilRounded, mlocWithDate,
// mlocMakeMonthDay and mlocGetDateFields. The rows were recorded by
// running the tests on the library through a reader of their JavaScript,
// kept in the docs with the oracle programs. A row giving a year as well
// as an era and its year is refused when the two name different years, as
// Temporal's field resolution does; the library takes one or the other, so
// that check is the host's.

#include "test_harness.h"

#include "maul-locale/calendar.h"

#include <string.h>

static const char* const s_rows[] = {
#include "test262_calendar.inc"
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

typedef struct CalendarName
{
    const char* name;
    mlocCalendar calendar;
} CalendarName;

// iso8601 is the Gregorian calendar without eras; its rows check no era.
static const CalendarName s_calendars[] = {
    {"iso8601", mloc_calendarGregorian},
    {"gregory", mloc_calendarGregorian},
    {"buddhist", mloc_calendarBuddhist},
    {"roc", mloc_calendarRoc},
    {"japanese", mloc_calendarJapanese},
    {"coptic", mloc_calendarCoptic},
    {"ethiopic", mloc_calendarEthiopic},
    {"ethioaa", mloc_calendarEthiopicAmeteAlem},
    {"indian", mloc_calendarIndian},
    {"persian", mloc_calendarPersian},
    {"hebrew", mloc_calendarHebrew},
    {"islamic-civil", mloc_calendarIslamicCivil},
    {"islamic-tbla", mloc_calendarIslamicTbla},
    {"islamic-umalqura", mloc_calendarIslamicUmalqura},
    {"chinese", mloc_calendarChinese},
    {"dangi", mloc_calendarDangi},
};

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
    return count;
}

static bool Is(Token token, const char* text)
{
    return token.length == strlen(text) && memcmp(token.text, text, token.length) == 0;
}

// A signed decimal of the whole token.
static bool Number(Token token, int64_t* valueOut)
{
    bool negative = token.length > 1 && token.text[0] == '-';
    size_t i = negative ? 1 : 0;
    if (i == token.length || token.length - i > 12)
    {
        return false;
    }
    int64_t value = 0;
    for (; i < token.length; i += 1)
    {
        char c = token.text[i];
        if (c < '0' || c > '9')
        {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    *valueOut = negative ? -value : value;
    return true;
}

static bool CalendarOf(Token token, mlocCalendar* calendarOut)
{
    for (size_t i = 0; i < sizeof s_calendars / sizeof s_calendars[0]; i += 1)
    {
        if (Is(token, s_calendars[i].name))
        {
            *calendarOut = s_calendars[i].calendar;
            return true;
        }
    }
    return false;
}

typedef enum Made
{
    madeDate,
    madeDuration,
    madeRefused,
    madeBadRow,
} Made;

// Each row's date and how it was made, for the rows that use it.
static mlocDate s_dates[rowCount];
static Made s_made[rowCount];

// The date of an ISO origin: "iso year month day".
static Made MakeIso(mlocCalendar calendar, const Token* tokens, mlocDate* dateOut)
{
    int64_t year = 0;
    int64_t month = 0;
    int64_t day = 0;
    if (!Number(tokens[0], &year) || !Number(tokens[1], &month) || !Number(tokens[2], &day))
    {
        return madeBadRow;
    }
    mlocDateInput input = {.year = (int32_t)year, .month = (uint32_t)month, .day = (uint32_t)day};
    mlocDate iso = {0};
    if (mlocMakeDate(mloc_calendarGregorian, &input, mloc_overflowReject, &iso) != mloc_success)
    {
        return madeBadRow;
    }
    return mlocMakeDateFromDay(calendar, iso.day, dateOut) == mloc_success ? madeDate : madeBadRow;
}

// The date of fields: "era eraYear year monthCode month day".
static Made MakeFields(mlocCalendar calendar, mlocOverflow overflow, const Token* tokens,
                       mlocDate* dateOut)
{
    bool hasEra = !Is(tokens[0], "-");
    bool hasYear = !Is(tokens[2], "-");
    int64_t eraYear = 0;
    int64_t year = 0;
    int64_t month = 0;
    int64_t day = 0;
    if ((hasEra && !Number(tokens[1], &eraYear)) || (hasYear && !Number(tokens[2], &year)) ||
        (!hasEra && !hasYear) || !Number(tokens[4], &month) || !Number(tokens[5], &day))
    {
        return madeBadRow;
    }
    bool hasCode = !Is(tokens[3], "-");
    mlocDateInput input = {
        .era = hasEra ? tokens[0].text : nullptr,
        .eraLength = hasEra ? tokens[0].length : 0,
        .year = (int32_t)(hasEra ? eraYear : year),
        .monthCode = hasCode ? tokens[3].text : nullptr,
        .monthCodeLength = hasCode ? tokens[3].length : 0,
        .month = (uint32_t)month,
        .day = (uint32_t)day,
    };
    mlocDate date = {0};
    if (mlocMakeDate(calendar, &input, overflow, &date) != mloc_success)
    {
        return madeRefused;
    }
    if (hasEra && hasYear)
    {
        mlocDateFields fields = {0};
        if (mlocGetDateFields(&date, &fields) != mloc_success)
        {
            return madeBadRow;
        }
        if (fields.year != year)
        {
            return madeRefused;
        }
    }
    *dateOut = date;
    return madeDate;
}

static bool SameText(Token value, const char* text, size_t length)
{
    return value.length == length && memcmp(value.text, text, length) == 0;
}

// Checks one "key=value" against the fields; ry and rd are the date's
// year and day of the month in the ISO calendar.
static bool Expected(Token expectation, const mlocDateFields* f, const mlocDate* date)
{
    const char* eq = memchr(expectation.text, '=', expectation.length);
    if (eq == nullptr)
    {
        return false;
    }
    Token key = {expectation.text, (size_t)(eq - expectation.text)};
    Token value = {eq + 1, expectation.length - key.length - 1};
    bool none = Is(value, "-");
    if (Is(key, "era"))
    {
        return none ? f->eraLength == 0 : SameText(value, f->era, f->eraLength);
    }
    if (Is(key, "mc"))
    {
        return SameText(value, f->monthCode, f->monthCodeLength);
    }
    if (Is(key, "ey") && none)
    {
        return f->eraLength == 0;
    }
    int64_t n = 0;
    if (!Number(value, &n))
    {
        return false;
    }
    if (Is(key, "ey"))
    {
        return f->eraLength != 0 && f->eraYear == n;
    }
    if (Is(key, "ry") || Is(key, "rd"))
    {
        mlocDate iso = {0};
        mlocDateFields g = {0};
        return mlocMakeDateFromDay(mloc_calendarGregorian, date->day, &iso) == mloc_success &&
               mlocGetDateFields(&iso, &g) == mloc_success &&
               (Is(key, "ry") ? g.year == n : g.day == n);
    }
    const struct
    {
        const char* key;
        int64_t field;
    } numbers[] = {
        {"y", f->year},          {"m", f->month},          {"d", f->day},
        {"leap", f->inLeapYear}, {"miy", f->monthsInYear}, {"dim", f->daysInMonth},
        {"diy", f->daysInYear},  {"doy", f->dayOfYear},
    };
    for (size_t i = 0; i < sizeof numbers / sizeof numbers[0]; i += 1)
    {
        if (Is(key, numbers[i].key))
        {
            return numbers[i].field == n;
        }
    }
    return false;
}

// An earlier row's index, "@index", whose date was made.
static bool EarlierDate(Token token, size_t row, size_t* indexOut)
{
    int64_t index = 0;
    Token at = {token.text + 1, token.length - 1};
    if (token.length < 2 || token.text[0] != '@' || !Number(at, &index) || index < 0 ||
        (size_t)index >= row || s_made[index] != madeDate)
    {
        return false;
    }
    *indexOut = (size_t)index;
    return true;
}

static bool UnitOf(Token token, mlocDateUnit* unitOut)
{
    static const char* const s_units[] = {"year", "month", "week", "day"};
    for (int u = 0; u < 4; u++)
    {
        if (Is(token, s_units[u]))
        {
            *unitOut = (mlocDateUnit)u;
            return true;
        }
    }
    return false;
}

static bool ModeOf(Token token, mlocRoundingMode* modeOut)
{
    // In mlocRoundingMode's order.
    static const char* const s_modes[] = {"halfExpand", "halfEven",  "halfTrunc",
                                          "halfCeil",   "halfFloor", "ceil",
                                          "floor",      "expand",    "trunc"};
    for (int m = 0; m < 9; m++)
    {
        if (Is(token, s_modes[m]))
        {
            *modeOut = (mlocRoundingMode)m;
            return true;
        }
    }
    return false;
}

// A difference: "@index until @index largest smallest increment mode";
// one in days to an increment of 1 is mlocDateUntil's as well.
static Made MakeDifference(const Token* tokens, size_t row, mlocDateDuration* durationOut)
{
    size_t one = 0;
    size_t two = 0;
    int64_t increment = 0;
    mlocDateRounding rounding = {0};
    if (!EarlierDate(tokens[0], row, &one) || !Is(tokens[1], "until") ||
        !EarlierDate(tokens[2], row, &two) || !UnitOf(tokens[3], &rounding.largestUnit) ||
        !UnitOf(tokens[4], &rounding.smallestUnit) || !Number(tokens[5], &increment) ||
        increment < 0 || increment > UINT32_MAX || !ModeOf(tokens[6], &rounding.mode))
    {
        return madeBadRow;
    }
    rounding.increment = (uint32_t)increment;
    mlocResult r = mlocDateUntilRounded(&s_dates[one], &s_dates[two], &rounding, durationOut);
    if (r == mloc_success && rounding.smallestUnit == mloc_dateUnitDay && increment == 1)
    {
        mlocDateDuration plain = {0};
        if (mlocDateUntil(&s_dates[one], &s_dates[two], rounding.largestUnit, &plain) !=
                mloc_success ||
            plain.years != durationOut->years || plain.months != durationOut->months ||
            plain.weeks != durationOut->weeks || plain.days != durationOut->days)
        {
            return madeBadRow;
        }
    }
    return r == mloc_success ? madeDuration : madeRefused;
}

// Fields given by name: era, era year, year, month code, month and day,
// '-' for one not given and '~' for the empty string.
static bool Given(const Token* tokens, mlocDateChanges* out)
{
    static const uint32_t s_flags[6] = {mloc_givenEra,       mloc_givenEraYear, mloc_givenYear,
                                        mloc_givenMonthCode, mloc_givenMonth,   mloc_givenDay};
    *out = (mlocDateChanges){0};
    for (size_t i = 0; i < 6; i++)
    {
        if (Is(tokens[i], "-"))
        {
            continue;
        }
        out->given |= s_flags[i];
        bool empty = Is(tokens[i], "~");
        const char* text = tokens[i].text;
        size_t length = empty ? 0 : tokens[i].length;
        int64_t n = 0;
        if (i == 0 || i == 3)
        {
            *(i == 0 ? &out->era : &out->monthCode) = text;
            *(i == 0 ? &out->eraLength : &out->monthCodeLength) = length;
        }
        else if (!Number(tokens[i], &n) || n < INT32_MIN || n > (i >= 4 ? UINT32_MAX : INT32_MAX))
        {
            return false;
        }
        else if (i == 1 || i == 2)
        {
            *(i == 1 ? &out->eraYear : &out->year) = (int32_t)n;
        }
        else
        {
            *(i == 4 ? &out->month : &out->day) = (uint32_t)n;
        }
    }
    return true;
}

static bool OverflowOf(Token token, mlocOverflow* out)
{
    if (!Is(token, "c") && !Is(token, "r"))
    {
        return false;
    }
    *out = Is(token, "r") ? mloc_overflowReject : mloc_overflowConstrain;
    return true;
}

// A date with fields changed: "@index with fields overflow".
static Made MakeWith(const Token* tokens, size_t row, mlocDate* dateOut)
{
    size_t index = 0;
    mlocDateChanges changes;
    mlocOverflow overflow = mloc_overflowConstrain;
    if (!EarlierDate(tokens[0], row, &index) || !Is(tokens[1], "with") ||
        !Given(tokens + 2, &changes) || !OverflowOf(tokens[8], &overflow))
    {
        return madeBadRow;
    }
    return mlocWithDate(&s_dates[index], &changes, overflow, dateOut) == mloc_success ? madeDate
                                                                                      : madeRefused;
}

// A month and day's reference date: "monthday overflow fields".
static Made MakeMonthDay(mlocCalendar calendar, const Token* tokens, mlocDate* dateOut)
{
    mlocDateChanges fields;
    mlocOverflow overflow = mloc_overflowConstrain;
    if (!OverflowOf(tokens[0], &overflow) || !Given(tokens + 1, &fields))
    {
        return madeBadRow;
    }
    return mlocMakeMonthDay(calendar, &fields, overflow, dateOut) == mloc_success ? madeDate
                                                                                  : madeRefused;
}

// A date added to: "@index add years months weeks days overflow", the
// index that of an earlier row which made its date.
static Made MakeAdded(const Token* tokens, size_t row, mlocDate* dateOut)
{
    size_t index = 0;
    int64_t values[4] = {0};
    if (!EarlierDate(tokens[0], row, &index) || !Is(tokens[1], "add") ||
        !(Is(tokens[6], "c") || Is(tokens[6], "r")))
    {
        return madeBadRow;
    }
    for (size_t i = 0; i < 4; i += 1)
    {
        if (!Number(tokens[2 + i], &values[i]))
        {
            return madeBadRow;
        }
    }
    mlocDateDuration duration = {values[0], values[1], values[2], values[3]};
    mlocOverflow overflow = Is(tokens[6], "r") ? mloc_overflowReject : mloc_overflowConstrain;
    return mlocAddToDate(&s_dates[index], &duration, overflow, dateOut) == mloc_success
               ? madeDate
               : madeRefused;
}

// Makes a row's date: from fields, from an ISO date, or by adding to an
// earlier row's; or its duration, the difference of two earlier rows'
// dates. equalsOut receives the place of its '='.
static Made MakeRow(const Token* tokens, size_t count, size_t row, mlocDate* dateOut,
                    mlocDateDuration* durationOut, size_t* equalsOut)
{
    mlocCalendar calendar = mloc_calendarGregorian;
    if (count < 2 || !CalendarOf(tokens[0], &calendar))
    {
        return madeBadRow;
    }
    bool earlier = tokens[1].length > 0 && tokens[1].text[0] == '@';
    bool difference = earlier && count > 2 && Is(tokens[2], "until");
    bool with = earlier && count > 2 && Is(tokens[2], "with");
    bool monthDay = Is(tokens[1], "monthday");
    bool iso = !earlier && count > 2 && Is(tokens[2], "iso");
    size_t equals = with ? 10 : monthDay ? 9 : difference ? 8 : earlier ? 8 : iso ? 6 : 8;
    *equalsOut = equals;
    if (count <= equals || !Is(tokens[equals], "="))
    {
        return madeBadRow;
    }
    if (difference)
    {
        return MakeDifference(tokens + 1, row, durationOut);
    }
    if (with)
    {
        return MakeWith(tokens + 1, row, dateOut);
    }
    if (monthDay)
    {
        return MakeMonthDay(calendar, tokens + 2, dateOut);
    }
    if (earlier)
    {
        return MakeAdded(tokens + 1, row, dateOut);
    }
    if (!(Is(tokens[1], "c") || Is(tokens[1], "r")))
    {
        return madeBadRow;
    }
    mlocOverflow overflow = Is(tokens[1], "r") ? mloc_overflowReject : mloc_overflowConstrain;
    return iso ? MakeIso(calendar, tokens + 3, dateOut)
               : MakeFields(calendar, overflow, tokens + 2, dateOut);
}

// Checks one "key=value" against a duration.
static bool ExpectedDuration(Token expectation, const mlocDateDuration* d)
{
    const char* eq = memchr(expectation.text, '=', expectation.length);
    int64_t n = 0;
    if (eq == nullptr ||
        !Number((Token){eq + 1, expectation.length - (size_t)(eq + 1 - expectation.text)}, &n))
    {
        return false;
    }
    Token key = {expectation.text, (size_t)(eq - expectation.text)};
    return Is(key, "dy")   ? d->years == n
           : Is(key, "dm") ? d->months == n
           : Is(key, "dw") ? d->weeks == n
           : Is(key, "dd") ? d->days == n
                           : false;
}

static void CheckRow(size_t row, size_t* checksOut)
{
    const char* text = s_rows[row];
    Token tokens[maxTokens];
    size_t count = Split(text, tokens);
    size_t equals = 0;
    mlocDate date = {0};
    mlocDateDuration duration = {0};
    Made made = MakeRow(tokens, count, row, &date, &duration, &equals);
    s_made[row] = made;
    s_dates[row] = date;
    if (made == madeBadRow)
    {
        printf("bad row: %s\n", text);
        CHECK(false, "a row is well formed");
        return;
    }
    if (count == equals + 2 && Is(tokens[equals + 1], "refused"))
    {
        *checksOut += 1;
        if (made != madeRefused)
        {
            printf("made, test262 refuses: %s\n", text);
            CHECK(false, "test262's refusal");
        }
        return;
    }
    if (made == madeRefused)
    {
        printf("refused, test262 makes it: %s\n", text);
        CHECK(false, "test262's date");
        return;
    }
    mlocDateFields fields = {0};
    CHECK(made == madeDuration || mlocGetDateFields(&date, &fields) == mloc_success,
          "a made date's fields");
    for (size_t i = equals + 1; i < count; i += 1)
    {
        *checksOut += 1;
        if (made == madeDuration ? !ExpectedDuration(tokens[i], &duration)
                                 : !Expected(tokens[i], &fields, &date))
        {
            printf("%.*s differs: %s\n", (int)tokens[i].length, tokens[i].text, text);
            CHECK(false, "test262's field");
        }
    }
}

int main(void)
{
    size_t checks = 0;
    size_t rows = sizeof s_rows / sizeof s_rows[0];
    for (size_t i = 0; i < rows; i += 1)
    {
        CheckRow(i, &checks);
    }
    printf("%zu rows, %zu checks\n", rows, checks);
    CHECK(rows > 0 && checks >= rows, "the table is read");
    return s_failures == 0 ? 0 : 1;
}
