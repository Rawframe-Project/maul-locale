// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C baseline for Maul Locale: the message functions'
// work, the same messages and arguments as maul-locale's
// bench/bench_messages.c, in en with ICU's default bidi isolation, run
// with TZ=UTC. MF2 where ICU4C 79 has the function (:currency,
// :percent, :date, :datetime), MF1 where MF1 can say it (currency,
// percent, date, spellout, duration), and the formatter itself where
// neither can (relative time, lists). Best of five runs after a warm-up
// of a tenth, nanoseconds per call.
#include <unicode/curramt.h>
#include <unicode/listformatter.h>
#include <unicode/messageformat2.h>
#include <unicode/messageformat2_arguments.h>
#include <unicode/msgfmt.h>
#include <unicode/reldatefmt.h>
#include <cstdio>
#include <ctime>
#include <map>
#include <string>

using namespace icu;
namespace m2 = icu::message2;

static volatile size_t sink;

static double now()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

template <typename F> static double Time(long iters, F body)
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
    return best;
}

static void Print(const char* name, double built, double formatted, const UnicodeString& text)
{
    std::string utf8;
    text.toUTF8String(utf8);
    printf("%-16s build %9.1f ns  format %9.1f ns  [%s]\n", name, built, formatted, utf8.c_str());
}

// 15:04:05 UTC on 17 October 2026.
static const UDate kDate = 1792249445000.0;

static void Mf2(const char* name, const char* source, const m2::MessageArguments& arguments)
{
    UnicodeString pattern = UnicodeString::fromUTF8(source);
    auto build = [&] {
        UErrorCode s = U_ZERO_ERROR;
        UParseError pe;
        m2::MessageFormatter::Builder b(s);
        b.setLocale(Locale("en"));
        b.setPattern(pattern, pe, s);
        m2::MessageFormatter f = b.build(s);
        sink += (size_t)s;
    };
    UErrorCode s = U_ZERO_ERROR;
    UParseError pe;
    m2::MessageFormatter::Builder b(s);
    b.setLocale(Locale("en"));
    b.setPattern(pattern, pe, s);
    m2::MessageFormatter f = b.build(s);
    UnicodeString text;
    auto format = [&] {
        UErrorCode s2 = U_ZERO_ERROR;
        text = f.formatToString(arguments, s2);
        sink += (size_t)text.length();
    };
    double built = Time(20000, build);
    double formatted = Time(20000, format);
    Print(name, built, formatted, text);
}

static void Mf1(const char* name, const char16_t* source, const Formattable* args, int count)
{
    auto build = [&] {
        UErrorCode s = U_ZERO_ERROR;
        MessageFormat f(source, Locale("en"), s);
        sink += (size_t)s;
    };
    UErrorCode s = U_ZERO_ERROR;
    MessageFormat f(source, Locale("en"), s);
    UnicodeString text;
    auto format = [&] {
        text.remove();
        FieldPosition fp;
        UErrorCode s1 = U_ZERO_ERROR;
        f.format(args, count, text, fp, s1);
        sink += (size_t)text.length();
    };
    double built = Time(20000, build);
    double formatted = Time(200000, format);
    Print(name, built, formatted, text);
}

int main()
{
    UErrorCode status = U_ZERO_ERROR;
    std::map<UnicodeString, m2::Formattable> map;
    map[u"x"] = m2::Formattable(0.25);
    map[u"a"] = m2::Formattable(42.5);
    map[u"d"] = m2::Formattable(m2::DateInfo{kDate, UnicodeString(u"UTC")});
    m2::MessageArguments arguments(map, status);
    Mf2("mf2-currency", "Total: {$a :currency currency=EUR}", arguments);
    Mf2("mf2-percent", "{$x :percent} done", arguments);
    Mf2("mf2-date", "{$d :date}", arguments);
    Mf2("mf2-datetime", "{$d :datetime dateStyle=medium timeStyle=short}", arguments);

    Formattable currency[] = {Formattable(new CurrencyAmount(42.5, u"EUR", status))};
    Mf1("mf1-currency", u"Total: {0,number,currency}", currency, 1);
    Formattable quarter[] = {Formattable(0.25)};
    Mf1("mf1-percent", u"{0,number,percent} done", quarter, 1);
    Formattable date[] = {Formattable(kDate, Formattable::kIsDate)};
    Mf1("mf1-date", u"{0,date,short}", date, 1);
    Mf1("mf1-datetime", u"{0,date,medium}, {0,time,short}", date, 1);
    Formattable number[] = {Formattable((int64_t)1234)};
    Mf1("mf1-spellout", u"{0,spellout}", number, 1);
    Formattable seconds[] = {Formattable((int64_t)3723)};
    Mf1("mf1-duration", u"{0,duration}", seconds, 1);

    // The formatters themselves: made once, then a call, as a message
    // would hold them.
    RelativeDateTimeFormatter relative(Locale("en"), status);
    UnicodeString text;
    double relativeNs = Time(200000, [&] {
        UErrorCode s = U_ZERO_ERROR;
        text.remove();
        relative.formatNumeric(3, UDAT_REL_UNIT_DAY, text, s);
        sink += (size_t)text.length();
    });
    Print("fmt-relative", 0, relativeNs, text);
    ListFormatter* list = ListFormatter::createInstance(Locale("en"), status);
    UnicodeString items[] = {u"Ann", u"Bob", u"Cem"};
    double listNs = Time(200000, [&] {
        UErrorCode s = U_ZERO_ERROR;
        text.remove();
        list->format(items, 3, text, s);
        sink += (size_t)text.length();
    });
    Print("fmt-list", 0, listNs, text);
    delete list;
    return status > U_ZERO_ERROR ? 1 : 0;
}
