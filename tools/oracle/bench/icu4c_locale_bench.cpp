// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C 74 baseline for Maul Locale's locale part: the inputs of
// maul-locale/bench/bench_main.c. ICU4C has no API for CLDR's parent
// locales outside resource bundles, so "parent" is left out; its
// regional values come from separate calls (measurement system, paper
// size, a calendar's first day and minimal days).
#include <cstdio>
#include <ctime>
#include <unicode/localematcher.h>
#include <unicode/locid.h>
#include <unicode/ucal.h>
#include <unicode/ulocdata.h>
#include <vector>

static volatile int sink;

static double now()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

template <class F> static void time(const char* name, long iters, F f)
{
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

static const char* kSupported =
    "af,am,ar,az,be,bg,bn,bs,ca,cs,cy,da,de,el,en,en-GB,es,es-419,et,eu,fa,fi,fil,fr,ga,gl,"
    "gu,hi,hr,hu,hy,id,is,it,iw,ja,ka,kk,km,kn,ko,ky,lo,lt,lv,mk,ml,mn,mr,ms,my,ne,nl,no,pa,"
    "pl,pt,pt-PT,ro,ru,si,sk,sl,sq,sr,sr-Latn,sv,sw,ta,te,th,tr,uk,ur,uz,vi,zh-CN,zh-TW,zu";

int main()
{
    using namespace icu;
    UErrorCode e = U_ZERO_ERROR;
    time("parse", 2000000,
         [&]
         {
             e = U_ZERO_ERROR;
             Locale l = Locale::forLanguageTag("zh-Hant-TW-u-nu-hanidec", e);
             sink = l.isBogus();
         });
    time("canonicalize", 2000000,
         [&]
         {
             e = U_ZERO_ERROR;
             Locale l = Locale::forLanguageTag("iw-IL", e);
             l.canonicalize(e);
             sink = l.isBogus();
         });
    time("canonicalize-hard", 1000000,
         [&]
         {
             e = U_ZERO_ERROR;
             Locale l = Locale::forLanguageTag("sh-SU-u-ms-imperial", e);
             l.canonicalize(e);
             sink = l.isBogus();
         });
    Locale zhTW("zh_TW");
    time("maximize", 5000000,
         [&]
         {
             e = U_ZERO_ERROR;
             Locale l(zhTW);
             l.addLikelySubtags(e);
             sink = l.isBogus();
         });
    Locale zhHantTW("zh_Hant_TW");
    time("minimize", 2000000,
         [&]
         {
             e = U_ZERO_ERROR;
             Locale l(zhHantTW);
             l.minimizeSubtags(e);
             sink = l.isBogus();
         });
    time("match-prepare", 50000,
         [&]
         {
             e = U_ZERO_ERROR;
             LocaleMatcher m =
                 LocaleMatcher::Builder().setSupportedLocalesFromListString(kSupported).build(e);
             sink = (int)e;
         });
    e = U_ZERO_ERROR;
    LocaleMatcher matcher =
        LocaleMatcher::Builder().setSupportedLocalesFromListString(kSupported).build(e);
    std::vector<Locale> desired = {Locale("de_AT"), Locale("en_GB")};
    time("match", 500000,
         [&]
         {
             e = U_ZERO_ERROR;
             Locale::RangeIterator<std::vector<Locale>::iterator> it(desired.begin(),
                                                                     desired.end());
             const Locale* best = matcher.getBestMatch(it, e);
             sink = best != nullptr;
         });
    time("preferences", 200000,
         [&]
         {
             e = U_ZERO_ERROR;
             sink = ulocdata_getMeasurementSystem("en_GB", &e);
             int32_t h = 0, w = 0;
             ulocdata_getPaperSize("en_GB", &h, &w, &e);
             UCalendar* c = ucal_open(nullptr, 0, "en_GB", UCAL_GREGORIAN, &e);
             sink += ucal_getAttribute(c, UCAL_FIRST_DAY_OF_WEEK) +
                     ucal_getAttribute(c, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK);
             ucal_close(c);
         });
    return 0;
}
