// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What a failed call of the text headers (collate.h, search.h, index.h,
// segment.h, list.h, grammar.h) leaves in its outputs: each output filled
// with a pattern, each call made to fail in the ways it can, and every
// byte of the pattern checked after. A failure leaves its outputs as they
// were (base.h), but for the length mloc_errorCapacity gives an mlocOutput
// and the length or size it gives a sort key or an index.

#include "test_harness.h"

#include "maul-locale/collate.h"
#include "maul-locale/grammar.h"
#include "maul-locale/index.h"
#include "maul-locale/list.h"
#include "maul-locale/search.h"
#include "maul-locale/segment.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
    pattern = 0xA5,
};

static const char* const s_files[] = {"collation.mloc", "segments.mloc", "spellout.mloc"};
enum
{
    fileCount = sizeof(s_files) / sizeof(s_files[0]),
};

static mlocData s_sources[fileCount + 1];
static mlocDataSet s_data;
static mlocDataSet s_empty;
static mlocLocale s_en;
static mlocLocale s_tr;
static mlocCollator s_collator;
static mlocCollator s_unmade;
static mlocSearcher s_searcher;
static uint8_t s_index[1 << 16];

static void Fill(void* output, size_t size)
{
    memset(output, pattern, size);
}

static bool Kept(const void* output, size_t size)
{
    const unsigned char* bytes = output;
    for (size_t i = 0; i < size; i++)
    {
        if (bytes[i] != pattern)
        {
            return false;
        }
    }
    return true;
}

// An output over a patterned buffer, its length a pattern too.
typedef struct Text
{
    char buffer[64];
    mlocOutput output;
} Text;

static void FillText(Text* text, size_t capacity)
{
    Fill(text->buffer, sizeof(text->buffer));
    text->output = (mlocOutput){.buffer = text->buffer, .capacity = capacity};
    Fill(&text->output.length, sizeof(text->output.length));
}

static bool KeptText(const Text* text)
{
    return Kept(text->buffer, sizeof(text->buffer)) &&
           Kept(&text->output.length, sizeof(text->output.length));
}

// A capacity failure: the buffer as it was, the length the one needed.
static bool Measured(const Text* text, size_t needed)
{
    return Kept(text->buffer, sizeof(text->buffer)) && text->output.length == needed;
}

static void TestCollate(void)
{
    mlocCollator collator;
    mlocCollatorOptions bad = {.strength = (mlocCollationStrength)-1};
    Fill(&collator, sizeof(collator));
    CHECK(mlocInitCollator(nullptr, &s_en, nullptr, &collator) == mloc_errorInvalid &&
              mlocInitCollator(&s_data, &s_en, &bad, &collator) == mloc_errorInvalid &&
              mlocInitCollator(&s_empty, &s_en, nullptr, &collator) == mloc_errorNotFound &&
              Kept(&collator, sizeof(collator)),
          "a collator refused leaves its output");
    static const uint16_t s_ab[2] = {'a', 'b'};
    int order;
    uint64_t version;
    Fill(&order, sizeof(order));
    Fill(&version, sizeof(version));
    CHECK(mlocCollator_Compare(&s_unmade, "a", 1, "b", 1, &order) == mloc_errorInvalid &&
              mlocCollator_Compare(&s_collator, nullptr, 1, "b", 1, &order) == mloc_errorInvalid &&
              mlocCollator_CompareUtf16(&s_unmade, s_ab, 1, s_ab + 1, 1, &order) ==
                  mloc_errorInvalid &&
              mlocCollator_GetVersion(&s_unmade, &version) == mloc_errorInvalid &&
              Kept(&order, sizeof(order)) && Kept(&version, sizeof(version)),
          "a comparison or version refused leaves its output");
    uint8_t key[64];
    size_t length;
    size_t needed = 0;
    CHECK(mlocCollator_GetSortKey(&s_collator, "abc", 3, nullptr, 0, &needed) ==
                  mloc_errorCapacity &&
              needed > 1 && needed <= sizeof(key),
          "a key measures");
    Fill(key, sizeof(key));
    Fill(&length, sizeof(length));
    CHECK(mlocCollator_GetSortKey(&s_unmade, "abc", 3, key, sizeof(key), &length) ==
                  mloc_errorInvalid &&
              mlocCollator_GetSortKeyUtf16(&s_unmade, s_ab, 2, key, sizeof(key), &length) ==
                  mloc_errorInvalid &&
              Kept(key, sizeof(key)) && Kept(&length, sizeof(length)),
          "a sort key refused leaves its outputs");
    CHECK(mlocCollator_GetSortKey(&s_collator, "abc", 3, key, needed - 1, &length) ==
                  mloc_errorCapacity &&
              length == needed && Kept(key, sizeof(key)),
          "a sort key too long leaves the buffer and gives the length");
    Fill(&length, sizeof(length));
    CHECK(mlocCollator_GetSortKeyUtf16(&s_collator, s_ab, 2, key, 1, &length) ==
                  mloc_errorCapacity &&
              length > 1 && Kept(key, sizeof(key)),
          "a UTF-16 sort key too long leaves the buffer and gives the length");
}

static void TestSearch(void)
{
    mlocSearcher searcher;
    mlocSearchOptions bad = {.comparison = (mlocSearchComparison)-1};
    static const uint16_t s_a[1] = {'a'};
    Fill(&searcher, sizeof(searcher));
    CHECK(mlocInitSearcher(&s_unmade, "a", 1, nullptr, &searcher) == mloc_errorInvalid &&
              mlocInitSearcher(&s_collator, "a", 1, &bad, &searcher) == mloc_errorInvalid &&
              mlocInitSearcherUtf16(&s_unmade, s_a, 1, nullptr, &searcher) == mloc_errorInvalid &&
              Kept(&searcher, sizeof(searcher)),
          "a searcher refused leaves its output");
    static char s_long[600];
    memset(s_long, 'a', sizeof(s_long));
    CHECK(mlocInitSearcher(&s_collator, s_long, sizeof(s_long), nullptr, &searcher) ==
                  mloc_errorLimit &&
              Kept(&searcher, sizeof(searcher)),
          "a searcher past its limit leaves its output");
    static const uint16_t s_text[3] = {'x', 'y', 'z'};
    size_t start;
    size_t end;
    Fill(&start, sizeof(start));
    Fill(&end, sizeof(end));
    mlocSearcher unmade;
    memset(&unmade, 0, sizeof(unmade));
    CHECK(mlocSearcher_FindNext(&s_searcher, "xyz", 3, 0, &start, &end) == mloc_errorNotFound &&
              mlocSearcher_FindNext(&s_searcher, "xyz", 3, 4, &start, &end) == mloc_errorInvalid &&
              mlocSearcher_FindNext(&unmade, "xyz", 3, 0, &start, &end) == mloc_errorInvalid &&
              mlocSearcher_FindPrevious(&s_searcher, "xyz", 3, 3, &start, &end) ==
                  mloc_errorNotFound &&
              mlocSearcher_FindNextUtf16(&s_searcher, s_text, 3, 0, &start, &end) ==
                  mloc_errorNotFound &&
              mlocSearcher_FindPreviousUtf16(&s_searcher, s_text, 3, 3, &start, &end) ==
                  mloc_errorNotFound &&
              Kept(&start, sizeof(start)) && Kept(&end, sizeof(end)),
          "a search without a match, or refused, leaves its outputs");
    CHECK(mlocSearcher_FindPrevious(&s_searcher, "xyz", 3, 4, &start, &end) == mloc_errorInvalid &&
              mlocSearcher_FindPrevious(&unmade, "xyz", 3, 3, &start, &end) == mloc_errorInvalid &&
              mlocSearcher_FindNextUtf16(&s_searcher, s_text, 3, 4, &start, &end) ==
                  mloc_errorInvalid &&
              mlocSearcher_FindNextUtf16(&unmade, s_text, 3, 0, &start, &end) ==
                  mloc_errorInvalid &&
              mlocSearcher_FindPreviousUtf16(&s_searcher, s_text, 3, 4, &start, &end) ==
                  mloc_errorInvalid &&
              mlocSearcher_FindPreviousUtf16(&unmade, s_text, 3, 3, &start, &end) ==
                  mloc_errorInvalid &&
              Kept(&start, sizeof(start)) && Kept(&end, sizeof(end)),
          "a backward or UTF-16 search refused leaves its outputs");
    // 120 "a" and 393 accents: a match one element past the ring, as
    // test_search's TestRingEdge.
    static const char marks[4][3] = {"\xCC\x81", "\xCC\x80", "\xCC\x82", "\xCC\x83"};
    static char text[1200];
    size_t size = 0;
    size_t left = 393;
    for (int letter = 0; letter < 120; letter++)
    {
        text[size++] = 'a';
        for (int m = 0; m < 4 && left > 0; m++, left--)
        {
            text[size++] = marks[m][0];
            text[size++] = marks[m][1];
        }
    }
    mlocSearchOptions wildcard = {.comparison = mloc_searchPatternBaseWildcard};
    CHECK(mlocInitSearcher(&s_collator, s_long, 120, &wildcard, &searcher) == mloc_success &&
              mlocSearcher_FindNext(&searcher, text, size, 0, &start, &end) == mloc_errorLimit &&
              mlocSearcher_FindPrevious(&searcher, text, size, size, &start, &end) ==
                  mloc_errorLimit &&
              Kept(&start, sizeof(start)) && Kept(&end, sizeof(end)),
          "a search past its window leaves its outputs");
}

static void TestIndex(void)
{
    size_t size = 0;
    CHECK(mlocBuildIndex(&s_collator, &s_data, &s_en, nullptr, s_index, sizeof(s_index), &size) ==
              mloc_success,
          "an index builds");
    static uint8_t s_small[64];
    size_t needed;
    Fill(s_small, sizeof(s_small));
    Fill(&needed, sizeof(needed));
    CHECK(mlocBuildIndex(&s_unmade, &s_data, &s_en, nullptr, s_small, sizeof(s_small), &needed) ==
                  mloc_errorInvalid &&
              Kept(s_small, sizeof(s_small)) && Kept(&needed, sizeof(needed)),
          "an index refused leaves its outputs");
    CHECK(mlocBuildIndex(&s_collator, &s_data, &s_en, nullptr, s_small, sizeof(s_small), &needed) ==
                  mloc_errorCapacity &&
              needed >= size && Kept(s_small, sizeof(s_small)),
          "an index too large leaves the buffer and gives the capacity");
    static char s_label[MLOC_MAX_INDEX_LABEL + 1];
    memset(s_label, 'x', sizeof(s_label));
    mlocIndexOptions longLabel = {.underflowLabel = s_label, .underflowLength = sizeof(s_label)};
    Fill(&needed, sizeof(needed));
    CHECK(mlocBuildIndex(&s_collator, &s_data, &s_en, &longLabel, s_small, sizeof(s_small),
                         &needed) == mloc_errorLimit &&
              Kept(s_small, sizeof(s_small)) && Kept(&needed, sizeof(needed)),
          "an index with a label past its limit leaves its outputs");
    static uint8_t s_garbage[256];
    uint32_t count;
    uint32_t bucket;
    mlocIndexBucket label;
    Fill(&count, sizeof(count));
    Fill(&bucket, sizeof(bucket));
    Fill(&label, sizeof(label));
    static const uint16_t s_name[1] = {'a'};
    CHECK(mlocGetIndexBucketCount(s_garbage, &count) == mloc_errorInvalid &&
              mlocGetIndexBucket(s_garbage, 0, &label) == mloc_errorInvalid &&
              mlocGetIndexBucket(s_index, UINT32_MAX, &label) == mloc_errorInvalid &&
              mlocFindIndexBucket(s_garbage, "a", 1, &bucket) == mloc_errorInvalid &&
              mlocFindIndexBucketUtf16(s_garbage, s_name, 1, &bucket) == mloc_errorInvalid &&
              Kept(&count, sizeof(count)) && Kept(&bucket, sizeof(bucket)) &&
              Kept(&label, sizeof(label)),
          "a bucket refused leaves its output");
}

static void TestSegment(void)
{
    mlocSegmenter segmenter;
    Fill(&segmenter, sizeof(segmenter));
    CHECK(mlocInitSegmenter(&s_data, &s_en, (mlocSegmentKind)-1, "a b", 3, false, &segmenter) ==
                  mloc_errorInvalid &&
              mlocInitSegmenter(&s_empty, &s_en, mloc_segmentWord, "a b", 3, false, &segmenter) ==
                  mloc_errorNotFound &&
              mlocInitSegmenter(&s_data, &s_en, mloc_segmentWord, nullptr, 3, false, &segmenter) ==
                  mloc_errorInvalid &&
              Kept(&segmenter, sizeof(segmenter)),
          "a segmenter refused leaves its output");
}

static void TestList(void)
{
    mlocListFormatter formatter;
    mlocListOptions bad = {.type = (mlocListType)-1};
    Fill(&formatter, sizeof(formatter));
    CHECK(mlocInitListFormatter(&s_data, &s_en, &bad, &formatter) == mloc_errorInvalid &&
              mlocInitListFormatter(&s_empty, &s_en, nullptr, &formatter) == mloc_errorNotFound &&
              Kept(&formatter, sizeof(formatter)),
          "a list formatter refused leaves its output");
    mlocListFormatter made;
    mlocListFormatter unmade;
    memset(&unmade, 0, sizeof(unmade));
    static const mlocListItem s_items[3] = {{"red", 3}, {"green", 5}, {"blue", 4}};
    Text text;
    FillText(&text, sizeof(text.buffer));
    CHECK(mlocInitListFormatter(&s_data, &s_en, nullptr, &made) == mloc_success &&
              mlocListFormatter_Format(&unmade, s_items, 3, &text.output) == mloc_errorInvalid &&
              mlocListFormatter_Format(&made, nullptr, 3, &text.output) == mloc_errorInvalid &&
              KeptText(&text),
          "a list refused leaves its output");
    // "red, green, and blue"
    FillText(&text, 19);
    CHECK(mlocListFormatter_Format(&made, s_items, 3, &text.output) == mloc_errorCapacity &&
              Measured(&text, 20),
          "a list too long leaves the buffer and gives the length");
    FillText(&text, sizeof(text.buffer));
    CHECK(SIZE_MAX == UINT32_MAX ||
              (mlocListFormatter_Format(&made, s_items, (size_t)MLOC_MAX_LIST_ITEMS + 1,
                                        &text.output) == mloc_errorLimit &&
               KeptText(&text)),
          "a list past its item limit leaves its output");
    mlocPersonGender gender;
    mlocPersonGender genders[2] = {mloc_personMale, (mlocPersonGender)7};
    Fill(&gender, sizeof(gender));
    CHECK(mlocGetListGender(&s_data, &s_en, genders, 2, &gender) == mloc_errorInvalid &&
              mlocGetListGender(&s_data, nullptr, genders, 1, &gender) == mloc_errorInvalid &&
              Kept(&gender, sizeof(gender)),
          "a gender refused leaves its output");
}

static void TestGrammar(void)
{
    mlocInflector inflector;
    Fill(&inflector, sizeof(inflector));
    CHECK(mlocInitInflector(&s_empty, &s_tr, &inflector) == mloc_errorNotFound &&
              mlocInitInflector(nullptr, &s_tr, &inflector) == mloc_errorInvalid &&
              Kept(&inflector, sizeof(inflector)),
          "an inflector refused leaves its output");
    mlocInflector turkish;
    mlocInflection dative = {.grammaticalCase = mloc_caseDative};
    mlocInflection ergative = {.grammaticalCase = mloc_caseErgative};
    Text text;
    FillText(&text, sizeof(text.buffer));
    CHECK(mlocInitInflector(&s_data, &s_tr, &turkish) == mloc_success &&
              mlocInflector_InflectText(&turkish, "", 0, &dative, &text.output) ==
                  mloc_errorInvalid &&
              mlocInflector_InflectText(&turkish, "\xFF", 1, &dative, &text.output) ==
                  mloc_errorInvalid &&
              mlocInflector_InflectText(&turkish, "kitap", 5, &ergative, &text.output) ==
                  mloc_errorInvalid &&
              KeptText(&text),
          "an inflection refused leaves its output");
    // "kitap'a"
    FillText(&text, 5);
    CHECK(mlocInflector_InflectText(&turkish, "kitap", 5, &dative, &text.output) ==
                  mloc_errorCapacity &&
              Measured(&text, 7),
          "an inflection too long leaves the buffer and gives the length");
    mlocDecimal five;
    mlocDecimal broken = {.kind = (mlocDecimalKind)7};
    CHECK(mlocParseDecimal("5", 1, &five).status == mloc_success, "5 parses");
    FillText(&text, sizeof(text.buffer));
    CHECK(mlocInflector_InflectNumber(&turkish, nullptr, &broken, &dative, &text.output) ==
                  mloc_errorInvalid &&
              mlocInflector_InflectNumber(&turkish, nullptr, &five, &ergative, &text.output) ==
                  mloc_errorInvalid &&
              KeptText(&text),
          "a number's inflection refused leaves its output");
    // "5'e"
    FillText(&text, 2);
    CHECK(mlocInflector_InflectNumber(&turkish, nullptr, &five, &dative, &text.output) ==
                  mloc_errorCapacity &&
              Measured(&text, 3),
          "a number's inflection too long leaves the buffer and gives the length");
    // The built-in data alone has no spell-out rules.
    mlocDataSet builtin = {s_sources, 1};
    mlocInflector bare;
    mlocDecimal huge;
    FillText(&text, sizeof(text.buffer));
    CHECK(mlocInitInflector(&builtin, &s_tr, &bare) == mloc_success &&
              mlocParseDecimal("1e2000", 6, &huge).status == mloc_success &&
              mlocInflector_InflectNumber(&bare, nullptr, &five, &dative, &text.output) ==
                  mloc_errorNotFound &&
              mlocInflector_InflectNumber(&turkish, nullptr, &huge, &dative, &text.output) ==
                  mloc_errorLimit &&
              KeptText(&text),
          "a number without spell-out rules, or past the inflected limit, leaves its output");
}

static bool OpenData(void)
{
    // Room for every file at once.
    alignas(8) static uint8_t s_bytes[8 << 20];
    size_t used = 0;
    mlocOpenBuiltinData(&s_sources[0]);
    for (size_t i = 0; i < fileCount; i++)
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", MLOC_GENERATED_DIR, s_files[i]);
        FILE* file = fopen(path, "rb");
        size_t size = file == nullptr ? 0 : fread(s_bytes + used, 1, sizeof(s_bytes) - used, file);
        if (file != nullptr)
        {
            fclose(file);
        }
        if (size == 0 || used + size == sizeof(s_bytes) ||
            mlocOpenData(s_bytes + used, size, &s_sources[i + 1]) != mloc_success)
        {
            fprintf(stderr, "test_text_outputs: cannot open %s\n", path);
            return false;
        }
        used += (size + 7) & ~(size_t)7;
    }
    s_data = (mlocDataSet){s_sources, fileCount + 1};
    s_empty = (mlocDataSet){nullptr, 0};
    return true;
}

int main(void)
{
    if (!OpenData())
    {
        return 1;
    }
    memset(&s_unmade, 0, sizeof(s_unmade));
    CHECK(mlocParseLocale("en", 2, &s_en).status == mloc_success &&
              mlocParseLocale("tr", 2, &s_tr).status == mloc_success &&
              mlocInitCollator(&s_data, &s_en, nullptr, &s_collator) == mloc_success &&
              mlocInitSearcher(&s_collator, "a", 1, nullptr, &s_searcher) == mloc_success,
          "a collator and a searcher to work with");
    TestCollate();
    TestSearch();
    TestIndex();
    TestSegment();
    TestList();
    TestGrammar();
    return s_failures == 0 ? 0 : 1;
}
