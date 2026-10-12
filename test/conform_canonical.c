// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's conformance data for canonicalization
// (testData/localeIdentifiers/localeCanonicalization.txt), run on the
// compiled-in data: every source is canonicalized and written as a BCP
// 47 tag, which must be CLDR's answer with "-" for "_". CI runs it on the
// pinned release.
//
// usage: conform_canonical <localeCanonicalization.txt>

#include "maul-locale/locale.h"

#include <stdio.h>
#include <string.h>

// Copies a field trimmed of spaces and tabs, "_" made "-"; returns its
// length.
static size_t Trimmed(const char* begin, const char* end, char* out, size_t capacity)
{
    while (begin < end && (*begin == ' ' || *begin == '\t'))
    {
        begin++;
    }
    while (end > begin && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
    {
        end--;
    }
    size_t length = (size_t)(end - begin) < capacity ? (size_t)(end - begin) : capacity - 1;
    for (size_t i = 0; i < length; i++)
    {
        out[i] = begin[i] == '_' ? '-' : begin[i];
    }
    out[length] = '\0';
    return length;
}

static bool CheckLine(const mlocDataSet* data, const char* line)
{
    const char* semicolon = strchr(line, ';');
    if (semicolon == nullptr)
    {
        return false;
    }
    char source[256];
    char expected[256];
    size_t sourceLength = Trimmed(line, semicolon, source, sizeof(source));
    size_t expectedLength = Trimmed(semicolon + 1, line + strlen(line), expected, sizeof(expected));
    mlocLocale locale;
    if (mlocCanonicalizeLocale(data, source, sourceLength, &locale, nullptr).status != mloc_success)
    {
        return false;
    }
    char tag[256];
    mlocOutput output = {.buffer = tag, .capacity = sizeof(tag)};
    return mlocFormatLocale(&locale, &output) == mloc_success && output.length == expectedLength &&
           memcmp(tag, expected, expectedLength) == 0;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fputs("usage: conform_canonical <localeCanonicalization.txt>\n", stderr);
        return 2;
    }
    FILE* file = fopen(argv[1], "r");
    if (file == nullptr)
    {
        fprintf(stderr, "conform_canonical: cannot open %s\n", argv[1]);
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
        if (line[0] == '#' || strchr(line, ';') == nullptr)
        {
            continue;
        }
        if (CheckLine(&data, line))
        {
            passed++;
        }
        else if (++failed <= 40)
        {
            fprintf(stderr, "differs: %s", line);
        }
    }
    fclose(file);
    printf("canonicalization: %d of %d lines agree with CLDR\n", passed, passed + failed);
    return failed == 0 ? 0 : 1;
}
