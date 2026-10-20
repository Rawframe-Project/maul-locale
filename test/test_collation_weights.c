// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailoring builder's weight allocator (record mloc-0071), on gaps
// whose weights follow from ICU4C's CollationWeights by hand: a middle
// range, a lower and an upper range that meet, ranges lengthened by one
// and two bytes, the tertiaries' 3F limit, a compressible lead byte's
// second byte, and the refusals.

#include "collation_weights.h"
#include "test_harness.h"

#include <stddef.h>

// Whether the n weights allocated in the gap begin with the expected
// ones, all rising, inside the gap, each within the level's bytes.
static bool Gives(CollationWeights* w, uint32_t lower, uint32_t upper, int32_t n,
                  const uint32_t* expected, size_t count)
{
    if (!CollationWeightsAllocate(w, lower, upper, n))
    {
        return false;
    }
    uint32_t previous = lower;
    for (int32_t i = 0; i < n; i++)
    {
        uint32_t weight = CollationWeightsNext(w);
        bool inside = weight > previous && weight < upper;
        for (int32_t b = 1; inside && b <= CollationWeightLength(weight); b++)
        {
            uint32_t byte = weight >> (8 * (4 - b)) & 0xFF;
            inside = byte >= w->minBytes[b] && byte <= w->maxBytes[b];
        }
        if (!inside || ((size_t)i < count && weight != expected[i]))
        {
            return false;
        }
        previous = weight;
    }
    // The ranges may hold more than n: ICU keeps a shortest range whole.
    return true;
}

static void TestPrimaries(void)
{
    CollationWeights w;
    CollationWeightsForPrimary(&w, false);
    const uint32_t one[] = {0x31000000};
    const uint32_t met[] = {0x30110000, 0x30120000, 0x30130000};
    CHECK(Gives(&w, 0x30000000, 0x32000000, 1, one, 1) &&
              Gives(&w, 0x30100000, 0x30200000, 3, met, 3),
          "one-byte primaries in the middle range; two-byte ones where a lower and an upper "
          "range overlap");
    // One two-byte weight in the gap; 300 take a byte, then a second.
    const uint32_t lengthened[] = {0x30110200, 0x30110300};
    bool ok = Gives(&w, 0x30100000, 0x30120000, 300, lengthened, 2);
    ok = ok && CollationWeightsAllocate(&w, 0x30100000, 0x30120000, 300);
    uint32_t weights[300];
    for (int i = 0; ok && i < 300; i++)
    {
        weights[i] = CollationWeightsNext(&w);
    }
    CHECK(ok && weights[252] == 0x3011FE00 && weights[253] == 0x3011FF02 &&
              weights[299] == 0x3011FF30,
          "a range lengthened by a byte, then its last weight by another");
    CollationWeightsForPrimary(&w, true);
    const uint32_t compressible[] = {0x30050000, 0x30060000, 0x30070000};
    const uint32_t top[] = {0x30FE0000};
    CHECK(Gives(&w, 0x30040000, 0x30080000, 3, compressible, 3) &&
              Gives(&w, 0x30FD0000, 0x31000000, 1, top, 1),
          "a compressible lead byte's second byte from 04 to FE, FF left to the compression");
}

static void TestLowerLevels(void)
{
    CollationWeights w;
    CollationWeightsForTertiary(&w);
    const uint32_t tertiaries[] = {0x00000502, 0x00000503, 0x00000504};
    CHECK(Gives(&w, 0x00000400, 0x00000510, 3, tertiaries, 3) &&
              !CollationWeightsAllocate(&w, 0x00000500, 0x00000510, 1),
          "tertiaries in an upper range; none where the upper limit extends the lower");
    CHECK(Gives(&w, 0x00003E00, 0x00004000, 1, nullptr, 0) &&
              !CollationWeightsAllocate(&w, 0x00003F00, 0x00004000, 1),
          "tertiary bytes at most 3F");
    CollationWeightsForSecondary(&w);
    const uint32_t secondaries[] = {0x00000600};
    CHECK(Gives(&w, 0x00000500, 0x00000700, 1, secondaries, 1), "secondaries in the middle range");
}

static void TestRefusals(void)
{
    CollationWeights w;
    CollationWeightsForPrimary(&w, false);
    CHECK(!CollationWeightsAllocate(&w, 0x31000000, 0x30000000, 1) &&
              !CollationWeightsAllocate(&w, 0x30000000, 0x30000000, 1) &&
              !CollationWeightsAllocate(&w, 0x30000000, 0x30050000, 1) &&
              !CollationWeightsAllocate(&w, 0x30000000, 0x31000000, 1) &&
              !CollationWeightsAllocate(&w, 0x30100000, 0x30120000, 70000),
          "limits out of order or equal, an upper limit extending the lower, adjacent "
          "one-byte weights, and more weights than four bytes hold, refused");
}

int main(void)
{
    TestPrimaries();
    TestLowerLevels();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
