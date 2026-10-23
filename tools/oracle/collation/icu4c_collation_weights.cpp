// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailorings' weight allocator against ICU4C:
// reads lines "level compressible lower upper n" (level 1 primary, 2
// secondary, 3 tertiary; the limits in hex) and writes "fail", or the
// first and last of the n weights and a hash of all n in order
// (hash = hash * 1000003 + weight, 64-bit), as test_collation_weights
// checks ours. CollationWeights is exported by ICU4C's i18n library;
// its header is ICU's internal collationweights.h (main branch):
//   g++ -O2 -I$ICU_SRC/i18n -I$ICU/include -DU_SHOW_CPLUSPLUS_API=1 \
//       icu4c_collation_weights.cpp -L$ICU/lib -licui18n -licuuc \
//       -Wl,-rpath,$ICU/lib -o icu_weights
// Cases: python3 -I weights_cases.py COUNT SEED.
#include "collationweights.h"

#include <cstdio>
using icu::CollationWeights;
int main()
{
    unsigned level, comp, lo, hi;
    int n;
    while (scanf("%u %u %x %x %d", &level, &comp, &lo, &hi, &n) == 5)
    {
        CollationWeights w;
        if (level == 1)
        {
            w.initForPrimary(comp);
        }
        else if (level == 2)
        {
            w.initForSecondary();
        }
        else
        {
            w.initForTertiary();
        }
        if (!w.allocWeights(lo, hi, n))
        {
            printf("fail\n");
            continue;
        }
        unsigned long long sum = 0;
        unsigned last = 0, first = 0;
        for (int i = 0; i < n; i++)
        {
            unsigned x = w.nextWeight();
            if (i == 0)
            {
                first = x;
            }
            last = x;
            sum = sum * 1000003ULL + x;
        }
        printf("%08x %08x %016llx\n", first, last, sum);
    }
}
