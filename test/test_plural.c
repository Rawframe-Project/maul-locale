// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Plural rules on the compiled-in data: cardinal and ordinal categories in
// languages of every shape, the digits shown, compact numbers, fallback
// along the plurals chain, ranges and the categories a locale uses, and
// the refusals. conform_plurals checks every sample of CLDR's files in CI.

#include "test_harness.h"

#include "maul-locale/plural.h"

#include <stdint.h>
#include <string.h>

static mlocDataSet s_data;

static bool Rules(const char* id, mlocPluralType type, mlocPluralRules* rules)
{
    mlocLocale locale;
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocInitPluralRules(&s_data, &locale, type, rules) >= 0;
}

// Whether a number, as text, takes the category in a locale.
static bool Takes(const char* id, mlocPluralType type, const char* number,
                  mlocPluralCategory expected)
{
    mlocPluralRules rules;
    mlocDecimal decimal;
    mlocPluralCategory category = (mlocPluralCategory)-1;
    return Rules(id, type, &rules) &&
           mlocParseDecimal(number, strlen(number), &decimal).status == mloc_success &&
           mlocPluralRules_Select(&rules, &decimal, &category) == mloc_success &&
           category == expected;
}

#define CARDINAL(id, number, category) Takes(id, mloc_pluralCardinal, number, category)
#define ORDINAL(id, number, category)  Takes(id, mloc_pluralOrdinal, number, category)

static void TestCardinal(void)
{
    CHECK(CARDINAL("en", "1", mloc_pluralOne), "1 file");
    CHECK(CARDINAL("en", "1.0", mloc_pluralOther), "1.0 files: the digits shown count");
    CHECK(CARDINAL("en", "0", mloc_pluralOther), "0 files");
    CHECK(CARDINAL("fr", "0", mloc_pluralOne) && CARDINAL("fr", "1.5", mloc_pluralOne),
          "French one covers 0 to 1.x");
    CHECK(CARDINAL("fr", "1000000", mloc_pluralMany), "a million in French");
    CHECK(CARDINAL("ar", "0", mloc_pluralZero) && CARDINAL("ar", "1", mloc_pluralOne) &&
              CARDINAL("ar", "2", mloc_pluralTwo) && CARDINAL("ar", "3", mloc_pluralFew) &&
              CARDINAL("ar", "11", mloc_pluralMany) && CARDINAL("ar", "100", mloc_pluralOther),
          "Arabic's six");
    CHECK(CARDINAL("ru", "21", mloc_pluralOne) && CARDINAL("ru", "22", mloc_pluralFew) &&
              CARDINAL("ru", "25", mloc_pluralMany) && CARDINAL("ru", "11", mloc_pluralMany) &&
              CARDINAL("ru", "1.5", mloc_pluralOther),
          "Russian by the last digits");
    CHECK(CARDINAL("ja", "1", mloc_pluralOther), "Japanese has other alone");
    CHECK(CARDINAL("tr", "1", mloc_pluralOne) && CARDINAL("tr", "2", mloc_pluralOther), "Turkish");
    CHECK(CARDINAL("pt", "0", mloc_pluralOne) && CARDINAL("pt-PT", "0", mloc_pluralOther),
          "Portugal's rules are its own");
    CHECK(CARDINAL("en", "1000000000000000000001", mloc_pluralOther),
          "a wide integer equals no literal");
    CHECK(CARDINAL("ru", "1000000000000000000021", mloc_pluralOne), "and keeps its last digits");
}

// Whether an integer selects as its decimal does, in both types.
static bool IntegerAgrees(const mlocPluralRules* cardinal, const mlocPluralRules* ordinal,
                          int64_t value)
{
    mlocDecimal decimal;
    if (mlocMakeDecimal(value, &decimal) != mloc_success)
    {
        return false;
    }
    const mlocPluralRules* both[2] = {cardinal, ordinal};
    for (int i = 0; i < 2; i++)
    {
        mlocPluralCategory fromDecimal = (mlocPluralCategory)-1;
        mlocPluralCategory fromInteger = (mlocPluralCategory)-2;
        if (mlocPluralRules_Select(both[i], &decimal, &fromDecimal) != mloc_success ||
            mlocPluralRules_SelectInteger(both[i], value, &fromInteger) != mloc_success ||
            fromDecimal != fromInteger)
        {
            return false;
        }
    }
    return true;
}

static void TestInteger(void)
{
    static const char* const s_ids[] = {"en", "fr", "ru", "ar", "pl", "lv", "gd", "br", "cy", "it"};
    const int64_t e18 = 1000000000000000000;
    static const int64_t s_far[] = {INT64_MAX, INT64_MIN, INT64_MIN + 1, INT64_MAX - 1};
    for (size_t l = 0; l < sizeof(s_ids) / sizeof(s_ids[0]); l++)
    {
        mlocPluralRules cardinal;
        mlocPluralRules ordinal;
        bool agrees = Rules(s_ids[l], mloc_pluralCardinal, &cardinal) &&
                      Rules(s_ids[l], mloc_pluralOrdinal, &ordinal);
        for (int64_t n = -3000; n <= 3000 && agrees; n++)
        {
            agrees = IntegerAgrees(&cardinal, &ordinal, n);
        }
        for (int64_t k = -1200; k <= 1200 && agrees; k++)
        {
            agrees = IntegerAgrees(&cardinal, &ordinal, e18 + k) &&
                     IntegerAgrees(&cardinal, &ordinal, -e18 - k) &&
                     IntegerAgrees(&cardinal, &ordinal, 9 * e18 + k * 1000003);
        }
        for (size_t k = 0; k < sizeof(s_far) / sizeof(s_far[0]) && agrees; k++)
        {
            agrees = IntegerAgrees(&cardinal, &ordinal, s_far[k]);
        }
        CHECK(agrees, s_ids[l]);
    }
    mlocPluralRules rules;
    mlocPluralCategory category = (mlocPluralCategory)-1;
    CHECK(Rules("ru", mloc_pluralCardinal, &rules) &&
              mlocPluralRules_SelectInteger(&rules, -21, &category) == mloc_success &&
              category == mloc_pluralOne,
          "the sign is no operand");
}

static void TestOrdinal(void)
{
    CHECK(ORDINAL("en", "1", mloc_pluralOne) && ORDINAL("en", "2", mloc_pluralTwo) &&
              ORDINAL("en", "3", mloc_pluralFew) && ORDINAL("en", "4", mloc_pluralOther),
          "1st, 2nd, 3rd, 4th");
    CHECK(ORDINAL("en", "11", mloc_pluralOther) && ORDINAL("en", "21", mloc_pluralOne),
          "11th, 21st");
    CHECK(ORDINAL("tr", "1", mloc_pluralOther), "Turkish ordinals have other alone");
}

static void TestCompact(void)
{
    mlocPluralRules rules;
    mlocDecimal decimal;
    mlocPluralOperands operands;
    mlocPluralCategory category = mloc_pluralZero;
    CHECK(Rules("fr", mloc_pluralCardinal, &rules) &&
              mlocParseDecimal("1e6", 3, &decimal).status == mloc_success &&
              mlocGetPluralOperands(&decimal, &operands) == mloc_success,
          "French and 1c6");
    operands.c = 6;
    CHECK(mlocPluralRules_SelectOperands(&rules, &operands, &category) == mloc_success &&
              category == mloc_pluralMany,
          "1 M is many");
    CHECK(mlocParseDecimal("1e3", 3, &decimal).status == mloc_success &&
              mlocGetPluralOperands(&decimal, &operands) == mloc_success,
          "and 1c3");
    operands.c = 3;
    CHECK(mlocPluralRules_SelectOperands(&rules, &operands, &category) == mloc_success &&
              category == mloc_pluralOther,
          "1 k is other");
    mlocDecimal infinity = {.kind = mloc_decimalInfinity};
    CHECK(mlocPluralRules_Select(&rules, &infinity, &category) == mloc_success &&
              category == mloc_pluralOther,
          "infinity is other");
}

static void TestFallback(void)
{
    mlocLocale locale;
    mlocPluralRules rules;
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocInitPluralRules(&s_data, &locale, mloc_pluralCardinal, &rules) == mloc_success,
          "en has its own");
    CHECK(mlocParseLocale("en-GB", 5, &locale).status == mloc_success &&
              mlocInitPluralRules(&s_data, &locale, mloc_pluralCardinal, &rules) == mloc_fellBack,
          "en-GB falls back to en");
    CHECK(mlocParseLocale("sr-Latn", 7, &locale).status == mloc_success &&
              mlocInitPluralRules(&s_data, &locale, mloc_pluralCardinal, &rules) == mloc_fellBack,
          "sr-Latn to sr in the plurals component");
    CHECK(mlocParseLocale("qaa", 3, &locale).status == mloc_success &&
              mlocInitPluralRules(&s_data, &locale, mloc_pluralOrdinal, &rules) == mloc_fellBack,
          "a language CLDR does not name takes root's");
    CHECK(CARDINAL("qaa", "1", mloc_pluralOther), "which are other");
    mlocDataSet empty = {nullptr, 0};
    CHECK(mlocInitPluralRules(&empty, &locale, mloc_pluralCardinal, &rules) == mloc_errorNotFound,
          "data without plurals");
}

static bool Range(const char* id, mlocPluralCategory start, mlocPluralCategory end,
                  mlocPluralCategory expected)
{
    mlocPluralRules rules;
    mlocPluralCategory category = (mlocPluralCategory)-1;
    return Rules(id, mloc_pluralCardinal, &rules) &&
           mlocPluralRules_SelectRange(&rules, start, end, &category) == mloc_success &&
           category == expected;
}

static void TestRanges(void)
{
    CHECK(Range("en", mloc_pluralOne, mloc_pluralOther, mloc_pluralOther), "1-2 days");
    CHECK(Range("en", mloc_pluralOther, mloc_pluralOne, mloc_pluralOther), "0-1 days");
    CHECK(Range("de", mloc_pluralOther, mloc_pluralOne, mloc_pluralOne), "German takes one");
    CHECK(Range("fr", mloc_pluralOther, mloc_pluralOne, mloc_pluralOne),
          "a pair the table lacks takes the end's");
    CHECK(Range("qaa", mloc_pluralOne, mloc_pluralOther, mloc_pluralOther),
          "and so does a locale without a table");
    mlocPluralRules ordinal;
    mlocPluralCategory category;
    CHECK(Rules("en", mloc_pluralOrdinal, &ordinal) &&
              mlocPluralRules_SelectRange(&ordinal, mloc_pluralOne, mloc_pluralOther, &category) ==
                  mloc_errorInvalid,
          "ranges are cardinal");
    mlocPluralRules rules;
    CHECK(Rules("en", mloc_pluralCardinal, &rules) &&
              mlocPluralRules_SelectRange(&rules, (mlocPluralCategory)6, mloc_pluralOther,
                                          &category) == mloc_errorInvalid &&
              mlocPluralRules_SelectRange(&rules, mloc_pluralOne, (mlocPluralCategory)6,
                                          &category) == mloc_errorInvalid &&
              mlocPluralRules_SelectRange(&rules, mloc_pluralOne, mloc_pluralOne, nullptr) ==
                  mloc_errorInvalid,
          "another category, or no output");
}

static void TestCategories(void)
{
    mlocPluralRules rules;
    uint32_t categories = 0;
    CHECK(Rules("en", mloc_pluralCardinal, &rules) &&
              mlocPluralRules_GetCategories(&rules, &categories) == mloc_success &&
              categories == (1u << mloc_pluralOne | 1u << mloc_pluralOther),
          "English cardinals: one and other");
    CHECK(Rules("ar", mloc_pluralCardinal, &rules) &&
              mlocPluralRules_GetCategories(&rules, &categories) == mloc_success &&
              categories == 0x3F,
          "Arabic's: all six");
    CHECK(Rules("en", mloc_pluralOrdinal, &rules) &&
              mlocPluralRules_GetCategories(&rules, &categories) == mloc_success &&
              categories == (1u << mloc_pluralOne | 1u << mloc_pluralTwo | 1u << mloc_pluralFew |
                             1u << mloc_pluralOther),
          "English ordinals");
    CHECK(mlocPluralRules_GetCategories(&rules, nullptr) == mloc_errorInvalid &&
              mlocPluralRules_GetCategories(nullptr, &categories) == mloc_errorInvalid,
          "null pointers");
}

static void TestRefusals(void)
{
    mlocLocale locale;
    mlocPluralRules rules;
    mlocDecimal decimal = {0};
    mlocPluralCategory category;
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocParseDecimal("1", 1, &decimal).status == mloc_success,
          "a locale and a number");
    CHECK(mlocInitPluralRules(&s_data, &locale, mloc_pluralCardinal, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitPluralRules(nullptr, &locale, mloc_pluralCardinal, &rules) ==
                  mloc_errorInvalid &&
              mlocInitPluralRules(&s_data, nullptr, mloc_pluralCardinal, &rules) ==
                  mloc_errorInvalid &&
              mlocInitPluralRules(&s_data, &locale, (mlocPluralType)2, &rules) == mloc_errorInvalid,
          "init refusals");
    mlocPluralRules zero = {0};
    CHECK(mlocPluralRules_Select(&zero, &decimal, &category) == mloc_errorInvalid,
          "rules the library did not make");
    CHECK(Rules("en", mloc_pluralCardinal, &rules), "English rules");
    mlocPluralRules spoiled = rules;
    memset(&spoiled, 0xEE, 1);
    CHECK(mlocPluralRules_Select(&spoiled, &decimal, &category) == mloc_errorInvalid,
          "a rule index past the sets");
    spoiled = rules;
    ((uint8_t*)&spoiled)[1] = 1;
    CHECK(mlocPluralRules_Select(&spoiled, &decimal, &category) == mloc_errorInvalid,
          "a cardinal rule called ordinal");
    spoiled = rules;
    ((uint8_t*)&spoiled)[2] = 2;
    CHECK(mlocPluralRules_Select(&spoiled, &decimal, &category) == mloc_errorInvalid,
          "a range flag past 1");
    spoiled = rules;
    ((uint8_t*)&spoiled)[4] = 9;
    CHECK(mlocPluralRules_SelectRange(&spoiled, mloc_pluralZero, mloc_pluralZero, &category) ==
                  mloc_errorInvalid &&
              mlocPluralRules_SelectRange(&spoiled, mloc_pluralZero, mloc_pluralOne, &category) ==
                  mloc_success,
          "a range table entry with no category, where it is read");
    CHECK(mlocPluralRules_Select(&rules, nullptr, &category) == mloc_errorInvalid &&
              mlocPluralRules_Select(&rules, &decimal, nullptr) == mloc_errorInvalid &&
              mlocPluralRules_Select(nullptr, &decimal, &category) == mloc_errorInvalid,
          "select refusals");
    mlocDecimal madeUp = decimal;
    madeUp.negative = 2;
    CHECK(mlocPluralRules_Select(&rules, &madeUp, &category) == mloc_errorInvalid,
          "a made-up decimal");
    CHECK(mlocPluralRules_SelectInteger(&rules, 1, nullptr) == mloc_errorInvalid &&
              mlocPluralRules_SelectInteger(nullptr, 1, &category) == mloc_errorInvalid &&
              mlocPluralRules_SelectInteger(&zero, 1, &category) == mloc_errorInvalid &&
              mlocPluralRules_SelectInteger(&spoiled, 1, &category) == mloc_success,
          "integer refusals");
    mlocPluralOperands operands = {0};
    CHECK(mlocPluralRules_SelectOperands(&rules, nullptr, &category) == mloc_errorInvalid &&
              mlocPluralRules_SelectOperands(&rules, &operands, nullptr) == mloc_errorInvalid &&
              mlocPluralRules_SelectOperands(&zero, &operands, &category) == mloc_errorInvalid,
          "operand refusals");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    TestCardinal();
    TestOrdinal();
    TestInteger();
    TestCompact();
    TestFallback();
    TestRanges();
    TestCategories();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
