// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C baseline for Maul Locale: the spell-out part's work,
// the same inputs as maul-locale's bench/bench_main.c. Best of five runs
// after a warm-up of a tenth, nanoseconds per call. A formatter is made
// by RuleBasedNumberFormat with its rule set chosen; numbers are written
// to a UnicodeString (UTF-16, ICU's own form), text parsed from one.
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <unicode/rbnf.h>
#include <unicode/ustring.h>

using namespace icu;

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

static RuleBasedNumberFormat* Make(URBNFRuleSetTag tag, const char* locale, const char* set)
{
    UErrorCode e = U_ZERO_ERROR;
    auto* f = new RuleBasedNumberFormat(tag, Locale(locale), e);
    f->setDefaultRuleSet(UnicodeString(set), e);
    if (U_FAILURE(e))
    {
        fprintf(stderr, "cannot make %s %s: %s\n", locale, set, u_errorName(e));
        return nullptr;
    }
    return f;
}

static void Show(const UnicodeString& s)
{
    std::string u;
    s.toUTF8String(u);
    printf("[%s]\n", u.c_str());
}

int main()
{
    RuleBasedNumberFormat* en = Make(URBNF_SPELLOUT, "en", "%spellout-cardinal");
    RuleBasedNumberFormat* ru = Make(URBNF_SPELLOUT, "ru", "%spellout-cardinal-feminine");
    RuleBasedNumberFormat* fr = Make(URBNF_SPELLOUT, "fr", "%spellout-ordinal-masculine");
    RuleBasedNumberFormat* roman = Make(URBNF_NUMBERING_SYSTEM, "en", "%roman-upper");
    if (en == nullptr || ru == nullptr || fr == nullptr || roman == nullptr)
    {
        return 1;
    }
    UnicodeString out;
    en->format((int64_t)1234567, out);
    Show(out);
    out.remove();
    ru->format(21.5, out);
    Show(out);
    out.remove();
    fr->format((int64_t)97, out);
    Show(out);
    out.remove();
    roman->format((int64_t)1999, out);
    Show(out);
    const UnicodeString words("one thousand two hundred thirty-four");
    Time("spellout-init", 2000,
         []
         {
             UErrorCode e = U_ZERO_ERROR;
             RuleBasedNumberFormat f(URBNF_SPELLOUT, Locale("en"), e);
             f.setDefaultRuleSet(UnicodeString("%spellout-cardinal"), e);
             sink = (int)e;
         });
    Time("spellout-integer", 200000,
         [&]
         {
             out.remove();
             en->format((int64_t)1234567, out);
             sink = out.length();
         });
    Time("spellout-fraction", 200000,
         [&]
         {
             out.remove();
             ru->format(21.5, out);
             sink = out.length();
         });
    Time("spellout-ordinal", 200000,
         [&]
         {
             out.remove();
             fr->format((int64_t)97, out);
             sink = out.length();
         });
    Time("spellout-roman", 500000,
         [&]
         {
             out.remove();
             roman->format((int64_t)1999, out);
             sink = out.length();
         });
    Time("spellout-parse", 20000,
         [&]
         {
             Formattable result;
             ParsePosition position(0);
             en->parse(words, result, position);
             sink = position.getIndex();
         });
    return 0;
}
