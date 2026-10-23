// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C baseline for Maul Locale: the number part's work, the
// same inputs as maul-locale's bench/bench_main.c. Best of five runs after
// a warm-up of a tenth, nanoseconds per call. ICU's NumberFormatter
// builds its formatter at the first format, so a formatter's making is
// timed with one format (number-init, currency-init, unit-init); the
// decimal formatter is made by unum_open.
#include <unicode/numberformatter.h>
#include <unicode/unum.h>
#include <unicode/ustring.h>
#include <cstdio>
#include <cstring>
#include <ctime>

using namespace icu;
using namespace icu::number;

static volatile int sink;

static double now()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

template <typename F> static void Time(const char* name, long iters, F body)
{
    for (long i = 0; i < iters / 10; ++i)
    {
        body();
    }
    double best = 1e30;
    for (int run = 0; run < 5; ++run)
    {
        double start = now();
        for (long i = 0; i < iters; ++i)
        {
            body();
        }
        double ns = (now() - start) / (double)iters;
        best = ns < best ? ns : best;
    }
    printf("%-18s %10.1f ns\n", name, best);
}

static UChar buf[128];

static void Show(const LocalizedNumberFormatter& f, const char* value)
{
    UErrorCode e = U_ZERO_ERROR;
    std::string s;
    f.formatDecimal(value, e).toString(e).toUTF8String(s);
    printf("  -> %s\n", s.c_str());
}

static LocalizedNumberFormatter Skeleton(const char* skeleton, const char* locale)
{
    UErrorCode e = U_ZERO_ERROR;
    return NumberFormatter::forSkeleton(UnicodeString(skeleton), e).locale(locale);
}

static void FormatOne(const LocalizedNumberFormatter& f, const char* value)
{
    UErrorCode e = U_ZERO_ERROR;
    FormattedNumber r = f.formatDecimal(value, e);
    sink = r.toTempString(e).extract(buf, 128, e);
}

int main()
{
    UErrorCode e = U_ZERO_ERROR;
    Time("number-init", 20000, [] {
        UErrorCode e = U_ZERO_ERROR;
        UNumberFormat* f = unum_open(UNUM_DECIMAL, nullptr, 0, "de", nullptr, &e);
        unum_close(f);
    });
    UNumberFormat* de = unum_open(UNUM_DECIMAL, nullptr, 0, "de", nullptr, &e);
    unum_setAttribute(de, UNUM_MAX_FRACTION_DIGITS, 3);
    Time("number-format", 2000000, [&] {
        UErrorCode e = U_ZERO_ERROR;
        sink = unum_formatDecimal(de, "1234567.891", -1, buf, 128, nullptr, &e);
    });
    LocalizedNumberFormatter compact = Skeleton("compact-short", "en");
    LocalizedNumberFormatter percent = Skeleton("percent scale/100", "en");
    LocalizedNumberFormatter scientific = Skeleton("scientific precision-unlimited", "en");
    LocalizedNumberFormatter usd = Skeleton("currency/USD", "en");
    LocalizedNumberFormatter meter = Skeleton("measure-unit/length-meter unit-width-full-name", "en");
    Time("compact-format", 2000000, [&] { FormatOne(compact, "1234567"); });
    Show(compact, "1234567");
    Time("percent-format", 2000000, [&] { FormatOne(percent, "0.256"); });
    Show(percent, "0.256");
    Time("scientific-format", 2000000, [&] { FormatOne(scientific, "1234567.891"); });
    Show(scientific, "1234567.891");
    Time("currency-init", 20000, [] { FormatOne(Skeleton("currency/USD", "en"), "1234567.89"); });
    Time("currency-format", 2000000, [&] { FormatOne(usd, "1234567.89"); });
    Show(usd, "1234567.89");
    Time("unit-init", 20000, [] {
        FormatOne(Skeleton("measure-unit/length-meter unit-width-full-name", "en"), "1234.5");
    });
    Time("unit-format", 2000000, [&] { FormatOne(meter, "1234.5"); });
    Show(meter, "1234.5");
    // Parsing: DecimalFormat's lenient parse, as the library's.
    UChar deText[32], usdText[32];
    u_uastrcpy(deText, "1.234.567,891");
    u_uastrcpy(usdText, "$1,234,567.89");
    UNumberFormat* money = unum_open(UNUM_CURRENCY, nullptr, 0, "en", nullptr, &e);
    UChar code[4];
    u_uastrcpy(code, "USD");
    unum_setTextAttribute(money, UNUM_CURRENCY_CODE, code, 3, &e);
    unum_setAttribute(de, UNUM_LENIENT_PARSE, 1);
    unum_setAttribute(money, UNUM_LENIENT_PARSE, 1);
    char out[64];
    Time("number-parse", 1000000, [&] {
        UErrorCode e = U_ZERO_ERROR;
        int32_t at = 0;
        sink = unum_parseDecimal(de, deText, 13, &at, out, 64, &e) + at;
    });
    printf("  -> %s\n", out);
    Time("currency-parse", 500000, [&] {
        UErrorCode e = U_ZERO_ERROR;
        int32_t at = 0;
        sink = unum_parseDecimal(money, usdText, 13, &at, out, 64, &e) + at;
    });
    printf("  -> %s\n", out);
    UChar read[4];
    double amount = 0;
    Time("currency-parse-any", 100000, [&] {
        UErrorCode e = U_ZERO_ERROR;
        int32_t at = 0;
        amount = unum_parseDoubleCurrency(money, usdText, 13, &at, read, &e);
        sink = at;
    });
    printf("  -> %.2f\n", amount);
    unum_close(money);
    unum_close(de);
    return 0;
}
