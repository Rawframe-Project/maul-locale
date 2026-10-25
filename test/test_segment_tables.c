// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The segmentation data file's unit (segment_tables.h): a unit built here,
// valid, and copies with one field spoiled; then segments.mloc: every unit
// valid, and each locale's tailoring as CLDR's common/segments gives it,
// its suppressions in ICU4C's two tables.

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
    // words, a word override of the full stop, two sentence overrides,
    // the backward entries "Dr." and "Mr." (matches) and "z." (partial),
    // read from the end, and the forward entries "z.B." and "z.T.".
    builtWords = 1,
    builtSentences = 2,
    builtBackward = 3,
    builtForward = 2,
    builtWordOverrides = segmentHeaderSize,
    builtSentenceOverrides = builtWordOverrides + segmentOverrideSize * builtWords,
    builtValues = builtSentenceOverrides + segmentOverrideSize * builtSentences,
    builtOffsets = builtValues + 4 * builtBackward,
    builtText = builtOffsets + 4 * (builtBackward + builtForward + 1),
};

static const char s_builtText[] = "Dr.Mr.z.z.B.z.T.";
static const uint32_t s_builtOffsets[] = {0, 3, 6, 8, 12, 16};

alignas(8) static uint8_t s_unit[256];

static uint32_t BuildUnit(void)
{
    memset(s_unit, 0, sizeof(s_unit));
    uint32_t textSize = (uint32_t)strlen(s_builtText);
    Put(s_unit, segmentGivesWord | segmentGivesSentence | segmentGivesLine, 4);
    Put(s_unit + 4, segmentCjIdeographic | segmentJapaneseWords, 4);
    Put(s_unit + 8, builtWords, 4);
    Put(s_unit + 12, builtSentences, 4);
    Put(s_unit + 16, builtBackward, 4);
    Put(s_unit + 20, builtForward, 4);
    Put(s_unit + 24, textSize, 4);
    Put(s_unit + builtWordOverrides, 0x2E, 4);
    Put(s_unit + builtWordOverrides + 4, segmentWordMidNum, 4);
    Put(s_unit + builtSentenceOverrides, 0x3B, 4);
    Put(s_unit + builtSentenceOverrides + 4, segmentSentenceSTerm, 4);
    Put(s_unit + builtSentenceOverrides + 8, 0x37E, 4);
    Put(s_unit + builtSentenceOverrides + 12, segmentSentenceSTerm, 4);
    Put(s_unit + builtValues, segmentSuppressionMatch, 4);
    Put(s_unit + builtValues + 4, segmentSuppressionMatch, 4);
    Put(s_unit + builtValues + 8, segmentSuppressionPartial, 4);
    for (uint32_t i = 0; i < builtBackward + builtForward + 1; i++)
    {
        Put(s_unit + builtOffsets + 4 * i, s_builtOffsets[i], 4);
    }
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
              Refuses(16, segmentMaxSuppressions + 1, 4) &&
              Refuses(20, segmentMaxSuppressions + 1, 4) && Refuses(24, 15, 4),
          "the counts past their bounds or the size");
    CHECK(Refuses(builtWordOverrides, 0xD800, 4) && Refuses(builtWordOverrides, 0x110000, 4),
          "a surrogate or a code point past U+10FFFF");
    CHECK(Refuses(builtSentenceOverrides + 8, 0x3B, 4) &&
              Refuses(builtSentenceOverrides + 8, 0x3A, 4),
          "overrides not strictly ascending");
    CHECK(Refuses(builtWordOverrides + 4, segmentWordClassCount, 4) &&
              Refuses(builtSentenceOverrides + 4, segmentSentenceClassCount, 4),
          "a class past its property's");
    CHECK(Refuses(builtValues + 4, segmentSuppressionPartial + 1, 4),
          "a value past the known ones");
    CHECK(Refuses(builtOffsets, 1, 4) && Refuses(builtOffsets + 20, 15, 4),
          "the offsets' first and last");
    CHECK(Refuses(builtOffsets + 4, 0, 4) && Refuses(builtOffsets + 8, 2, 4),
          "an empty entry, offsets out of order");
    CHECK(TextRefused("Mr.Dr.z.z.B.z.T.") && TextRefused("Mr.Mr.z.z.B.z.T."),
          "backward entries out of order read from the end, or twice");
    CHECK(TextRefused("Dr.Mr.z.z.T.z.B.") && TextRefused("Dr.Mr.z.z.B.z.B."),
          "forward entries out of order, or twice");
    CHECK(TextRefused("Dr.Mr\x01z.z.B.z.T."), "a control byte");
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
// many backward and forward entries.
static bool Is(const char* key, uint32_t given, uint32_t flags, const uint32_t* words,
               uint32_t wordCount, const uint32_t* sentences, uint32_t sentenceCount,
               uint32_t backward, uint32_t forward)
{
    DataSpan unit = Unit(key);
    if (unit.bytes == nullptr || mlocLoad32(unit.bytes) != given ||
        mlocLoad32(unit.bytes + 4) != flags || mlocLoad32(unit.bytes + 8) != wordCount ||
        mlocLoad32(unit.bytes + 12) != sentenceCount || mlocLoad32(unit.bytes + 16) != backward ||
        mlocLoad32(unit.bytes + 20) != forward)
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

enum
{
    // What Has looks for: a backward entry with its value, or a forward
    // entry.
    forwardEntry = 2,
};

// Whether a key's unit holds an entry: a backward one with this value, or
// a forward one.
static bool Has(const char* key, const char* text, uint32_t value)
{
    DataSpan unit = Unit(key);
    if (unit.bytes == nullptr)
    {
        return false;
    }
    uint32_t overrides = mlocLoad32(unit.bytes + 8) + mlocLoad32(unit.bytes + 12);
    uint32_t backward = mlocLoad32(unit.bytes + 16);
    uint32_t count = backward + mlocLoad32(unit.bytes + 20);
    const uint8_t* values =
        unit.bytes + segmentHeaderSize + segmentOverrideSize * (size_t)overrides;
    const uint8_t* offsets = values + 4 * (size_t)backward;
    const uint8_t* texts = offsets + 4 * ((size_t)count + 1);
    for (uint32_t i = value == forwardEntry ? backward : 0; i < count; i++)
    {
        uint32_t start = mlocLoad32(offsets + 4 * (size_t)i);
        uint32_t end = mlocLoad32(offsets + 4 * (size_t)i + 4);
        if (end - start == strlen(text) && memcmp(texts + start, text, end - start) == 0)
        {
            return value == forwardEntry
                       ? true
                       : i < backward && mlocLoad32(values + 4 * (size_t)i) == value;
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
    CHECK(Is("el", segmentGivesSentence, 0, nullptr, 0, s_greek, 2, 0, 0),
          "Greek ends a sentence at U+003B and U+037E");
    CHECK(Is("en_US_POSIX", segmentGivesWord, 0, s_posix, 1, nullptr, 0, 0, 0),
          "POSIX English's full stop is MidNum, its sentences its parents'");
    CHECK(Is("ja", segmentGivesWord | segmentGivesLine, segmentCjIdeographic | segmentJapaneseWords,
             nullptr, 0, nullptr, 0, 0, 0),
          "Japanese: CJ as an ideograph and the word rules 13.3 and 13.4");
    CHECK(Is("zh", segmentGivesLine, segmentCjIdeographic, nullptr, 0, nullptr, 0, 0, 0) &&
              Is("zh_Hant", segmentGivesLine, segmentCjIdeographic, nullptr, 0, nullptr, 0, 0, 0),
          "Chinese: CJ as an ideograph");
    CHECK(Is("de", segmentGivesSentence, 0, nullptr, 0, nullptr, 0, 192, 74) &&
              Is("en", segmentGivesSentence, 0, nullptr, 0, nullptr, 0, 119, 51) &&
              Is("ru", segmentGivesSentence, 0, nullptr, 0, nullptr, 0, 17, 3),
          "the suppressions' tables of German, English and Russian");
    CHECK(Has("en", "Mr.", segmentSuppressionMatch) &&
              Has("de", "Ursprüngl.", segmentSuppressionMatch) &&
              !Has("en", "z.B.", segmentSuppressionMatch),
          "matches as written, non-ASCII");
    CHECK(Has("de", "z.", segmentSuppressionPartial) && Has("de", "z.B.", forwardEntry) &&
              Has("fr", "trav.", segmentSuppressionPartial) &&
              Has("fr", "trav. publ.", forwardEntry) && !Has("de", "z.B.", segmentSuppressionMatch),
          "partials and their families' forward entries, with a space");
    CHECK(Unit("en_US").bytes == nullptr && Unit("root").bytes == nullptr,
          "no unit for a file that gives no segmentation, or for root");
}

int main(void)
{
    TestUnit();
    TestFile();
    return s_failures == 0 ? 0 : 1;
}
