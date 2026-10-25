// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The segmentation data file's unit (segment_tables.h): a unit built here,
// valid, and copies with one field spoiled; then segments.mloc: every unit
// valid, and each locale's tailoring as CLDR's common/segments gives it.

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "segment_tables.h"
#include "test_harness.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

enum
{
    // The unit built here gives every type: CJ as an ideograph, Japanese
    // words, a word override of the full stop, two sentence overrides
    // and two suppressions, "z.B." before "Mr." read from the end.
    builtWords = 1,
    builtSentences = 2,
    builtSuppressions = 2,
    builtWordOverrides = segmentHeaderSize,
    builtSentenceOverrides = builtWordOverrides + segmentOverrideSize * builtWords,
    builtOffsets = builtSentenceOverrides + segmentOverrideSize * builtSentences,
    builtText = builtOffsets + 4 * (builtSuppressions + 1),
};

static const char s_builtText[] = "z.B.Mr.";

alignas(8) static uint8_t s_unit[256];

static uint32_t BuildUnit(void)
{
    memset(s_unit, 0, sizeof(s_unit));
    uint32_t textSize = (uint32_t)strlen(s_builtText);
    Put(s_unit, segmentGivesWord | segmentGivesSentence | segmentGivesLine, 4);
    Put(s_unit + 4, segmentCjIdeographic | segmentJapaneseWords, 4);
    Put(s_unit + 8, builtWords, 4);
    Put(s_unit + 12, builtSentences, 4);
    Put(s_unit + 16, builtSuppressions, 4);
    Put(s_unit + 20, textSize, 4);
    Put(s_unit + builtWordOverrides, 0x2E, 4);
    Put(s_unit + builtWordOverrides + 4, segmentWordMidNum, 4);
    Put(s_unit + builtSentenceOverrides, 0x3B, 4);
    Put(s_unit + builtSentenceOverrides + 4, segmentSentenceSTerm, 4);
    Put(s_unit + builtSentenceOverrides + 8, 0x37E, 4);
    Put(s_unit + builtSentenceOverrides + 12, segmentSentenceSTerm, 4);
    Put(s_unit + builtOffsets + 4, 4, 4);
    Put(s_unit + builtOffsets + 8, textSize, 4);
    memcpy(s_unit + builtText, s_builtText, textSize);
    return builtText + textSize;
}

static bool Valid(uint32_t size)
{
    DataView view = {0};
    return mlocValidSegments(&view, (DataSpan){s_unit, size});
}

// Whether the unit with bytes at an offset spoiled is refused.
static bool Refuses(uint32_t at, uint64_t value, int bytes)
{
    uint32_t size = BuildUnit();
    Put(s_unit + at, value, bytes);
    return !Valid(size);
}

// Whether the unit with its text replaced (the same length) is refused.
static bool TextRefused(const char* text)
{
    uint32_t size = BuildUnit();
    memcpy(s_unit + builtText, text, strlen(s_builtText));
    return !Valid(size);
}

static void TestUnit(void)
{
    uint32_t size = BuildUnit();
    CHECK(Valid(size), "the unit built here");
    CHECK(!Valid(size - 1) && !Valid(segmentHeaderSize + 3), "a short unit");
    alignas(8) static uint8_t s_line[segmentHeaderSize + 4] = {segmentGivesLine};
    DataView view = {0};
    CHECK(mlocValidSegments(&view, (DataSpan){s_line, sizeof(s_line)}),
          "a unit that gives the line alone, root's");
    CHECK(Refuses(0, 0, 4) && Refuses(0, 8, 4) && Refuses(4, 4, 4),
          "no type given, a type or a flag past the known ones");
    CHECK(Refuses(0, segmentGivesWord | segmentGivesSentence, 4) &&
              Refuses(0, segmentGivesSentence | segmentGivesLine, 4) &&
              Refuses(0, segmentGivesWord | segmentGivesLine, 4),
          "a flag or overrides or suppressions of a type not given");
    CHECK(Refuses(8, segmentMaxOverrides + 1, 4) && Refuses(12, segmentMaxOverrides + 1, 4) &&
              Refuses(16, segmentMaxSuppressions + 1, 4) && Refuses(20, 8, 4),
          "the counts past their bounds or the size");
    CHECK(Refuses(builtWordOverrides, 0xD800, 4) && Refuses(builtWordOverrides, 0x110000, 4),
          "a surrogate or a code point past U+10FFFF");
    CHECK(Refuses(builtSentenceOverrides + 8, 0x3B, 4) &&
              Refuses(builtSentenceOverrides + 8, 0x3A, 4),
          "overrides not strictly ascending");
    CHECK(Refuses(builtWordOverrides + 4, segmentWordClassCount, 4) &&
              Refuses(builtSentenceOverrides + 4, segmentSentenceClassCount, 4),
          "a class past its property's");
    CHECK(Refuses(builtOffsets, 1, 4) && Refuses(builtOffsets + 8, 6, 4),
          "the offsets' first and last");
    CHECK(Refuses(builtOffsets + 4, 0, 4) && Refuses(builtOffsets + 4, 8, 4),
          "an empty suppression, offsets out of order");
    CHECK(TextRefused("Mr.z.B."), "suppressions out of order read from the end");
    // The same suppression twice: "Mr." and "Mr.".
    uint32_t twice = BuildUnit() - 1;
    Put(s_unit + 20, 6, 4);
    Put(s_unit + builtOffsets + 4, 3, 4);
    Put(s_unit + builtOffsets + 8, 6, 4);
    memcpy(s_unit + builtText, "Mr.Mr.", 6);
    CHECK(!Valid(twice), "the same suppression twice");
    CHECK(TextRefused("z.B\x01Mr."), "a control byte");
}

alignas(8) static uint8_t s_buffer[1 << 16];
static mlocData s_source;
static mlocDataSet s_data;

static bool OpenFile(const char* path)
{
    FILE* file = fopen(path, "rb");
    size_t read = file == nullptr ? 0 : fread(s_buffer, 1, sizeof(s_buffer), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    return read > 0 && read < sizeof(s_buffer) &&
           mlocOpenData(&s_source, s_buffer, read) == mloc_success;
}

// A locale key's unit, or a span of no bytes.
static DataSpan Unit(const char* key)
{
    DataSpan unit = {nullptr, 0};
    if (!mlocFindDataSetUnit(&s_data, kindSegments, key, strlen(key), &unit))
    {
        unit = (DataSpan){nullptr, 0};
    }
    return unit;
}

// Whether a key's unit gives these types and flags, with these overrides
// (code point and class pairs, the word's then the sentence's) and this
// many suppressions.
static bool Is(const char* key, uint32_t given, uint32_t flags, const uint32_t* words,
               uint32_t wordCount, const uint32_t* sentences, uint32_t sentenceCount,
               uint32_t suppressions)
{
    DataSpan unit = Unit(key);
    if (unit.bytes == nullptr || mlocLoad32(unit.bytes) != given ||
        mlocLoad32(unit.bytes + 4) != flags || mlocLoad32(unit.bytes + 8) != wordCount ||
        mlocLoad32(unit.bytes + 12) != sentenceCount || mlocLoad32(unit.bytes + 16) != suppressions)
    {
        printf("  %s: no unit or another header\n", key);
        return false;
    }
    const uint8_t* at = unit.bytes + segmentHeaderSize;
    for (uint32_t i = 0; i < 2 * (wordCount + sentenceCount); i++)
    {
        uint32_t want = i < 2 * wordCount ? words[i] : sentences[i - 2 * wordCount];
        if (mlocLoad32(at + 4 * (size_t)i) != want)
        {
            printf("  %s: override field %u is %u, want %u\n", key, i,
                   mlocLoad32(at + 4 * (size_t)i), want);
            return false;
        }
    }
    return true;
}

// Whether a key's unit holds a suppression.
static bool Suppresses(const char* key, const char* text)
{
    DataSpan unit = Unit(key);
    if (unit.bytes == nullptr)
    {
        return false;
    }
    uint32_t overrides = mlocLoad32(unit.bytes + 8) + mlocLoad32(unit.bytes + 12);
    uint32_t count = mlocLoad32(unit.bytes + 16);
    const uint8_t* offsets =
        unit.bytes + segmentHeaderSize + segmentOverrideSize * (size_t)overrides;
    const uint8_t* texts = offsets + 4 * ((size_t)count + 1);
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t start = mlocLoad32(offsets + 4 * (size_t)i);
        uint32_t end = mlocLoad32(offsets + 4 * (size_t)i + 4);
        if (end - start == strlen(text) && memcmp(texts + start, text, end - start) == 0)
        {
            return true;
        }
    }
    return false;
}

static void TestFile(void)
{
    bool open = OpenFile(MLOC_SEGMENTS_FILE);
    CHECK(open, "segments.mloc opens: every unit valid");
    if (!open)
    {
        return;
    }
    s_data = (mlocDataSet){&s_source, 1};
    static const uint32_t s_greek[] = {0x3B, segmentSentenceSTerm, 0x37E, segmentSentenceSTerm};
    static const uint32_t s_posix[] = {0x2E, segmentWordMidNum};
    CHECK(Is("el", segmentGivesSentence, 0, nullptr, 0, s_greek, 2, 0),
          "Greek ends a sentence at U+003B and U+037E");
    CHECK(Is("en_US_POSIX", segmentGivesWord, 0, s_posix, 1, nullptr, 0, 0),
          "POSIX English's full stop is MidNum, its sentences its parents'");
    CHECK(Is("ja", segmentGivesWord | segmentGivesLine, segmentCjIdeographic | segmentJapaneseWords,
             nullptr, 0, nullptr, 0, 0),
          "Japanese: CJ as an ideograph and the word rules 13.3 and 13.4");
    CHECK(Is("zh", segmentGivesLine, segmentCjIdeographic, nullptr, 0, nullptr, 0, 0) &&
              Is("zh_Hant", segmentGivesLine, segmentCjIdeographic, nullptr, 0, nullptr, 0, 0),
          "Chinese: CJ as an ideograph");
    CHECK(Is("de", segmentGivesSentence, 0, nullptr, 0, nullptr, 0, 241) &&
              Is("en", segmentGivesSentence, 0, nullptr, 0, nullptr, 0, 151) &&
              Is("ru", segmentGivesSentence, 0, nullptr, 0, nullptr, 0, 18),
          "the suppressions of German, English and Russian");
    CHECK(Suppresses("de", "z.B.") && Suppresses("en", "Mr.") && Suppresses("de", "Ursprüngl.") &&
              Suppresses("fr", "trav. publ.") && !Suppresses("en", "z.B."),
          "suppressions as written, non-ASCII and with a space");
    CHECK(Unit("en_US").bytes == nullptr && Unit("root").bytes == nullptr,
          "no unit for a file that gives no segmentation, or for root");
}

int main(void)
{
    TestUnit();
    TestFile();
    return s_failures == 0 ? 0 : 1;
}
