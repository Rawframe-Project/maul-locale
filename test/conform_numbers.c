// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's decimal format test data against the library: reads the lines of
// testData/decimal's files ("locale<TAB>number_format
// <TAB>format_length<TAB>input<TAB>expected") and formats each input, a
// double taken as its shortest decimal, on the compiled-in data. CLDR made
// the files with ICU4J's NumberFormatter defaults, which these options
// state: at most six fraction digits (of the mantissa in scientific
// notation), half to even; compact rows round by compact notation's own
// rule, half to even. Where the files predate the pinned release's data,
// tools/number-exceptions.txt names the text to replace in a locale's
// expected lines, or a line's expected text, with its reason; each entry
// must be used. CI runs it.
//
// usage: cat decimals*.tsv | conform_numbers <number-exceptions.txt>

#include "maul-locale/number.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxExceptions = 32,
    maxField = 128,
};

// An entry of the exceptions file: a text to replace in a locale's
// expected lines (locale, text, replacement, reason), or one line's
// expected text (locale, format, length, input, text, reason).
typedef struct Exception
{
    char locale[16];
    // For a line, its format, length and input, tab-separated.
    char line[maxField];
    char from[maxField];
    char to[maxField];
    bool used;
} Exception;

// Copies a field into one of maxField bytes; false when it does not fit.
static bool CopyField(char* out, const char* field, size_t capacity)
{
    size_t length = strlen(field);
    if (length >= capacity)
    {
        return false;
    }
    memcpy(out, field, length + 1);
    return true;
}

// Splits a line at its tabs, its end of line removed; the field count.
static int Split(char* line, char** fields, int capacity)
{
    line[strcspn(line, "\r\n")] = '\0';
    int n = 1;
    fields[0] = line;
    for (char* at = line; *at != '\0' && n < capacity; at++)
    {
        if (*at == '\t')
        {
            *at = '\0';
            fields[n++] = at + 1;
        }
    }
    return n;
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
    bool valid = true;
    char text[1024];
    while (valid && fgets(text, sizeof(text), file) != nullptr)
    {
        if (text[0] == '#' || text[0] == '\n')
        {
            continue;
        }
        char* f[6];
        int n = Split(text, f, 6);
        Exception* e = &out[count];
        memset(e, 0, sizeof(*e));
        if (count == maxExceptions || (n != 4 && n != 6) ||
            !CopyField(e->locale, f[0], sizeof(e->locale)))
        {
            valid = false;
        }
        else if (n == 4)
        {
            valid = CopyField(e->from, f[1], maxField) && CopyField(e->to, f[2], maxField);
        }
        else
        {
            valid =
                (size_t)snprintf(e->line, maxField, "%s\t%s\t%s", f[1], f[2], f[3]) < maxField &&
                CopyField(e->to, f[4], maxField);
        }
        count += valid;
    }
    fclose(file);
    return valid ? count : -1;
}

// Applies a locale's exceptions to a line's expected text.
static void ApplyExceptions(Exception* exceptions, int count, char** fields, char* out,
                            size_t capacity)
{
    snprintf(out, capacity, "%s", fields[4]);
    char line[3 * maxField];
    snprintf(line, sizeof(line), "%s\t%s\t%s", fields[1], fields[2], fields[3]);
    for (int i = 0; i < count; i++)
    {
        Exception* e = &exceptions[i];
        if (strcmp(e->locale, fields[0]) != 0)
        {
            continue;
        }
        if (e->line[0] != '\0')
        {
            if (strcmp(e->line, line) == 0)
            {
                snprintf(out, capacity, "%s", e->to);
                e->used = true;
            }
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

// Formats one row's input; false when a call fails. Compact rows round
// by compact notation's own rule.
static bool Format(const mlocDataSet* data, const char* id, const char* format, const char* length,
                   const char* input, char* out, size_t capacity, size_t* lengthOut)
{
    mlocLocale locale;
    bool compact = length[0] != '\0';
    mlocNotation notation = strcmp(format, "scientific") == 0 ? mloc_notationScientific
                            : strcmp(length, "short") == 0    ? mloc_notationCompactShort
                            : strcmp(length, "long") == 0     ? mloc_notationCompactLong
                                                              : mloc_notationStandard;
    mlocNumberOptions options = {
        .notation = notation,
        .style = strcmp(format, "percent") == 0 ? mloc_numberPercent : mloc_numberDecimal,
        .precision = {.kind = compact ? mloc_precisionNone : mloc_precisionFraction,
                      .mode = mloc_roundHalfEven,
                      .maxFractionDigits = compact ? 0 : 6},
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
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        char* fields[5];
        if (Split(line, fields, 5) != 5 || strcmp(fields[0], "locale") == 0)
        {
            continue;
        }
        total++;
        char text[256];
        size_t length = 0;
        char expected[256];
        ApplyExceptions(s_exceptions, exceptionCount, fields, expected, sizeof(expected));
        bool ok =
            Format(&data, fields[0], fields[1], fields[2], fields[3], text, sizeof(text), &length);
        if (ok && length == strlen(expected) && memcmp(text, expected, length) == 0)
        {
            agreed++;
        }
        else if (total - agreed <= 20)
        {
            printf("%s %s %s %s: got \"%.*s\", expected \"%s\"\n", fields[0], fields[1], fields[2],
                   fields[3], ok ? (int)length : 8, ok ? text : "(failed)", expected);
        }
    }
    printf("numbers: %ld of %ld lines agree with CLDR\n", agreed, total);
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
