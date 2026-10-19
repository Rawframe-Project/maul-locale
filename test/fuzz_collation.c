// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the general compare (records mloc-0067 and
// mloc-0069) over the generated root. The first byte chooses the
// strength and the alternate setting, the second backwards, the case
// level, case first and numeric; the third where the rest is cut into
// two texts. Each text equals itself, and the order of two is the
// mirror of their order the other way.

#include "collation_compare.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>

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
    if (size < 3)
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
    const uint8_t* text = data + 3;
    size_t length = size - 3;
    size_t cut = length == 0 ? 0 : data[2] % (length + 1);
    const uint8_t* b = text + cut;
    size_t bLength = length - cut;
    int ab = mlocCollationCompare(root, &s, text, cut, b, bLength);
    int ba = mlocCollationCompare(root, &s, b, bLength, text, cut);
    Require(ab >= -1 && ab <= 1 && ab == -ba);
    Require(mlocCollationCompare(root, &s, text, cut, text, cut) == 0);
    Require(mlocCollationCompare(root, &s, b, bLength, b, bLength) == 0);
    return 0;
}
