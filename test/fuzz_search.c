// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for collation search, through the public searcher.
// The first byte chooses a collator; the second the comparison and the
// form, the third where the rest is cut into a pattern and a text, and
// the fourth the position to find from.
//
// In the mapped form each byte after them is a character: ASCII, Latin,
// combining marks, Hangul jamo and syllables, Thai, Khmer and Devanagari
// with their joiners, emoji and their modifiers, Greek, Cyrillic, Arabic,
// Hebrew, CJK and kana, noncharacters, private use and supplementary code
// points. The UTF-8 and UTF-16 searchers must agree, and so must each
// find forward and backward from the same character, a match's bounds
// falling on characters and within the text.
//
// In the raw form the pattern and the text are the bytes as given, often
// not UTF-8, and as 16-bit units, often lone surrogates: each find must
// give a match within the text, no match or a capacity refusal.

#include "maul-locale/collate.h"
#include "maul-locale/data.h"
#include "maul-locale/search.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

enum
{
    maxChars = 512,
};

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static const char* const s_tags[] = {"und",
                                     "en",
                                     "de",
                                     "sv",
                                     "cs",
                                     "da",
                                     "es-u-co-trad",
                                     "vi",
                                     "th",
                                     "km",
                                     "hi",
                                     "ko",
                                     "ja",
                                     "zh",
                                     "ar",
                                     "he",
                                     "en-u-ks-level1",
                                     "en-u-ks-level2",
                                     "en-u-ka-shifted",
                                     "en-u-ka-shifted-ks-level4",
                                     "en-u-ks-identic",
                                     "en-u-kf-upper-kc",
                                     "en-u-kr-grek-latn",
                                     "en-u-kn"};

enum
{
    tags = sizeof(s_tags) / sizeof(s_tags[0]),
};

static mlocData s_sources[2];
static mlocDataSet s_set = {s_sources, 2};
static mlocCollator s_collators[tags];

// The collators made once; false when the data does not open.
static bool Ready(void)
{
    alignas(8) static uint8_t s_file[1 << 22];
    static int s_state;
    if (s_state == 0)
    {
        s_state = -1;
        FILE* f = fopen(MLOC_COLLATION_FILE, "rb");
        size_t n = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
        if (f != nullptr)
        {
            fclose(f);
        }
        mlocOpenBuiltinData(&s_sources[0]);
        bool ok =
            n > 0 && n < sizeof(s_file) && mlocOpenData(&s_sources[1], s_file, n) == mloc_success;
        for (size_t i = 0; ok && i < tags; i++)
        {
            mlocLocale locale;
            ok = mlocParseLocale(s_tags[i], strlen(s_tags[i]), &locale).status == mloc_success &&
                 mlocInitCollator(&s_collators[i], &s_set, &locale, nullptr) == mloc_success;
        }
        s_state = ok ? 1 : -1;
    }
    return s_state == 1;
}

// The character a byte names.
static uint32_t CodePoint(uint8_t b)
{
    static const uint32_t s_groups[8][16] = {
        {0x0300, 0x0301, 0x0302, 0x0303, 0x0308, 0x030A, 0x030C, 0x0323, 0x0327, 0x0328, 0x0334,
         0x034F, 0x035C, 0x1AB0, 0x20D0, 0x3099},
        {0x1100, 0x1101, 0x1112, 0x115F, 0x1160, 0x1161, 0x1175, 0x11A8, 0x11C2, 0xAC00, 0xAC01,
         0xD7A3, 0x3131, 0x314F, 0xFFA0, 0x1F1E6},
        {0x0E01, 0x0E40, 0x0E47, 0x0E48, 0x0EC0, 0x1780, 0x17D2, 0x17B6, 0x0915, 0x094D, 0x0937,
         0x093F, 0x200C, 0x200D, 0x0009, 0x000A},
        {0x1F600, 0x1F469, 0x1F4BB, 0x1F3FB, 0x1F1F7, 0xFE0F, 0x2764, 0x1F44D, 0x20000, 0x2A6D6,
         0x10400, 0x10428, 0xE0061, 0xE007F, 0x1D400, 0x10FFFD},
        {0x0391, 0x03B1, 0x03C2, 0x0410, 0x0430, 0x0451, 0x0627, 0x0644, 0x064B, 0x0670, 0x05D0,
         0x05B4, 0x06F0, 0x0660, 0x0966, 0xFF10},
        {0x4E00, 0x4E2D, 0x5B57, 0x3042, 0x304B, 0x3099, 0x30AB, 0x30FC, 0x3005, 0x2F00, 0xF900,
         0xFF21, 0x3000, 0x3001, 0x31F0, 0x3400},
        {0xFFFD, 0xFFFE, 0xFFFF, 0xFDD0, 0xFDD1, 0xE000, 0xF8FF, 0x00AD, 0x2028, 0x0378, 0xD7FF,
         0x10FFFF, 0x0000, 0x00A0, 0x2060, 0x1D16E},
        {0x0030, 0x0039, 0x002D, 0x2010, 0x2019, 0x201C, 0x00B7, 0x0130, 0x0131, 0x00DF, 0x1E9E,
         0x0149, 0x01C4, 0x01C6, 0xFB01, 0x2126},
    };
    if (b < 0x60)
    {
        return 0x20u + b;
    }
    if (b < 0x80)
    {
        return 0xC0u + (uint32_t)(b - 0x60) * 3;
    }
    return s_groups[(b - 0x80) >> 4][b & 15];
}

// A text in both forms, with the unit each byte boundary falls on.
typedef struct Text
{
    char bytes[maxChars * 4];
    size_t length;
    uint16_t units[maxChars * 2];
    size_t unitLength;
    // For each byte offset: the unit there, or SIZE_MAX inside a
    // character.
    size_t unitAt[maxChars * 4 + 1];
} Text;

static void Encode(const uint8_t* in, size_t count, Text* t)
{
    t->length = 0;
    t->unitLength = 0;
    for (size_t i = 0; i < count; i++)
    {
        uint32_t c = CodePoint(in[i]);
        size_t at = t->length;
        if (c < 0x80)
        {
            t->bytes[t->length++] = (char)c;
        }
        else if (c < 0x800)
        {
            t->bytes[t->length++] = (char)(0xC0 | c >> 6);
            t->bytes[t->length++] = (char)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            t->bytes[t->length++] = (char)(0xE0 | c >> 12);
            t->bytes[t->length++] = (char)(0x80 | (c >> 6 & 0x3F));
            t->bytes[t->length++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            t->bytes[t->length++] = (char)(0xF0 | c >> 18);
            t->bytes[t->length++] = (char)(0x80 | (c >> 12 & 0x3F));
            t->bytes[t->length++] = (char)(0x80 | (c >> 6 & 0x3F));
            t->bytes[t->length++] = (char)(0x80 | (c & 0x3F));
        }
        for (size_t j = at; j < t->length; j++)
        {
            t->unitAt[j] = j == at ? t->unitLength : SIZE_MAX;
        }
        if (c < 0x10000)
        {
            t->units[t->unitLength++] = (uint16_t)c;
        }
        else
        {
            t->units[t->unitLength++] = (uint16_t)(0xD800 | (c - 0x10000) >> 10);
            t->units[t->unitLength++] = (uint16_t)(0xDC00 | (c & 0x3FF));
        }
    }
    t->unitAt[t->length] = t->unitLength;
}

static bool Expected(mlocResult r)
{
    return r == mloc_success || r == mloc_errorNotFound || r == mloc_errorCapacity;
}

// The UTF-8 and UTF-16 finds agree, a match on characters within the
// text.
static void Agree(mlocResult r8, size_t start8, size_t end8, mlocResult r16, size_t start16,
                  size_t end16, const Text* t)
{
    Require(Expected(r8) && r16 == r8);
    if (r8 == mloc_success)
    {
        Require(start8 <= end8 && end8 <= t->length);
        Require(t->unitAt[start8] == start16 && t->unitAt[end8] == end16);
    }
}

static void Mapped(const mlocCollator* c, mlocSearchOptions options, const uint8_t* chars,
                   size_t count, size_t cut, size_t position)
{
    static Text s_pattern;
    static Text s_text;
    static mlocSearcher s_searcher8;
    static mlocSearcher s_searcher16;
    Encode(chars, cut, &s_pattern);
    Encode(chars + cut, count - cut, &s_text);
    mlocResult r8 = mlocInitSearcher(&s_searcher8, c, s_pattern.bytes, s_pattern.length, &options);
    mlocResult r16 =
        mlocInitSearcherUtf16(&s_searcher16, c, s_pattern.units, s_pattern.unitLength, &options);
    Require(r8 == r16);
    if (r8 != mloc_success)
    {
        Require(r8 == mloc_errorInvalid || r8 == mloc_errorCapacity);
        return;
    }
    // From the start of a character.
    size_t from8 = 0;
    for (size_t k = position % (count - cut + 1); k > 0 && from8 < s_text.length;)
    {
        from8++;
        k -= s_text.unitAt[from8] != SIZE_MAX;
    }
    size_t from16 = s_text.unitAt[from8];
    size_t s8 = 0;
    size_t e8 = 0;
    size_t s16 = 0;
    size_t e16 = 0;
    r8 = mlocFindNext(&s_searcher8, s_text.bytes, s_text.length, from8, &s8, &e8);
    r16 = mlocFindNextUtf16(&s_searcher16, s_text.units, s_text.unitLength, from16, &s16, &e16);
    Agree(r8, s8, e8, r16, s16, e16, &s_text);
    r8 = mlocFindPrevious(&s_searcher8, s_text.bytes, s_text.length, from8, &s8, &e8);
    r16 = mlocFindPreviousUtf16(&s_searcher16, s_text.units, s_text.unitLength, from16, &s16, &e16);
    Agree(r8, s8, e8, r16, s16, e16, &s_text);
}

static void Raw(const mlocCollator* c, mlocSearchOptions options, const uint8_t* bytes,
                size_t count, size_t cut, size_t position)
{
    static mlocSearcher s_searcher;
    static uint16_t s_units[maxChars * 4 / 2];
    const char* pattern = (const char*)bytes;
    const char* text = (const char*)bytes + cut;
    size_t length = count - cut;
    size_t start = 0;
    size_t end = 0;
    if (mlocInitSearcher(&s_searcher, c, pattern, cut, &options) == mloc_success)
    {
        size_t from = position % (length + 1);
        mlocResult r = mlocFindNext(&s_searcher, text, length, from, &start, &end);
        Require(Expected(r) && (r != mloc_success || (start <= end && end <= length)));
        r = mlocFindPrevious(&s_searcher, text, length, from, &start, &end);
        Require(Expected(r) && (r != mloc_success || (start <= end && end <= length)));
    }
    size_t patternUnits = cut / 2;
    size_t units = (count - patternUnits * 2) / 2;
    memcpy(s_units, bytes + patternUnits * 2, units * 2);
    static uint16_t s_patternUnits[maxChars * 4 / 2];
    memcpy(s_patternUnits, bytes, patternUnits * 2);
    if (mlocInitSearcherUtf16(&s_searcher, c, s_patternUnits, patternUnits, &options) ==
        mloc_success)
    {
        size_t from = position % (units + 1);
        mlocResult r = mlocFindNextUtf16(&s_searcher, s_units, units, from, &start, &end);
        Require(Expected(r) && (r != mloc_success || (start <= end && end <= units)));
        r = mlocFindPreviousUtf16(&s_searcher, s_units, units, from, &start, &end);
        Require(Expected(r) && (r != mloc_success || (start <= end && end <= units)));
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Require(Ready());
    if (size < 4 || size > 4 + maxChars)
    {
        return 0;
    }
    const mlocCollator* c = &s_collators[data[0] % tags];
    mlocSearchOptions options = {.comparison = (mlocSearchComparison)(data[1] % 3)};
    size_t count = size - 4;
    size_t cut = data[2] % (count + 1);
    if ((data[1] & 0x80) == 0)
    {
        Mapped(c, options, data + 4, count, cut, data[3]);
    }
    else
    {
        Raw(c, options, data + 4, count, cut, data[3]);
    }
    return 0;
}
