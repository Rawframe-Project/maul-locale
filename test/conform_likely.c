// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's conformance data for likely subtags
// (testData/localeIdentifiers/likelySubtags.txt), run on the compiled-in
// data: every source is maximized and minimized, favoring the script and
// the region, and compared with CLDR's answers. CI runs it on the pinned
// release.
//
// The file writes FAIL for every source whose language alone has no
// likely subtags (GenerateLikelyTestData.java does so before it
// maximizes). UTS #35 returns a source with a language, script and
// region unchanged without a lookup, as ICU does, so such a line
// (qaa-Cyrl-CH) is checked as unchanged.
//
// usage: conform_likely <likelySubtags.txt>

#include "maul-locale/locale.h"

#include <stdio.h>
#include <string.h>

// The field'th ";"-separated field of a line, trimmed of spaces and tabs.
static size_t Field(const char* line, int field, const char** startOut)
{
    const char* at = line;
    for (int i = 0; i < field && at != nullptr; i++)
    {
        at = strchr(at, ';');
        at = at == nullptr ? nullptr : at + 1;
    }
    if (at == nullptr)
    {
        *startOut = "";
        return 0;
    }
    size_t length = strcspn(at, ";\r\n");
    while (length > 0 && (*at == ' ' || *at == '\t'))
    {
        at++;
        length--;
    }
    while (length > 0 && (at[length - 1] == ' ' || at[length - 1] == '\t'))
    {
        length--;
    }
    *startOut = at;
    return length;
}

// Whether a result is the text expected ("FAIL" for mloc_errorNotFound).
static bool Matches(mlocResult result, const mlocLocale* locale, const char* expected,
                    size_t length)
{
    if (result != mloc_success)
    {
        return result == mloc_errorNotFound && length == 4 && memcmp(expected, "FAIL", 4) == 0;
    }
    char tag[256];
    mlocOutput output = {.buffer = tag, .capacity = sizeof(tag)};
    return mlocFormatLocale(locale, &output) == mloc_success && output.length == length &&
           memcmp(tag, expected, length) == 0;
}

static bool CheckLine(const mlocDataSet* data, const char* line)
{
    const char* fields[4];
    size_t lengths[4];
    for (int i = 0; i < 4; i++)
    {
        lengths[i] = Field(line, i, &fields[i]);
    }
    // An empty field repeats the one before it.
    for (int i = 2; i < 4; i++)
    {
        if (lengths[i] == 0)
        {
            fields[i] = fields[i - 1];
            lengths[i] = lengths[i - 1];
        }
    }
    mlocLocale source;
    if (mlocParseLocale(fields[0], lengths[0], &source).status != mloc_success)
    {
        return false;
    }
    mlocLocale result;
    bool failing = lengths[1] == 4 && memcmp(fields[1], "FAIL", 4) == 0;
    bool full = source.script[0] != '\0' && source.region[0] != '\0' &&
                memcmp(source.language, "und\0", 4) != 0;
    if (failing && full)
    {
        return mlocMaximizeLocale(data, &source, &result) == mloc_success &&
               memcmp(&result, &source, sizeof(result)) == 0;
    }
    bool ok = Matches(mlocMaximizeLocale(data, &source, &result), &result, fields[1], lengths[1]);
    if (!failing)
    {
        ok = ok && Matches(mlocMinimizeLocale(data, &source, mloc_favorScript, &result), &result,
                           fields[2], lengths[2]);
        ok = ok && Matches(mlocMinimizeLocale(data, &source, mloc_favorRegion, &result), &result,
                           fields[3], lengths[3]);
    }
    return ok;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fputs("usage: conform_likely <likelySubtags.txt>\n", stderr);
        return 2;
    }
    FILE* file = fopen(argv[1], "r");
    if (file == nullptr)
    {
        fprintf(stderr, "conform_likely: cannot open %s\n", argv[1]);
        return 1;
    }
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    char line[1024];
    int passed = 0;
    int failed = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
        {
            continue;
        }
        if (CheckLine(&data, line))
        {
            passed++;
        }
        else
        {
            failed++;
            if (failed <= 20)
            {
                fprintf(stderr, "differs: %s", line);
            }
        }
    }
    fclose(file);
    printf("likely subtags: %d of %d lines agree with CLDR\n", passed, passed + failed);
    return failed == 0 ? 0 : 1;
}
