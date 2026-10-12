// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Speed of the locale part on the compiled-in data, in nanoseconds per
// call, the best of five runs: parsing, canonicalizing, adding and
// removing likely subtags, a parent, matching against 80 supported
// locales (ICU's test list) and regional preferences. The inputs are the
// ones the ICU4C and ICU4X baselines time (record mloc-0004). Given a
// file of times in its own output's form, such as an earlier run's, it
// prints each time's ratio to the recorded one as well.

#include "maul-locale/locale.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
