// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A calendar's interval set in a locale (interval_gen.h): the skeleton
// of each option (a date entry's classical skeleton; a standard format's
// own skeleton, as DateTimePatternGenerator::staticGetSkeleton reads a
// pattern; a time's clock, precision and zone letters), resolved with
// the locale's intervalFormats and its Gregorian medium date-time glue.

#include "interval_gen.h"

#include "date_source.h"
#include "skeleton.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxSkeleton = 64,
};

static const char* const s_lengths[4] = {"full", "long", "medium", "short"};

// A pattern's skeleton: its fields as written, in UTS #35's order.
static void SkeletonOfPattern(const char* pattern, char out[maxSkeleton])
{
    Skeleton s;
    SkeletonParse(pattern, &s);
    size_t n = 0;
    for (int f = 0; f < skeletonFieldCount; f++)
    {
        // A bounded fill: GCC 14 on arm64 took the letter loop's vector
        // stores past the buffer.
        size_t room = maxSkeleton - 1 - n;
        size_t take = s.original.lengths[f] < room ? s.original.lengths[f] : room;
        memset(out + n, s.original.chars[f], take);
        n += take;
    }
    out[n] = '\0';
}

// The fallback as a compiled-pattern source: the text between {0} and
// {1} quoted, a quote doubled.
static char* QuoteFallback(const char* fallback)
{
    char* out = calloc(1, 4 * strlen(fallback) + 3);
    if (out == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    size_t n = 0;
    bool open = false;
    for (const char* at = fallback; *at != '\0'; at++)
    {
        bool value = strncmp(at, "{0}", 3) == 0 || strncmp(at, "{1}", 3) == 0;
        if (value == open)
        {
            out[n++] = '\'';
            open = !open;
        }
        if (value)
        {
            memcpy(out + n, at, 3);
            n += 3;
            at += 2;
            continue;
        }
        if (*at == '\'')
        {
            out[n++] = '\'';
        }
        out[n++] = *at;
    }
    if (open)
    {
        out[n++] = '\'';
    }
    return out;
}

// A standard format's skeleton, or the skeleton given where the locale
// has no such format.
static void StandardSkeleton(Cldr* cldr, const char* locale, const char* calendar, const char* kind,
                             int length, char out[maxSkeleton])
{
    char rest[256];
    snprintf(rest, sizeof(rest), "%sFormats/%sFormatLength[@type=\"%s\"]/%sFormat/pattern", kind,
             kind, s_lengths[length], kind);
    const char* pattern = DateCalendarValue(cldr, locale, calendar, rest);
    SkeletonOfPattern(pattern != nullptr ? pattern : "", out);
}

// The time entries' skeletons: each zone form (none first), clock and
// precision.
static void TimeSkeleton(int entry, char out[maxSkeleton])
{
    static const char* const s_zones[9] = {"",     "zzzz", "z",    "vvvv", "v",
                                           "VVVV", "VVV",  "OOOO", "O"};
    static const char* const s_times[2][3] = {{"h", "hm", "hms"}, {"H", "Hm", "Hms"}};
    int form = entry / 6;
    int clock = entry / 3 % 2;
    int precision = entry % 3;
    snprintf(out, maxSkeleton, "%s%s", s_times[clock][precision], s_zones[form]);
}

// A time's date differences are always fallbacks of the short date with
// the full year (and the era) and the time, matched whole; the run time
// joins those itself, so the set keeps none of them.
static void DropDateFallbacks(IntervalPatterns* p)
{
    for (int f = intervalEra; f <= intervalDay; f++)
    {
        IntervalPair* pair = &p->pairs[f];
        if (pair->first != nullptr && pair->first[0] != '\0')
        {
            fprintf(stderr, "mlocgen: a time's date difference with a pattern pair\n");
            exit(1);
        }
        free(pair->first);
        free(pair->second);
        pair->first = nullptr;
        pair->second = nullptr;
    }
}

// The date-time glue style ICU's generator joins a skeleton's date with:
// full for four month letters and a weekday, long for four, medium for
// three, else short.
static int GlueStyle(const char* skeleton)
{
    size_t months = strspn(skeleton + strcspn(skeleton, "ML"), "ML");
    bool weekday = strpbrk(skeleton, "Ece") != nullptr;
    return months == 4 ? (weekday ? 0 : 1) : months == 3 ? 2 : 3;
}

// Whether a date entry may join a time: a field set with a day or a
// weekday, or a standard format; the others keep no prefixes.
static bool TakesTime(int e)
{
    int entry = e / dateLengthCount;
    return e >= dateIntervalDateStyle || (entry != dateMonth && entry < dateYear);
}

// A date skeleton's prefixes, as DateIntervalFormat::initializePattern
// prefixes a date-time's skeleton: d, then M, then y, then G, each where
// the date lacks it, each on the one before.
static void SetPrefix(PatternGenerator* g, const char* date, IntervalPrefix* out)
{
    static const char s_letters[dateIntervalPrefixes] = {'d', 'M', 'y', 'G'};
    char skeleton[maxSkeleton];
    snprintf(skeleton, sizeof(skeleton), "%s", date);
    for (int level = 0; level < dateIntervalPrefixes; level++)
    {
        out->patterns[level] = nullptr;
        out->styles[level] = 0;
        if (strchr(date, s_letters[level]) == nullptr)
        {
            memmove(skeleton + 1, skeleton, strlen(skeleton) + 1);
            skeleton[0] = s_letters[level];
            out->patterns[level] = PatternGeneratorBest(g, skeleton);
            out->styles[level] = GlueStyle(skeleton);
        }
    }
}

void IntervalSetResolve(Cldr* cldr, PatternGenerator* g, const char* locale, const char* calendar,
                        const char* const dates[dateIntervalDateStyle], IntervalSet* out)
{
    memset(out, 0, sizeof(*out));
    IntervalItems* items = calloc(1, sizeof(IntervalItems));
    if (items == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    DateSourceLoadIntervals(cldr, locale, calendar, items);
    const char* glue = DateCalendarValue(
        cldr, locale, "gregorian",
        "dateTimeFormats/dateTimeFormatLength[@type=\"medium\"]/dateTimeFormat/pattern");
    for (int e = 0; e < dateIntervalEntryCount; e++)
    {
        char skeleton[maxSkeleton];
        if (e < dateIntervalDateStyle)
        {
            snprintf(skeleton, sizeof(skeleton), "%s", dates[e]);
        }
        else if (e < dateIntervalTime)
        {
            StandardSkeleton(cldr, locale, calendar, "date", e - dateIntervalDateStyle, skeleton);
        }
        else if (e < dateIntervalTimeStyle)
        {
            TimeSkeleton(e - dateIntervalTime, skeleton);
        }
        else
        {
            StandardSkeleton(cldr, locale, calendar, "time", e - dateIntervalTimeStyle, skeleton);
        }
        IntervalResolve(g, items, glue, skeleton, &out->entries[e]);
        if (e >= dateIntervalTime)
        {
            DropDateFallbacks(&out->entries[e]);
        }
        else if (TakesTime(e))
        {
            SetPrefix(g, skeleton, &out->prefixes[e]);
        }
    }
    out->shortDate = PatternGeneratorBest(g, "yMd");
    out->shortEraDate = PatternGeneratorBest(g, "GyMd");
    out->fallback =
        QuoteFallback(items->fallback != nullptr ? items->fallback : "{0} \xE2\x80\x93 {1}");
    IntervalItemsFree(items);
    free(items);
}

void IntervalSetFree(IntervalSet* set)
{
    for (int e = 0; e < dateIntervalEntryCount; e++)
    {
        IntervalPatternsFree(&set->entries[e]);
    }
    for (int e = 0; e < dateIntervalTime; e++)
    {
        for (int level = 0; level < dateIntervalPrefixes; level++)
        {
            free(set->prefixes[e].patterns[level]);
        }
    }
    free(set->shortDate);
    free(set->shortEraDate);
    free(set->fallback);
    memset(set, 0, sizeof(*set));
}
