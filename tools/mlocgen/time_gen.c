// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A calendar's time patterns (time_gen.h).

#include "time_gen.h"

#include "date_source.h"
#include "semantic.h"
#include "table_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* const s_lengths[4] = {"full", "long", "medium", "short"};

static char* Copy(const char* text)
{
    size_t length = strlen(text);
    char* copy = GenAllocate(length + 1);
    memcpy(copy, text, length + 1);
    return copy;
}

// Whether glue is literal text but its two placeholders, each once.
static bool ValidGlue(const char* glue)
{
    bool quoted = false;
    for (const char* p = glue; *p != '\0'; p++)
    {
        if (*p == '\'')
        {
            quoted = !quoted;
        }
        else if (!quoted && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')))
        {
            return false;
        }
    }
    const char* zero = strstr(glue, "{0}");
    const char* one = strstr(glue, "{1}");
    return !quoted && zero != nullptr && one != nullptr && strstr(zero + 3, "{0}") == nullptr &&
           strstr(one + 3, "{1}") == nullptr;
}

// Whether a pattern's hour, outside its quoted text, is on the 12-hour
// clock (hsb's "HH 'hodź'." is not).
static bool TwelveHour(const char* pattern)
{
    bool quoted = false;
    for (const char* p = pattern; *p != '\0'; p++)
    {
        if (*p == '\'')
        {
            quoted = !quoted;
        }
        else if (!quoted && strchr("hHKk", *p) != nullptr)
        {
            return *p == 'h' || *p == 'K';
        }
    }
    return false;
}

// A glue with its unquoted letters quoted: ICU reads a glue as a
// SimpleFormatter, where letters are text (da's generic calendar has
// "{1} kl. {0}" beside "{1} 'kl'. {0}").
static char* QuoteLetters(const char* glue)
{
    char* out = GenAllocate(2 * strlen(glue) + 1);
    size_t at = 0;
    // Inside the glue's own quotes; inside quotes added here.
    bool quoted = false;
    bool added = false;
    for (const char* p = glue; *p != '\0'; p++)
    {
        bool letter = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z');
        if (added && !letter)
        {
            out[at++] = '\'';
            added = false;
        }
        if (!quoted && letter && !added)
        {
            out[at++] = '\'';
            added = true;
        }
        quoted = *p == '\'' ? !quoted : quoted;
        out[at++] = *p;
    }
    if (added)
    {
        out[at++] = '\'';
    }
    out[at] = '\0';
    return out;
}

// The kinds of date-time glue: standard, atTime, relative; a kind the
// calendar lacks takes the one before.
typedef enum GlueKind
{
    glueStandard = 0,
    glueAtTime = 1,
    glueRelative = 2,
} GlueKind;

static char* Glue(Cldr* cldr, const char* locale, const char* calendar, int length, GlueKind kind)
{
    static const char* const s_types[] = {"", "[@type=\"atTime\"]", "[@type=\"relative\"]"};
    char rest[256];
    snprintf(rest, sizeof(rest),
             "dateTimeFormats/dateTimeFormatLength[@type=\"%s\"]/dateTimeFormat%s/pattern",
             s_lengths[length], s_types[kind]);
    const char* glue = DateCalendarValue(cldr, locale, calendar, rest);
    if (glue == nullptr && kind != glueStandard)
    {
        return Glue(cldr, locale, calendar, length, (GlueKind)(kind - 1));
    }
    char* quoted = glue != nullptr ? QuoteLetters(glue) : nullptr;
    if (quoted == nullptr || !ValidGlue(quoted))
    {
        GenDie("date-time glue missing or not plain", locale);
    }
    return quoted;
}

void TimeSetPatterns(Cldr* cldr, PatternGenerator* g, PatternGenerator* items, const char* locale,
                     const char* calendar, char* out[dateTimePatternCount], uint16_t* flags)
{
    char* own = PatternGeneratorBest(g, "j");
    *flags = TwelveHour(own) ? dateTimeOwnTwelve : 0;
    free(own);
    for (int length = 0; length < dateLengthCount; length++)
    {
        const char* skeleton = DateSkeletonOf(cldr, locale, calendar, length);
        size_t month = strspn(skeleton + strcspn(skeleton, "ML"), "ML");
        uint16_t code = month >= 4 ? 2 : month == 3 ? 1 : 0;
        *flags |= (uint16_t)(code << (dateTimeMonthShift + 2 * length));
    }
    // A weekday with a time, which one item covers (Ehm).
    static const char* const s_weekdays[2] = {"EEEE", "EEE"};
    static const char* const s_times[2][3] = {{"h", "hm", "hms"}, {"H", "Hm", "Hms"}};
    for (int clock = 0; clock < 2; clock++)
    {
        for (int width = 0; width < 2; width++)
        {
            for (int precision = 0; precision < 3; precision++)
            {
                char skeleton[16];
                snprintf(skeleton, sizeof(skeleton), "%s%s", s_weekdays[width],
                         s_times[clock][precision]);
                out[dateTimeWeekday + 6 * clock + 3 * width + precision] =
                    PatternGeneratorBest(g, skeleton);
            }
        }
    }
    for (int clock = 0; clock < 2; clock++)
    {
        for (int precision = 0; precision < 3; precision++)
        {
            SemanticSkeleton s = {
                .fields = semanticTime,
                .hourCycle = clock == 0 ? semanticHourClock12 : semanticHourClock24,
                .precision = precision,
            };
            char classical[64];
            if (!SemanticClassical(&s, "", classical, sizeof(classical)))
            {
                GenDie("a time skeleton UTS #35 does not define", locale);
            }
            out[(clock == 0 ? dateTimeClock12 : dateTimeClock24) + precision] =
                PatternGeneratorBest(g, classical);
        }
    }
    // A zone alone, and a time with a zone, in each form, matched among
    // the items alone: CLDR's datetime test data writes th's Hmsz as
    // its Hmsv item gives (HH:mm:ss z), where ICU4C's generator, which
    // also holds the standard formats, takes th's long time format.
    static const char* const s_zones[8] = {"zzzz", "z", "vvvv", "v", "VVVV", "VVV", "OOOO", "O"};
    for (int form = 0; form < 8; form++)
    {
        out[dateTimeZoneAlone + form] = PatternGeneratorBest(items, s_zones[form]);
        for (int clock = 0; clock < 2; clock++)
        {
            for (int precision = 0; precision < 3; precision++)
            {
                char skeleton[16];
                snprintf(skeleton, sizeof(skeleton), "%s%s", s_times[clock][precision],
                         s_zones[form]);
                out[dateTimeZoned + 6 * form + 3 * clock + precision] =
                    PatternGeneratorBest(items, skeleton);
            }
        }
    }
    static const char* const s_styles[4] = {"hmm", "hmmss", "Hmm", "Hmmss"};
    static const char* const s_zoneStyles[4] = {"hmmsszzzz", "hmmssz", "Hmmsszzzz", "Hmmssz"};
    for (int i = 0; i < 4; i++)
    {
        out[dateTimeStyle12 + i] = PatternGeneratorBest(items, s_styles[i]);
        out[dateTimeZoneStyle12 + i] = PatternGeneratorBest(items, s_zoneStyles[i]);
    }
    for (int length = 0; length < 4; length++)
    {
        char rest[256];
        snprintf(rest, sizeof(rest),
                 "timeFormats/timeFormatLength[@type=\"%s\"]/timeFormat/pattern",
                 s_lengths[length]);
        const char* pattern = DateCalendarValue(cldr, locale, calendar, rest);
        if (pattern == nullptr)
        {
            GenDie("a standard time format missing", locale);
        }
        out[dateTimeStandard + length] = Copy(pattern);
        out[dateTimeGlue + length] = Glue(cldr, locale, calendar, length, glueStandard);
        out[dateTimeAtGlue + length] = Glue(cldr, locale, calendar, length, glueAtTime);
        out[dateTimeRelativeGlue + length] = Glue(cldr, locale, calendar, length, glueRelative);
    }
}
