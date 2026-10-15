// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The time zone database's source (tzdb.h), read field by field as zic's
// infile, inrule, inzone and inlink do.

#include "tzdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxFields = 16,
    maxLine = 1024,
    // zic's limit on an hour field, a week in hours.
    maxHours = 167,
};

static const char* const s_files[] = {"africa",       "antarctica", "asia",
                                      "australasia",  "europe",     "northamerica",
                                      "southamerica", "etcetera",   "backward"};

static const char* const s_months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static const char* const s_weekdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// The months' lengths in a leap year: lastSun and its kind count back
// from these, and a common February takes one day off (zic's rpytime).
static const uint8_t s_leapMonthDays[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// The parse in progress: the line being read and where errors go.
typedef struct Reader
{
    Tzdb* tzdb;
    int lineNumber;
    char* errorOut;
    size_t errorCapacity;
    // The zone a continuation line belongs to, or -1.
    int openZone;
    // The rule set name of each line until the rules are sorted.
    char (*lineRules)[tzdbMaxName];
    bool rearguard;
    // Within a vanguard section (1) or its rearguard alternative (2).
    int section;
} Reader;

static int Fail(Reader* r, const char* what, const char* field)
{
    snprintf(r->errorOut, r->errorCapacity, "tzdb line %d: %s: %s", r->lineNumber, what, field);
    return 0;
}

static void* Grow(void* items, int count, size_t size)
{
    // Doubles at each power of two.
    if (count != 0 && (count & (count - 1)) != 0)
    {
        return items;
    }
    void* grown = realloc(items, (size_t)(count == 0 ? 16 : count * 2) * size);
    if (grown == nullptr)
    {
        fputs("mlocgen: out of memory reading tzdb\n", stderr);
        exit(1);
    }
    return grown;
}

static bool Digits(const char* s, size_t length, int32_t limit, int32_t* out)
{
    if (length == 0 || length > 9)
    {
        return false;
    }
    int32_t value = 0;
    for (size_t i = 0; i < length; i++)
    {
        if (s[i] < '0' || s[i] > '9')
        {
            return false;
        }
        value = value * 10 + (s[i] - '0');
    }
    *out = value;
    return value <= limit;
}

// [-]h[:mm[:ss]], then a suffix from those allowed (the found one, or 0,
// in *suffixOut). "-" alone is zero, as zic reads it.
static bool ParseTime(const char* s, const char* suffixes, int32_t* out, char* suffixOut)
{
    *suffixOut = 0;
    if (strcmp(s, "-") == 0)
    {
        *out = 0;
        return true;
    }
    int sign = *s == '-' ? -1 : 1;
    s += *s == '-';
    size_t length = strlen(s);
    if (length > 0 && suffixes != nullptr && strchr(suffixes, s[length - 1]) != nullptr)
    {
        *suffixOut = s[--length];
    }
    int32_t parts[3] = {0};
    static const int32_t s_limits[3] = {maxHours, 59, 59};
    int part = 0;
    for (size_t at = 0;;)
    {
        size_t end = at;
        while (end < length && s[end] != ':')
        {
            end++;
        }
        if ((part > 0 && end - at != 2) || !Digits(s + at, end - at, s_limits[part], &parts[part]))
        {
            return false;
        }
        if (end == length)
        {
            break;
        }
        if (++part == 3)
        {
            return false;
        }
        at = end + 1;
    }
    *out = sign * (parts[0] * 3600 + parts[1] * 60 + parts[2]);
    return true;
}

static bool ParseYear(const char* s, int32_t* out)
{
    int sign = *s == '-' ? -1 : 1;
    s += *s == '-';
    if (!Digits(s, strlen(s), 999999, out))
    {
        return false;
    }
    *out *= sign;
    return true;
}

static int Find(const char* const* names, int count, const char* s)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(names[i], s) == 0)
        {
            return i;
        }
    }
    return -1;
}

// A time with its base letter: w (or none) wall, s standard, u, g or z
// universal.
static bool ParseAt(const char* s, TzdbDate* date)
{
    char suffix = 0;
    if (!ParseTime(s, "wsugz", &date->time, &suffix))
    {
        return false;
    }
    date->base = suffix == 's'                  ? tzdbStandard
                 : suffix == 0 || suffix == 'w' ? tzdbWall
                                                : tzdbUniversal;
    return true;
}

// A day in date->month: N, lastSun, Sun>=N or Sun<=N.
static bool ParseOn(const char* s, TzdbDate* date)
{
    uint8_t length = s_leapMonthDays[date->month];
    int32_t day = 0;
    if (strncmp(s, "last", 4) == 0)
    {
        int weekday = Find(s_weekdays, 7, s + 4);
        date->day = tzdbWeekdayOnOrBefore;
        date->weekday = (uint8_t)weekday;
        date->dayOfMonth = length;
        return weekday >= 0;
    }
    const char* relation = strpbrk(s, "<>");
    if (relation == nullptr)
    {
        date->day = tzdbDayOfMonth;
        bool ok = Digits(s, strlen(s), length, &day) && day >= 1;
        date->dayOfMonth = (uint8_t)day;
        return ok;
    }
    char weekday[4] = {0};
    if (relation - s != 3 || relation[1] != '=')
    {
        return false;
    }
    memcpy(weekday, s, 3);
    int found = Find(s_weekdays, 7, weekday);
    date->day = *relation == '>' ? tzdbWeekdayOnOrAfter : tzdbWeekdayOnOrBefore;
    date->weekday = (uint8_t)found;
    bool ok = found >= 0 && Digits(relation + 2, strlen(relation + 2), length, &day) && day >= 1;
    date->dayOfMonth = (uint8_t)day;
    return ok;
}

static bool CopyName(char* out, const char* name)
{
    size_t length = strlen(name);
    if (length == 0 || length >= tzdbMaxName)
    {
        return false;
    }
    memcpy(out, name, length + 1);
    return true;
}

// Rule NAME FROM TO - IN ON AT SAVE LETTERS
static int ReadRule(Reader* r, char** f, int n)
{
    if (n != 10)
    {
        return Fail(r, "a Rule line needs 10 fields", f[0]);
    }
    Tzdb* t = r->tzdb;
    t->rules = Grow(t->rules, t->ruleCount, sizeof(TzdbRule));
    TzdbRule* rule = &t->rules[t->ruleCount];
    *rule = (TzdbRule){.sourceIndex = t->ruleCount};
    int month = Find(s_months, 12, f[5]);
    char suffix = 0;
    if (!CopyName(rule->name, f[1]) || !ParseYear(f[2], &rule->from) || strcmp(f[4], "-") != 0 ||
        month < 0)
    {
        return Fail(r, "a Rule's name, FROM, TYPE or IN is not taken", f[1]);
    }
    rule->toMax = strcmp(f[3], "max") == 0;
    rule->to = rule->from;
    if (!rule->toMax && strcmp(f[3], "only") != 0 &&
        (!ParseYear(f[3], &rule->to) || rule->to < rule->from))
    {
        return Fail(r, "a Rule's TO is not taken", f[3]);
    }
    rule->at.month = (uint8_t)month;
    if (!ParseOn(f[6], &rule->at) || !ParseAt(f[7], &rule->at) ||
        !ParseTime(f[8], nullptr, &rule->save, &suffix))
    {
        return Fail(r, "a Rule's ON, AT or SAVE is not taken", f[6]);
    }
    t->ruleCount++;
    return 1;
}

// STDOFF RULES FORMAT [UNTIL], from fields f[0] on.
static int ReadZoneFields(Reader* r, char** f, int n)
{
    if (n < 3 || n > 7)
    {
        return Fail(r, "a Zone line needs 3 to 7 fields", n > 0 ? f[0] : "");
    }
    Tzdb* t = r->tzdb;
    t->lines = Grow(t->lines, t->lineCount, sizeof(TzdbLine));
    r->lineRules = Grow(r->lineRules, t->lineCount, sizeof(*r->lineRules));
    TzdbLine* line = &t->lines[t->lineCount];
    *line = (TzdbLine){0};
    char* rules = r->lineRules[t->lineCount];
    rules[0] = '\0';
    char suffix = 0;
    if (!ParseTime(f[0], nullptr, &line->standard, &suffix))
    {
        return Fail(r, "a Zone's STDOFF is not taken", f[0]);
    }
    bool fixed = strcmp(f[1], "-") == 0 || f[1][0] == '-' || (f[1][0] >= '0' && f[1][0] <= '9');
    if (fixed ? !ParseTime(f[1], nullptr, &line->save, &suffix) : !CopyName(rules, f[1]))
    {
        return Fail(r, "a Zone's RULES is not taken", f[1]);
    }
    line->hasUntil = n > 3;
    line->until = (TzdbDate){.dayOfMonth = 1};
    int month = n > 4 ? Find(s_months, 12, f[4]) : 0;
    line->until.month = (uint8_t)(month < 0 ? 0 : month);
    if ((n > 3 && !ParseYear(f[3], &line->untilYear)) || month < 0 ||
        (n > 5 && !ParseOn(f[5], &line->until)) || (n > 6 && !ParseAt(f[6], &line->until)))
    {
        return Fail(r, "a Zone's UNTIL is not taken", f[3]);
    }
    t->zones[r->openZone].lineCount++;
    t->lineCount++;
    r->openZone = line->hasUntil ? r->openZone : -1;
    return 1;
}

static int ReadZone(Reader* r, char** f, int n)
{
    Tzdb* t = r->tzdb;
    t->zones = Grow(t->zones, t->zoneCount, sizeof(TzdbZone));
    TzdbZone* zone = &t->zones[t->zoneCount];
    *zone = (TzdbZone){.lineFirst = t->lineCount};
    if (n < 2 || !CopyName(zone->name, f[1]))
    {
        return Fail(r, "a Zone's name is not taken", n > 1 ? f[1] : f[0]);
    }
    r->openZone = t->zoneCount++;
    return ReadZoneFields(r, f + 2, n - 2);
}

static int ReadLink(Reader* r, char** f, int n)
{
    Tzdb* t = r->tzdb;
    t->links = Grow(t->links, t->linkCount, sizeof(TzdbLink));
    TzdbLink* link = &t->links[t->linkCount];
    if (n != 3 || !CopyName(link->target, f[1]) || !CopyName(link->name, f[2]))
    {
        return Fail(r, "a Link line needs a target and a name", f[0]);
    }
    t->linkCount++;
    return 1;
}

// Splits a line at blanks after cutting its comment. Quoted fields are
// not taken.
static int Split(char* line, char** fields, bool* quoted)
{
    line[strcspn(line, "#")] = '\0';
    *quoted = strchr(line, '"') != nullptr;
    int n = 0;
    for (char* p = strtok(line, " \t\r\v\f"); p != nullptr && n < maxFields;
         p = strtok(nullptr, " \t\r\v\f"))
    {
        fields[n++] = p;
    }
    return n;
}

// Follows the section markers. Returns the text to read: the line, the
// line after its "#" in a rearguard alternative read in that form, or
// null for a line to skip.
static char* SectionText(Reader* r, char* text)
{
    static const char* const s_markers[3] = {"# End of rearguard section", "# Vanguard section",
                                             "# Rearguard section"};
    for (int i = 0; i < 3; i++)
    {
        if (strncmp(text, s_markers[i], strlen(s_markers[i])) == 0)
        {
            r->section = i;
            return nullptr;
        }
    }
    if (r->rearguard && r->section == 1)
    {
        return nullptr;
    }
    bool data =
        text[0] == '#' && (text[1] == '\t' || strncmp(text + 1, "Rule", 4) == 0 ||
                           strncmp(text + 1, "Zone", 4) == 0 || strncmp(text + 1, "Link", 4) == 0);
    return r->rearguard && r->section == 2 && data ? text + 1 : text;
}

static int ReadLine(Reader* r, char* line)
{
    char* text = SectionText(r, line);
    if (text == nullptr)
    {
        return 1;
    }
    bool continuation = text[0] == ' ' || text[0] == '\t';
    char* fields[maxFields];
    bool quoted = false;
    int n = Split(text, fields, &quoted);
    if (quoted || n == maxFields)
    {
        return Fail(r, "a quoted field or too many fields", n > 0 ? fields[0] : "");
    }
    if (n == 0)
    {
        return 1;
    }
    if (r->openZone >= 0)
    {
        return continuation ? ReadZoneFields(r, fields, n)
                            : Fail(r, "a Zone line's UNTIL needs a continuation", fields[0]);
    }
    if (strcmp(fields[0], "Rule") == 0)
    {
        return ReadRule(r, fields, n);
    }
    if (strcmp(fields[0], "Zone") == 0)
    {
        return ReadZone(r, fields, n);
    }
    if (strcmp(fields[0], "Link") == 0)
    {
        return ReadLink(r, fields, n);
    }
    return Fail(r, "a line is not Rule, Zone or Link", fields[0]);
}

static int CompareRules(const void* a, const void* b)
{
    const TzdbRule* x = a;
    const TzdbRule* y = b;
    int c = strcmp(x->name, y->name);
    // The source order within a set, for the same result on every libc.
    return c != 0 ? c : (x->sourceIndex > y->sourceIndex) - (x->sourceIndex < y->sourceIndex);
}

static int CompareZones(const void* a, const void* b)
{
    return strcmp(((const TzdbZone*)a)->name, ((const TzdbZone*)b)->name);
}

static int CompareLinks(const void* a, const void* b)
{
    return strcmp(((const TzdbLink*)a)->name, ((const TzdbLink*)b)->name);
}

// Sorts rule sets, zones and links, and points each line at its rules.
static int Finish(Reader* r)
{
    Tzdb* t = r->tzdb;
    qsort(t->rules, (size_t)t->ruleCount, sizeof(TzdbRule), CompareRules);
    qsort(t->zones, (size_t)t->zoneCount, sizeof(TzdbZone), CompareZones);
    qsort(t->links, (size_t)t->linkCount, sizeof(TzdbLink), CompareLinks);
    for (int i = 1; i < t->zoneCount; i++)
    {
        if (strcmp(t->zones[i - 1].name, t->zones[i].name) == 0)
        {
            return Fail(r, "a zone twice", t->zones[i].name);
        }
    }
    for (int i = 1; i < t->linkCount; i++)
    {
        if (strcmp(t->links[i - 1].name, t->links[i].name) == 0)
        {
            return Fail(r, "a link twice", t->links[i].name);
        }
    }
    for (int i = 0; i < t->lineCount; i++)
    {
        const char* name = r->lineRules[i];
        if (name[0] == '\0')
        {
            continue;
        }
        int first = 0;
        while (first < t->ruleCount && strcmp(t->rules[first].name, name) != 0)
        {
            first++;
        }
        int last = first;
        while (last < t->ruleCount && strcmp(t->rules[last].name, name) == 0)
        {
            last++;
        }
        if (first == last)
        {
            return Fail(r, "a zone line names no rule set", name);
        }
        t->lines[i].ruleFirst = first;
        t->lines[i].ruleCount = last - first;
    }
    return 1;
}

int TzdbParse(const char* text, bool rearguard, Tzdb* out, char* errorOut, size_t errorCapacity)
{
    *out = (Tzdb){0};
    Reader r = {.tzdb = out,
                .errorOut = errorOut,
                .errorCapacity = errorCapacity,
                .openZone = -1,
                .rearguard = rearguard};
    char line[maxLine];
    int ok = 1;
    for (const char* p = text; ok && *p != '\0';)
    {
        size_t length = strcspn(p, "\n");
        r.lineNumber++;
        if (length >= sizeof(line))
        {
            ok = Fail(&r, "a line is too long", "");
            break;
        }
        memcpy(line, p, length);
        line[length] = '\0';
        ok = ReadLine(&r, line);
        p += length + (p[length] == '\n');
    }
    if (ok && r.openZone >= 0)
    {
        ok = Fail(&r, "the last Zone line's UNTIL needs a continuation", "");
    }
    ok = ok && Finish(&r);
    free(r.lineRules);
    if (!ok)
    {
        TzdbFree(out);
    }
    return ok;
}

int TzdbRead(const char* directory, bool rearguard, Tzdb* out, char* errorOut, size_t errorCapacity)
{
    char* text = nullptr;
    size_t size = 0;
    for (size_t i = 0; i < sizeof(s_files) / sizeof(s_files[0]); i++)
    {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", directory, s_files[i]);
        FILE* file = fopen(path, "rb");
        if (file == nullptr)
        {
            free(text);
            snprintf(errorOut, errorCapacity, "cannot open %s", path);
            return 0;
        }
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        char* grown = length < 0 ? nullptr : realloc(text, size + (size_t)length + 2);
        bool read =
            grown != nullptr && fread(grown + size, 1, (size_t)length, file) == (size_t)length;
        fclose(file);
        if (!read)
        {
            free(grown != nullptr ? grown : text);
            snprintf(errorOut, errorCapacity, "cannot read %s", path);
            return 0;
        }
        text = grown;
        size += (size_t)length;
        // A file's last line ends here even without its newline.
        text[size++] = '\n';
        text[size] = '\0';
    }
    int ok = TzdbParse(text, rearguard, out, errorOut, errorCapacity);
    free(text);
    return ok;
}

void TzdbFree(Tzdb* tzdb)
{
    free(tzdb->rules);
    free(tzdb->lines);
    free(tzdb->zones);
    free(tzdb->links);
    *tzdb = (Tzdb){0};
}
