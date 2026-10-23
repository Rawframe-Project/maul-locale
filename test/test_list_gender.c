// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The gender of a list of persons (maul-locale/list.h): each rule on every
// list of up to three genders against UTS #35's definitions written out
// here, the locales by their rules, aliases, scripts and regions, and the
// refusals.

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/list.h"

#include <string.h>

static mlocDataSet s_data;

static mlocPersonGender Gender(const char* tag, const mlocPersonGender* genders, size_t count)
{
    mlocLocale locale;
    mlocPersonGender gender = (mlocPersonGender)99;
    if (mlocParseLocale(tag, strlen(tag), &locale).status != mloc_success ||
        mlocGetListGender(&s_data, &locale, genders, count, &gender) != mloc_success)
    {
        return (mlocPersonGender)99;
    }
    return gender;
}

// UTS #35's rules for two persons or more, written out; a list of none
// or one as ICU answers it.
static mlocPersonGender Expected(int rule, const mlocPersonGender* genders, size_t count)
{
    if (count == 0)
    {
        return mloc_personOther;
    }
    if (count == 1)
    {
        return genders[0];
    }
    size_t males = 0;
    size_t females = 0;
    for (size_t i = 0; i < count; i++)
    {
        males += genders[i] == mloc_personMale ? 1 : 0;
        females += genders[i] == mloc_personFemale ? 1 : 0;
    }
    if (rule == 0)
    {
        return mloc_personOther;
    }
    if (females == count)
    {
        return mloc_personFemale;
    }
    if (rule == 2)
    {
        return mloc_personMale;
    }
    return males == count ? mloc_personMale : mloc_personOther;
}

// Whether every list of up to three genders takes the rule's answer in
// the locale.
static bool RuleHolds(const char* tag, int rule)
{
    bool good = true;
    for (size_t count = 0; count <= 3; count++)
    {
        size_t lists = count == 0 ? 1 : count == 1 ? 3 : count == 2 ? 9 : 27;
        for (size_t n = 0; n < lists; n++)
        {
            mlocPersonGender genders[3];
            size_t value = n;
            for (size_t i = 0; i < count; i++)
            {
                genders[i] = (mlocPersonGender)(value % 3);
                value /= 3;
            }
            good = good && Gender(tag, genders, count) == Expected(rule, genders, count);
        }
    }
    return good;
}

static void TestRules(void)
{
    CHECK(RuleHolds("en", 0), "neutral: en");
    CHECK(RuleHolds("el", 1), "mixed neutral: el");
    CHECK(RuleHolds("fr", 2), "male taints: fr");
}

static void TestLocales(void)
{
    static const mlocPersonGender s_mixed[2] = {mloc_personMale, mloc_personFemale};
    static const char* const s_neutral[] = {"af", "de", "en-GB", "fil", "ja",  "ko", "no",
                                            "tr", "zu", "nb",    "xx",  "und", "tl"};
    static const char* const s_mixedNeutral[] = {"el", "is", "el-CY"};
    static const char* const s_maleTaints[] = {"ar",  "es-MX", "fr-CA",   "he",
                                               "lij", "pt-BR", "sr-Latn", "zh-Hant-TW",
                                               "zh",  "iw",    "mo",      "sh"};
    bool good = true;
    for (size_t i = 0; i < sizeof(s_neutral) / sizeof(s_neutral[0]); i++)
    {
        good = good && Gender(s_neutral[i], s_mixed, 2) == mloc_personOther;
    }
    CHECK(good, "neutral locales, unlisted ones and tl (fil) among them");
    good = true;
    for (size_t i = 0; i < sizeof(s_mixedNeutral) / sizeof(s_mixedNeutral[0]); i++)
    {
        good = good && Gender(s_mixedNeutral[i], s_mixed, 2) == mloc_personOther &&
               Gender(s_mixedNeutral[i], s_mixed, 1) == mloc_personMale;
    }
    CHECK(good, "mixed neutral locales");
    good = true;
    for (size_t i = 0; i < sizeof(s_maleTaints) / sizeof(s_maleTaints[0]); i++)
    {
        good = good && Gender(s_maleTaints[i], s_mixed, 2) == mloc_personMale;
    }
    CHECK(good, "male-taints locales, regions and scripts, iw, mo and sh by their aliases");
}

static void TestRefusals(void)
{
    mlocLocale locale;
    mlocPersonGender gender;
    mlocPersonGender bad[2] = {mloc_personMale, (mlocPersonGender)3};
    CHECK(mlocParseLocale("fr", 2, &locale).status == mloc_success &&
              mlocGetListGender(&s_data, &locale, bad, 2, &gender) == mloc_errorInvalid &&
              mlocGetListGender(nullptr, &locale, bad, 1, &gender) == mloc_errorInvalid &&
              mlocGetListGender(&s_data, nullptr, bad, 1, &gender) == mloc_errorInvalid &&
              mlocGetListGender(&s_data, &locale, nullptr, 1, &gender) == mloc_errorInvalid &&
              mlocGetListGender(&s_data, &locale, bad, 1, nullptr) == mloc_errorInvalid &&
              mlocGetListGender(&s_data, &locale, nullptr, 0, &gender) == mloc_success &&
              gender == mloc_personOther,
          "a gender past female, null pointers; an empty list");
}

int main(void)
{
    static mlocData source;
    mlocOpenBuiltinData(&source);
    s_data = (mlocDataSet){&source, 1};
    TestRules();
    TestLocales();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
