// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Chinese and Dangi tables (record mloc-0040): every year from 1600
// to 2400 is the model's but for the overrides, and the days at the
// tables' ends belong to the same years whichever side computes them.

#include "calendar_arith.h"
#include "calendar_chinese.h"
#include "calendar_lunar.h"
#include "test_harness.h"

#include <string.h>

static bool SameYear(const LunisolarYear* a, const LunisolarYear* b)
{
    return a->year == b->year && a->count == b->count && a->leap == b->leap &&
           memcmp(a->start, b->start, sizeof(int32_t) * (a->count + 1u)) == 0;
}

// Whether tools/calendar-overrides.txt gives the year: the Qing years.
static bool Overridden(int64_t year)
{
    return year >= 1900 && year <= 1911;
}

static void TestTablesAreTheModel(void)
{
    bool all = true;
    int differing[2] = {0};
    for (int c = 0; c < 2; c++)
    {
        for (int64_t year = 1600; year <= 2400; year++)
        {
            LunisolarYear table;
            LunisolarYear model;
            mlocLunarYear(c == 1, year, &table);
            mlocChineseModelYear(c == 1, year, &model);
            bool same = SameYear(&table, &model);
            differing[c] += !same;
            all = all && (same || Overridden(year));
        }
    }
    // China's 1906; Korea's 1903, 1904, 1905, 1908 and 1911.
    CHECK(all && differing[0] == 1 && differing[1] == 5,
          "the tables are the model's years but for the Qing overrides");
}

static void TestEnds(void)
{
    bool all = true;
    for (int c = 0; c < 2; c++)
    {
        LunisolarYear first;
        LunisolarYear last;
        mlocLunarYear(c == 1, 1600, &first);
        mlocLunarYear(c == 1, 2400, &last);
        const int32_t days[] = {first.start[0] - 1, first.start[0], last.start[last.count] - 1,
                                last.start[last.count], last.start[last.count] + 200};
        for (size_t i = 0; i < sizeof(days) / sizeof(days[0]); i++)
        {
            LunisolarYear table;
            LunisolarYear model;
            mlocLunarYearOf(c == 1, days[i], &table);
            mlocChineseModelYearOf(c == 1, days[i], &model);
            all = all && SameYear(&table, &model) && table.start[0] <= days[i] &&
                  days[i] < table.start[table.count];
        }
    }
    CHECK(all, "the days at the tables' ends in the same years either way");
}

int main(void)
{
    TestTablesAreTheModel();
    TestEnds();
    return s_failures == 0 ? 0 : 1;
}
