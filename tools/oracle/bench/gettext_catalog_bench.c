// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// GNU gettext baseline for Maul Locale catalogs: the lookup of
// the benchmark set maul-locale's bench/catalog_set.cmake writes (2,000
// keys, en and fr), exported by `mloccat export-po` and compiled by
// msgfmt into DIR/fr/LC_MESSAGES/bench.mo. A message is looked up as
// pgettext does, by its key (msgctxt) and English text (msgid). Cases:
// every key in turn, one key again and again, and a key the catalog
// lacks. Best of five runs after a warm-up of a tenth, nanoseconds per
// call.
//
// cc -O2 gettext_catalog_bench.c -o gettext_catalog_bench
// LANGUAGE=fr ./gettext_catalog_bench DIR [CASE]
// A CASE (every, one, miss, or none) runs that case alone, for an
// instruction count by perf stat less the run of none.
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum
{
    keyCount = 2000,
};

static char s_ids[keyCount][160];
static volatile size_t s_sink;
static unsigned s_next;

static double Now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

static double Time(long iterations, void (*body)(void))
{
    for (long i = 0; i < iterations / 10; i++)
    {
        body();
    }
    double best = 1e30;
    for (int run = 0; run < 5; run++)
    {
        double start = Now();
        for (long i = 0; i < iterations; i++)
        {
            body();
        }
        double ns = (Now() - start) / (double)iterations;
        best = ns < best ? ns : best;
    }
    return best;
}

// The msgctxt, EOT and msgid of key i, as catalog_set.cmake writes it.
static void MakeId(int i)
{
    char key[24];
    char text[128];
    snprintf(key, sizeof(key), "screen.item.%04d", i);
    switch (i % 4)
    {
    case 0:
        snprintf(text, sizeof(text), "Item %d", i);
        break;
    case 1:
        snprintf(text, sizeof(text), "Hello {$name}, item %d", i);
        break;
    case 2:
        snprintf(text, sizeof(text),
                 ".input {$n :integer} .match $n one {{One file in %d}} * {{{$n} files in %d}}", i,
                 i);
        break;
    default:
        snprintf(text, sizeof(text), "Saved {$n :integer} of {$total :integer} in %d", i);
        break;
    }
    snprintf(s_ids[i], sizeof(s_ids[i]), "%s\004%s", key, text);
}

static void Every(void)
{
    const char* id = s_ids[s_next++ % keyCount];
    s_sink += (size_t)dcgettext("bench", id, LC_MESSAGES)[0];
}

static void One(void)
{
    s_sink += (size_t)dcgettext("bench", s_ids[1], LC_MESSAGES)[0];
}

static void Miss(void)
{
    s_sink += (size_t)dcgettext("bench", "screen.none\004Nothing", LC_MESSAGES)[0];
}

int main(int argc, char** argv)
{
    if (argc < 2 || setlocale(LC_ALL, "en_US.UTF-8") == NULL)
    {
        fputs("usage: LANGUAGE=fr gettext_catalog_bench DIR (with en_US.UTF-8)\n", stderr);
        return 1;
    }
    bindtextdomain("bench", argv[1]);
    for (int i = 0; i < keyCount; i++)
    {
        MakeId(i);
    }
    const char* fr = dcgettext("bench", s_ids[1], LC_MESSAGES);
    if (strcmp(fr, "Bonjour {$name}, élément 1") != 0)
    {
        fprintf(stderr, "no French catalog: %s\n", fr);
        return 1;
    }
    static const struct
    {
        const char* name;
        void (*body)(void);
    } cases[] = {{"every", Every}, {"one", One}, {"miss", Miss}};
    for (int i = 0; i < 3; i++)
    {
        if (argc < 3 || strcmp(argv[2], cases[i].name) == 0)
        {
            printf("gettext-%-10s %10.1f ns\n", cases[i].name, Time(5000000, cases[i].body));
        }
    }
    return 0;
}
