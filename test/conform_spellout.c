// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's rule-based number format test data against the library (record
// mloc-0034): reads testData/rbnf's files ("type;rule set;number;
// expected", the file named for its locale) and formats each number by
// the rule set on the compiled-in data and the spell-out file. A line
// that begins with a stray ';' (ak.ssv has 23) is read without it.
// "Infinity", "-Infinity" and "NaN" are those values. CI runs it.
//
// usage: conform_spellout <spellout.mloc> <file.ssv>...

#include "maul-locale/data.h"
#include "maul-locale/spellout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxLine = 4096,
    maxFile = 4 << 20,
};

static bool OpenFile(const char* path, mlocData* data)
{
    alignas(8) static uint8_t s_bytes[maxFile];
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    size_t size = fread(s_bytes, 1, sizeof(s_bytes), file);
    fclose(file);
    return size < sizeof(s_bytes) && mlocOpenData(data, s_bytes, size) == mloc_success;
}

// The locale of a file's name ("sr_Latn.ssv" is sr-Latn).
static bool LocaleOf(const char* path, mlocLocale* locale)
{
    const char* slash = strrchr(path, '/');
    const char* name = slash != nullptr ? slash + 1 : path;
    const char* dot = strrchr(name, '.');
    char tag[64];
    size_t length = dot != nullptr ? (size_t)(dot - name) : strlen(name);
    if (length >= sizeof(tag))
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        tag[i] = name[i] == '_' ? '-' : name[i];
    }
    return mlocParseLocale(tag, length, locale).status == mloc_success;
}

static bool ReadNumber(const char* text, mlocDecimal* out)
{
    if (strcmp(text, "Infinity") == 0 || strcmp(text, "-Infinity") == 0)
    {
        *out = (mlocDecimal){.kind = mloc_decimalInfinity, .negative = text[0] == '-' ? 1 : 0};
        return true;
    }
    if (strcmp(text, "NaN") == 0)
    {
        *out = (mlocDecimal){.kind = mloc_decimalNaN};
        return true;
    }
    return mlocParseDecimal(text, strlen(text), out).status == mloc_success;
}

// Checks a line; false with a message when it fails.
static bool CheckLine(const mlocDataSet* data, const mlocLocale* locale, char* line,
                      const char* path)
{
    char* fields[4];
    char* at = line[0] == ';' ? line + 1 : line;
    for (int k = 0; k < 4; k++)
    {
        fields[k] = at;
        char* semi = k < 3 ? strchr(at, ';') : nullptr;
        if (k < 3 && semi == nullptr)
        {
            printf("%s: a line of another shape: %s\n", path, line);
            return false;
        }
        if (semi != nullptr)
        {
            *semi = '\0';
            at = semi + 1;
        }
    }
    mlocSpelloutFormatter formatter;
    mlocDecimal decimal;
    static char s_text[maxLine];
    mlocOutput output = {.buffer = s_text, .capacity = sizeof(s_text)};
    const char* name = fields[1][0] != '\0' ? fields[1] : nullptr;
    if (mlocInitSpelloutFormatter(&formatter, data, locale, name,
                                  name != nullptr ? strlen(name) : 0) < 0 ||
        !ReadNumber(fields[2], &decimal) ||
        mlocFormatSpellout(&formatter, &decimal, &output) != mloc_success ||
        output.length != strlen(fields[3]) || memcmp(s_text, fields[3], output.length) != 0)
    {
        printf("%s: %s %s: got \"%.*s\", expected \"%s\"\n", path, fields[1], fields[2],
               (int)output.length, s_text, fields[3]);
        return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    mlocData sources[2];
    if (argc < 3 || !OpenFile(argv[1], &sources[1]))
    {
        fputs("usage: conform_spellout <spellout.mloc> <file.ssv>...\n", stderr);
        return 2;
    }
    mlocOpenBuiltinData(&sources[0]);
    mlocDataSet data = {sources, 2};
    int lines = 0;
    int failures = 0;
    for (int i = 2; i < argc; i++)
    {
        mlocLocale locale;
        FILE* file = fopen(argv[i], "r");
        if (file == nullptr || !LocaleOf(argv[i], &locale))
        {
            printf("%s: cannot read\n", argv[i]);
            return 2;
        }
        static char s_line[maxLine];
        while (fgets(s_line, sizeof(s_line), file) != nullptr)
        {
            s_line[strcspn(s_line, "\r\n")] = '\0';
            if (s_line[0] == '#' || s_line[0] == '\0')
            {
                continue;
            }
            lines++;
            failures += CheckLine(&data, &locale, s_line, argv[i]) ? 0 : 1;
        }
        fclose(file);
    }
    printf("conform_spellout: %d lines, %d failed\n", lines, failures);
    return failures == 0 && lines > 0 ? 0 : 1;
}
