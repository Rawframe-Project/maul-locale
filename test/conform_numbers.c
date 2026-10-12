// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's decimal format test data against the library (record mloc-0021):
// reads the lines of testData/decimal's files ("locale<TAB>number_format
// <TAB>format_length<TAB>input<TAB>expected") and formats each input, a
// double taken as its shortest decimal, on the compiled-in data. CLDR
// made the files with ICU4J's NumberFormatter defaults, which these
// options state: at most six fraction digits (of the mantissa in
// scientific notation), half to even. Rows of compact notation, not yet
// built, are counted as skipped. Where the files
// predate the pinned release's data, tools/number-exceptions.txt names
// the text to replace in a locale's expected lines, with its reason; each
// entry must be used. CI runs it.
//
// usage: cat decimals*.tsv | conform_numbers <number-exceptions.txt>

#include "maul-locale/number.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxExceptions = 32,
};

typedef struct Exception
{
    char locale[16];
    char from[16];
    char to[16];
    bool used;
} Exception;

// Copies a field into a 16-byte one; false when it does not fit.
static bool CopyField(char* out, const char* field)
{
    size_t length = strlen(field);
    if (length >= 16)
    {
        return false;
    }
    memcpy(out, field, length + 1);
    return true;
}

// Reads the exceptions file; -1 when it cannot be read.
static int ReadExceptions(const char* path, Exception* out)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return -1;
    }
    int count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), file) != nullptr && count < maxExceptions)
    {
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }
        Exception* e = &out[count];
        char* fields[3] = {line};
        int n = 1;
        for (char* at = line; *at != '\0' && n < 3; at++)
        {
            if (*at == '\t')
            {
                *at = '\0';
                fields[n++] = at + 1;
            }
        }
        char* end = n == 3 ? strchr(fields[2], '\t') : nullptr;
        if (end == nullptr)
        {
            fclose(file);
            return -1;
        }
        *end = '\0';
        if (!CopyField(e->locale, fields[0]) || !CopyField(e->from, fields[1]) ||
            !CopyField(e->to, fields[2]))
        {
            fclose(file);
            return -1;
        }
        count++;
    }
    fclose(file);
    return count;
}

// Applies a locale's exceptions to an expected text.
static void ApplyExceptions(Exception* exceptions, int count, const char* locale,
                            const char* expected, char* out, size_t capacity)
{
    snprintf(out, capacity, "%s", expected);
    for (int i = 0; i < count; i++)
    {
        Exception* e = &exceptions[i];
        if (strcmp(e->locale, locale) != 0)
        {
            continue;
        }
        char result[256] = "";
        size_t at = 0;
        size_t fromLength = strlen(e->from);
        for (const char* c = out; *c != '\0';)
        {
            const char* piece = strncmp(c, e->from, fromLength) == 0 ? e->to : nullptr;
            size_t length = piece != nullptr ? strlen(piece) : 1;
            if (at + length < sizeof(result))
            {
                memcpy(result + at, piece != nullptr ? piece : c, length);
                at += length;
            }
            e->used |= piece != nullptr;
            c += piece != nullptr ? fromLength : 1;
        }
        result[at] = '\0';
        snprintf(out, capacity, "%s", result);
    }
}

// Formats one row's input; false when a call fails.
static bool Format(const mlocDataSet* data, const char* id, const char* format, const char* input,
                   char* out, size_t capacity, size_t* lengthOut)
{
    mlocLocale locale;
    mlocNumberOptions options = {
        .notation =
            strcmp(format, "scientific") == 0 ? mloc_notationScientific : mloc_notationStandard,
        .style = strcmp(format, "percent") == 0 ? mloc_numberPercent : mloc_numberDecimal,
        .precision = {.kind = mloc_precisionFraction,
                      .mode = mloc_roundHalfEven,
                      .maxFractionDigits = 6},
    };
    mlocNumberFormatter formatter;
    mlocDecimal decimal;
    mlocOutput output = {.buffer = out, .capacity = capacity};
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success ||
        mlocInitNumberFormatter(&formatter, data, &locale, &options) < 0 ||
        mlocMakeShortestDecimal(strtod(input, nullptr), &decimal) != mloc_success ||
        mlocFormatNumber(&formatter, &decimal, &output) != mloc_success)
    {
        return false;
    }
    *lengthOut = output.length;
    return true;
}

int main(int argc, char** argv)
{
    static Exception s_exceptions[maxExceptions];
    int exceptionCount = argc == 2 ? ReadExceptions(argv[1], s_exceptions) : -1;
    if (exceptionCount < 0)
    {
        fputs("usage: conform_numbers <number-exceptions.txt>, the lines on standard input\n",
              stderr);
        return 2;
    }
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    char line[1024];
    long total = 0;
    long agreed = 0;
    long skipped = 0;
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        char* fields[5] = {line};
        int count = 1;
        for (char* at = line; *at != '\0' && count < 5; at++)
        {
            if (*at == '\t')
            {
                *at = '\0';
                fields[count++] = at + 1;
            }
        }
        if (count != 5 || strcmp(fields[0], "locale") == 0)
        {
            continue;
        }
        if (fields[2][0] != '\0')
        {
            skipped++;
            continue;
        }
        total++;
        char text[256];
        size_t length = 0;
        char expected[256];
        ApplyExceptions(s_exceptions, exceptionCount, fields[0], fields[4], expected,
                        sizeof(expected));
        bool ok = Format(&data, fields[0], fields[1], fields[3], text, sizeof(text), &length);
        if (ok && length == strlen(expected) && memcmp(text, expected, length) == 0)
        {
            agreed++;
        }
        else if (total - agreed <= 20)
        {
            printf("%s %s %s: got \"%.*s\", expected \"%s\"\n", fields[0], fields[1], fields[3],
                   ok ? (int)length : 8, ok ? text : "(failed)", expected);
        }
    }
    printf("numbers: %ld of %ld lines agree with CLDR; %ld of notations not yet built\n", agreed,
           total, skipped);
    bool allUsed = true;
    for (int i = 0; i < exceptionCount; i++)
    {
        if (!s_exceptions[i].used)
        {
            printf("exception for %s no longer used\n", s_exceptions[i].locale);
            allUsed = false;
        }
    }
    return agreed == total && total > 0 && allUsed ? 0 : 1;
}
