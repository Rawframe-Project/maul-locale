// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C baseline for Maul Locale: the names part's work, the
// same inputs as maul-locale's bench/bench_main.c. Best of five runs after
// a warm-up of a tenth, nanoseconds per call. The LocaleDisplayNames
// objects and the Locale values are made before the timing, as Maul's
// parsed locales are; each call writes a UnicodeString, ICU's only
// output. ICU names a key's value by its legacy key and type
// ("calendar", "gregorian"). Given a case name, only that case is timed.
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <unicode/locdspnm.h>
#include <unicode/locid.h>

using namespace icu;

static volatile int sink;

static double now()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

static const char* only;

template <typename F> static void Time(const char* name, long iters, F body)
{
    if (only != nullptr && strcmp(only, name) != 0)
    {
        return;
    }
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

static void Show(const UnicodeString& s)
{
    std::string text;
    s.toUTF8String(text);
    printf("  -> %s\n", text.c_str());
}

int main(int argc, char** argv)
{
    only = argc > 1 ? argv[1] : nullptr;
    LocaleDisplayNames* enDialect =
        LocaleDisplayNames::createInstance(Locale("en"), ULDN_DIALECT_NAMES);
    LocaleDisplayNames* enStandard =
        LocaleDisplayNames::createInstance(Locale("en"), ULDN_STANDARD_NAMES);
    LocaleDisplayNames* fr = LocaleDisplayNames::createInstance(Locale("fr"), ULDN_DIALECT_NAMES);
    UDisplayContext sentence[] = {UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE};
    LocaleDisplayNames* frSentence = LocaleDisplayNames::createInstance(Locale("fr"), sentence, 1);
    UErrorCode e = U_ZERO_ERROR;
    Locale deATBuddhist = Locale::forLanguageTag("de-AT-u-ca-buddhist", e);
    Locale zhHantTW = Locale::forLanguageTag("zh-Hant-TW", e);
    if (U_FAILURE(e))
    {
        return 1;
    }
    UnicodeString r;
    if (only == nullptr)
    {
        Show(enDialect->languageDisplayName("de", r));
        Show(fr->regionDisplayName("DE", r));
        Show(enDialect->keyValueDisplayName("calendar", "gregorian", r));
        Show(frSentence->languageDisplayName("de", r));
        Show(enDialect->localeDisplayName(deATBuddhist, r));
        Show(enStandard->localeDisplayName(zhHantTW, r));
    }
    Time("name-language", 2000000,
         [&] { sink = enDialect->languageDisplayName("de", r).length(); });
    Time("name-region", 2000000, [&] { sink = fr->regionDisplayName("DE", r).length(); });
    Time("name-key-value", 2000000,
         [&] { sink = enDialect->keyValueDisplayName("calendar", "gregorian", r).length(); });
    Time("name-sentence", 2000000,
         [&] { sink = frSentence->languageDisplayName("de", r).length(); });
    Time("locale-name", 500000,
         [&] { sink = enDialect->localeDisplayName(deATBuddhist, r).length(); });
    Time("locale-name-std", 500000,
         [&] { sink = enStandard->localeDisplayName(zhHantTW, r).length(); });
    delete enDialect;
    delete enStandard;
    delete fr;
    delete frSentence;
    return 0;
}
