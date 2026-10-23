// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C 74 baseline for Maul Locale's decimal and plural parts: the inputs
// of maul-locale/bench/bench_main.c. ICU4C has no public decimal type;
// Formattable parses a decimal string and gives a double's shortest
// digits (getDecimalNumber), which is the nearest it offers. It has no
// public rounding of a decimal alone, so that case is left out.
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unicode/fmtable.h>
#include <unicode/plurrule.h>
#include <unicode/upluralrules.h>

static volatile int sink;
// Given a case name, only that case runs (for perf stat).
static const char* only;

static double now()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

template <class F> static void time(const char* name, long iters, F f)
{
    if (only != nullptr && std::strcmp(only, name) != 0)
    {
        return;
    }
    for (long i = 0; i < iters / 10; ++i)
    {
        f();
    }
    double best = 1e30;
    for (int r = 0; r < 5; ++r)
    {
        double s = now();
        for (long i = 0; i < iters; ++i)
        {
            f();
        }
        double ns = (now() - s) / (double)iters;
        best = ns < best ? ns : best;
    }
    std::printf("%-18s %10.1f ns\n", name, best);
}

int main(int argc, char** argv)
{
    using namespace icu;
    only = argc > 1 ? argv[1] : nullptr;
    time("decimal-parse", 2000000,
         [&]
         {
             UErrorCode e = U_ZERO_ERROR;
             Formattable f(StringPiece("1234567.891"), e);
             sink = f.getType();
         });
    time("decimal-shortest", 2000000,
         [&]
         {
             UErrorCode e = U_ZERO_ERROR;
             Formattable f(1234567.891);
             sink = (int)f.getDecimalNumber(e).length();
         });
    time("plural-init", 200000,
         [&]
         {
             UErrorCode e = U_ZERO_ERROR;
             UPluralRules* p = uplrules_open("ru", &e);
             uplrules_close(p);
         });
    UErrorCode e = U_ZERO_ERROR;
    PluralRules* rules = PluralRules::forLocale(Locale("ru"), e);
    unsigned n = 0;
    time("plural-select-int", 10000000,
         [&]
         {
             n = (n + 7) % 1000;
             sink = rules->select((int32_t)n).length();
         });
    time("plural-select-dec", 10000000, [&] { sink = rules->select(1234567.891).length(); });
    delete rules;
    return 0;
}
