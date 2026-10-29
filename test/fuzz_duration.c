// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for durations, the unit names file beside the
// compiled-in data. The first byte chooses a locale, the next 22 the
// options (the style, each field's style and display, fixed digits and
// their count), the next byte how the fields are read: as doubles from
// their bits, or as signed 32-bit integers of one sign. A formatter is
// made or refused as invalid alone; a duration is written or refused as
// invalid alone, the same text twice, its measure the same length, its
// parts covering the text in order.

#include "maul-locale/data.h"
#include "maul-locale/duration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    optionBytes = 22,
    maxText = 2048,
};

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// The compiled-in data and the unit names file; false when the file does
// not open.
static bool OpenData(mlocDataSet* set)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_names[8 << 20];
    static int s_state;
    if (s_state == 0)
    {
        s_state = -1;
        mlocOpenBuiltinData(&s_sources[0]);
        FILE* file = fopen(MLOC_UNITS_FILE, "rb");
        if (file != nullptr)
        {
            size_t size = fread(s_names, 1, sizeof(s_names), file);
            fclose(file);
            if (size > 0 && size < sizeof(s_names) &&
                mlocOpenData(s_names, size, &s_sources[1]) == mloc_success)
            {
                s_state = 1;
            }
        }
    }
    *set = (mlocDataSet){s_sources, 2};
    return s_state == 1;
}

static mlocDurationOptions OptionsOf(const uint8_t* b)
{
    mlocDurationOptions o = {.style = (mlocDurationStyle)(b[0] % 5)};
    for (int i = 0; i < mloc_durationFieldCount; i++)
    {
        o.fields[i] = (mlocDurationFieldStyle)(b[1 + i] % 7);
        o.displays[i] = (mlocDurationDisplay)(b[11 + i] % 4);
    }
    o.fixedFractionalDigits = (b[21] & 0x80) != 0;
    o.fractionalDigits = (uint8_t)(b[21] % 11);
    return o;
}

static mlocDurationValues FieldsOf(const uint8_t* b, size_t size, bool bits)
{
    double v[mloc_durationFieldCount] = {0};
    bool negative = size > 0 && (b[0] & 1) != 0;
    for (size_t i = 0; i < mloc_durationFieldCount; i++)
    {
        size_t width = bits ? 8 : 4;
        size_t at = 1 + i * width;
        if (at + width > size)
        {
            break;
        }
        if (bits)
        {
            memcpy(&v[i], b + at, sizeof(double));
        }
        else
        {
            uint32_t n = (uint32_t)b[at] | (uint32_t)b[at + 1] << 8 | (uint32_t)b[at + 2] << 16 |
                         (uint32_t)b[at + 3] << 24;
            v[i] = negative ? -(double)n : (double)n;
        }
    }
    return (mlocDurationValues){v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]};
}

typedef struct Covered
{
    size_t end;
    bool ordered;
} Covered;

static void Part(void* context, uint32_t kind, size_t start, size_t length)
{
    Covered* c = context;
    c->ordered = c->ordered && start == c->end && length > 0 &&
                 kind / mloc_durationPartField <= mloc_durationFieldCount &&
                 kind % mloc_durationPartField <= mloc_numberPartUnit;
    c->end = start + length;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static const char* const s_locales[] = {"en", "da", "af",      "ar",           "he", "es",
                                            "ja", "my", "ff-Adlm", "hi-u-nu-deva", "cy", "sr"};
    mlocDataSet set;
    if (size < 2 + optionBytes || !OpenData(&set))
    {
        return 0;
    }
    const char* id = s_locales[data[0] % (sizeof(s_locales) / sizeof(s_locales[0]))];
    mlocLocale locale;
    Require(mlocCanonicalizeLocale(&set, id, strlen(id), &locale, nullptr).status == mloc_success);
    mlocDurationOptions o = OptionsOf(data + 1);
    mlocDurationFormatter formatter;
    mlocResult r = mlocInitDurationFormatter(&set, &locale, &o, &formatter);
    Require(r == mloc_success || r == mloc_errorInvalid);
    if (r != mloc_success)
    {
        return 0;
    }
    const uint8_t* rest = data + 1 + optionBytes;
    size_t restSize = size - 1 - optionBytes;
    mlocDurationValues d = FieldsOf(rest + 1, restSize - 1, (rest[0] & 1) != 0);
    mlocDateTimeDuration checked;
    bool valid = mlocMakeDuration(&d, &checked) == mloc_success;
    static char s_text[2][maxText];
    mlocOutput one = {.buffer = s_text[0], .capacity = maxText};
    r = mlocDurationFormatter_Format(&formatter, &d, &one);
    Require(valid ? r == mloc_success : r == mloc_errorInvalid);
    if (!valid)
    {
        return 0;
    }
    Covered covered = {0, true};
    mlocOutput two = {.buffer = s_text[1], .capacity = maxText, .part = Part, .context = &covered};
    mlocOutput measure = {0};
    Require(mlocDurationFormatter_Format(&formatter, &d, &two) == mloc_success &&
            two.length == one.length && memcmp(s_text[0], s_text[1], one.length) == 0 &&
            covered.ordered && covered.end == one.length);
    Require((mlocDurationFormatter_Format(&formatter, &d, &measure) == mloc_errorCapacity ||
             one.length == 0) &&
            measure.length == one.length);
    return 0;
}
