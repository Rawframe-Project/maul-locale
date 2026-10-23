// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C baseline for Maul Locale: the message part's work, the
// same messages and arguments as maul-locale's bench/bench_messages.c, in
// en with ICU's default bidi isolation. MF2: the formatter built (parsing
// included) and a call formatting; MF1: the same where MF1 can say it
// (two selectors as a select holding a plural). Best of five runs after a
// warm-up of a tenth, nanoseconds per call.
#include <unicode/messageformat2.h>
#include <unicode/messageformat2_arguments.h>
#include <unicode/msgfmt.h>
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

static const char* const mf2[] = {
    "Hello, {$name}!",
    "You have {$n :number} new messages.",
    ".input {$n :number} .match $n 0 {{No messages.}} one {{One message.}} * {{{$n} messages.}}",
    ".input {$g :string} .input {$n :number} .match $g $n female one {{She has {$n} cat.}} "
    "female * {{She has {$n} cats.}} * one {{They have {$n} cat.}} * * {{They have {$n} cats.}}",
};
static const char16_t* const mf1[] = {
    u"Hello, {0}!",
    u"You have {1,number} new messages.",
    u"{1,plural,=0{No messages.} one{One message.} other{# messages.}}",
    u"{2,select,female{{1,plural,one{She has # cat.} other{She has # cats.}}} "
    u"other{{1,plural,one{They have # cat.} other{They have # cats.}}}}",
};
static const char* const names[] = {"hello", "number", "plural", "two"};

int main()
{
    std::map<UnicodeString, m2::Formattable> map;
    map[u"name"] = m2::Formattable(UnicodeString(u"Ayşe"));
    map[u"n"] = m2::Formattable((int64_t)3);
    map[u"g"] = m2::Formattable(UnicodeString(u"female"));
    UErrorCode status = U_ZERO_ERROR;
    m2::MessageArguments arguments(map, status);
    Formattable args1[] = {Formattable(UnicodeString(u"Ayşe")), Formattable((int64_t)3),
                           Formattable(UnicodeString(u"female"))};
    for (int m = 0; m < 4; ++m)
    {
        UnicodeString source = UnicodeString::fromUTF8(mf2[m]);
        auto build = [&] {
            UErrorCode s = U_ZERO_ERROR;
            UParseError pe;
            m2::MessageFormatter::Builder b(s);
            b.setLocale(Locale("en"));
            b.setPattern(source, pe, s);
            m2::MessageFormatter f = b.build(s);
            sink += (size_t)s;
        };
        UErrorCode s = U_ZERO_ERROR;
        UParseError pe;
        m2::MessageFormatter::Builder b(s);
        b.setLocale(Locale("en"));
        b.setPattern(source, pe, s);
        m2::MessageFormatter f = b.build(s);
        UnicodeString text;
        auto format = [&] {
            UErrorCode s2 = U_ZERO_ERROR;
            text = f.formatToString(arguments, s2);
            sink += (size_t)text.length();
        };
        double built = Time(20000, build);
        double formatted = Time(20000, format);
        std::string utf8;
        text.toUTF8String(utf8);
        printf("mf2-%-8s build %9.1f ns  format %9.1f ns  [%s]\n", names[m], built, formatted,
               utf8.c_str());
        auto build1 = [&] {
            UErrorCode s1 = U_ZERO_ERROR;
            MessageFormat f1(mf1[m], Locale("en"), s1);
            sink += (size_t)s1;
        };
        MessageFormat f1(mf1[m], Locale("en"), s);
        UnicodeString text1;
        auto format1 = [&] {
            text1.remove();
            FieldPosition fp;
            UErrorCode s1 = U_ZERO_ERROR;
            f1.format(args1, 3, text1, fp, s1);
            sink += (size_t)text1.length();
        };
        double built1 = Time(20000, build1);
        double formatted1 = Time(200000, format1);
        utf8.clear();
        text1.toUTF8String(utf8);
        printf("mf1-%-8s build %9.1f ns  format %9.1f ns  [%s]\n", names[m], built1, formatted1,
               utf8.c_str());
    }
    return 0;
}
