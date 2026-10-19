// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The general compare (record mloc-0067) over the generated root: each
// level alone, its strength's stop, a text ending first, completely
// ignorable code points, canonically equal texts, Han and unassigned
// code points, and every answer's mirror.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_compare.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 19];
static CollationRoot s_root;

// The order of a and b up to the strength, checked against its mirror.
static int Order(const char* a, const char* b, uint32_t strength)
{
    int ab = mlocCollationCompare(&s_root, strength, (const uint8_t*)a, strlen(a),
                                  (const uint8_t*)b, strlen(b));
    int ba = mlocCollationCompare(&s_root, strength, (const uint8_t*)b, strlen(b),
                                  (const uint8_t*)a, strlen(a));
    return ab == -ba ? ab : 2;
}

static void TestLevels(void)
{
    CHECK(Order("a", "b", collationPrimary) == -1 && Order("b", "a", collationTertiary) == 1 &&
              Order("a", "a", collationTertiary) == 0,
          "a before b at the first level");
    CHECK(Order("a", "\xC3\xA1", collationPrimary) == 0 &&
              Order("a", "\xC3\xA1", collationSecondary) == -1 &&
              Order("\xC3\xA1", "b", collationSecondary) == -1,
          "an accent only at the second level, after the first's order");
    CHECK(Order("a", "A", collationSecondary) == 0 && Order("a", "A", collationTertiary) == -1 &&
              Order("A", "\xC3\xA1", collationTertiary) == -1,
          "case only at the third level, after the second's order");
    CHECK(Order("cot\xC3\xA9", "c\xC3\xB4te", collationSecondary) == -1,
          "secondaries forwards: cot\xC3\xA9 before c\xC3\xB4te");
}

static void TestTexts(void)
{
    CHECK(Order("", "a", collationPrimary) == -1 && Order("a", "ab", collationTertiary) == -1 &&
              Order("", "", collationTertiary) == 0,
          "a text that ends first is the lesser");
    CHECK(Order("\xC3\xA0", "a\xCC\x80", collationTertiary) == 0 &&
              Order("\xE1\xBB\x87", "e\xCC\xA3\xCC\x82", collationTertiary) == 0 &&
              Order("\xE1\xBB\x87", "e\xCC\x82\xCC\xA3", collationTertiary) == 0,
          "canonically equal texts equal");
    CHECK(mlocCollationCompare(&s_root, collationTertiary, (const uint8_t*)"a\0b", 3,
                               (const uint8_t*)"ab", 2) == 0 &&
              Order("a\xC2\xAD"
                    "b",
                    "ab", collationTertiary) == 0,
          "U+0000 and a soft hyphen completely ignorable");
    CHECK(Order("\xE4\xB8\x80", "\xE4\xB8\x81", collationPrimary) == -1 &&
              Order("z", "\xE4\xB8\x80", collationPrimary) == -1 &&
              Order("\xE4\xB8\x80", "\xCD\xB8", collationPrimary) == -1 &&
              Order("\xCD\xB8", "\xEF\xBF\xBD", collationPrimary) == -1,
          "Latin, then Han in order, then unassigned, then U+FFFD");
    CHECK(Order("ab", "a\xFF", collationPrimary) == -1, "a bad byte sorts as U+FFFD");
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
    if (size == 0 || size == sizeof(s_file) || mlocOpenData(&data, s_file, size) != mloc_success)
    {
        fputs("test_collation_compare: cannot open " MLOC_COLLATION_FILE "\n", stderr);
        return 2;
    }
    DataView view = mlocViewOf(&data);
    DataSpan span = {0};
    if (!mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) ||
        !mlocReadCollationRoot(span, &s_root))
    {
        fputs("test_collation_compare: no root unit\n", stderr);
        return 2;
    }
    TestLevels();
    TestTexts();
    return s_failures == 0 ? 0 : 1;
}
