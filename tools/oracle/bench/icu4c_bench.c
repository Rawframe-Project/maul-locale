// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C baseline for Maul Locale: the same work as icu4x_bench.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unicode/ucol.h>
#include <unicode/udat.h>
#include <unicode/uloc.h>
#include <unicode/unum.h>
#include <unicode/upluralrules.h>
#include <unicode/ustring.h>

static volatile int sink;

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

#define TIME(name, iters, body)                                                                    \
    do                                                                                             \
    {                                                                                              \
        for (long w_ = 0; w_ < (iters) / 10; ++w_)                                                 \
        {                                                                                          \
            body;                                                                                  \
        }                                                                                          \
        double s_ = now();                                                                         \
        for (long i_ = 0; i_ < (iters); ++i_)                                                      \
        {                                                                                          \
            body;                                                                                  \
        }                                                                                          \
        printf("%-28s %10.1f ns\n", name, (now() - s_) / (double)(iters));                         \
    } while (0)

int main(void)
{
    UErrorCode e = U_ZERO_ERROR;
    UChar ubuf[64];
    char cbuf[128];

    TIME("plurals construct ru", 20000, {
        e = U_ZERO_ERROR;
        UPluralRules* p = uplrules_open("ru", &e);
        uplrules_close(p);
    });
    e = U_ZERO_ERROR;
    UPluralRules* pr = uplrules_open("ru", &e);
    unsigned n = 0;
    TIME("plurals select ru int", 10000000, {
        n = (n + 7) % 1000;
        e = U_ZERO_ERROR;
        sink = uplrules_select(pr, (double)n, ubuf, 64, &e);
    });
    TIME("plurals select ru decimal", 10000000, {
        e = U_ZERO_ERROR;
        sink = uplrules_select(pr, 1234567.891, ubuf, 64, &e);
    });

    TIME("decimal construct de", 20000, {
        e = U_ZERO_ERROR;
        UNumberFormat* f = unum_open(UNUM_DECIMAL, NULL, 0, "de", NULL, &e);
        unum_close(f);
    });
    e = U_ZERO_ERROR;
    UNumberFormat* nf = unum_open(UNUM_DECIMAL, NULL, 0, "de", NULL, &e);
    unum_setAttribute(nf, UNUM_MAX_FRACTION_DIGITS, 3);
    TIME("decimal format de", 5000000, {
        e = U_ZERO_ERROR;
        sink = unum_formatDecimal(nf, "1234567.891", -1, ubuf, 64, NULL, &e);
    });
    u_austrcpy(cbuf, ubuf);
    printf("  -> %s\n", cbuf);

    UChar skel[8];
    u_uastrcpy(skel, "yMMMd");
    TIME("date construct en YMD medium", 20000, {
        e = U_ZERO_ERROR;
        UDateFormat* d =
            udat_open(UDAT_PATTERN, UDAT_PATTERN, "en", u"UTC", -1, u"MMM d, y", -1, &e);
        udat_close(d);
    });
    e = U_ZERO_ERROR;
    UDateFormat* df = udat_open(UDAT_NONE, UDAT_MEDIUM, "en", u"UTC", -1, NULL, 0, &e);
    UDate date = 1791763200000.0; // 2026-10-12T00:00:00Z
    TIME("date format en YMD medium", 5000000, {
        e = U_ZERO_ERROR;
        sink = udat_format(df, date, ubuf, 64, NULL, &e);
    });
    u_austrcpy(cbuf, ubuf);
    printf("  -> %s\n", cbuf);

    static const char* words[8] = {"Äpfel", "apfel",  "Zebra",   "über",
                                   "Ufer",  "Straße", "Strasse", "Öl"};
    TIME("collator construct de", 20000, {
        e = U_ZERO_ERROR;
        UCollator* c = ucol_open("de", &e);
        ucol_close(c);
    });
    e = U_ZERO_ERROR;
    UCollator* col = ucol_open("de", &e);
    size_t lens[8];
    for (int k = 0; k < 8; ++k)
    {
        lens[k] = strlen(words[k]);
    }
    unsigned i = 0;
    TIME("collator compare de", 10000000, {
        i = (i + 3) % 64;
        e = U_ZERO_ERROR;
        sink = ucol_strcollUTF8(col, words[i / 8], (int32_t)lens[i / 8], words[i % 8],
                                (int32_t)lens[i % 8], &e);
    });

    TIME("locale parse", 5000000, {
        e = U_ZERO_ERROR;
        sink = uloc_forLanguageTag("zh-Hant-TW-u-nu-hanidec", cbuf, 128, NULL, &e);
    });
    TIME("locale maximize zh-TW", 5000000, {
        e = U_ZERO_ERROR;
        sink = uloc_addLikelySubtags("zh_TW", cbuf, 128, &e);
    });
    printf("  -> %s (%s)\n", cbuf, u_errorName(e));
    return 0;
}
