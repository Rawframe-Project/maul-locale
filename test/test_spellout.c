// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Spell-out formatting with the spell-out data file: cardinals, years,
// ordinals and ordinal suffixes, radix 20, fraction rule sets and plural
// blocks, the decimal comma's fraction rule, Roman and Hebrew numerals,
// NaN and the infinities, each expected text ICU4C 79's for the same
// input; then where the library is exact and ICU is not (long fractions,
// values past int64_t, plural blocks past 2^31), root's rules with
// mloc_fellBack, the rule sets listed, and the refusals. CLDR's
// testData/rbnf runs in CI (conform_spellout).

#include "test_harness.h"

#include "maul-locale/spellout.h"

#include <stdio.h>
#include <string.h>

#define SHY "\xC2\xAD"

static mlocDataSet s_builtin;
static mlocDataSet s_data;

static bool Locale(const char* id, mlocLocale* out)
{
    return mlocParseLocale(id, strlen(id), out).status == mloc_success;
}

static bool Decimal(const char* text, mlocDecimal* out)
{
    if (strcmp(text, "NaN") == 0 || strcmp(text, "Infinity") == 0 || strcmp(text, "-Infinity") == 0)
    {
        *out = (mlocDecimal){
            .kind = text[0] == 'N' ? mloc_decimalNaN : mloc_decimalInfinity,
            .negative = text[0] == '-' ? 1 : 0,
        };
        return true;
    }
    return mlocParseDecimal(text, strlen(text), out).status == mloc_success;
}

// Whether a number formats as expected by a locale's rule set, or with a
// leading '@' its numbering system, in a capitalization context.
static bool SpellsIn(const char* id, const char* ruleSet, mlocCapitalization capitalization,
                     const char* number, const char* expected)
{
    mlocLocale locale;
    mlocSpelloutFormatter formatter;
    mlocDecimal decimal;
    char buffer[512];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    mlocSpelloutOptions options = {.capitalization = capitalization};
    bool system = ruleSet != nullptr && ruleSet[0] == '@';
    bool same =
        Locale(id, &locale) &&
        (system ? mlocInitNumberingSystemFormatter(&formatter, &s_data, &locale, ruleSet + 1,
                                                   strlen(ruleSet + 1), &options)
                : mlocInitSpelloutFormatter(&formatter, &s_data, &locale, ruleSet,
                                            ruleSet != nullptr ? strlen(ruleSet) : 0, &options)) >=
            0 &&
        Decimal(number, &decimal) &&
        mlocFormatSpellout(&formatter, &decimal, &output) == mloc_success &&
        output.length == strlen(expected) && memcmp(buffer, expected, output.length) == 0;
    if (!same)
    {
        printf("%s %s %s: \"%.*s\"\n", id, ruleSet != nullptr ? ruleSet : "(default)", number,
               (int)output.length, buffer);
    }
    return same;
}

static bool Spells(const char* id, const char* ruleSet, const char* number, const char* expected)
{
    return SpellsIn(id, ruleSet, mloc_capitalizationNone, number, expected);
}

static void TestEnglish(void)
{
    CHECK(Spells("en", "%spellout-cardinal", "0", "zero") &&
              Spells("en", "%spellout-cardinal", "21", "twenty-one") &&
              Spells("en", "%spellout-cardinal", "101", "one hundred one") &&
              Spells("en", "%spellout-cardinal", "1999", "one thousand nine hundred ninety-nine") &&
              Spells("en", "%spellout-cardinal", "1000000", "one million"),
          "cardinals, the rollback of 101 and 200");
    CHECK(Spells("en", "%spellout-cardinal", "12345678901234567",
                 "twelve quadrillion three hundred forty-five trillion six hundred seventy-eight "
                 "billion nine hundred one million two hundred thirty-four thousand five hundred "
                 "sixty-seven"),
          "an integer of int64_t");
    CHECK(Spells("en", "%spellout-cardinal", "-5", "minus five") &&
              Spells("en", "%spellout-cardinal", "-0", "zero") &&
              Spells("en", "%spellout-cardinal", "2.3", "two point three") &&
              Spells("en", "%spellout-cardinal", "1.10", "one point one") &&
              Spells("en", "%spellout-cardinal", "0.05", "zero point zero five"),
          "signs, fractions digit by digit, the value not the digits shown");
    CHECK(Spells("en", "%spellout-numbering-year", "1999", "nineteen ninety-nine") &&
              Spells("en", "%spellout-numbering-year", "2005", "two thousand five") &&
              Spells("en", "%spellout-ordinal", "21", "twenty-first") &&
              Spells("en", "%spellout-ordinal", "100", "one hundredth") &&
              Spells("en", nullptr, "21", "twenty-one"),
          "years, ordinals and the default rule set");
    CHECK(Spells("en", "%digits-ordinal", "21", "21st") &&
              Spells("en", "%digits-ordinal", "112", "112th") &&
              Spells("en", "%digits-ordinal", "1003", "1,003rd") &&
              Spells("en", "%digits-ordinal", "-3",
                     "\xE2\x88\x92"
                     "3rd"),
          "ordinal suffixes: a pattern and a plural block of ordinal rules");
    CHECK(Spells("en", "%spellout-cardinal", "NaN", "not a number") &&
              Spells("en", "%spellout-cardinal", "Infinity", "infinite") &&
              Spells("en", "%spellout-cardinal", "-Infinity", "minus infinite"),
          "NaN and the infinities");
}

static void TestOthers(void)
{
    CHECK(Spells("fr", "%spellout-cardinal-masculine", "71", "soixante-et-onze") &&
              Spells("fr", "%spellout-cardinal-masculine", "80", "quatre-vingts") &&
              Spells("fr", "%spellout-cardinal-masculine", "81", "quatre-vingt-un") &&
              Spells("fr", "%spellout-cardinal-masculine", "91", "quatre-vingt-onze"),
          "French: radix 20 and \"|\"");
    CHECK(Spells("ru", "%spellout-cardinal-feminine", "1.5",
                 "\xD0\xBE\xD0\xB4\xD0\xBD\xD0\xB0 \xD1\x86\xD0\xB5\xD0\xBB\xD0\xB0\xD1\x8F "
                 "\xD0\xBF\xD1\x8F\xD1\x82\xD1\x8C "
                 "\xD0\xB4\xD0\xB5\xD1\x81\xD1\x8F\xD1\x82\xD1\x8B\xD1\x85") &&
              Spells("ru", "%spellout-cardinal-feminine", "2.3",
                     "\xD0\xB4\xD0\xB2\xD0\xB5 \xD1\x86\xD0\xB5\xD0\xBB\xD1\x8B\xD1\x85 "
                     "\xD1\x82\xD1\x80\xD0\xB8 "
                     "\xD0\xB4\xD0\xB5\xD1\x81\xD1\x8F\xD1\x82\xD1\x8B\xD1\x85") &&
              Spells("ru", "%spellout-cardinal-feminine", "0.123",
                     "\xD1\x81\xD1\x82\xD0\xBE \xD0\xB4\xD0\xB2\xD0\xB0\xD0\xB4\xD1\x86\xD0\xB0"
                     "\xD1\x82\xD1\x8C \xD1\x82\xD1\x80\xD0\xB8 \xD1\x82\xD1\x8B\xD1\x81\xD1\x8F"
                     "\xD1\x87\xD0\xBD\xD1\x8B\xD1\x85"),
          "Russian: fraction rule sets and plural blocks");
    // A fraction far below the point, whose places pass a decimal's
    // digits, reaches the fraction rule set as ICU's double does.
    CHECK(Spells("ru", "%spellout-cardinal-feminine", "1e-100",
                 "\xD0\xBD\xD0\xBE\xD0\xBB\xD1\x8C "
                 "\xD0\xB4\xD0\xB5\xD1\x81\xD1\x8F\xD1\x82\xD1\x8B\xD1\x85") &&
              Spells("ru", "%spellout-cardinal-feminine", "-1e-100",
                     "\xD0\xBC\xD0\xB8\xD0\xBD\xD1\x83\xD1\x81 "
                     "\xD0\xBD\xD0\xBE\xD0\xBB\xD1\x8C "
                     "\xD0\xB4\xD0\xB5\xD1\x81\xD1\x8F\xD1\x82\xD1\x8B\xD1\x85"),
          "a fraction of a hundred places");
    CHECK(Spells("es", "%spellout-numbering", "2.5", "dos coma cinco") &&
              Spells("es-MX", "%spellout-numbering", "2.5", "dos punto cinco"),
          "the fraction rule of the locale's decimal separator");
    CHECK(Spells("en", "%roman-upper", "1999", "MCMXCIX") &&
              Spells("en", "%roman-lower", "2024", "mmxxiv") &&
              Spells("en", "%hebrew", "5784",
                     "\xD7\x94\xD7\xB3\xD7\xAA\xD7\xA9\xD7\xA4\xD7\xB4\xD7\x93"),
          "numbering systems' rules");
    CHECK(Spells("ja", "%spellout-cardinal", "12345",
                 "\xE4\xB8\x80\xE4\xB8\x87\xE4\xBA\x8C\xE5\x8D\x83\xE4\xB8\x89\xE7\x99\xBE\xE5\x9B"
                 "\x9B\xE5\x8D\x81\xE4\xBA\x94") &&
              Spells("de", "%spellout-numbering", "1001", "ein" SHY "tausend" SHY "eins") &&
              Spells("de", "%spellout-numbering", "21", "ein" SHY "und" SHY "zwanzig"),
          "Japanese and German");
}

// Where ICU's int64_t or double loses the value and the library does not.
static void TestExact(void)
{
    CHECK(Spells("en", "%spellout-cardinal", "12345678901234567.5",
                 "twelve quadrillion three hundred forty-five trillion six hundred seventy-eight "
                 "billion nine hundred one million two hundred thirty-four thousand five hundred "
                 "sixty-seven point five") &&
              Spells("en", "%spellout-cardinal", "1.12345678901234567891",
                     "one point one two three four five six seven eight nine zero one two three "
                     "four five six seven eight nine one"),
          "fractions past a double's digits");
    // Digit by digit, ICU rounds to 20 places, half even.
    CHECK(Spells("en", "%spellout-cardinal", "1.123456789012345678905",
                 "one point one two three four five six seven eight nine zero one two three four "
                 "five six seven eight nine") &&
              Spells("en", "%spellout-cardinal", "1e-21", "zero point zero") &&
              Spells("en", "%spellout-cardinal", "2.5e-20",
                     "zero point zero zero zero zero zero zero zero zero zero zero zero zero zero "
                     "zero zero zero zero zero zero two") &&
              Spells("en", "%spellout-cardinal", "1.5e-20",
                     "zero point zero zero zero zero zero zero zero zero zero zero zero zero zero "
                     "zero zero zero zero zero zero two") &&
              Spells("en", "%spellout-cardinal", "1e-999999", "zero point zero"),
          "twenty places at most");
    CHECK(Spells("en", "%spellout-cardinal", "1e30", "1,000,000,000,000,000,000,000,000,000,000") &&
              Spells("en", "%digits-ordinal", "1e20", "100,000,000,000,000,000,000th") &&
              Spells("en", "%spellout-cardinal", "-9223372036854775808",
                     "minus 9,223,372,036,854,775,808"),
          "values past int64_t by the rules, a suffix kept");
    CHECK(Spells("af", "%digits-ordinal", "10000000000",
                 "10\xC2\xA0"
                 "000\xC2\xA0"
                 "000\xC2\xA0"
                 "000ste"),
          "a plural block's operand past 2^31 (ICU's wraps to int32_t)");
    // A pattern's digits in the locale's system, Adlam's of four bytes
    // each; its fraction digits rounded half even.
    CHECK(Spells("en-u-nu-adlm", "%spellout-cardinal", "1e6", "one million") &&
              Spells("en-u-nu-adlm", "%spellout-cardinal", "1e18",
                     "\xF0\x9E\xA5\x91,\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90,"
                     "\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90,"
                     "\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90,"
                     "\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90,"
                     "\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90,"
                     "\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90\xF0\x9E\xA5\x90") &&
              Spells("fr", "%spellout-ordinal-masculine", "1.25", "1,2") &&
              Spells("fr", "%spellout-ordinal-masculine", "1.35", "1,4"),
          "a pattern's digits and rounding");
}

static void TestFellBack(void)
{
    mlocLocale locale;
    mlocSpelloutFormatter formatter;
    CHECK(Locale("eu", &locale) &&
              mlocInitSpelloutFormatter(&formatter, &s_data, &locale, "%spellout-cardinal", 18,
                                        nullptr) == mloc_fellBack &&
              Spells("eu", "%spellout-cardinal", "1234", "1.234") &&
              mlocInitSpelloutFormatter(&formatter, &s_data, &locale, "%roman-upper", 12,
                                        nullptr) == mloc_success,
          "root's rules write digits with mloc_fellBack; numbering systems are root's alone");
}

static void TestRuleSets(void)
{
    mlocLocale en;
    size_t count = Locale("en", &en) ? mlocGetSpelloutRuleSetCount(&s_data, &en) : 0;
    bool cardinal = false;
    bool ordinal = false;
    bool all = count > 0;
    for (size_t i = 0; i < count; i++)
    {
        const char* name = nullptr;
        size_t length = 0;
        all = all && mlocGetSpelloutRuleSet(&s_data, &en, i, &name, &length) == mloc_success &&
              length > 1 && name[0] == '%' && name[1] != '%';
        cardinal |= length == 18 && memcmp(name, "%spellout-cardinal", 18) == 0;
        ordinal |= length == 15 && memcmp(name, "%digits-ordinal", 15) == 0;
    }
    const char* name = nullptr;
    size_t length = 0;
    CHECK(all && cardinal && ordinal, "en's public rule sets, both groups");
    CHECK(mlocGetSpelloutRuleSet(&s_data, &en, count, &name, &length) == mloc_errorNotFound &&
              mlocGetSpelloutRuleSet(&s_data, &en, 0, nullptr, &length) == mloc_errorInvalid &&
              mlocGetSpelloutRuleSetCount(&s_builtin, &en) == 0 &&
              mlocGetSpelloutRuleSetCount(nullptr, &en) == 0,
          "an index past the last, a null pointer, data without the rules");
}

static void Ignore(void* context, uint32_t part, size_t start, size_t length)
{
    (void)context;
    (void)part;
    (void)start;
    (void)length;
}

static void TestRefusals(void)
{
    mlocLocale en;
    mlocSpelloutFormatter formatter;
    CHECK(Locale("en", &en) &&
              mlocInitSpelloutFormatter(&formatter, &s_data, &en, "%nope", 5, nullptr) ==
                  mloc_errorNotFound &&
              mlocInitSpelloutFormatter(&formatter, &s_data, &en, "%%and", 5, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitSpelloutFormatter(&formatter, &s_data, &en, nullptr, 3, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitSpelloutFormatter(&formatter, &s_data, &en, "%x", 0, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitSpelloutFormatter(&formatter, &s_builtin, &en, nullptr, 0, nullptr) ==
                  mloc_errorNotFound &&
              mlocInitSpelloutFormatter(nullptr, &s_data, &en, nullptr, 0, nullptr) ==
                  mloc_errorInvalid,
          "an unknown or private rule set, a bad length, data without the rules, null");
    mlocDecimal decimal = {0};
    char small[4];
    mlocOutput output = {.buffer = small, .capacity = sizeof(small)};
    CHECK(mlocInitSpelloutFormatter(&formatter, &s_data, &en, "%spellout-cardinal", 18, nullptr) ==
                  mloc_success &&
              Decimal("21", &decimal) &&
              mlocFormatSpellout(&formatter, &decimal, &output) == mloc_errorCapacity &&
              output.length == 10,
          "a buffer too small gives the size needed");
    mlocOutput empty = {0};
    mlocOutput parts = {.buffer = small, .capacity = sizeof(small), .part = Ignore};
    CHECK(mlocFormatSpellout(&formatter, &decimal, &empty) == mloc_errorCapacity &&
              empty.length == 10 &&
              mlocFormatSpellout(&formatter, &decimal, &parts) == mloc_errorInvalid,
          "no buffer; an output asking for parts");
    mlocSpelloutFormatter forged;
    memset(&forged, 0, sizeof(forged));
    mlocDecimal bad = decimal;
    bad.digits[0] = 10;
    CHECK(mlocFormatSpellout(&forged, &decimal, &output) == mloc_errorInvalid &&
              mlocFormatSpellout(&formatter, &bad, &output) == mloc_errorInvalid &&
              mlocFormatSpellout(nullptr, &decimal, &output) == mloc_errorInvalid,
          "a formatter the library did not make, an invalid decimal");
    // A formatter whose symbols' text is gone (its first field): formatting
    // reads the symbols only for a pattern (10^18 and past) or a symbol,
    // parsing always.
    mlocSpelloutFormatter hollow = formatter;
    memset(&hollow, 0, sizeof(void*));
    mlocDecimal huge;
    mlocDecimal back;
    size_t end = 0;
    char text[256];
    mlocOutput words = {.buffer = text, .capacity = sizeof(text)};
    mlocOutput more = {.buffer = text, .capacity = sizeof(text)};
    CHECK(Decimal("1e18", &huge) && mlocFormatSpellout(&hollow, &decimal, &words) == mloc_success &&
              words.length == 10 &&
              mlocFormatSpellout(&hollow, &huge, &more) == mloc_errorInvalid &&
              mlocParseSpellout(&hollow, "one", 3, &back, &end) == mloc_errorInvalid,
          "a formatter without its symbols: refused where they are read");
}

// ICU4C 79's results with each context (the double path for NaN).
static void TestCapitalization(void)
{
    CHECK(
        SpellsIn("en", "%spellout-cardinal", mloc_capitalizationNone, "21", "twenty-one") &&
            SpellsIn("en", "%spellout-cardinal", mloc_capitalizationSentence, "21", "Twenty-one") &&
            SpellsIn("en", "%spellout-cardinal", mloc_capitalizationListOrMenu, "21",
                     "Twenty-one") &&
            SpellsIn("en", "%spellout-cardinal", mloc_capitalizationStandAlone, "-5",
                     "Minus five") &&
            SpellsIn("en-GB", "%spellout-ordinal", mloc_capitalizationListOrMenu, "3", "Third") &&
            SpellsIn("en", "%spellout-cardinal", mloc_capitalizationSentence, "NaN",
                     "Not a number"),
        "English titlecases in every context, en-GB by inheritance");
    CHECK(
        SpellsIn("de", nullptr, mloc_capitalizationSentence, "21", "Ein" SHY "und" SHY "zwanzig") &&
            SpellsIn("de", nullptr, mloc_capitalizationListOrMenu, "21",
                     "ein" SHY "und" SHY "zwanzig") &&
            SpellsIn("fr", nullptr, mloc_capitalizationStandAlone, "21", "vingt-et-un"),
        "other languages only at the beginning of a sentence");
    CHECK(
        SpellsIn("tr", "%spellout-cardinal", mloc_capitalizationSentence, "2", "\xC4\xB0ki") &&
            SpellsIn("tr", "%spellout-cardinal", mloc_capitalizationSentence, "3",
                     "\xC3\x9C\xC3\xA7") &&
            SpellsIn("az", "%spellout-cardinal", mloc_capitalizationSentence, "2", "\xC4\xB0ki") &&
            SpellsIn("ru", nullptr, mloc_capitalizationSentence, "NaN",
                     "\xD0\x9D\xD0\xB5\xC2\xA0\xD1\x87\xD0\xB8\xD1\x81\xD0\xBB\xD0\xBE"),
        "the Turkic dotted I, a symbol's first letter");
    CHECK(
        SpellsIn("en", "@romanlow", mloc_capitalizationSentence, "1999", "Mcmxcix") &&
            SpellsIn("en", "@armnlow", mloc_capitalizationSentence, "12", "\xD4\xBA\xD5\xA2") &&
            SpellsIn("en", "@greklow", mloc_capitalizationSentence, "12",
                     "\xCE\x99\xCE\xB2\xC2\xB4") &&
            SpellsIn("en", "@geor", mloc_capitalizationSentence, "12", "\xE1\x83\x98\xE1\x83\x91"),
        "lowercase numerals; Georgian has no titlecase");
    mlocLocale tr;
    mlocSpelloutFormatter formatter;
    mlocSpelloutOptions sentence = {.capitalization = mloc_capitalizationSentence};
    mlocDecimal two;
    char buffer[4];
    mlocOutput small = {.buffer = buffer, .capacity = 3};
    mlocOutput measure = {0};
    mlocOutput exact = {.buffer = buffer, .capacity = 4};
    CHECK(Locale("tr", &tr) &&
              mlocInitSpelloutFormatter(&formatter, &s_data, &tr, "%spellout-cardinal", 18,
                                        &sentence) == mloc_success &&
              Decimal("2", &two) &&
              mlocFormatSpellout(&formatter, &two, &small) == mloc_errorCapacity &&
              small.length == 4 &&
              mlocFormatSpellout(&formatter, &two, &measure) == mloc_errorCapacity &&
              measure.length == 4 && mlocFormatSpellout(&formatter, &two, &exact) == mloc_success &&
              exact.length == 4 && memcmp(buffer, "\xC4\xB0ki", 4) == 0,
          "a titlecase longer than its letter: the size needed, then the text");
    mlocSpelloutOptions bad = {.capitalization = (mlocCapitalization)4};
    CHECK(mlocInitSpelloutFormatter(&formatter, &s_data, &tr, nullptr, 0, &bad) ==
                  mloc_errorInvalid &&
              mlocInitNumberingSystemFormatter(&formatter, &s_data, &tr, "roman", 5, &bad) ==
                  mloc_errorInvalid,
          "a capitalization out of range");
}

// ICU4C 79's NumberFormat with -u-nu of each system.
static void TestNumberingSystems(void)
{
    CHECK(Spells("en", "@roman", "1999", "MCMXCIX") &&
              Spells("fr", "@hans", "123",
                     "\xE4\xB8\x80\xE7\x99\xBE\xE4\xBA\x8C\xE5\x8D\x81"
                     "\xE4\xB8\x89") &&
              Spells("fr", "@hans", "1.5", "\xE4\xB8\x80\xE7\x82\xB9\xE4\xBA\x94") &&
              Spells("en", "@hant", "12", "\xE5\x8D\x81\xE4\xBA\x8C") &&
              Spells("en", "@jpanyear", "2026", "2026") &&
              Spells("en", "@jpanyear", "1", "\xE5\x85\x83") &&
              Spells("en", "@hebr", "5786",
                     "\xD7\x94\xD7\xB3\xD7\xAA\xD7\xA9\xD7\xA4\xD7\xB4\xD7\x95"),
          "root's systems by the locale asked for, the others by the locale they name");
    mlocLocale en;
    mlocSpelloutFormatter formatter;
    CHECK(Locale("en", &en) &&
              mlocInitNumberingSystemFormatter(&formatter, &s_data, &en, "latn", 4, nullptr) ==
                  mloc_errorNotFound &&
              mlocInitNumberingSystemFormatter(&formatter, &s_data, &en, "rom", 3, nullptr) ==
                  mloc_errorNotFound &&
              mlocInitNumberingSystemFormatter(&formatter, &s_builtin, &en, "roman", 5, nullptr) ==
                  mloc_errorNotFound &&
              mlocInitNumberingSystemFormatter(&formatter, &s_data, &en, nullptr, 0, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitNumberingSystemFormatter(nullptr, &s_data, &en, "roman", 5, nullptr) ==
                  mloc_errorInvalid,
          "a numeric system, an unknown id, data without the rules, null");
}

// Whether text reads back as a value (mlocFormatDecimal's text) ending at
// a byte, by a formatter of a locale's rule set or ('@') numbering system.
static bool Parses(const char* id, const char* ruleSet, const char* text, const char* expected,
                   size_t expectedEnd)
{
    mlocLocale locale;
    mlocSpelloutFormatter formatter;
    mlocDecimal decimal;
    size_t end = 0;
    char value[64] = "";
    mlocOutput output = {.buffer = value, .capacity = sizeof(value) - 1};
    bool system = ruleSet[0] == '@';
    bool same =
        Locale(id, &locale) &&
        (system ? mlocInitNumberingSystemFormatter(&formatter, &s_data, &locale, ruleSet + 1,
                                                   strlen(ruleSet + 1), nullptr)
                : mlocInitSpelloutFormatter(&formatter, &s_data, &locale, ruleSet, strlen(ruleSet),
                                            nullptr)) >= 0 &&
        mlocParseSpellout(&formatter, text, strlen(text), &decimal, &end) == mloc_success &&
        mlocFormatDecimal(&decimal, &output) == mloc_success && end == expectedEnd &&
        output.length == strlen(expected) && memcmp(value, expected, output.length) == 0;
    if (!same)
    {
        printf("%s %s \"%s\": %.*s, end %zu\n", id, ruleSet, text, (int)output.length, value, end);
    }
    return same;
}

// ICU4C 79's parse of each text (its value a double).
static void TestParse(void)
{
    CHECK(Parses("en", "%spellout-cardinal", "twenty-one", "21", 10) &&
              Parses("en", "%spellout-cardinal", "one thousand two hundred thirty-four", "1234",
                     36) &&
              Parses("en", "%spellout-cardinal", "one point five", "1.5", 14) &&
              Parses("en", "%spellout-cardinal", "zero point zero five", "0.05", 20) &&
              Parses("en", "%spellout-cardinal", "minus five", "-5", 10) &&
              Parses("en", "%spellout-cardinal", "minus zero", "0", 10) &&
              Parses("en", "%spellout-cardinal", "one billion", "1000000000", 11),
          "English cardinals, fractions, negatives; minus zero is zero");
    CHECK(Parses("en", "%spellout-cardinal", "twenty-one apples", "21", 10) &&
              Parses("en", "%spellout-cardinal", "twenty one", "2001", 10) &&
              Parses("en", "%spellout-cardinal", "one thousand thousand", "1000", 12) &&
              Parses("en", "%spellout-cardinal", "three quarters", "3", 5) &&
              Parses("en", "%spellout-cardinal", "one hundred and one", "101", 19) &&
              Parses("en", "%spellout-cardinal", "twenty-first", "21", 12),
          "every rule set of the group: a year, an ordinal, the verbose cardinal; text after");
    CHECK(Parses("en", "%spellout-cardinal", "not a number", "NaN", 12) &&
              Parses("en", "%spellout-cardinal", "infinite", "Infinity", 8) &&
              Parses("en", "%spellout-cardinal", "minus infinite", "-Infinity", 14),
          "NaN and the infinities by the rules");
    CHECK(Parses("en", "%spellout-cardinal", "infinite point five", "Infinity", 19) &&
              Parses("en", "%spellout-cardinal", "infinite thousand", "Infinity", 8) &&
              Parses("en", "%spellout-cardinal", "minus not a number", "NaN", 18) &&
              Parses("en", "%spellout-cardinal", "not a number thousand", "NaN", 12) &&
              Parses("en", "%spellout-cardinal", "minus infinite thousand", "-Infinity", 14) &&
              Parses("en", "%spellout-cardinal", "one thousand infinite", "1000", 12) &&
              Parses("en", "%spellout-cardinal", "three point one four", "3.14", 20),
          "values that are not finite composed as doubles compose them; fraction digits");
    CHECK(Parses("en", "%digits-ordinal", "21st", "21", 4) &&
              Parses("en", "%digits-ordinal", "1,234th", "1234", 7) &&
              Parses("en", "%digits-ordinal", "1234th", "1234", 6) &&
              Parses("en", "%digits-ordinal", "21nd", "21", 4) &&
              Parses("en", "%digits-ordinal",
                     "\xE2\x88\x92"
                     "5th",
                     "-5", 6),
          "ordinal suffixes: digits read strictly, any of the block's forms");
    CHECK(Parses("en", "@roman", "MCMXCIX", "1999", 7) &&
              Parses("en", "@roman", "mcmxcix", "1999", 7),
          "Roman numerals, upper and lower: every set of the group");
    CHECK(Parses("ru", "%spellout-cardinal-feminine",
                 "\xD0\xB4\xD0\xB2\xD0\xB0\xD0\xB4\xD1\x86\xD0\xB0\xD1\x82\xD1\x8C "
                 "\xD0\xBE\xD0\xB4\xD0\xBD\xD0\xB0 \xD1\x86\xD0\xB5\xD0\xBB\xD0\xB0\xD1\x8F "
                 "\xD0\xBF\xD1\x8F\xD1\x82\xD1\x8C "
                 "\xD0\xB4\xD0\xB5\xD1\x81\xD1\x8F\xD1\x82\xD1\x8B\xD1\x85",
                 "21.5", 60),
          "a Russian fraction rule set and its plural blocks");
    CHECK(Parses("fr", "%spellout-cardinal-masculine", "quatre-vingt-dix-sept", "97", 21) &&
              Parses("fr", "%spellout-cardinal-masculine", "vingt et un", "20", 5) &&
              Parses("de", "%spellout-cardinal-masculine", "einundzwanzig", "1", 3) &&
              Parses("de", "%spellout-cardinal-masculine", "eins Komma f\xC3\xBCnf", "1.5", 16),
          "the text exactly as the rules write it: no spaces for hyphens, German's soft hyphens");
    CHECK(Parses("pl", "%spellout-cardinal-masculine", "zero przecinek zero pi\xC4\x99\xC4\x87",
                 "0.05", 26) &&
              Parses("pl", "%spellout-cardinal-masculine", "jeden przecinek zero zero jeden",
                     "1.001", 31),
          "Polish numerators with their zeros, exact where ICU's double is not");
    // Lithuanian's 123456789012345678.75 nests past ICU's limit, which
    // keeps ICU's search busy for minutes.
    mlocLocale lt;
    mlocSpelloutFormatter words;
    mlocDecimal big;
    char text[512];
    mlocOutput written = {.buffer = text, .capacity = sizeof(text)};
    size_t stop = 99;
    CHECK(Locale("lt", &lt) &&
              mlocInitSpelloutFormatter(&words, &s_data, &lt, "%spellout-numbering", 19, nullptr) ==
                  mloc_success &&
              Decimal("123456789012345678.75", &big) &&
              mlocFormatSpellout(&words, &big, &written) == mloc_success &&
              mlocParseSpellout(&words, text, written.length, &big, &stop) == mloc_errorCapacity &&
              stop == 0,
          "a search past its steps");
    // A long Lithuanian number reads within the steps, its searches kept
    // over cheaper ones; capitalized, no rule starts where it does, which
    // ends the search at once.
    static const char s_long[] =
        "trys \xC5\xA1imtai \xC5\xA1"
        "e\xC5\xA1iasde\xC5\xA1imt \xC5\xA1"
        "e\xC5\xA1i milijardai penki \xC5\xA1imtai trys milijonai a\xC5\xA1tuoni "
        "\xC5\xA1imtai septyniasde\xC5\xA1imt penki t\xC5\xABkstan\xC4\x8Diai devyni "
        "\xC5\xA1imtai dvide\xC5\xA1imt penkios";
    static const char s_capital[] =
        "Vienas milijardas keturi \xC5\xA1imtai a\xC5\xA1tuoniasde\xC5\xA1imt trys milijonai "
        "a\xC5\xA1tuoni \xC5\xA1imtai trisde\xC5\xA1imt vienas t\xC5\xABkstantis devyni "
        "\xC5\xA1imtai \xC5\xA1"
        "e\xC5\xA1iasde\xC5\xA1imt";
    stop = 99;
    CHECK(Parses("lt", "%spellout-numbering", s_long, "366503875925", 163) &&
              mlocParseSpellout(&words, s_capital, sizeof(s_capital) - 1, &big, &stop) ==
                  mloc_errorSyntax &&
              stop == 0,
          "a long number within the steps, a text no rule starts ended at once");
    mlocLocale en;
    mlocSpelloutFormatter formatter;
    mlocDecimal decimal;
    size_t end = 99;
    CHECK(Locale("en", &en) &&
              mlocInitSpelloutFormatter(&formatter, &s_data, &en, nullptr, 0, nullptr) >= 0 &&
              mlocParseSpellout(&formatter, "Twenty-one", 10, &decimal, &end) == mloc_errorSyntax &&
              end == 0 &&
              mlocParseSpellout(&formatter, nullptr, 0, &decimal, &end) == mloc_errorSyntax &&
              mlocParseSpellout(&formatter, nullptr, 1, &decimal, &end) == mloc_errorInvalid &&
              mlocParseSpellout(&formatter, "one", 3, nullptr, &end) == mloc_errorInvalid &&
              mlocParseSpellout(&formatter, "one", 3, &decimal, nullptr) == mloc_errorInvalid &&
              mlocParseSpellout(nullptr, "one", 3, &decimal, &end) == mloc_errorInvalid,
          "nothing read, empty text, null pointers");
}

static bool OpenData(void)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_rules[4 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    s_builtin = (mlocDataSet){s_sources, 1};
    FILE* file = fopen(MLOC_SPELLOUT_FILE, "rb");
    size_t size = file == nullptr ? 0 : fread(s_rules, 1, sizeof(s_rules), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_data = (mlocDataSet){s_sources, 2};
    return size > 0 && size < sizeof(s_rules) &&
           mlocOpenData(&s_sources[1], s_rules, size) == mloc_success;
}

int main(void)
{
    if (!OpenData())
    {
        fputs("test_spellout: cannot open " MLOC_SPELLOUT_FILE "\n", stderr);
        return 2;
    }
    TestEnglish();
    TestOthers();
    TestExact();
    TestFellBack();
    TestRuleSets();
    TestRefusals();
    TestCapitalization();
    TestNumberingSystems();
    TestParse();
    return s_failures == 0 ? 0 : 1;
}
