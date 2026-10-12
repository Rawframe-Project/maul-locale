// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The likely subtags table's validator: the compiled-in table passes,
// and a copy with one entry spoiled is refused for each rule.

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_likely.h"
#include "locale_tables.h"
#include "test_harness.h"

#include <stdlib.h>
#include <string.h>

static DataSpan s_builtin;
static uint8_t* s_copy;

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// Restores the copy, writes value at offset, and asks the validator.
static bool ValidWith(size_t offset, uint64_t value, int bytes)
{
    memcpy(s_copy, s_builtin.bytes, s_builtin.size);
    Put(s_copy + offset, value, bytes);
    return mlocValidLikely(nullptr, (DataSpan){s_copy, s_builtin.size});
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    CHECK(mlocFindDataSetUnit(&data, kindLocaleLikely, nullptr, 0, &s_builtin),
          "the compiled-in data has the table");
    s_copy = malloc(s_builtin.size);
    CHECK(s_copy != nullptr && ValidWith(0, mlocLoad32(s_builtin.bytes), 4),
          "the compiled-in table is valid");
    uint32_t languageCount = mlocLoad32(s_builtin.bytes);
    uint32_t generalCount = mlocLoad32(s_builtin.bytes + 4);
    size_t generalKeys = 16;
    size_t generalValues = generalKeys + (size_t)generalCount * 8;
    size_t languageValues = generalValues + (size_t)generalCount * 8;
    size_t languageKeys = languageValues + (size_t)languageCount * 4;
    uint32_t en = mlocPackLanguage("en", 2);
    uint32_t latn = mlocPackScript("Latn", 4);
    uint32_t us = mlocPackRegion("US", 2);
    CHECK(!ValidWith(8, 1, 8), "the reserved word is zero");
    CHECK(!ValidWith(0, languageCount + 1, 4), "the counts give the size");
    CHECK(!ValidWith(generalKeys, mlocLoad64(s_builtin.bytes + generalKeys + 8), 8),
          "general keys in order");
    CHECK(!ValidWith(generalKeys, mlocPackTag(1, latn, us), 8), "a language of one letter");
    // The last general key is the largest, so a larger one keeps the
    // order and fails only on what it spoils.
    size_t lastKey = generalKeys + (size_t)(generalCount - 1) * 8;
    CHECK(!ValidWith(lastKey, mlocPackTag(27 << 10 | 1 << 5, 0, us), 8), "a letter past z");
    CHECK(!ValidWith(generalKeys, mlocPackTag(1 << 10 | 1 << 5, 1 << 15 | 1, 0), 8),
          "a script with a missing letter");
    CHECK(!ValidWith(lastKey, mlocLoad64(s_builtin.bytes + lastKey) | (uint64_t)1 << 50, 8),
          "bits past the tag");
    CHECK(!ValidWith(generalValues, mlocPackTag(en, latn, 0), 8), "a value without a region");
    CHECK(!ValidWith(generalValues, mlocPackTag(en, 0, us), 8), "a value without a script");
    CHECK(!ValidWith(generalValues, mlocPackTag(en, latn, 2024), 8), "a region past 999");
    // und in the slot of the first language after it keeps the order.
    uint32_t und = mlocPackLanguage("und", 3);
    size_t slot = languageKeys;
    while (mlocLoad16(s_builtin.bytes + slot) < und)
    {
        slot += 2;
    }
    CHECK(!ValidWith(slot, und, 2), "und alone is general");
    CHECK(!ValidWith(languageKeys, mlocLoad16(s_builtin.bytes + languageKeys + 2), 2),
          "language keys in order");
    CHECK(!ValidWith(languageValues, (uint64_t)1 << 31 | latn << 11 | us, 4),
          "a language value's top bit");
    CHECK(!ValidWith(languageValues, latn << 11, 4), "a language value without a region");
    CHECK(!mlocValidLikely(nullptr, (DataSpan){s_copy, 8}), "shorter than the header");
    free(s_copy);
    return s_failures == 0 ? 0 : 1;
}
