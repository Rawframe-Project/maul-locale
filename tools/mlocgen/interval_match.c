// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Date interval patterns resolved as ICU4C 79's DateIntervalFormat
// resolves them (interval_match.h; dtitvfmt.cpp and dtitvinf.cpp, read
// in research 40). Patterns are UTF-8; every letter is ASCII, so the
// byte loops below see the letters ICU's UTF-16 loops see.

#include "interval_match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    // Field widths are kept per letter from 'A' to 'z', as ICU keeps them.
    letterBase = 'A',
    letterCount = 58,
    // getBestSkeleton's distances.
    differentField = 0x1000,
    stringNumeric = 0x100,
};

static const char s_letters[intervalFieldCount] = {'G', 'y', 'M', 'd', 'a', 'h', 'm'};
// SimpleDateFormat's levels of the calendar fields: era 0, year 10,
// month 20, day 30, am or pm 40, hour 50, minute 60.
static const int s_fieldLevels[intervalFieldCount] = {0, 10, 20, 30, 40, 50, 60};

static void* Allocate(size_t size)
{
    void* p = calloc(1, size);
    if (p == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

static char* Copy(const char* text)
{
    size_t length = strlen(text);
    char* copy = Allocate(length + 1);
    memcpy(copy, text, length);
    return copy;
}

static char* CopyPart(const char* text, size_t length)
{
    char* copy = Allocate(length + 1);
    memcpy(copy, text, length);
    return copy;
}

static bool IsLetter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Each letter's count in a skeleton (DateIntervalInfo::parseSkeleton).
static void Widths(const char* skeleton, int widths[letterCount])
{
    memset(widths, 0, sizeof(int) * letterCount);
    for (const char* at = skeleton; *at != '\0'; at++)
    {
        if (*at >= letterBase && *at < letterBase + letterCount)
        {
            widths[*at - letterBase]++;
        }
    }
}

// SimpleDateFormat::getLevelFromChar for the letters; -1 for others.
static int LevelOf(char c)
{
    static const char s_chars[] = "ADEFGHKLMOQSUVWXYZacdeghklmqrsuvwxyz";
    static const int s_levels[] = {40, 20, 30, 30, 0,  50, 50, 20, 20, 0,  20, 80,
                                   10, 0,  30, 0,  10, 0,  40, 30, 30, 30, 0,  50,
                                   50, 0,  60, 20, 10, 70, 10, 0,  20, 0,  10, 0};
    const char* at = c == '\0' ? nullptr : strchr(s_chars, c);
    return at == nullptr ? -1 : s_levels[at - s_chars];
}

// SimpleDateFormat::isFieldUnitIgnored: whether no field of a pattern or
// skeleton is as small as a calendar field.
static bool FieldIgnored(const char* pattern, int field)
{
    bool quoted = false;
    for (const char* at = pattern; *at != '\0'; at++)
    {
        if (*at == '\'')
        {
            if (at[1] == '\'')
            {
                at++;
            }
            else
            {
                quoted = !quoted;
            }
        }
        else if (!quoted && IsLetter(*at) && s_fieldLevels[field] <= LevelOf(*at))
        {
            return false;
        }
    }
    return true;
}

// Replaces every occurrence of a text outside quotes
// (DateIntervalFormat::findReplaceInPattern); the result is new.
static char* ReplaceOutsideQuotes(const char* text, const char* from, const char* to)
{
    size_t fromLength = strlen(from);
    size_t toLength = strlen(to);
    char* out = Allocate(strlen(text) * (toLength + 1) + 1);
    size_t n = 0;
    bool quoted = false;
    for (const char* at = text; *at != '\0';)
    {
        if (!quoted && strncmp(at, from, fromLength) == 0)
        {
            memcpy(out + n, to, toLength);
            n += toLength;
            at += fromLength;
            continue;
        }
        quoted = *at == '\'' ? !quoted : quoted;
        out[n++] = *at++;
    }
    return out;
}

static void ReplaceIn(char** text, const char* from, const char* to)
{
    char* replaced = ReplaceOutsideQuotes(*text, from, to);
    free(*text);
    *text = replaced;
}

void IntervalItemsPut(IntervalItems* items, const char* skeleton, const char* letter,
                      const char* pattern)
{
    static const char* const s_differences[] = {"G", "y", "M", "d", "a", "B", "h", "H", "m"};
    static const int s_fields[] = {intervalEra,  intervalYear, intervalMonth,
                                   intervalDay,  intervalAmPm, intervalAmPm,
                                   intervalHour, intervalHour, intervalMinute};
    int field = -1;
    for (int i = 0; i < 9; i++)
    {
        field = strcmp(letter, s_differences[i]) == 0 ? s_fields[i] : field;
    }
    int at = 0;
    while (at < items->count && strcmp(items->skeletons[at], skeleton) != 0)
    {
        at++;
    }
    if (field < 0 || (at == items->count && at == intervalMaxItems))
    {
        return;
    }
    if (at == items->count)
    {
        items->skeletons[items->count++] = Copy(skeleton);
    }
    if (items->patterns[at][field] == nullptr)
    {
        items->patterns[at][field] = Copy(pattern);
    }
}

void IntervalItemsFree(IntervalItems* items)
{
    for (int i = 0; i < items->count; i++)
    {
        free(items->skeletons[i]);
        for (int f = 0; f < intervalFieldCount; f++)
        {
            free(items->patterns[i][f]);
        }
    }
    free(items->fallback);
    memset(items, 0, sizeof(*items));
}

// Whether the month's widths differ as number and text.
static bool NumericAgainstText(int a, int b, int letter)
{
    return letter == 'M' - letterBase && ((a <= 2 && b > 2) || (a > 2 && b <= 2));
}

// DateIntervalInfo::getBestSkeleton: the item nearest a skeleton (z as
// v, k as H, K as h, a and b dropped), or -1 for none; the difference
// is -1 where fields differ, 1 where widths do, 0 for an exact match and
// 2 where alternates were replaced. Ties keep the first met.
static int BestSkeleton(const IntervalItems* items, const char* skeleton, int* difference)
{
    char* input = Copy(skeleton);
    bool replaced = strpbrk(skeleton, "zkKab") != nullptr;
    ReplaceIn(&input, "z", "v");
    ReplaceIn(&input, "k", "H");
    ReplaceIn(&input, "K", "h");
    ReplaceIn(&input, "a", "");
    ReplaceIn(&input, "b", "");
    int want[letterCount];
    Widths(input, want);
    free(input);
    int best = -1;
    int bestDistance = 0x7FFFFFFF;
    *difference = 0;
    for (int i = 0; i < items->count; i++)
    {
        int have[letterCount];
        Widths(items->skeletons[i], have);
        int distance = 0;
        int fields = 1;
        for (int l = 0; l < letterCount; l++)
        {
            if (want[l] == have[l])
            {
                continue;
            }
            if (want[l] == 0 || have[l] == 0)
            {
                fields = -1;
                distance += differentField;
            }
            else
            {
                distance += NumericAgainstText(want[l], have[l], l) ? stringNumeric
                                                                    : abs(want[l] - have[l]);
            }
        }
        if (distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
            *difference = fields;
        }
        if (distance == 0)
        {
            *difference = 0;
            break;
        }
    }
    *difference = replaced && *difference != -1 ? 2 : *difference;
    return best;
}

static void Append(char* text, char c, int times)
{
    size_t n = strlen(text);
    for (int i = 0; i < times; i++)
    {
        text[n++] = c;
    }
    text[n] = '\0';
}

// DateIntervalFormat::getDateTimeSkeleton: a skeleton's date and time
// letters, each also normalized (one y, d, hour letter, m, z and v; M
// and E kept wide from three and four letters).
static void SplitSkeleton(const char* skeleton, char* date, char* normalDate, char* time,
                          char* normalTime)
{
    int e = 0, d = 0, month = 0, y = 0, m = 0, v = 0, z = 0;
    char hour = '\0';
    date[0] = normalDate[0] = time[0] = normalTime[0] = '\0';
    for (const char* at = skeleton; *at != '\0'; at++)
    {
        char c = *at;
        e += c == 'E', d += c == 'd', month += c == 'M', y += c == 'y';
        m += c == 'm', v += c == 'v', z += c == 'z';
        if (strchr("EdMy", c) != nullptr)
        {
            Append(date, c, 1);
        }
        else if (strchr("GYuQqLlWwDFgecUr", c) != nullptr)
        {
            Append(normalDate, c, 1);
            Append(date, c, 1);
        }
        else if (strchr("hHkKmzv", c) != nullptr)
        {
            Append(time, c, 1);
            hour = hour == '\0' && strchr("hHkK", c) != nullptr ? c : hour;
        }
        else if (strchr("aVZjsSAbB", c) != nullptr)
        {
            Append(time, c, 1);
            Append(normalTime, c, 1);
        }
    }
    Append(normalDate, 'y', y);
    Append(normalDate, 'M', month == 0 ? 0 : month < 3 ? 1 : (month > 5 ? 5 : month));
    Append(normalDate, 'E', e == 0 ? 0 : e <= 3 ? 1 : (e > 5 ? 5 : e));
    Append(normalDate, 'd', d != 0);
    Append(normalTime, hour, hour != '\0');
    Append(normalTime, 'm', m != 0);
    Append(normalTime, 'z', z != 0);
    Append(normalTime, 'v', v != 0);
}

// DateIntervalFormat::normalizeHourMetacharacters: the first hour field
// (j, J, C, h, H, k or K) as the locale's hour letter, with its day
// period after it on the 12-hour clocks, the skeleton's own day period
// field removed.
static char* NormalizeHour(PatternGenerator* g, const char* skeleton)
{
    size_t length = strlen(skeleton);
    char hourChar = '\0', period = '\0';
    size_t hourStart = 0, hourLength = 0, periodStart = 0, periodLength = 0;
    for (size_t i = 0; i < length; i++)
    {
        char c = skeleton[i];
        if (strchr("jJChHkK", c) != nullptr)
        {
            hourStart = hourChar == '\0' ? i : hourStart;
            hourChar = hourChar == '\0' ? c : hourChar;
            hourLength++;
        }
        else if (strchr("abB", c) != nullptr)
        {
            periodStart = period == '\0' ? i : periodStart;
            period = period == '\0' ? c : period;
            periodLength++;
        }
        else if (hourChar != '\0' && period != '\0')
        {
            break;
        }
    }
    if (hourChar == '\0')
    {
        return Copy(skeleton);
    }
    char asked[2] = {hourChar, '\0'};
    char* found = PatternGeneratorBest(g, asked);
    // Quoted text is dropped, so "Uhr" is not read as an hour.
    char* plain = Allocate(strlen(found) + 1);
    bool quoted = false;
    for (size_t i = 0, n = 0; found[i] != '\0'; i++)
    {
        quoted = found[i] == '\'' ? !quoted : quoted;
        if (!quoted && found[i] != '\'')
        {
            plain[n++] = found[i];
        }
    }
    free(found);
    char hour = strchr(plain, 'h')   ? 'h'
                : strchr(plain, 'K') ? 'K'
                : strchr(plain, 'k') ? 'k'
                                     : 'H';
    period = strchr(plain, 'b') ? 'b' : strchr(plain, 'B') ? 'B' : period == '\0' ? 'a' : period;
    free(plain);
    char replacement[8] = {hour, '\0'};
    if (hour != 'H' && hour != 'k')
    {
        int count = periodLength >= 5 || hourLength >= 5   ? 5
                    : periodLength >= 3 || hourLength >= 3 ? 3
                                                           : 1;
        Append(replacement, period, count);
    }
    char* out = Allocate(length + 8);
    memcpy(out, skeleton, hourStart);
    strcat(out, replacement);
    strcat(out, skeleton + hourStart + hourLength);
    if (periodLength > 0)
    {
        size_t at =
            periodStart > hourStart ? periodStart + strlen(replacement) - hourLength : periodStart;
        memmove(out + at, out + at + periodLength, strlen(out + at + periodLength) + 1);
    }
    return out;
}

// Writes a run of a letter widened from the best skeleton's width to the
// asked one, where the run has the best skeleton's width.
static void Widen(char* out, size_t* n, char letter, int count, const int best[letterCount],
                  const int want[letterCount])
{
    int index = (letter == 'L' ? 'M' : letter) - letterBase;
    if (best[index] == count && want[index] > best[index])
    {
        for (int j = 0; j < want[index] - best[index]; j++)
        {
            out[(*n)++] = letter;
        }
    }
}

// DateIntervalFormat::adjustFieldWidth: an item's pattern for the asked
// skeleton: the alternates put back (z, K, k, b) where the match replaced
// them, the day period dropped for J, each run of a field that has the
// best skeleton's width widened to the asked width.
static char* AdjustWidths(const char* input, const char* best, const char* pattern, int difference,
                          bool suppressPeriod)
{
    int want[letterCount];
    int have[letterCount];
    Widths(input, want);
    Widths(best, have);
    char* p = Copy(pattern);
    if (suppressPeriod)
    {
        ReplaceIn(&p,
                  "\xC2\xA0"
                  "a",
                  "");
        ReplaceIn(&p,
                  "\xE2\x80\xAF"
                  "a",
                  "");
        ReplaceIn(&p, "a\xC2\xA0", "");
        ReplaceIn(&p, "a\xE2\x80\xAF", "");
        ReplaceIn(&p, "a", "");
        ReplaceIn(&p, "  ", " ");
        size_t start = strspn(p, " ");
        size_t end = strlen(p);
        while (end > start && p[end - 1] == ' ')
        {
            end--;
        }
        memmove(p, p + start, end - start);
        p[end - start] = '\0';
    }
    if (difference == 2)
    {
        static const char* const s_alternates[4][3] = {
            {"z", "v", "z"}, {"K", "h", "K"}, {"k", "H", "k"}, {"b", "a", "b"}};
        for (int i = 0; i < 4; i++)
        {
            if (strchr(input, s_alternates[i][0][0]) != nullptr)
            {
                ReplaceIn(&p, s_alternates[i][1], s_alternates[i][2]);
            }
        }
    }
    have['a' - letterBase] += strchr(p, 'a') != nullptr && have['a' - letterBase] == 0;
    have['b' - letterBase] += strchr(p, 'b') != nullptr && have['b' - letterBase] == 0;
    char* out = Allocate(strlen(p) * 6 + 1);
    size_t n = 0;
    bool quoted = false;
    char previous = '\0';
    int count = 0;
    for (const char* at = p; *at != '\0'; at++)
    {
        if (*at != previous && count > 0)
        {
            Widen(out, &n, previous, count, have, want);
            count = 0;
        }
        if (*at == '\'' && at[1] == '\'')
        {
            out[n++] = *at++;
        }
        else if (*at == '\'')
        {
            quoted = !quoted;
        }
        else if (!quoted && IsLetter(*at))
        {
            previous = *at;
            count++;
        }
        out[n++] = *at;
    }
    if (count > 0)
    {
        Widen(out, &n, previous, count, have, want);
    }
    free(p);
    return out;
}

// DateIntervalFormat::splitPatternInto2Part: where the second value's
// half starts, at the first field letter met again.
static size_t SplitPoint(const char* pattern)
{
    bool seen[letterCount] = {false};
    bool quoted = false;
    bool repeated = false;
    char previous = '\0';
    size_t count = 0;
    size_t i = 0;
    for (; pattern[i] != '\0'; i++)
    {
        char c = pattern[i];
        if (c != previous && count > 0)
        {
            if (seen[previous - letterBase])
            {
                repeated = true;
                break;
            }
            seen[previous - letterBase] = true;
            count = 0;
        }
        if (c == '\'' && pattern[i + 1] == '\'')
        {
            i++;
        }
        else if (c == '\'')
        {
            quoted = !quoted;
        }
        else if (!quoted && IsLetter(c))
        {
            previous = c;
            count++;
        }
    }
    if (count > 0 && !repeated && !seen[previous - letterBase])
    {
        count = 0;
    }
    return i - count;
}

// DateIntervalFormat::setIntervalPattern: a pattern, after its
// "latestFirst:" or "earliestFirst:" prefix, split into its halves.
static void SetPair(IntervalPair* pair, const char* pattern, bool laterFirst)
{
    static const char s_later[] = "latestFirst:";
    static const char s_earlier[] = "earliestFirst:";
    if (strncmp(pattern, s_later, sizeof(s_later) - 1) == 0)
    {
        laterFirst = true;
        pattern += sizeof(s_later) - 1;
    }
    else if (strncmp(pattern, s_earlier, sizeof(s_earlier) - 1) == 0)
    {
        laterFirst = false;
        pattern += sizeof(s_earlier) - 1;
    }
    size_t split = SplitPoint(pattern);
    free(pair->first);
    free(pair->second);
    pair->first = CopyPart(pattern, split);
    pair->second = Copy(pattern + split);
    pair->laterFirst = laterFirst;
}

// A fallback: the whole pattern in the second half, none in the first.
static void SetFallback(IntervalPair* pair, char* pattern, bool laterFirst)
{
    free(pair->first);
    free(pair->second);
    pair->first = nullptr;
    pair->second = pattern;
    pair->laterFirst = laterFirst;
}

static bool Empty(const char* text)
{
    return text == nullptr || text[0] == '\0';
}

// The state of one resolution, as DateIntervalFormat keeps it.
typedef struct Resolution
{
    PatternGenerator* g;
    const IntervalItems* items;
    const char* skeleton;
    bool laterFirst;
    IntervalPatterns* out;
} Resolution;

// A best item's pattern for a field, widened, set for the field.
static void SetAdjusted(Resolution* r, int field, const char* skeleton, const char* best,
                        const char* pattern, int difference)
{
    bool suppress = strchr(r->skeleton, 'J') != nullptr;
    if (difference != 0 || suppress)
    {
        char* adjusted = AdjustWidths(skeleton, best, pattern, difference, suppress);
        SetPair(&r->out->pairs[field], adjusted, r->laterFirst);
        free(adjusted);
    }
    else
    {
        SetPair(&r->out->pairs[field], pattern, r->laterFirst);
    }
}

// An item's pattern for a field by its skeleton; null for none.
static const char* PatternOf(const IntervalItems* items, const char* skeleton, int field)
{
    for (int i = 0; i < items->count; i++)
    {
        if (strcmp(items->skeletons[i], skeleton) == 0)
        {
            return items->patterns[i][field];
        }
    }
    return nullptr;
}

// DateIntervalFormat::setIntervalPattern for a field from the best item:
// a missing larger field looked for in the best skeleton with its letter
// prefixed (then that skeleton's best), am or pm taking the hour's. The
// extended skeletons are the caller's, kept across its calls as ICU keeps
// them. Returns whether the field's pattern was set with them extended.
static bool SetFromItem(Resolution* r, int field, const char* skeleton, const char* best,
                        int difference, char* extended, char* extendedBest)
{
    const char* pattern = PatternOf(r->items, best, field);
    if (Empty(pattern))
    {
        if (FieldIgnored(best, field))
        {
            return false;
        }
        if (field == intervalAmPm)
        {
            const char* hour = PatternOf(r->items, best, intervalHour);
            if (!Empty(hour))
            {
                char* adjusted = AdjustWidths(skeleton, best, hour, difference,
                                              strchr(r->skeleton, 'J') != nullptr);
                SetPair(&r->out->pairs[field], adjusted, r->laterFirst);
                free(adjusted);
            }
            return false;
        }
        if (extended != nullptr)
        {
            snprintf(extended, 64, "%c%s", s_letters[field], skeleton);
            snprintf(extendedBest, 64, "%c%s", s_letters[field], best);
            pattern = PatternOf(r->items, extendedBest, field);
            if (Empty(pattern) && difference == 0)
            {
                int match = BestSkeleton(r->items, extendedBest, &difference);
                if (match >= 0 && difference != -1)
                {
                    best = r->items->skeletons[match];
                    pattern = r->items->patterns[match][field];
                }
            }
        }
    }
    if (Empty(pattern))
    {
        return false;
    }
    SetAdjusted(r, field, skeleton, best, pattern, difference);
    return extended != nullptr && extended[0] != '\0';
}

// DateIntervalFormat::setSeparateDateTimePtn: the items' patterns for
// the time part, else the date part; false where no item serves or
// fields differ, as ICU then falls back.
static bool SetSeparate(Resolution* r, const char* date, const char* time)
{
    const char* skeleton = time[0] != '\0' ? time : date;
    int difference = 0;
    int best = BestSkeleton(r->items, skeleton, &difference);
    if (best < 0)
    {
        return false;
    }
    if (date[0] != '\0')
    {
        r->out->datePattern = PatternGeneratorBest(r->g, date);
    }
    if (time[0] != '\0')
    {
        r->out->timePattern = PatternGeneratorBest(r->g, time);
    }
    if (difference == -1)
    {
        return false;
    }
    const char* bestSkeleton = r->items->skeletons[best];
    if (time[0] != '\0')
    {
        SetFromItem(r, intervalMinute, skeleton, bestSkeleton, difference, nullptr, nullptr);
        SetFromItem(r, intervalHour, skeleton, bestSkeleton, difference, nullptr, nullptr);
        SetFromItem(r, intervalAmPm, skeleton, bestSkeleton, difference, nullptr, nullptr);
        return true;
    }
    char extended[64] = "";
    char extendedBest[64] = "";
    SetFromItem(r, intervalDay, skeleton, bestSkeleton, difference, extended, extendedBest);
    if (SetFromItem(r, intervalMonth, skeleton, bestSkeleton, difference, extended, extendedBest))
    {
        bestSkeleton = extendedBest;
        skeleton = extended;
    }
    // The extended skeletons are read and rewritten by the calls below, as
    // ICU passes the same strings in and out.
    char asked[64];
    char best2[64];
    snprintf(asked, sizeof(asked), "%s", skeleton);
    snprintf(best2, sizeof(best2), "%s", bestSkeleton);
    SetFromItem(r, intervalYear, asked, best2, difference, extended, extendedBest);
    snprintf(asked, sizeof(asked), "%s", skeleton);
    snprintf(best2, sizeof(best2), "%s", bestSkeleton);
    SetFromItem(r, intervalEra, asked, best2, difference, extended, extendedBest);
    return true;
}

// A fallback of the skeleton with letters prefixed, for every field
// from the day up that the date part lacks
// (DateIntervalFormat::setFallbackPattern).
static void SetPrefixedFallbacks(Resolution* r, const char* date)
{
    char skeleton[80];
    snprintf(skeleton, sizeof(skeleton), "%s", r->skeleton);
    for (int field = intervalDay; field >= intervalEra; field--)
    {
        if (strchr(date, s_letters[field]) == nullptr)
        {
            memmove(skeleton + 1, skeleton, strlen(skeleton) + 1);
            skeleton[0] = s_letters[field];
            SetFallback(&r->out->pairs[field], PatternGeneratorBest(r->g, skeleton), r->laterFirst);
        }
    }
}

// A time alone: the date fields' fallbacks of yMd short (and G) with it.
static void SetTimeFallbacks(Resolution* r, const char* time)
{
    char skeleton[80];
    snprintf(skeleton, sizeof(skeleton), "yMd%s", time);
    char* pattern = PatternGeneratorBest(r->g, skeleton);
    for (int field = intervalYear; field <= intervalDay; field++)
    {
        SetFallback(&r->out->pairs[field], Copy(pattern), r->laterFirst);
    }
    free(pattern);
    snprintf(skeleton, sizeof(skeleton), "GyMd%s", time);
    SetFallback(&r->out->pairs[intervalEra], PatternGeneratorBest(r->g, skeleton), r->laterFirst);
}

// DateIntervalFormat::concatSingleDate2TimeInterval: a time range's
// pattern inside the glue, with the date's pattern.
static void JoinDate(Resolution* r, const char* glue, const char* datePattern, int field)
{
    IntervalPair* pair = &r->out->pairs[field];
    if (Empty(pair->first))
    {
        return;
    }
    size_t size =
        strlen(glue) + strlen(pair->first) + strlen(pair->second) + strlen(datePattern) + 1;
    char* joined = Allocate(size);
    size_t n = 0;
    for (const char* at = glue; *at != '\0'; at++)
    {
        if (strncmp(at, "{0}", 3) == 0 || strncmp(at, "{1}", 3) == 0)
        {
            const char* part = at[1] == '0' ? pair->first : datePattern;
            memcpy(joined + n, part, strlen(part));
            n += strlen(part);
            if (at[1] == '0')
            {
                memcpy(joined + n, pair->second, strlen(pair->second));
                n += strlen(pair->second);
            }
            at += 2;
            continue;
        }
        joined[n++] = *at;
    }
    SetPair(pair, joined, pair->laterFirst);
    free(joined);
}

void IntervalResolve(PatternGenerator* g, const IntervalItems* items, const char* glue,
                     const char* skeleton, IntervalPatterns* out)
{
    memset(out, 0, sizeof(*out));
    Resolution r = {g, items, skeleton, false, out};
    const char* fallback = items->fallback != nullptr ? items->fallback : "{0} \xE2\x80\x93 {1}";
    const char* first = strstr(fallback, "{0}");
    const char* second = strstr(fallback, "{1}");
    r.laterFirst = first != nullptr && second != nullptr && first > second;
    for (int f = 0; f < intervalFieldCount; f++)
    {
        out->pairs[f].laterFirst = r.laterFirst;
    }
    out->pattern = PatternGeneratorBest(g, skeleton);
    char* normalized = NormalizeHour(g, skeleton);
    char date[80], normalDate[80], time[80], normalTime[80];
    SplitSkeleton(normalized, date, normalDate, time, normalTime);
    free(normalized);
    bool found = SetSeparate(&r, normalDate, normalTime);
    if (time[0] != '\0' && date[0] == '\0')
    {
        SetTimeFallbacks(&r, time);
    }
    else if (found && time[0] != '\0')
    {
        SetPrefixedFallbacks(&r, date);
        if (glue != nullptr && strlen(glue) >= 3)
        {
            char* datePattern = PatternGeneratorBest(g, date);
            JoinDate(&r, glue, datePattern, intervalAmPm);
            JoinDate(&r, glue, datePattern, intervalHour);
            JoinDate(&r, glue, datePattern, intervalMinute);
            free(datePattern);
        }
    }
}

void IntervalPatternsFree(IntervalPatterns* p)
{
    for (int f = 0; f < intervalFieldCount; f++)
    {
        free(p->pairs[f].first);
        free(p->pairs[f].second);
    }
    free(p->pattern);
    free(p->datePattern);
    free(p->timePattern);
    memset(p, 0, sizeof(*p));
}
