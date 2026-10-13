// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Speed of the locale, decimal and plural parts on the compiled-in data,
// in nanoseconds per call, the best of five runs: parsing, canonicalizing,
// adding and removing likely subtags, a parent, matching against 80
// supported locales (ICU's test list), regional preferences; a decimal
// parsed, made from a double and rounded; Russian plural rules made and
// selecting for an integer and a decimal; number, currency and unit
// formatters made and formatting (decimal, compact, percent, scientific),
// and numbers and amounts of money parsed. The inputs are the ones the
// ICU4C and ICU4X baselines time (record mloc-0004). Given a
// file of times in its own output's form, such as an earlier run's, it
// prints each time's ratio to the recorded one as well. With BENCH_SHOW
// set in the environment it first prints the texts the formatters write.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "maul-locale/currency.h"
#include "maul-locale/decimal.h"
#include "maul-locale/locale.h"
#include "maul-locale/number.h"
#include "maul-locale/plural.h"
#include "maul-locale/unit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// The unit names file, which bench/Local.cmake names from the source
// tree; relative to the working directory otherwise.
#ifndef MLOC_UNITS_FILE
#define MLOC_UNITS_FILE "src/generated/units.mloc"
#endif

static volatile int s_sink;
static mlocDataSet s_data;

static double Seconds(void)
{
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

static const char s_supported[] =
    "af,am,ar,az,be,bg,bn,bs,ca,cs,cy,da,de,el,en,en-GB,es,es-419,et,eu,fa,fi,fil,fr,ga,gl,"
    "gu,hi,hr,hu,hy,id,is,it,iw,ja,ka,kk,km,kn,ko,ky,lo,lt,lv,mk,ml,mn,mr,ms,my,ne,nl,no,pa,"
    "pl,pt,pt-PT,ro,ru,si,sk,sl,sq,sr,sr-Latn,sv,sw,ta,te,th,tr,uk,ur,uz,vi,zh-CN,zh-TW,zu";

static mlocLocale s_list[96];
static mlocMatchEntry s_entries[96];
static size_t s_count;
static mlocLocale s_desired[2];

static mlocLocale Parsed(const char* text)
{
    mlocLocale locale = {0};
    if (mlocParseLocale(text, strlen(text), &locale).status != mloc_success)
    {
        fprintf(stderr, "bench: cannot parse %s\n", text);
        exit(1);
    }
    return locale;
}

static void Prepare(void)
{
    char copy[sizeof(s_supported)];
    memcpy(copy, s_supported, sizeof(copy));
    for (char* at = copy; *at != '\0' && s_count < 96;)
    {
        size_t length = strcspn(at, ",");
        if (mlocCanonicalizeLocale(&s_data, at, length, &s_list[s_count], nullptr).status ==
            mloc_success)
        {
            s_count++;
        }
        at += length + (at[length] == ',');
    }
    s_desired[0] = Parsed("de-AT");
    s_desired[1] = Parsed("en-GB");
}

typedef void (*Body)(void);

static void ParseTag(void)
{
    mlocLocale locale;
    s_sink = mlocParseLocale("zh-Hant-TW-u-nu-hanidec", 23, &locale).status;
}

static void Canonicalize(void)
{
    mlocLocale locale;
    s_sink = mlocCanonicalizeLocale(&s_data, "iw-IL", 5, &locale, nullptr).status;
}

static void CanonicalizeHard(void)
{
    mlocLocale locale;
    s_sink = mlocCanonicalizeLocale(&s_data, "sh-SU-u-ms-imperial", 19, &locale, nullptr).status;
}

static mlocLocale s_zhTW;
static mlocLocale s_zhHantTW;
static mlocLocale s_esMX;
static mlocLocale s_enGB;

static void Maximize(void)
{
    mlocLocale out;
    s_sink = mlocMaximizeLocale(&s_data, &s_zhTW, &out);
}

static void Minimize(void)
{
    mlocLocale out;
    s_sink = mlocMinimizeLocale(&s_data, &s_zhHantTW, mloc_favorRegion, &out);
}

static void Parent(void)
{
    mlocLocale out;
    s_sink = mlocGetParentLocale(&s_data, mloc_componentMain, &s_esMX, &out);
}

static void PrepareMatch(void)
{
    s_sink = mlocPrepareMatch(&s_data, s_list, s_count, &s_list[0], s_entries);
}

static void Match(void)
{
    mlocMatch match;
    s_sink = mlocMatchLocales(&s_data, s_list, s_entries, s_count, s_desired, 2, nullptr, &match) +
             match.supported;
}

static void Preferences(void)
{
    mlocLocalePreferences p;
    s_sink = mlocGetLocalePreferences(&s_data, &s_enGB, &p) + p.firstDay;
}

static mlocLocale s_ru;
static mlocPluralRules s_ruRules;
static mlocDecimal s_value;
static volatile double s_double = 1234567.891;
static uint32_t s_n;

static void DecimalParse(void)
{
    mlocDecimal d;
    s_sink = mlocParseDecimal("1234567.891", 11, &d).status + d.count;
}

static void DecimalShortest(void)
{
    mlocDecimal d;
    s_sink = mlocMakeShortestDecimal(s_double, &d) + d.count;
}

static void DecimalRound(void)
{
    mlocDecimal d = s_value;
    s_sink = mlocRoundDecimal(&d, -2, 1, mloc_roundHalfEven) + d.count;
}

static void PluralInit(void)
{
    mlocPluralRules rules;
    s_sink = mlocInitPluralRules(&rules, &s_data, &s_ru, mloc_pluralCardinal);
}

static void PluralSelectInteger(void)
{
    s_n = (s_n + 7) % 1000;
    mlocPluralCategory category;
    s_sink = mlocSelectPluralInteger(&s_ruRules, s_n, &category) + (int)category;
}

static void PluralSelectDecimal(void)
{
    mlocPluralCategory category;
    s_sink = mlocSelectPlural(&s_ruRules, &s_value, &category) + (int)category;
}

// The number part: formatters of de, en, en's dollar and meter (long),
// the values they format and the texts they parse.
static mlocLocale s_de;
static mlocLocale s_en;
static mlocDataSet s_withUnits;
static mlocNumberFormatter s_deDecimal;
static mlocNumberFormatter s_enCompact;
static mlocNumberFormatter s_enPercent;
static mlocNumberFormatter s_enScientific;
static mlocCurrencyFormatter s_usd;
static mlocUnitFormatter s_meter;
static mlocDecimal s_million;
static mlocDecimal s_ratio;
static mlocDecimal s_money;
static mlocDecimal s_length;
static char s_text[128];

static void NumberInit(void)
{
    mlocNumberFormatter f;
    s_sink = mlocInitNumberFormatter(&f, &s_data, &s_de, nullptr);
}

static void Format(const mlocNumberFormatter* f, const mlocDecimal* d)
{
    mlocOutput output = {.buffer = s_text, .capacity = sizeof(s_text)};
    s_sink = mlocFormatNumber(f, d, &output) + (int)output.length;
}

static void NumberFormat(void)
{
    Format(&s_deDecimal, &s_value);
}

static void CompactFormat(void)
{
    Format(&s_enCompact, &s_million);
}

static void PercentFormat(void)
{
    Format(&s_enPercent, &s_ratio);
}

static void ScientificFormat(void)
{
    Format(&s_enScientific, &s_value);
}

static void CurrencyInit(void)
{
    mlocCurrencyFormatter f;
    s_sink = mlocInitCurrencyFormatter(&f, &s_data, &s_en, "USD", 3, nullptr);
}

static void CurrencyFormat(void)
{
    mlocOutput output = {.buffer = s_text, .capacity = sizeof(s_text)};
    s_sink = mlocFormatCurrency(&s_usd, &s_money, &output) + (int)output.length;
}

static const mlocUnitOptions s_long = {.width = mloc_unitWidthLong};

static void UnitInit(void)
{
    static mlocUnitFormatter f;
    s_sink = mlocInitUnitFormatter(&f, &s_withUnits, &s_en, "meter", 5, &s_long);
}

static void UnitFormat(void)
{
    mlocOutput output = {.buffer = s_text, .capacity = sizeof(s_text)};
    s_sink = mlocFormatUnit(&s_meter, &s_length, &output) + (int)output.length;
}

static void NumberParse(void)
{
    mlocDecimal d;
    size_t end = 0;
    s_sink =
        mlocParseNumber(&s_deDecimal, "1.234.567,891", 13, mloc_parseLenient, &d, &end) + (int)end;
}

static void CurrencyParse(void)
{
    mlocDecimal d;
    char code[3];
    size_t end = 0;
    s_sink = mlocParseCurrency(&s_usd, "$1,234,567.89", 13, mloc_parseLenient, 0, &d, code, &end) +
             (int)end;
}

static void CurrencyParseAny(void)
{
    mlocDecimal d;
    char code[3];
    size_t end = 0;
    s_sink = mlocParseCurrency(&s_usd, "$1,234,567.89", 13, mloc_parseLenient, 1, &d, code, &end) +
             (int)end;
}

static bool Number(const char* text, mlocDecimal* out)
{
    return mlocParseDecimal(text, strlen(text), out).status == mloc_success;
}

// The number part's formatters and values; false when one cannot be
// made (units.mloc missing).
static bool PrepareNumbers(void)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_units[8 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    FILE* file = fopen(MLOC_UNITS_FILE, "rb");
    size_t size = file == nullptr ? 0 : fread(s_units, 1, sizeof(s_units), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_withUnits = (mlocDataSet){s_sources, 2};
    s_de = Parsed("de");
    s_en = Parsed("en");
    mlocNumberOptions compact = {.notation = mloc_notationCompactShort};
    mlocNumberOptions percent = {.style = mloc_numberPercent};
    mlocNumberOptions scientific = {.notation = mloc_notationScientific};
    return size > 0 && size < sizeof(s_units) &&
           mlocOpenData(&s_sources[1], s_units, size) == mloc_success &&
           mlocInitNumberFormatter(&s_deDecimal, &s_data, &s_de, nullptr) == mloc_success &&
           mlocInitNumberFormatter(&s_enCompact, &s_data, &s_en, &compact) == mloc_success &&
           mlocInitNumberFormatter(&s_enPercent, &s_data, &s_en, &percent) == mloc_success &&
           mlocInitNumberFormatter(&s_enScientific, &s_data, &s_en, &scientific) == mloc_success &&
           mlocInitCurrencyFormatter(&s_usd, &s_data, &s_en, "USD", 3, nullptr) == mloc_success &&
           mlocInitUnitFormatter(&s_meter, &s_withUnits, &s_en, "meter", 5, &s_long) ==
               mloc_success &&
           Number("1234567", &s_million) && Number("0.256", &s_ratio) &&
           Number("1234567.89", &s_money) && Number("1234.5", &s_length);
}

typedef struct Case
{
    const char* name;
    Body body;
    long iterations;
} Case;

static double Time(const Case* c)
{
    for (long i = 0; i < c->iterations / 10; i++)
    {
        c->body();
    }
    double best = 1e30;
    for (int run = 0; run < 5; run++)
    {
        double start = Seconds();
        for (long i = 0; i < c->iterations; i++)
        {
            c->body();
        }
        double ns = (Seconds() - start) * 1e9 / (double)c->iterations;
        best = ns < best ? ns : best;
    }
    return best;
}

// The recorded time of a case, or 0.
static double Baseline(FILE* file, const char* name)
{
    if (file == nullptr)
    {
        return 0;
    }
    rewind(file);
    char line[256];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        size_t length = strlen(name);
        if (strncmp(line, name, length) == 0 && line[length] == ' ')
        {
            return atof(line + length);
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    Prepare();
    s_zhTW = Parsed("zh-TW");
    s_zhHantTW = Parsed("zh-Hant-TW");
    s_esMX = Parsed("es-MX");
    s_enGB = Parsed("en-GB");
    PrepareMatch();
    s_ru = Parsed("ru");
    if (mlocInitPluralRules(&s_ruRules, &s_data, &s_ru, mloc_pluralCardinal) != mloc_success ||
        mlocParseDecimal("1234567.891", 11, &s_value).status != mloc_success)
    {
        fprintf(stderr, "bench: no Russian rules\n");
        return 1;
    }
    if (!PrepareNumbers())
    {
        fprintf(stderr, "bench: cannot make the number formatters (%s)\n", MLOC_UNITS_FILE);
        return 1;
    }
    if (getenv("BENCH_SHOW") != nullptr)
    {
        void (*const show[])(void) = {NumberFormat,     CompactFormat,  PercentFormat,
                                      ScientificFormat, CurrencyFormat, UnitFormat};
        for (size_t i = 0; i < 6; i++)
        {
            memset(s_text, 0, sizeof(s_text));
            show[i]();
            printf("[%s]\n", s_text);
        }
    }
    FILE* baseline = argc > 1 ? fopen(argv[1], "r") : nullptr;
    static const Case s_cases[] = {
        {"parse", ParseTag, 2000000},
        {"canonicalize", Canonicalize, 2000000},
        {"canonicalize-hard", CanonicalizeHard, 1000000},
        {"maximize", Maximize, 5000000},
        {"minimize", Minimize, 2000000},
        {"parent", Parent, 2000000},
        {"match-prepare", PrepareMatch, 50000},
        {"match", Match, 500000},
        {"preferences", Preferences, 2000000},
        {"decimal-parse", DecimalParse, 5000000},
        {"decimal-shortest", DecimalShortest, 5000000},
        {"decimal-round", DecimalRound, 5000000},
        {"plural-init", PluralInit, 200000},
        {"plural-select-int", PluralSelectInteger, 10000000},
        {"plural-select-dec", PluralSelectDecimal, 10000000},
        {"number-init", NumberInit, 500000},
        {"number-format", NumberFormat, 2000000},
        {"compact-format", CompactFormat, 2000000},
        {"percent-format", PercentFormat, 2000000},
        {"scientific-format", ScientificFormat, 2000000},
        {"currency-init", CurrencyInit, 200000},
        {"currency-format", CurrencyFormat, 2000000},
        {"unit-init", UnitInit, 200000},
        {"unit-format", UnitFormat, 2000000},
        {"number-parse", NumberParse, 1000000},
        {"currency-parse", CurrencyParse, 500000},
        {"currency-parse-any", CurrencyParseAny, 100000},
    };
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        double ns = Time(&s_cases[i]);
        double was = Baseline(baseline, s_cases[i].name);
        if (was > 0)
        {
            printf("%-18s %10.1f ns  (%.2fx the baseline)\n", s_cases[i].name, ns, ns / was);
        }
        else
        {
            printf("%-18s %10.1f ns\n", s_cases[i].name, ns);
        }
    }
    if (baseline != nullptr)
    {
        fclose(baseline);
    }
    return 0;
}
