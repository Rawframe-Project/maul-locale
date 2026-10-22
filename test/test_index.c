// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The alphabetic index (record mloc-0075): English's A to Z between the
// underflow and overflow buckets, names found in UTF-8 and UTF-16, the
// value measured, built, copied to an odd address and still read; the
// options (fewer labels, own special labels); Swedish's letters after Z,
// Japanese kana and Chinese pinyin shown as A to Z, an inflow bucket
// merged into the overflow bucket; the refusals.
// conform_index checks every CLDR locale against ICU4C's index.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/index.h"

#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};
static mlocCollator s_collator;
static mlocLocale s_locale;
static uint8_t s_index[1 << 16];
static uint8_t s_copy[(1 << 16) + 1];

// Builds the index of a locale into s_index; its size.
static size_t Build(const char* id, const mlocIndexOptions* options)
{
    size_t size = 0;
    bool ok = mlocParseLocale(id, strlen(id), &s_locale).status == mloc_success &&
              mlocInitCollator(&s_collator, &s_data, &s_locale, nullptr) == mloc_success &&
              mlocBuildIndex(&s_collator, &s_data, &s_locale, options, s_index, sizeof(s_index),
                             &size) == mloc_success;
    return ok ? size : 0;
}

// Whether the index's buckets are these labels, space-separated.
static bool Shows(const void* index, const char* labels)
{
    uint32_t count = 0;
    if (mlocGetIndexBucketCount(index, &count) != mloc_success)
    {
        return false;
    }
    char joined[1024] = "";
    size_t used = 0;
    for (uint32_t b = 0; b < count; b++)
    {
        mlocIndexBucket bucket;
        if (mlocGetIndexBucket(index, b, &bucket) != mloc_success ||
            used + bucket.length + 2 > sizeof(joined))
        {
            return false;
        }
        if (b > 0)
        {
            joined[used++] = ' ';
        }
        memcpy(joined + used, bucket.label, bucket.length);
        used += bucket.length;
        joined[used] = '\0';
    }
    return strcmp(joined, labels) == 0;
}

static bool In(const void* index, const char* name, uint32_t expected)
{
    uint32_t bucket = 0;
    return mlocFindIndexBucket(index, name, strlen(name), &bucket) == mloc_success &&
           bucket == expected;
}

static void TestEnglish(void)
{
    size_t size = Build("en", nullptr);
    CHECK(size > 0 && Shows(s_index, "\xE2\x80\xA6 A B C D E F G H I J K L M N O P Q R S T U V W X "
                                     "Y Z \xE2\x80\xA6"),
          "English: A to Z between the underflow and overflow buckets");
    mlocIndexBucket first;
    mlocIndexBucket last;
    CHECK(mlocGetIndexBucket(s_index, 0, &first) == mloc_success &&
              first.type == mloc_indexUnderflow &&
              mlocGetIndexBucket(s_index, 27, &last) == mloc_success &&
              last.type == mloc_indexOverflow,
          "the special buckets' types");
    CHECK(In(s_index, "apple", 1) && In(s_index, "\xC3\x81lvarez", 1) && In(s_index, "Zebra", 26) &&
              In(s_index, "1984", 0) && In(s_index, "", 0) && In(s_index, "\xCE\xB1", 27),
          "names: a letter's bucket, an accent at the primary level, digits and the empty name "
          "before the labels, Greek after them");
    static const uint16_t s_name[] = {'z', 'o', 'o'};
    uint32_t bucket = 0;
    CHECK(mlocFindIndexBucketUtf16(s_index, s_name, 3, &bucket) == mloc_success && bucket == 26,
          "a UTF-16 name");
    memcpy(s_copy + 1, s_index, size);
    memset(s_index, 0, size);
    CHECK(Shows(s_copy + 1, "\xE2\x80\xA6 A B C D E F G H I J K L M N O P Q R S T U V W X Y Z "
                            "\xE2\x80\xA6") &&
              In(s_copy + 1, "mango", 13),
          "the index copied to an odd address and the original cleared");
}

static void TestMeasure(void)
{
    mlocLocale locale;
    mlocCollator collator;
    size_t needed = 0;
    size_t size = 0;
    bool made = mlocParseLocale("de", 2, &locale).status == mloc_success &&
                mlocInitCollator(&collator, &s_data, &locale, nullptr) == mloc_success;
    CHECK(made &&
              mlocBuildIndex(&collator, &s_data, &locale, nullptr, nullptr, 0, &needed) ==
                  mloc_errorCapacity &&
              mlocBuildIndex(&collator, &s_data, &locale, nullptr, s_index, needed - 1, &size) ==
                  mloc_errorCapacity &&
              mlocBuildIndex(&collator, &s_data, &locale, nullptr, s_index, needed, &size) ==
                  mloc_success &&
              size <= needed,
          "measured with no buffer, refused one byte short, built in the size given");
}

static void TestOptions(void)
{
    mlocIndexOptions options = {5, "U", 1, "I", 1, "O", 1};
    CHECK(Build("de", &options) > 0 && Shows(s_index, "U A E J O T Y O"),
          "five labels kept of 26 as ICU4C keeps them, the special labels given");
    CHECK(Build("sv", nullptr) > 0 &&
              Shows(s_index, "\xE2\x80\xA6 A B C D E F G H I J K L M N O P Q R S T U V W X Y Z "
                             "\xC3\x85 \xC3\x84 \xC3\x96 \xE2\x80\xA6") &&
              In(s_index, "\xC3\x84ngel", 28),
          "Swedish: \xC3\x85, \xC3\x84 and \xC3\x96 after Z");
    CHECK(Build("ja", nullptr) > 0 && In(s_index, "\xE3\x81\x8B\xE3\x81\x8D", 2) &&
              In(s_index, "\xE3\x82\xAB\xE3\x83\xA9\xE3\x82\xB9", 2),
          "Japanese: hiragana and katakana in the same kana bucket");
    CHECK(Build("zh", nullptr) > 0 &&
              Shows(s_index, "\xE2\x80\xA6 A B C D E F G H I J K L M N O P Q R S T U V W X Y Z "
                             "\xE2\x80\xA6") &&
              In(s_index, "\xE4\xB8\xAD", 26) && In(s_index, "\xE4\xB8\x80", 25),
          "Chinese pinyin: the U+FDD0 labels shown as A to Z, a character in its reading's "
          "letter");
    CHECK(Build("zh-u-kr-latn-grek-hani", nullptr) > 0 &&
              Shows(s_index, "\xE2\x80\xA6 A B C D E F G H I J K L M N O P Q R S T U V W X Y Z "
                             "\xE2\x80\xA6") &&
              In(s_index, "\xCE\xB1", 27) && In(s_index, "\xE4\xB8\xAD", 26),
          "Greek between Latin and Han: its inflow bucket, before the hidden pinyin ones, "
          "merged into the overflow bucket");
    CHECK(Build("ko", nullptr) > 0 &&
              Shows(s_index, "\xE2\x80\xA6 \xE3\x84\xB1 \xE3\x84\xB4 \xE3\x84\xB7 "
                             "\xE3\x84\xB9 \xE3\x85\x81 \xE3\x85\x82 \xE3\x85\x85 "
                             "\xE3\x85\x87 \xE3\x85\x88 \xE3\x85\x8A \xE3\x85\x8B "
                             "\xE3\x85\x8C \xE3\x85\x8D \xE3\x85\x8E \xE2\x80\xA6") &&
              In(s_index, "\xED\x95\x9C\xEA\xB5\xAD", 14) && In(s_index, "\xEA\xB0\x80", 1),
          "Korean: the 14 initial consonants, a syllable in its initial's bucket (the Hangul "
          "script's boundary read in NFD)");
}

static void TestRefusals(void)
{
    size_t size = 0;
    uint32_t count = 0;
    mlocIndexBucket bucket;
    mlocIndexOptions bad = {0, "\xC3", 1, nullptr, 0, nullptr, 0};
    mlocIndexOptions dangling = {0, nullptr, 2, nullptr, 0, nullptr, 0};
    static char s_long[256];
    memset(s_long, 'a', sizeof(s_long));
    mlocIndexOptions longer = {0, s_long, sizeof(s_long), nullptr, 0, nullptr, 0};
    CHECK(Build("en", nullptr) > 0 &&
              mlocBuildIndex(nullptr, &s_data, &s_locale, nullptr, s_index, 100, &size) ==
                  mloc_errorInvalid &&
              mlocBuildIndex(&s_collator, nullptr, &s_locale, nullptr, s_index, 100, &size) ==
                  mloc_errorInvalid &&
              mlocBuildIndex(&s_collator, &s_data, &s_locale, nullptr, nullptr, 100, &size) ==
                  mloc_errorInvalid &&
              mlocBuildIndex(&s_collator, &s_data, &s_locale, nullptr, s_index, 100, nullptr) ==
                  mloc_errorInvalid &&
              mlocBuildIndex(&s_collator, &s_data, &s_locale, &bad, s_index, 100, &size) ==
                  mloc_errorInvalid &&
              mlocBuildIndex(&s_collator, &s_data, &s_locale, &dangling, s_index, 100, &size) ==
                  mloc_errorInvalid &&
              mlocBuildIndex(&s_collator, &s_data, &s_locale, &longer, s_index, 100, &size) ==
                  mloc_errorInvalid,
          "a null collator, data, buffer or size, and special labels ill-formed, missing or "
          "past 255 bytes refused");
    Build("en", nullptr);
    uint32_t found = 0;
    CHECK(mlocGetIndexBucket(s_index, 28, &bucket) == mloc_errorInvalid &&
              mlocGetIndexBucket(s_index, 0, nullptr) == mloc_errorInvalid &&
              mlocGetIndexBucketCount(s_index, nullptr) == mloc_errorInvalid &&
              mlocFindIndexBucket(s_index, nullptr, 1, &found) == mloc_errorInvalid &&
              mlocFindIndexBucket(s_index, "a", 1, nullptr) == mloc_errorInvalid &&
              mlocFindIndexBucketUtf16(s_index, nullptr, 1, &found) == mloc_errorInvalid &&
              mlocGetIndexBucketCount(s_copy, &count) == mloc_errorInvalid &&
              mlocGetIndexBucketCount(nullptr, &count) == mloc_errorInvalid,
          "a bucket past the count, null pointers and bytes mlocBuildIndex did not write");
}

int main(void)
{
    FILE* f = fopen(MLOC_COLLATION_FILE, "rb");
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocOpenBuiltinData(&s_sources[0]);
    if (size == 0 || size == sizeof(s_file) ||
        mlocOpenData(&s_sources[1], s_file, size) != mloc_success)
    {
        fputs("test_index: cannot open " MLOC_COLLATION_FILE "\n", stderr);
        return 2;
    }
    TestEnglish();
    TestMeasure();
    TestOptions();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
