// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// test262's Temporal calendar cases (record mloc-0040): every assertion its
// PlainDate tests make about a date made from fields or by adding
// durations, in test262_calendar.inc, checked against mlocMakeDate,
// mlocAddToDate and mlocGetDateFields.
// The rows were recorded by running the tests on the library through a
// reader of their JavaScript, kept in the docs with the oracle programs.
// A row giving a year as well as an era and its year is refused when the
// two name different years, as Temporal's field resolution does; the
// library takes one or the other, so that check is the host's.

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
    madeRefused,
    madeBadRow,
} Made;

// Each row's date and how it was made, for the rows that add to it.
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

// Checks one "key=value" against the fields.
static bool Expected(Token expectation, const mlocDateFields* f)
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

// A date added to: "@index add years months weeks days overflow", the
// index that of an earlier row which made its date.
static Made MakeAdded(const Token* tokens, size_t row, mlocDate* dateOut)
{
    int64_t index = 0;
    int64_t values[4] = {0};
    Token at = {tokens[0].text + 1, tokens[0].length - 1};
    if (tokens[0].length < 2 || tokens[0].text[0] != '@' || !Number(at, &index) || index < 0 ||
        (size_t)index >= row || s_made[index] != madeDate || !Is(tokens[1], "add") ||
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
// earlier row's; equalsOut receives the place of its '='.
static Made MakeRow(const Token* tokens, size_t count, size_t row, mlocDate* dateOut,
                    size_t* equalsOut)
{
    mlocCalendar calendar = mloc_calendarGregorian;
    if (count < 2 || !CalendarOf(tokens[0], &calendar))
    {
        return madeBadRow;
    }
    bool added = tokens[1].length > 0 && tokens[1].text[0] == '@';
    bool iso = !added && count > 2 && Is(tokens[2], "iso");
    size_t equals = added ? 8 : iso ? 6 : 8;
    *equalsOut = equals;
    if (count <= equals || !Is(tokens[equals], "="))
    {
        return madeBadRow;
    }
    if (added)
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

static void CheckRow(size_t row, size_t* checksOut)
{
    const char* text = s_rows[row];
    Token tokens[maxTokens];
    size_t count = Split(text, tokens);
    size_t equals = 0;
    mlocDate date = {0};
    Made made = MakeRow(tokens, count, row, &date, &equals);
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
    CHECK(mlocGetDateFields(&date, &fields) == mloc_success, "a made date's fields");
    for (size_t i = equals + 1; i < count; i += 1)
    {
        *checksOut += 1;
        if (!Expected(tokens[i], &fields))
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
