// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Chinese and Dangi tables: every year from 1600 to 2400 is the
// model's but for the overrides, and the days at the tables' ends belong
// to the same years whichever side computes them. Months are numbered by
// their new moons alike in the tables and the model, and each number opens
// the year that holds its month.

#include "calendar_arith.h"
#include "calendar_chinese.h"
#include "calendar_lunar.h"
#include "test_harness.h"

#include <string.h>

static bool SameYear(const LunisolarYear* a, const LunisolarYear* b)
{
    return a->year == b->year && a->count == b->count && a->leap == b->leap &&
           a->lunation == b->lunation &&
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

// Every year's months follow the year before's, in the tables and over
// their ends, and the overrides keep the model's numbers.
static void TestLunations(void)
{
    bool all = true;
    for (int c = 0; c < 2; c++)
    {
        LunisolarYear before;
        mlocLunarYear(c == 1, 1599, &before);
        for (int64_t year = 1600; year <= 2401; year++)
        {
            LunisolarYear y;
            LunisolarYear model;
            mlocLunarYear(c == 1, year, &y);
            mlocChineseModelYear(c == 1, year, &model);
            all =
                all && y.lunation == before.lunation + before.count && y.lunation == model.lunation;
            before = y;
        }
    }
    CHECK(all, "months numbered on from year to year, as the model numbers them");
}

static void TestYearOfLunation(void)
{
    bool all = true;
    for (int c = 0; c < 2; c++)
    {
        // Every month of the tables and of the years at their ends.
        for (int64_t year = 1599; year <= 2401; year++)
        {
            LunisolarYear y;
            mlocLunarYear(c == 1, year, &y);
            for (uint32_t m = 0; m < y.count; m++)
            {
                LunisolarYear found;
                all = all && mlocLunarYearOfLunation(c == 1, (int64_t)y.lunation + m, &found) &&
                      SameYear(&found, &y);
            }
        }
        // The model's first and last months, and none past them.
        const int64_t years[] = {lunarMinYear, -2000, 1000, 3000, lunarMaxYear};
        for (size_t i = 0; i < sizeof(years) / sizeof(years[0]); i++)
        {
            LunisolarYear y;
            mlocLunarYear(c == 1, years[i], &y);
            for (uint32_t m = 0; m < y.count; m++)
            {
                LunisolarYear found;
                all = all && mlocLunarYearOfLunation(c == 1, (int64_t)y.lunation + m, &found) &&
                      SameYear(&found, &y);
            }
            all = all && (years[i] != lunarMinYear || y.lunation == lunarMinLunation) &&
                  (years[i] != lunarMaxYear || y.lunation + y.count - 1 == lunarMaxLunation);
        }
        LunisolarYear none;
        all = all && !mlocLunarYearOfLunation(c == 1, lunarMinLunation - 1, &none) &&
              !mlocLunarYearOfLunation(c == 1, lunarMaxLunation + 1, &none) &&
              !mlocLunarYearOfLunation(c == 1, INT64_MIN, &none) &&
              !mlocLunarYearOfLunation(c == 1, INT64_MAX, &none);
    }
    CHECK(all, "each new moon's number opens the year of its month, and none past the range");
}

int main(void)
{
    TestTablesAreTheModel();
    TestEnds();
    TestLunations();
    TestYearOfLunation();
    return s_failures == 0 ? 0 : 1;
}
