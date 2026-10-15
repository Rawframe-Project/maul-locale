// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Formats dates read one per line from standard input, for
// tools/check_datetime.py to compare with CLDR's datetime conformance
// data (record mloc-0042). Each line is tab-separated: the locale (its
// -u-ca key naming the calendar), the field set, length, year style and
// standard style as mlocDateOptions' numbers, and an ISO date
// (YYYY-MM-DD); the output line is the formatted date, or "error N"
// with the library's result.
//
// usage: conform_datetime dates.mloc spellout.mloc < lines

#define _CRT_SECURE_NO_WARNINGS

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mlocData s_sources[3];

// Reads a data file into memory and opens it as source index.
static bool Open(const char* path, int index)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    uint8_t* bytes = malloc((size_t)16 << 20);
    size_t read = bytes == nullptr ? 0 : fread(bytes, 1, (size_t)16 << 20, file);
    fclose(file);
    return read > 0 && mlocOpenData(&s_sources[index], bytes, read) == mloc_success;
}

static mlocResult Format(const mlocDataSet* set, const char* id, const mlocDateOptions* options,
                         int year, int month, int day, char* out, size_t* length)
{
    mlocLocale locale;
    mlocDate date;
    mlocDateFormatter formatter;
    mlocDateInput input = {.year = year, .month = (uint32_t)month, .day = (uint32_t)day};
    if (mlocCanonicalizeLocale(set, id, strlen(id), &locale, nullptr).status != mloc_success)
    {
        return mloc_errorSyntax;
    }
    mlocResult result = mlocMakeDate(mloc_calendarIso8601, &input, mloc_overflowReject, &date);
    result = result < 0 ? result : mlocInitDateFormatter(&formatter, set, &locale, options);
    if (result < 0)
    {
        return result;
    }
    mlocOutput output = {.buffer = out, .capacity = 1024};
    result = mlocFormatDate(&formatter, &date, &output);
    *length = output.length;
    return result;
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fputs("usage: conform_datetime dates.mloc spellout.mloc < lines\n", stderr);
        return 2;
    }
    mlocOpenBuiltinData(&s_sources[0]);
    if (!Open(argv[1], 1) || !Open(argv[2], 2))
    {
        fputs("conform_datetime: cannot open the data files\n", stderr);
        return 2;
    }
    mlocDataSet set = {s_sources, 3};
    char line[1024];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        char id[256];
        int fields = 0;
        int length = 0;
        int yearStyle = 0;
        int style = 0;
        int year = 0;
        int month = 0;
        int day = 0;
        char out[1024];
        size_t written = 0;
        if (sscanf(line, "%255s %d %d %d %d %d-%d-%d", id, &fields, &length, &yearStyle, &style,
                   &year, &month, &day) != 8)
        {
            puts("error syntax");
            continue;
        }
        mlocDateOptions options = {(mlocDateFieldSet)fields, (mlocDateLength)length,
                                   (mlocYearStyle)yearStyle, (mlocDateStyle)style};
        mlocResult result = Format(&set, id, &options, year, month, day, out, &written);
        if (result < 0)
        {
            printf("error %d\n", result);
            continue;
        }
        printf("%.*s\n", (int)written, out);
    }
    return 0;
}
