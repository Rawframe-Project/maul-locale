// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU's data-driven test of its LocaleMatcher (localeMatcherTest.txt), run
// on the compiled-in data as ICU4J's LocaleMatcherTest.dataDriven reads
// it: "** test:" resets the parameters, "@supported=", "@default=",
// "@favor=" and "@threshold=" set them, and "desired >> match | desired |
// combined" checks a match. Every locale is canonicalized first, and
// answers compare as canonical locales. An entry that is no locale id
// ("50", "91") is to ICU a language no data has, so it stands here as
// "qaa", which is reserved for local use. A line listed in the exceptions
// file, with its reason, must differ; any other must agree. CI runs it on
// the pinned file.
//
// usage: conform_match <localeMatcherTest.txt> <match-exceptions.txt>

#include "maul-locale/locale.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxLocales = 1024,
    maxLine = 16384,
    maxExceptions = 256,
};

typedef struct Params
{
    char supported[maxLine];
    char defaultLocale[64];
    char favor[16];
    char threshold[16];
} Params;

typedef struct List
{
    int count;
    mlocLocale locales[maxLocales];
} List;

static const mlocDataSet* g_data;

static void Trim(const char** begin, const char** end)
{
    while (*begin < *end && (**begin == ' ' || **begin == '\t'))
    {
        (*begin)++;
    }
    while (*end > *begin && ((*end)[-1] == ' ' || (*end)[-1] == '\t'))
    {
        (*end)--;
    }
}

static bool Canonical(const char* begin, const char* end, mlocLocale* locale)
{
    return end - begin < 256 &&
           mlocCanonicalizeLocale(g_data, begin, (size_t)(end - begin), locale, nullptr).status ==
               mloc_success;
}

// Reads a comma-separated list as ICU's LocalePriorityList does (no
// weights occur in the file).
static void ReadList(const char* text, size_t length, List* list)
{
    list->count = 0;
    const char* end = text + length;
    for (const char* at = text; at < end && list->count < maxLocales;)
    {
        const char* comma = memchr(at, ',', (size_t)(end - at));
        const char* stop = comma != nullptr ? comma : end;
        const char* b = at;
        const char* e = stop;
        Trim(&b, &e);
        if (b < e)
        {
            if (!Canonical(b, e, &list->locales[list->count]))
            {
                static const char s_unknown[] = "qaa";
                (void)Canonical(s_unknown, s_unknown + 3, &list->locales[list->count]);
            }
            list->count++;
        }
        at = stop + 1;
    }
}

static bool SameAs(const mlocLocale* locale, const char* begin, const char* end)
{
    mlocLocale expected;
    return Canonical(begin, end, &expected) && memcmp(locale, &expected, sizeof(expected)) == 0;
}

static bool IsNull(const char* begin, const char* end)
{
    return end - begin == 4 && memcmp(begin, "null", 4) == 0;
}

// Whether an answer agrees: "null" for none, empty for not tested.
static bool Agrees(const mlocLocale* got, const char* begin, const char* end)
{
    Trim(&begin, &end);
    if (begin == end)
    {
        return true;
    }
    return IsNull(begin, end) ? got == nullptr : got != nullptr && SameAs(got, begin, end);
}

typedef struct Case
{
    const char* desired;
    const char* desiredEnd;
    const char* fields[3];
    const char* fieldEnds[3];
} Case;

static bool Run(const Params* p, const Case* c)
{
    static List s_supported;
    static List s_desired;
    static mlocMatchEntry s_entries[maxLocales];
    ReadList(p->supported, strlen(p->supported), &s_supported);
    ReadList(c->desired, (size_t)(c->desiredEnd - c->desired), &s_desired);
    mlocLocale explicitDefault;
    bool hasDefault = p->defaultLocale[0] != '\0';
    if (hasDefault &&
        !Canonical(p->defaultLocale, p->defaultLocale + strlen(p->defaultLocale), &explicitDefault))
    {
        return false;
    }
    const mlocLocale* def = hasDefault              ? &explicitDefault
                            : s_supported.count > 0 ? &s_supported.locales[0]
                                                    : nullptr;
    mlocMatchOptions options = {.flags =
                                    strcmp(p->favor, "script") == 0 ? mloc_matchFavorScript : 0,
                                .threshold = (uint32_t)atoi(p->threshold)};
    mlocMatch match;
    if (mlocPrepareMatch(g_data, s_supported.locales, (size_t)s_supported.count, def, s_entries) !=
            mloc_success ||
        mlocMatchLocales(g_data, s_supported.locales, s_entries, (size_t)s_supported.count,
                         s_desired.locales, (size_t)s_desired.count, &options,
                         &match) != mloc_success)
    {
        return false;
    }
    const mlocLocale* supported =
        match.supported >= 0 ? &s_supported.locales[match.supported] : def;
    const mlocLocale* desired = match.desired >= 0 ? &s_desired.locales[match.desired] : nullptr;
    mlocLocale combined;
    const mlocLocale* resolved = supported;
    if (supported != nullptr && desired != nullptr)
    {
        if (mlocCombineMatch(desired, supported, &combined) != mloc_success)
        {
            return false;
        }
        resolved = &combined;
    }
    return Agrees(supported, c->fields[0], c->fieldEnds[0]) &&
           Agrees(desired, c->fields[1], c->fieldEnds[1]) &&
           Agrees(resolved, c->fields[2], c->fieldEnds[2]);
}

// Splits "desired >> match | desired | combined" up to limit.
static bool ReadCase(const char* line, const char* limit, Case* c)
{
    const char* sep = strstr(line, ">>");
    if (sep == nullptr || sep + 2 >= limit)
    {
        return false;
    }
    *c = (Case){.desired = line, .desiredEnd = sep};
    Trim(&c->desired, &c->desiredEnd);
    const char* at = sep + 2;
    for (int i = 0; i < 3; i++)
    {
        const char* bar = i < 2 ? memchr(at, '|', (size_t)(limit - at)) : nullptr;
        c->fields[i] = at;
        c->fieldEnds[i] = bar != nullptr ? bar : limit;
        if (bar == nullptr)
        {
            for (int j = i + 1; j < 3; j++)
            {
                c->fields[j] = c->fieldEnds[j] = limit;
            }
            break;
        }
        at = bar + 1;
    }
    return true;
}

static bool Set(const char* line, const char* limit, const char* prefix, char* field, size_t size)
{
    size_t length = strlen(prefix);
    if ((size_t)(limit - line) < length || strncmp(line, prefix, length) != 0)
    {
        return false;
    }
    snprintf(field, size, "%.*s", (int)(limit - line - (ptrdiff_t)length), line + length);
    return true;
}

static int ReadExceptions(const char* path, int* lines)
{
    FILE* file = fopen(path, "r");
    if (file == nullptr)
    {
        fprintf(stderr, "conform_match: cannot open %s\n", path);
        exit(1);
    }
    char text[512];
    int count = 0;
    while (fgets(text, sizeof(text), file) != nullptr && count < maxExceptions)
    {
        if (text[0] >= '0' && text[0] <= '9')
        {
            lines[count++] = atoi(text);
        }
    }
    fclose(file);
    return count;
}

static bool Listed(const int* lines, int count, int line)
{
    for (int i = 0; i < count; i++)
    {
        if (lines[i] == line)
        {
            return true;
        }
    }
    return false;
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fputs("usage: conform_match <localeMatcherTest.txt> <match-exceptions.txt>\n", stderr);
        return 2;
    }
    FILE* file = fopen(argv[1], "r");
    if (file == nullptr)
    {
        fprintf(stderr, "conform_match: cannot open %s\n", argv[1]);
        return 1;
    }
    static int s_exceptions[maxExceptions];
    int exceptionCount = ReadExceptions(argv[2], s_exceptions);
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    g_data = &data;
    static char s_line[maxLine];
    static Params s_params;
    int number = 0;
    int agreed = 0;
    int differed = 0;
    int failures = 0;
    while (fgets(s_line, sizeof(s_line), file) != nullptr)
    {
        number++;
        s_line[strcspn(s_line, "\r\n")] = '\0';
        const char* limit = s_line + strcspn(s_line, "#");
        while (limit > s_line && (limit[-1] == ' ' || limit[-1] == '\t'))
        {
            limit--;
        }
        Case c;
        if (limit == s_line)
        {
            continue;
        }
        if (strncmp(s_line, "** test: ", 9) == 0)
        {
            memset(&s_params, 0, sizeof(s_params));
        }
        else if (Set(s_line, limit, "@supported=", s_params.supported,
                     sizeof(s_params.supported)) ||
                 Set(s_line, limit, "@default=", s_params.defaultLocale,
                     sizeof(s_params.defaultLocale)) ||
                 Set(s_line, limit, "@favor=", s_params.favor, sizeof(s_params.favor)) ||
                 Set(s_line, limit, "@threshold=", s_params.threshold, sizeof(s_params.threshold)))
        {
            continue;
        }
        else if (ReadCase(s_line, limit, &c))
        {
            bool agrees = Run(&s_params, &c);
            bool listed = Listed(s_exceptions, exceptionCount, number);
            agreed += agrees;
            differed += !agrees;
            if (agrees == listed)
            {
                failures++;
                fprintf(stderr, "%s line %d: %s\n", agrees ? "listed but agrees" : "differs",
                        number, s_line);
            }
        }
    }
    fclose(file);
    printf("matching: %d of %d cases agree with ICU, %d listed exceptions\n", agreed,
           agreed + differed, exceptionCount);
    return failures == 0 ? 0 : 1;
}
