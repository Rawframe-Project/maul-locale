// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Sort keys over the generated root, against the bytes ICU4C 79 writes for
// the same texts and options: each level's form, the common weights' runs,
// backwards secondaries, the case level, shifted quaternaries, the
// identical level's BOCSU, numeric order and reordering; the caller's
// capacity, and the count behind a key's length.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_key.h"
#include "collation_reorder.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"

#include "maul-locale/data.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static CollationRoot s_root;

// Whether the text's key under the settings is the hex given.
static bool Key(const char* text, const CollationSettings* s, const char* hex)
{
    uint8_t key[256];
    size_t n =
        mlocCollationSortKey(&s_root, s, (const uint8_t*)text, strlen(text), key, sizeof(key));
    char written[2 * sizeof(key) + 1] = "";
    for (size_t i = 0; i < n && i < sizeof(key); i++)
    {
        snprintf(written + 2 * i, 3, "%02x", key[i]);
    }
    bool same = n <= sizeof(key) && strcmp(written, hex) == 0;
    if (!same)
    {
        printf("  key of \"%s\": %s, ICU %s\n", text, written, hex);
    }
    return same;
}

static void TestLevels(void)
{
    CollationSettings s = mlocCollationDefaults(&s_root);
    CHECK(Key("a", &s, "2b0105010500") && Key("", &s, "010100") &&
              Key("A\xCC\x81"
                  "b",
                  &s, "2b2d0145880501dc0600") &&
              Key("a-b", &s, "2b050e2d0107010700") && Key("\xCE\xB1", &s, "61060105010500"),
          "tertiary keys: primaries, compressed in a run, then secondaries and tertiaries with "
          "their common runs");
    s.backwards = true;
    CHECK(Key("c\xC3\xB4te", &s, "2f47513301448e06010900") && Key("l\xEF\xBF\xBE"
                                                                  "f",
                                                                  &s, "41023501050205010700"),
          "backwards secondaries, each segment reversed at the merge separator");
    s = mlocCollationDefaults(&s_root);
    s.caseFirst = collationUpperFirst;
    CHECK(Key("a", &s, "2b0105018500") && Key("A\xCC\x81"
                                              "b",
                                              &s, "2b2d01458805011c8600"),
          "upper first: the case bits in the tertiaries");
    s = mlocCollationDefaults(&s_root);
    s.strength = collationPrimary;
    s.caseLevel = true;
    CHECK(Key("a", &s, "2b0100") && Key("A\xCC\x81"
                                        "b",
                                        &s, "2b2d01f100"),
          "the case level at the primary strength, in nibbles");
}

static void TestShiftedAndIdentical(void)
{
    CollationSettings s = mlocCollationDefaults(&s_root);
    s.strength = collationQuaternary;
    s.shifted = true;
    CHECK(Key("a-b", &s, "2b2d01060106011c050e1c00") && Key("", &s, "01010100"),
          "shifted: the variable's primary in the quaternaries, the others' runs compressed");
    s = mlocCollationDefaults(&s_root);
    s.strength = collationIdentical;
    CHECK(Key("a", &s, "2b0105010501019200") &&
              Key("A\xCC\x81"
                  "b",
                  &s, "2b2d0145880501dc06010172d4ba2e0c00") &&
              Key("\xCE\xB1", &s, "6106010501050101d56d00") &&
              Key("l\xEF\xBF\xBE"
                  "f",
                  &s, "4102350107010701019d029700"),
          "identical: the NFD code points in BOCSU after an empty quaternary level, U+FFFE as "
          "02");
    s.shifted = true;
    s.backwards = true;
    s.caseFirst = collationUpperFirst;
    s.caseLevel = true;
    CHECK(Key("c\xC3\xB4te", &s, "2f47513301448e060170010901200194a0d4bb2e1e9600"),
          "every level together");
}

static void TestNumericAndReorder(void)
{
    CollationSettings s = mlocCollationDefaults(&s_root);
    s.numeric = true;
    CHECK(Key("a10", &s, "2b100c0106010600"), "numeric: the value's primary");
    static CollationReorder reorder;
    const uint32_t greek[] = {(uint32_t)'G' << 24 | (uint32_t)'r' << 16 | (uint32_t)'e' << 8 | 'k'};
    bool changes = false;
    s = mlocCollationDefaults(&s_root);
    s.reorder =
        mlocCollationMakeReorder(&s_root, greek, 1, &reorder, &changes) == mloc_success && changes
            ? &reorder
            : nullptr;
    CHECK(s.reorder != nullptr && Key("\xCE\xB1", &s, "28060105010500") &&
              Key("a", &s, "2b0105010500"),
          "reordering: Greek's lead byte moved, Latin's kept");
}

static void TestBocsu(void)
{
    // Differences at each of BOCSU's bounds: 80 and 81 from the first
    // base, -80 and -81, 10,667 and 10,668, -10,668 and -10,669, 192,785
    // and 192,786, -192,786 and -192,787.
    static const char* const cases[][2] = {
        {"\xC2\xA0", "04010501e80101d100"},
        {"\xC2\xA1", "075c010501050101d25400"},
        {"\xEE\x84\x80\xEE\x84\x80", "fe109a10109a10010601060101fce65c3100"},
        {"\xEE\x84\x80\xEE\x83\xBF", "fe109a10109a02010601060101fce65c30af00"},
        {"\xE2\xA7\xBB", "0cf08b010501050101fc2c00"},
        {"\xE2\xA7\xBC", "099e010501050101fc2d2d00"},
        {"\xF3\xB0\x80\x80\xF3\xAD\x9A\xA4", "fedb0564d8b32c010601060101ff125d3a06d600"},
        {"\xF3\xB0\x80\x80\xF3\xAD\x9A\xA3", "fedb0564d8b31e010601060101ff125d3a06d5d500"},
        {"\xF0\xAF\x85\xA1", "fe2e30c6010501050101ff05ff00"},
        {"\xF0\xAF\x85\xA2", "fe2e30d4010501050101ff06060300"},
        {"\xF4\x8F\xBF\xBF\xF3\xA0\xBA\xBE", "fef7af1ecd822c010601060101ff14694b03fd0300"},
        {"\xF4\x8F\xBF\xBF\xF3\xA0\xBA\xBD", "fef7af1ecd821e010601060101ff14694b03fcfcff00"},
    };
    CollationSettings s = mlocCollationDefaults(&s_root);
    s.strength = collationIdentical;
    bool ok = true;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        ok = Key(cases[i][0], &s, cases[i][1]) && ok;
    }
    CHECK(ok, "the identical level's BOCSU at each bound of one, two, three and four bytes");
}

static void TestCapacity(void)
{
    CollationSettings s = mlocCollationDefaults(&s_root);
    uint8_t key[16];
    memset(key, 0xEE, sizeof(key));
    size_t full = mlocCollationSortKey(&s_root, &s, (const uint8_t*)"abc", 3, nullptr, 0);
    size_t part = mlocCollationSortKey(&s_root, &s, (const uint8_t*)"abc", 3, key, 4);
    s.backwards = true;
    size_t backwards = mlocCollationSortKey(&s_root, &s, (const uint8_t*)"c\xC3\xB4te", 6, key, 4);
    CHECK(full > 4 && part == full && backwards > 4 && key[4] == 0xEE && key[15] == 0xEE,
          "a key's length with no room or too little, nothing written past the room");
}

// The count behind a key's length. A key past a 32-bit size_t needs more
// text than a test can allocate, so the count starts near 2^32 instead.
static void TestLengthCount(void)
{
    uint8_t out[4] = {0};
    CollationSink sink = {out, sizeof(out), UINT32_MAX - 1};
    mlocCollationSinkPut(&sink, 0x41);
    mlocCollationSinkPut(&sink, 0x42);
    CHECK(sink.length == (uint64_t)UINT32_MAX + 1, "a count past 2^32");
    size_t expected = SIZE_MAX == UINT32_MAX ? SIZE_MAX : (size_t)((uint64_t)UINT32_MAX + 1);
    CHECK(mlocCollationSinkLength(&sink) == expected,
          "its length, SIZE_MAX where a size_t has 32 bits");
    CHECK(memcmp(out, "\0\0\0\0", 4) == 0, "nothing written past the capacity");
    CollationSink full = {out, sizeof(out), UINT64_MAX};
    CHECK(mlocCollationSinkLength(&full) == SIZE_MAX, "a count past SIZE_MAX gives SIZE_MAX");
}

int main(void)
{
    FILE* f = fopen(MLOC_COLLATION_FILE, "rb");
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocData data;
    DataSpan span = {0};
    if (size == 0 || size == sizeof(s_file) || mlocOpenData(&data, s_file, size) != mloc_success)
    {
        fputs("test_collation_key: cannot open " MLOC_COLLATION_FILE "\n", stderr);
        return 2;
    }
    DataView view = mlocViewOf(&data);
    if (!mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) ||
        !mlocReadCollationRoot(span, &s_root))
    {
        fputs("test_collation_key: no root unit\n", stderr);
        return 2;
    }
    TestLevels();
    TestShiftedAndIdentical();
    TestNumericAndReorder();
    TestBocsu();
    TestCapacity();
    TestLengthCount();
    return s_failures == 0 ? 0 : 1;
}
