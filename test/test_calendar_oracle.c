// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Every day's fields against ICU4X 2.3 (record mloc-0040): each line
// "day year era eraYear monthCode day inLeapYear daysInYear daysInMonth"
// as the oracle program prints it (era "-" for none), hashed by FNV-1a
// over every day of the span. The hashes were taken from ICU4X's output
// by that program, kept in the docs as icu4x_calendar_fields.rs; a
// mismatch is found day by day by running it.

#include "test_harness.h"

#include "maul-locale/calendar.h"

#include <stdio.h>

typedef struct OracleSpan
{
    mlocCalendar calendar;
    const char* name;
    int32_t first;
    int32_t last;
    uint64_t hash;
} OracleSpan;

// 1600 to 2400 (days 584023 to 876582); Chinese and Dangi 1900 to 2099,
// where ICU4X follows the observatories rather than a mean motion.
static const OracleSpan s_spans[] = {
    {mloc_calendarGregorian, "gregory", 584023, 876582, UINT64_C(0x2b3ce75e398d4d8e)},
    {mloc_calendarBuddhist, "buddhist", 584023, 876582, UINT64_C(0xcaedd2af386513ae)},
    {mloc_calendarRoc, "roc", 584023, 876582, UINT64_C(0xd73d72687e50508f)},
    {mloc_calendarJapanese, "japanese", 584023, 876582, UINT64_C(0xd60459af5afc7e62)},
    {mloc_calendarCoptic, "coptic", 584023, 876582, UINT64_C(0x3b3f9847404153bb)},
    {mloc_calendarEthiopic, "ethiopic", 584023, 876582, UINT64_C(0x5ad9ffa2c98f4b8f)},
    {mloc_calendarEthiopicAmeteAlem, "ethioaa", 584023, 876582, UINT64_C(0xeb7c07634f956dcb)},
    {mloc_calendarIndian, "indian", 584023, 876582, UINT64_C(0x7c1449fb472d06c1)},
    {mloc_calendarPersian, "persian", 584023, 876582, UINT64_C(0x70f232ca5c98d560)},
    {mloc_calendarHebrew, "hebrew", 584023, 876582, UINT64_C(0xc75d17c8f68c7f92)},
    {mloc_calendarIslamicCivil, "islamic-civil", 584023, 876582, UINT64_C(0x4100959558e58db5)},
    {mloc_calendarIslamicTbla, "islamic-tbla", 584023, 876582, UINT64_C(0xbf7221a0c4610b83)},
    {mloc_calendarIslamicUmalqura, "islamic-umalqura", 584023, 876582,
     UINT64_C(0xa4e0c2161cec8208)},
    {mloc_calendarChinese, "chinese", 693596, 766644, UINT64_C(0x25a6ea2efe491e13)},
    {mloc_calendarDangi, "dangi", 693596, 766644, UINT64_C(0x7bda1c2fd12177c3)},
};

static uint64_t Hash(uint64_t h, const char* bytes, int count)
{
    for (int i = 0; i < count; i++)
    {
        h = (h ^ (uint8_t)bytes[i]) * UINT64_C(0x100000001b3);
    }
    return h;
}

static uint64_t SpanHash(const OracleSpan* s)
{
    uint64_t h = UINT64_C(0xcbf29ce484222325);
    for (int32_t day = s->first; day <= s->last; day++)
    {
        mlocDate date;
        mlocDateFields f;
        if (mlocMakeDateFromDay(s->calendar, day, &date) != mloc_success ||
            mlocGetDateFields(&date, &f) != mloc_success)
        {
            return 0;
        }
        char line[96];
        int n =
            snprintf(line, sizeof(line), "%d %d %.*s %d %.*s %d %d %d %d\n", (int)day, (int)f.year,
                     f.eraLength > 0 ? (int)f.eraLength : 1, f.eraLength > 0 ? f.era : "-",
                     (int)f.eraYear, (int)f.monthCodeLength, f.monthCode, (int)f.day,
                     (int)f.inLeapYear, (int)f.daysInYear, (int)f.daysInMonth);
        h = Hash(h, line, n);
    }
    return h;
}

int main(void)
{
    bool all = true;
    for (size_t i = 0; i < sizeof(s_spans) / sizeof(s_spans[0]); i++)
    {
        uint64_t h = SpanHash(&s_spans[i]);
        if (h != s_spans[i].hash)
        {
            printf("%s: %016llx\n", s_spans[i].name, (unsigned long long)h);
            all = false;
        }
    }
    CHECK(all, "every day's fields as ICU4X 2.3 gives them, 1600 to 2400 (Chinese and Dangi "
               "1900 to 2099)");
    return s_failures == 0 ? 0 : 1;
}
