// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Regional preferences on the compiled-in data (UTS #35): the region's
// order of priority (rg, the region, sd, the likely one, 001), the keys
// that override (hc with c12 and c24, fw, ms, ca-iso8601), the hours of
// a language in a region, and the refusals. tools/check_preferences.py
// compares every region with CLDR's JSON in CI.

#include "test_harness.h"

#include "maul-locale/locale.h"

#include <string.h>

static mlocDataSet s_data;

static mlocLocalePreferences Of(const char* text)
{
    mlocLocale locale;
    mlocLocalePreferences p;
    memset(&p, 0xEE, sizeof(p));
    if (mlocCanonicalizeLocale(&s_data, text, strlen(text), &locale, nullptr).status !=
            mloc_success ||
        mlocGetLocalePreferences(&s_data, &locale, &p) != mloc_success)
    {
        memset(&p, 0xEE, sizeof(p));
    }
    return p;
}

static bool From(const char* text, const char* region)
{
    mlocLocalePreferences p = Of(text);
    char expected[4] = {0};
    memcpy(expected, region, strlen(region));
    return memcmp(p.region, expected, 4) == 0;
}

static void TestRegion(void)
{
    CHECK(From("en", "US"), "the likely region");
    CHECK(From("en-GB", "GB"), "the locale's region");
    CHECK(From("en-GB-u-rg-uszzzz", "US"), "the region override first");
    CHECK(From("en-u-sd-cabc", "CA"), "a subdivision's region after the locale's");
    CHECK(From("en-AU-u-sd-cabc", "AU"), "but not before it");
    CHECK(From("zxx", "001"), "001 without a region");
    CHECK(From("es-419", "419"), "a macro region as given");
    CHECK(From("en-u-rg-zz", "US"), "an override no subdivision is, left out");
    CHECK(From("en-u-rg-gbx", "GB"), "a subdivision of one character");
    CHECK(From("en-GB-u-rg-zzzzzz", "ZZ") && Of("en-GB-u-rg-zzzzzz").firstDay == 1 &&
              Of("en-GB-u-rg-zzzzzz").paperSize == mloc_paperA4,
          "the unknown region, which has no data and takes 001's");
    mlocLocalePreferences us = Of("en");
    CHECK(us.hourCycle == mloc_hourFormatH12 && us.firstDay == 7 && us.minimalDays == 1 &&
              us.weekendStart == 6 && us.weekendEnd == 7 &&
              us.measurementSystem == mloc_measurementUs && us.paperSize == mloc_paperUsLetter,
          "the US's values");
    mlocLocalePreferences gb = Of("en-GB");
    CHECK(gb.hourCycle == mloc_hourFormatH23 && gb.firstDay == 1 && gb.minimalDays == 4 &&
              gb.measurementSystem == mloc_measurementUk && gb.paperSize == mloc_paperA4,
          "Britain's");
    mlocLocalePreferences in = Of("hi-IN");
    CHECK(in.weekendStart == 7 && in.weekendEnd == 7, "a one-day weekend");
    CHECK(Of("qaa-YT").firstDay == 1, "a region without data takes 001's");
}

static void TestHours(void)
{
    mlocLocalePreferences ca = Of("ca-ES");
    mlocLocalePreferences es = Of("es-ES");
    CHECK(ca.allowedHourCount == 3 && ca.allowedHours[2] == mloc_hourFormatH12FlexibleDayPeriod,
          "Catalan in Spain has its own hours");
    CHECK(es.allowedHourCount == 4 && es.allowedHours[2] == mloc_hourFormatH12FlexibleDayPeriod &&
              es.allowedHours[3] == mloc_hourFormatH12DayPeriod,
          "and Spanish the region's");
    CHECK(Of("en-001").hourCycle == mloc_hourFormatH12, "a language in 001");
    CHECK(Of("de").hourCycle == mloc_hourFormatH23, "24 hours from 0");
    CHECK(Of("en-u-hc-h23").hourCycle == mloc_hourFormatH23, "hc overrides");
    CHECK(Of("de-u-hc-h11").hourCycle == mloc_hourFormatH11, "any cycle");
    CHECK(Of("de-u-hc-h24").hourCycle == mloc_hourFormatH24, "even h24");
    CHECK(Of("ja-u-hc-c12").hourCycle == mloc_hourFormatH11, "c12 takes K listed before h");
    CHECK(Of("en-u-hc-c12").hourCycle == mloc_hourFormatH12, "else h");
    CHECK(Of("en-u-hc-c24").hourCycle == mloc_hourFormatH23, "c24 takes H");
    CHECK(Of("hi-IN-u-hc-c12").hourCycle == mloc_hourFormatH12, "a day period counts as h");
    CHECK(Of("ru-u-hc-c12").hourCycle == mloc_hourFormatH12, "h where none of 12 is allowed");
    CHECK(Of("en-u-hc-h25").hourCycle == mloc_hourFormatH12, "a value hc does not take");
    mlocLocalePreferences en = Of("en");
    CHECK(en.allowedHourCount == 4 && en.allowedHours[0] == mloc_hourFormatH12 &&
              en.allowedHours[1] == mloc_hourFormatH12DayPeriod &&
              en.allowedHours[2] == mloc_hourFormatH23 &&
              en.allowedHours[3] == mloc_hourFormatH12FlexibleDayPeriod &&
              en.allowedHours[4] == 0 && en.allowedHours[5] == 0,
          "the allowed formats in order, the rest zero");
}

static void TestWeekAndMeasures(void)
{
    // UTS #35's own examples of the first day.
    CHECK(Of("en-AU-u-ca-iso8601-fw-tue-rg-afzzzz-sd-cabc").firstDay == 2, "fw first");
    CHECK(Of("en-AU-u-ca-iso8601-rg-afzzzz-sd-cabc").firstDay == 6, "then rg");
    CHECK(Of("en-AU-u-ca-iso8601-sd-cabc").firstDay == 1, "then the ISO calendar");
    CHECK(Of("en-u-sd-cabc").firstDay == 7, "then the region");
    CHECK(Of("en-US-u-ca-iso8601").minimalDays == 4, "ISO weeks need four days");
    CHECK(Of("en-US-u-ca-gregory").firstDay == 7, "another calendar does not count");
    CHECK(Of("en-u-fw-mon").firstDay == 1 && Of("en-u-fw-sun").firstDay == 7, "fw by name");
    CHECK(Of("en-u-fw-xyz").firstDay == 7, "a day fw does not take");
    CHECK(Of("en-u-ms-metric").measurementSystem == mloc_measurementMetric, "ms overrides");
    CHECK(Of("de-u-ms-ussystem").measurementSystem == mloc_measurementUs, "ussystem");
    CHECK(Of("de-u-ms-uksystem").measurementSystem == mloc_measurementUk, "uksystem");
    CHECK(Of("en-u-ms-imperial").measurementSystem == mloc_measurementUk,
          "its alias, canonicalized");
    CHECK(Of("de-u-ms-xyz").measurementSystem == mloc_measurementMetric,
          "a value it does not take");
    CHECK(Of("en-x-u-ms-metric").measurementSystem == mloc_measurementUs,
          "a u key inside private use is not one");
    CHECK(Of("en-u-ca-buddhist-ms-metric-x-y").measurementSystem == mloc_measurementMetric,
          "among other keys");
}

static void TestRefusals(void)
{
    mlocLocale locale;
    mlocLocalePreferences p;
    memset(&p, 0x55, sizeof(p));
    mlocLocalePreferences before = p;
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success, "a locale");
    CHECK(mlocGetLocalePreferences(nullptr, &locale, &p) == mloc_errorInvalid &&
              mlocGetLocalePreferences(&s_data, nullptr, &p) == mloc_errorInvalid &&
              mlocGetLocalePreferences(&s_data, &locale, nullptr) == mloc_errorInvalid,
          "null pointers");
    mlocDataSet empty = {nullptr, 0};
    CHECK(mlocGetLocalePreferences(&empty, &locale, &p) == mloc_errorNotFound,
          "data without preferences");
    CHECK(memcmp(&p, &before, sizeof(p)) == 0, "the output left unchanged");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    TestRegion();
    TestHours();
    TestWeekAndMeasures();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
