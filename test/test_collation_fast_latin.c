// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin tables in the collation data file (record mloc-0072):
// the root's as long as ICU4C 79's with its head words, every type's
// read as its own, the root's or none, and the section's reader
// refusing a table that does not hold together.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_fast_latin.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
alignas(8) static uint8_t s_copy[1 << 22];
static size_t s_size;
static DataSpan s_root;
static size_t s_words;
static uint32_t s_count;

static void Put16(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void Put32(uint8_t* p, uint32_t value)
{
    Put16(p, value & 0xFFFF);
    Put16(p + 2, value >> 16);
}

// The root unit read with one table word changed.
static bool ReadsWithWord(uint32_t index, uint32_t word)
{
    memcpy(s_copy, s_root.bytes, s_root.size);
    Put16(s_copy + s_words + (size_t)index * 2, word);
    CollationRoot r;
    return mlocReadCollationRoot((DataSpan){s_copy, s_root.size}, &r);
}

// The root unit read with its section's count changed and cut or grown
// to a size.
static bool ReadsWithCount(uint32_t count, uint32_t size, CollationRoot* r)
{
    memcpy(s_copy, s_root.bytes, s_root.size);
    Put32(s_copy + s_words - 4, count);
    return mlocReadCollationRoot((DataSpan){s_copy, size}, r);
}

// The root unit read with a table of its characters' words alone, those
// of lists and expansions made ignorable.
static bool ReadsPlain(void)
{
    uint32_t count = fastLatinHeader + fastLatinChars;
    memcpy(s_copy, s_root.bytes, s_root.size);
    Put32(s_copy + s_words - 4, count);
    for (uint32_t i = fastLatinHeader; i < count; i++)
    {
        uint32_t word = mlocFastLatinWord(s_copy + s_words, i);
        Put16(s_copy + s_words + (size_t)i * 2,
              word >= fastContraction && word < fastMinLong ? 0 : word);
    }
    CollationRoot r;
    return mlocReadCollationRoot((DataSpan){s_copy, (uint32_t)s_words + count * 2}, &r) &&
           r.fastLatinCount == count;
}

static void TestRoot(const DataView* view)
{
    CollationRoot r;
    bool found = mlocFindDataUnit(view, kindCollationRoot, dataNoLocale, &s_root) &&
                 mlocReadCollationRoot(s_root, &r) && r.fastLatin != nullptr;
    CHECK(found && r.fastLatinCount == 480, "the root's table, 480 words as ICU4C 79's");
    if (!found)
    {
        return;
    }
    s_count = r.fastLatinCount;
    s_words = (size_t)(r.fastLatin - s_root.bytes);
    static const uint16_t head[] = {0x205, 0xC40, 0xDE0, 0xE80, 0xEA8};
    bool same = true;
    for (uint32_t i = 0; i < fastLatinHeader; i++)
    {
        same = same && mlocFastLatinWord(r.fastLatin, i) == head[i];
    }
    CHECK(same, "its head: version 2, five words, the groups' tops as ICU4C's");
    uint32_t a = mlocFastLatinWord(r.fastLatin, fastLatinHeader + 'a');
    uint32_t b = mlocFastLatinWord(r.fastLatin, fastLatinHeader + 'b');
    uint32_t nul = mlocFastLatinWord(r.fastLatin, fastLatinHeader);
    uint32_t aGrave = mlocFastLatinWord(r.fastLatin, fastLatinHeader + 0xE0);
    CHECK(a >= fastMinShort && b > a && nul >= fastContraction && nul < fastExpansion &&
              (aGrave & fastShortPrimaryMask) == (a & fastShortPrimaryMask) &&
              (aGrave & fastSecondaryMask) >= fastMinSecHigh,
          "a below b as short primaries, U+0000 a contraction, a grave as a with the grave's "
          "secondary merged in");
}

static void TestWordRefusals(void)
{
    uint32_t last = s_count - 1;
    CHECK(!ReadsWithWord(0, 0x206), "a table of another head refused");
    CHECK(!ReadsWithWord(2, 0), "group tops not rising refused");
    CHECK(!ReadsWithWord(fastLatinHeader, fastContraction | fastIndexMask),
          "a contraction past the table refused");
    CHECK(mlocFastLatinWord(s_root.bytes + s_words, last) == fastContractionCharMask &&
              !ReadsWithWord(last, 1U << fastContractionLengthShift),
          "a contraction list with no end refused");
    CHECK(!ReadsWithWord(last, 0) && ReadsWithWord(last, fastContractionCharMask),
          "the end read as it is");
}

// Words at the bounds: an expansion ending at the table's end, a list
// where none starts, equal group tops.
static void TestBoundaryWords(void)
{
    CHECK(!ReadsWithWord(fastLatinHeader + 0xE0, fastExpansion | fastIndexMask) &&
              !ReadsWithWord(fastLatinHeader + 0xE0,
                             fastExpansion | (s_count - fastLatinHeader - fastLatinChars - 1)) &&
              ReadsWithWord(fastLatinHeader + 0xE0,
                            fastExpansion | (s_count - fastLatinHeader - fastLatinChars - 2)),
          "an expansion past the table refused, its last word the table's last");
    CHECK(!ReadsWithWord(fastLatinHeader + 'a', fastContraction),
          "a contraction at the first word after the characters, which is no list, refused");
    CHECK(ReadsWithWord(2, mlocFastLatinWord(s_root.bytes + s_words, 1)),
          "two groups with the same top read");
}

// No mini CE may be the end of string mark: it would end a text early
// and the later levels would read the rest unchecked.
static void TestEndMarkRefusals(void)
{
    const uint8_t* words = s_root.bytes + s_words;
    uint32_t expansion = 0;
    for (uint32_t i = 0; expansion == 0 && i < fastLatinChars; i++)
    {
        uint32_t word = mlocFastLatinWord(words, fastLatinHeader + i);
        expansion = word >= fastExpansion && word < fastMinLong
                        ? fastLatinHeader + fastLatinChars + (word & fastIndexMask)
                        : 0;
    }
    uint32_t nul = mlocFastLatinWord(words, fastLatinHeader);
    uint32_t list = fastLatinHeader + fastLatinChars + (nul & fastIndexMask);
    CHECK(!ReadsWithWord(fastLatinHeader + 'a', fastEndOfString) && expansion != 0 &&
              !ReadsWithWord(expansion, fastEndOfString) &&
              !ReadsWithWord(expansion + 1, fastEndOfString) &&
              mlocFastLatinWord(words, list) >> fastContractionLengthShift == 2 &&
              !ReadsWithWord(list + 1, fastEndOfString) && ReadsWithWord(list + 1, 0),
          "the end of string mark refused as a character's, an expansion's or a contraction's "
          "mini CE");
}

static void TestCountRefusals(void)
{
    uint32_t size = s_root.size;
    CollationRoot r;
    CHECK(!ReadsWithCount(s_count + 1, size, &r) && !ReadsWithCount(s_count - 1, size, &r) &&
              !ReadsWithCount(s_count, size - 1, &r) && !ReadsWithCount(s_count, size + 2, &r),
          "a count other than the words there refused");
    CHECK(ReadsPlain(), "a table of the characters alone, none a list or expansion, read");
    CHECK(!ReadsWithCount(fastLatinHeader + fastLatinChars - 1,
                          (uint32_t)s_words + (fastLatinHeader + fastLatinChars - 1) * 2, &r),
          "a table short of its characters refused");
    CHECK(!ReadsWithCount(fastLatinRootTable, (uint32_t)s_words, &r),
          "the root's table named by the root refused");
    CHECK(ReadsWithCount(0, (uint32_t)s_words, &r) && r.fastLatin == nullptr &&
              r.fastLatinCount == 0 &&
              mlocReadCollationRoot((DataSpan){s_root.bytes, (uint32_t)s_words - 4}, &r) &&
              r.fastLatin == nullptr && r.fastLatinCount == 0,
          "a count of 0 and no section both read as no table");
    CHECK(!ReadsWithCount(0, (uint32_t)s_words - 2, &r), "a cut count refused");
}

static void TestTypes(const mlocData* data, const DataView* view)
{
    size_t own = 0;
    size_t shared = 0;
    size_t none = 0;
    size_t refused = 0;
    size_t searches = 0;
    size_t searchesNone = 0;
    for (size_t i = 0; i < mlocGetDataLocaleCount(data); i++)
    {
        const char* id = nullptr;
        size_t length = 0;
        uint32_t locale = 0;
        DataSpan unit = {0};
        if (mlocGetDataLocale(data, i, &id, &length) != mloc_success ||
            !mlocFindDataLocale(view, id, length, &locale) ||
            !mlocFindDataUnit(view, kindCollationTailorings, locale, &unit))
        {
            continue;
        }
        CollationTypes types = mlocReadCollationTypes(unit);
        for (uint32_t t = 0; t < types.count; t++)
        {
            const uint8_t* entry = types.bytes + collationTypesHead + (size_t)t * collationTypeSize;
            CollationTailoring tailoring = {0};
            DataSpan body = {types.bytes + mlocLoad32(entry + 8), mlocLoad32(entry + 12)};
            bool reads = mlocReadCollationTailoring(body, &tailoring);
            uint32_t count = tailoring.view.fastLatinCount;
            bool search = mlocLoad32(entry + 4) >= 6 &&
                          memcmp(types.names + mlocLoad32(entry), "search", 6) == 0;
            refused += !reads;
            shared += reads && count == fastLatinRootTable;
            none += reads && count == 0;
            own += reads && tailoring.view.fastLatin != nullptr;
            searches += search;
            searchesNone += search && reads && count == 0;
        }
    }
    CHECK(refused == 0 && own > 20 && shared > 50,
          "every type's table reads: its own (Czech's, Danish's...) or the root's");
    // en_US_POSIX gives every ASCII character a primary of its own, more
    // than the short ones there are, and ICU4C builds none for it either.
    CHECK(searches > 10 && none == searches + 1 && searchesNone == searches,
          "no table for the search types, as ICU4C builds none for them, and for en_US_POSIX");
}

int main(void)
{
    FILE* file = fopen(MLOC_COLLATION_FILE, "rb");
    s_size = file == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    mlocData data;
    if (s_size == 0 || s_size == sizeof(s_file) ||
        mlocOpenData(&data, s_file, s_size) != mloc_success)
    {
        fputs("test_collation_fast_latin: cannot open " MLOC_COLLATION_FILE "\n", stderr);
        return 2;
    }
    DataView view = mlocViewOf(&data);
    TestRoot(&view);
    if (s_count != 0)
    {
        TestWordRefusals();
        TestBoundaryWords();
        TestEndMarkRefusals();
        TestCountRefusals();
    }
    TestTypes(&data, &view);
    return s_failures == 0 ? 0 : 1;
}
