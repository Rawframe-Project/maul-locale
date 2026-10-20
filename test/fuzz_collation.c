// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the general compare (records mloc-0067 and
// mloc-0069) over the generated root. The first byte chooses the
// strength and the alternate setting, the second backwards, the case
// level, case first and numeric; the third and fourth up to three
// reorder codes, which may be refused; the fifth where the rest is cut
// into two texts. Each text equals itself, the order of two is the
// mirror of their order the other way, and their sort keys (record
// mloc-0070) order them alike.

#include "collation_compare.h"
#include "collation_key.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

// The root read from the file once; null when it does not open.
static const CollationRoot* Root(void)
{
    alignas(8) static uint8_t s_file[1 << 19];
    static CollationRoot s_root;
    static int s_state;
    if (s_state == 0)
    {
        s_state = -1;
        FILE* f = fopen(MLOC_COLLATION_FILE, "rb");
        size_t n = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
        if (f != nullptr)
        {
            fclose(f);
        }
        mlocData file;
        DataSpan span = {0};
        if (n > 0 && n < sizeof(s_file) && mlocOpenData(&file, s_file, n) == mloc_success)
        {
            DataView view = mlocViewOf(&file);
            s_state = mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) &&
                              mlocReadCollationRoot(span, &s_root)
                          ? 1
                          : -1;
        }
    }
    return s_state == 1 ? &s_root : nullptr;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const CollationRoot* root = Root();
    Require(root != nullptr);
    if (size < 5)
    {
        return 0;
    }
    CollationSettings s = mlocCollationDefaults(root);
    s.strength = collationPrimary + data[0] % 5;
    s.shifted = (data[0] & 0x80) != 0;
    s.backwards = (data[1] & 1) != 0;
    s.caseLevel = (data[1] & 2) != 0;
    s.caseFirst = (data[1] >> 2) % 3;
    s.numeric = (data[1] & 0x10) != 0;
    static const uint32_t codes[8] = {
        'G' << 24 | 'r' << 16 | 'e' << 8 | 'k',
        'L' << 24 | 'a' << 16 | 't' << 8 | 'n',
        'C' << 24 | 'y' << 16 | 'r' << 8 | 'l',
        'H' << 24 | 'a' << 16 | 'n' << 8 | 'i',
        'Z' << 24 | 'z' << 16 | 'z' << 8 | 'z',
        collationGroupDigit,
        collationGroupPunctuation,
        collationGroupSpace,
    };
    uint32_t chosen[3];
    size_t count = (size_t)(data[2] >> 6);
    for (size_t i = 0; i < count; i++)
    {
        chosen[i] = codes[(i == 0 ? data[2] : data[3] >> (3 * (i - 1))) & 7];
    }
    static CollationReorder reorder;
    bool changes = false;
    mlocResult made = mlocCollationMakeReorder(root, chosen, count, &reorder, &changes);
    s.reorder = made == mloc_success && changes ? &reorder : nullptr;
    const uint8_t* text = data + 5;
    size_t length = size - 5;
    size_t cut = length == 0 ? 0 : data[4] % (length + 1);
    const uint8_t* b = text + cut;
    size_t bLength = length - cut;
    int ab = mlocCollationCompare(root, &s, text, cut, b, bLength);
    int ba = mlocCollationCompare(root, &s, b, bLength, text, cut);
    Require(ab >= -1 && ab <= 1 && ab == -ba);
    // The sort keys order the texts as the compare does, and a short
    // buffer gives the same length.
    static uint8_t keys[2][1 << 16];
    size_t n = mlocCollationSortKey(root, &s, text, cut, keys[0], sizeof(keys[0]));
    size_t m = mlocCollationSortKey(root, &s, b, bLength, keys[1], sizeof(keys[1]));
    Require(n <= sizeof(keys[0]) && m <= sizeof(keys[1]) &&
            mlocCollationSortKey(root, &s, text, cut, keys[0], n / 2) == n);
    mlocCollationSortKey(root, &s, text, cut, keys[0], n);
    int r = memcmp(keys[0], keys[1], n < m ? n : m);
    r = r != 0 ? r : (n > m) - (n < m);
    Require(((r > 0) - (r < 0)) == ab);
    Require(mlocCollationCompare(root, &s, text, cut, text, cut) == 0);
    Require(mlocCollationCompare(root, &s, b, bLength, b, bLength) == 0);
    return 0;
}
