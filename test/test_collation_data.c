// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation data file (record mloc-0065): its root unit found and
// valid, a few elements read through the iterator as CLDR 49's root
// gives them, and a copy with the unit spoiled refused at open.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_iterator.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 19];
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
    // A unit whose trie value names no list is refused with the file.
    DataView view = mlocViewOf(&data);
    DataSpan span = {0};
    CollationRoot r;
    if (mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) &&
        mlocReadCollationRoot(span, &r))
    {
        size_t at = (size_t)(r.trie.data - s_file);
        static const uint8_t bad[4] = {0xC9, 0xFF, 0xFF, 0xFF};
        memcpy(s_file + at, bad, 4);
        mlocData broken;
        CHECK(mlocOpenData(&broken, s_file, s_size) != mloc_success,
              "a file with a spoiled root unit refused at open");
    }
    return s_failures == 0 ? 0 : 1;
}
