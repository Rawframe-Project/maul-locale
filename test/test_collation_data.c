// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation data file: its root unit found and valid, a few elements
// read through the iterator as CLDR 49's root gives them, and a copy with
// a unit spoiled refused at open: the checks a collator's making trusts
// (each trie offset, the fast Latin table, the unsafe ranges, a stored
// reordering) run there. Every stored reordering is the one the root
// makes of the tailoring's codes, which no validator can see.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_iterator.h"
#include "collation_reorder.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-unicode/properties.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static size_t s_size;

static uint32_t Elements(const CollationRoot* r, const char* text, uint64_t* out, uint32_t cap)
{
    CollationIterator it;
    mlocCollationIteratorStart(&it, r, (const uint8_t*)text, strlen(text));
    uint32_t n = 0;
    uint64_t e = 0;
    while (n < cap && mlocCollationNextElement(&it, &e))
    {
        out[n] = e;
        n += e != 0;
    }
    return n;
}

static void TestRoot(const mlocData* data)
{
    DataView view = mlocViewOf(data);
    DataSpan span = {0};
    CollationRoot r;
    bool found = mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) &&
                 mlocReadCollationRoot(span, &r);
    CHECK(found && r.version == (18U << 16), "the root unit found, UCA 18.0.0");
    CHECK(found && mlocCollationLastPrimary(&r, collationGroupPunctuation) == 0x0BFFFFFF &&
              mlocCollationLastPrimary(&r, collationGroupSpace) <
                  mlocCollationLastPrimary(&r, collationGroupPunctuation) &&
              mlocCollationLastPrimary(&r, collationGroupSymbol) <
                  mlocCollationLastPrimary(&r, collationGroupCurrency) &&
              mlocCollationScriptIndex(&r, MUNI_SCRIPT('L', 'a', 't', 'n')) != 0 &&
              mlocCollationScriptIndex(&r, MUNI_SCRIPT('H', 'r', 'k', 't')) ==
                  mlocCollationScriptIndex(&r, MUNI_SCRIPT('H', 'i', 'r', 'a')) &&
              mlocCollationScriptIndex(&r, MUNI_SCRIPT('H', 'a', 'n', 's')) ==
                  mlocCollationScriptIndex(&r, MUNI_SCRIPT('H', 'a', 'n', 'i')) &&
              r.numericPrimary == 0x10000000,
          "punctuation ends at 0B FF FF FF as FractionalUCA.txt's variable top, the groups in "
          "order, Hrkt with Hira and Hans with Hani, numeric order's lead byte 10");
    uint32_t unknown = found ? mlocCollationScriptIndex(&r, MUNI_SCRIPT('Z', 'z', 'z', 'z')) : 0;
    CHECK(unknown != 0 && mlocCollationStart(&r, unknown) == 0xFE00 &&
              mlocCollationStart(&r, unknown + 1) == 0xFF00,
          "the unassigned range starts at its lead byte FE, which ICU counts as compressible");
    if (!found)
    {
        return;
    }
    uint64_t e[8];
    uint64_t f[8];
    CHECK(Elements(&r, "a", e, 8) == 1 && e[0] == 0x2B00000005000500ULL &&
              Elements(&r, "b", f, 8) == 1 && f[0] == 0x2D00000005000500ULL,
          "a and b as CLDR's root gives them");
    CHECK(Elements(&r, "\xC3\xA0", e, 8) == 2 && e[0] == 0x2B00000005000500ULL &&
              e[1] == 0x000000008A000500ULL && Elements(&r, "a\xCC\x80", f, 8) == 2 &&
              memcmp(e, f, 16) == 0,
          "a grave through NFD, as a and the combining grave");
    CHECK(Elements(&r, "\xEA\xB0\x80", e, 8) == 2 && e[0] == 0x7C06000005000500ULL &&
              e[1] == 0x7C67000005000500ULL,
          "a Hangul syllable as its jamo");
    CHECK(Elements(&r, "\xE4\xB8\x80", e, 8) == 1 && (e[0] >> 56) >= 0xF9 &&
              (e[0] >> 56) < collationUnassignedByte && Elements(&r, "\xE4\xB8\x81", f, 8) == 1 &&
              f[0] > e[0],
          "Han below the unassigned lead byte, in code point order");
}

// A pointer into the file, writable.
static uint8_t* At(const uint8_t* p)
{
    return s_file + (p - s_file);
}

// Spoils bytes of a unit, seals the unit's hash again so that only the
// values can refuse it, and expects the file refused at open; then puts
// both back.
static void Spoil(DataSpan unit, uint32_t hashAt, const uint8_t* where, const uint8_t* bytes,
                  size_t length, const char* what)
{
    uint8_t saved[8];
    uint8_t savedHash[8];
    uint8_t* hash = At(unit.bytes) + hashAt;
    memcpy(saved, where, length);
    memcpy(savedHash, hash, 8);
    memcpy(At(where), bytes, length);
    uint64_t sealed = mlocCollationUnitHash(unit, hashAt);
    for (int i = 0; i < 8; i++)
    {
        hash[i] = (uint8_t)(sealed >> (8 * i));
    }
    mlocData broken;
    CHECK(mlocCollationHashValid(unit, hashAt) &&
              mlocOpenData(&broken, s_file, s_size) == mloc_errorFormat,
          what);
    memcpy(At(where), saved, length);
    memcpy(hash, savedHash, 8);
}

// The three checks on a unit's view: a trie block past the data, a fast
// Latin table of another version, a range past U+10FFFF.
static void SpoilViews(DataSpan unit, uint32_t hashAt, const CollationRoot* r, const char* what)
{
    static const uint8_t past[4] = {0xF0, 0xFF, 0xFF, 0xFF};
    static const uint8_t beyond[4] = {0x00, 0x00, 0x11, 0x00};
    char message[96];
    snprintf(message, sizeof(message), "%s: a trie block past its data", what);
    Spoil(unit, hashAt, r->trie.bmp, past, 4, message);
    CHECK(r->fastLatin != nullptr && r->unsafeCount > 0, what);
    if (r->fastLatin != nullptr)
    {
        // Word 0 is the version above the header's length: its high byte.
        uint8_t version[1] = {(uint8_t)(r->fastLatin[1] ^ 1)};
        snprintf(message, sizeof(message), "%s: a fast Latin table of another version", what);
        Spoil(unit, hashAt, r->fastLatin + 1, version, 1, message);
    }
    if (r->unsafeCount > 0)
    {
        snprintf(message, sizeof(message), "%s: an unsafe range past U+10FFFF", what);
        Spoil(unit, hashAt, r->unsafe + 4, beyond, 4, message);
    }
}

// The first tailoring of the file with a fast Latin table and unsafe
// ranges of its own, and the unit of types that holds it.
static bool FindTailoring(const DataView* view, DataSpan* typesOut, CollationTailoring* out)
{
    for (uint32_t u = 0; u < view->unitCount; u++)
    {
        const uint8_t* record = view->bytes + dataHeaderSize + (size_t)u * dataUnitSize;
        if (mlocLoad32(record) != kindCollationTailorings)
        {
            continue;
        }
        DataSpan types = {view->bytes + mlocLoad32(record + 8), mlocLoad32(record + 12)};
        CollationTypes t = mlocReadCollationTypes(types);
        for (uint32_t i = 0; i < t.count; i++)
        {
            const uint8_t* entry = types.bytes + collationTypesHead + (size_t)i * collationTypeSize;
            DataSpan span = {types.bytes + mlocLoad32(entry + 8), mlocLoad32(entry + 12)};
            if (mlocReadCollationTailoring(span, out) && out->view.fastLatin != nullptr &&
                out->view.fastLatinCount != fastLatinRootTable && out->view.unsafeCount > 0)
            {
                *typesOut = types;
                return true;
            }
        }
    }
    return false;
}

// Each tailoring's reordering against the one the root makes of its
// codes; gives the unit of types and the tailoring of the first stored
// reordering with more than one range.
static void TestReorderings(const DataView* view, const CollationRoot* root, DataSpan* typesOut,
                            CollationTailoring* out)
{
    uint32_t checked = 0;
    uint32_t stored = 0;
    uint32_t same = 0;
    for (uint32_t u = 0; u < view->unitCount; u++)
    {
        const uint8_t* record = view->bytes + dataHeaderSize + (size_t)u * dataUnitSize;
        if (mlocLoad32(record) != kindCollationTailorings)
        {
            continue;
        }
        DataSpan types = {view->bytes + mlocLoad32(record + 8), mlocLoad32(record + 12)};
        CollationTypes t = mlocReadCollationTypes(types);
        for (uint32_t i = 0; i < t.count; i++)
        {
            const uint8_t* entry = types.bytes + collationTypesHead + (size_t)i * collationTypeSize;
            DataSpan span = {types.bytes + mlocLoad32(entry + 8), mlocLoad32(entry + 12)};
            CollationTailoring tailoring;
            uint32_t codes[collationMaxReorderCodes];
            if (!mlocReadCollationTailoring(span, &tailoring) ||
                (tailoring.settings & collationReorderGiven) == 0 ||
                tailoring.reorderCount > collationMaxReorderCodes)
            {
                continue;
            }
            for (uint32_t c = 0; c < tailoring.reorderCount; c++)
            {
                codes[c] = mlocLoad32(tailoring.reorder + (size_t)c * 4);
            }
            CollationReorder made;
            CollationReorder read;
            bool changes = false;
            bool ok = mlocCollationMakeReorder(root, codes, tailoring.reorderCount, &made,
                                               &changes) == mloc_success;
            if (ok && changes && tailoring.reordering != nullptr)
            {
                stored++;
                mlocCollationLoadReorder(tailoring.reordering, tailoring.reorderRanges, &read);
                ok = read.minHighNoReorder == made.minHighNoReorder &&
                     read.rangeCount == made.rangeCount &&
                     memcmp(read.ranges, made.ranges, made.rangeCount * sizeof(uint32_t)) == 0 &&
                     memcmp(read.table, made.table, sizeof(made.table)) == 0;
                if (ok && read.rangeCount > 1 && typesOut->bytes == nullptr)
                {
                    *typesOut = types;
                    *out = tailoring;
                }
            }
            else
            {
                ok = ok && !changes && tailoring.reordering == nullptr;
            }
            checked++;
            same += ok;
        }
    }
    CHECK(stored > 0, "tailorings that reorder store their reordering");
    CHECK(same == checked, "each stored reordering is the one the root makes of the codes");
}

// Spoils a stored reordering: its leading word, its range count, the low
// bits of its boundary, and its range limits, made equal.
static void SpoilReordering(DataSpan types, const CollationTailoring* t)
{
    static const uint8_t two[4] = {2, 0, 0, 0};
    static const uint8_t over[4] = {collationMaxReorderRanges + 1, 0, 0, 0};
    static const uint8_t low[1] = {1};
    const uint8_t* r = t->reordering;
    Spoil(types, collationTypesHashAt, r - 4, two, 4, "a stored reordering's leading word 2");
    Spoil(types, collationTypesHashAt, r + 4, over, 4, "more ranges than a collator holds");
    Spoil(types, collationTypesHashAt, r, low, 1, "a boundary that is not a lead byte's start");
    Spoil(types, collationTypesHashAt, r + 12, r + 8, 4, "two ranges with the same limit");
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
        fputs("test_collation_data: cannot open " MLOC_COLLATION_FILE "\n", stderr);
        return 2;
    }
    TestRoot(&data);
    // A unit whose trie value names no list is refused with the file:
    // by its stored hash, and sealed again, by the value.
    DataView view = mlocViewOf(&data);
    DataSpan span = {0};
    CollationRoot r;
    if (mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) &&
        mlocReadCollationRoot(span, &r))
    {
        size_t at = (size_t)(r.trie.data - s_file);
        uint8_t* unit = s_file + (span.bytes - s_file);
        static const uint8_t bad[4] = {0xC9, 0xFF, 0xFF, 0xFF};
        memcpy(s_file + at, bad, 4);
        mlocData broken;
        bool stale = mlocOpenData(&broken, s_file, s_size) != mloc_success;
        uint64_t hash = mlocCollationUnitHash(span, collationRootHashAt);
        for (int i = 0; i < 8; i++)
        {
            unit[collationRootHashAt + i] = (uint8_t)(hash >> (8 * i));
        }
        CHECK(stale && mlocCollationHashValid(span, collationRootHashAt) &&
                  mlocOpenData(&broken, s_file, s_size) != mloc_success,
              "a file with a spoiled root unit refused at open, by its hash and by its values");
    }
    // Spoiled from a good copy, each check a collator's making trusts.
    file = fopen(MLOC_COLLATION_FILE, "rb");
    s_size = file == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    CHECK(mlocOpenData(&data, s_file, s_size) == mloc_success, "the good copy opens");
    view = mlocViewOf(&data);
    if (mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) &&
        mlocReadCollationRoot(span, &r))
    {
        SpoilViews(span, collationRootHashAt, &r, "the root");
    }
    DataSpan types = {0};
    CollationTailoring tailoring;
    CHECK(FindTailoring(&view, &types, &tailoring), "a tailoring with tables of its own");
    if (types.bytes != nullptr)
    {
        SpoilViews(types, collationTypesHashAt, &tailoring.view, "a tailoring");
    }
    DataSpan reordering = {0};
    if (mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) &&
        mlocReadCollationRoot(span, &r))
    {
        TestReorderings(&view, &r, &reordering, &tailoring);
    }
    CHECK(reordering.bytes != nullptr, "a stored reordering of more than one range");
    if (reordering.bytes != nullptr)
    {
        SpoilReordering(reordering, &tailoring);
    }
    CHECK(mlocOpenData(&data, s_file, s_size) == mloc_success, "and put back, it opens again");
    return s_failures == 0 ? 0 : 1;
}
