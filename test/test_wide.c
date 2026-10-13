// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fixed-width integers (wide.h): products, sums and differences of
// known values, the width's limit, decimal digit counts, and division
// checked by its own identity (quotient * divisor + remainder is the
// numerator, the remainder below the divisor) over pseudo-random values
// and over the cases where algorithm D's estimate is two too large and
// must add the divisor back (Hacker's Delight's test values).

#include "test_harness.h"
#include "wide.h"

#include <string.h>

static Wide Limbs(const uint32_t* limbs, uint32_t count)
{
    Wide w = {0};
    memcpy(w.limb, limbs, count * sizeof(uint32_t));
    w.count = count;
    while (w.count > 0 && w.limb[w.count - 1] == 0)
    {
        w.count--;
    }
    return w;
}

static bool Equals(const Wide* w, const uint32_t* limbs, uint32_t count)
{
    Wide e = Limbs(limbs, count);
    return mlocWideCompare(w, &e) == 0 && memcmp(w, &e, sizeof(Wide)) == 0;
}

// Whether a division satisfies its identity.
static bool Divides(const Wide* n, const Wide* d)
{
    Wide q;
    Wide r;
    Wide back;
    mlocWideDivide(n, d, &q, &r);
    return mlocWideCompare(&r, d) < 0 && mlocWideMultiply(&back, &q, d) &&
           mlocWideAdd(&back, &back, &r) && mlocWideCompare(&back, n) == 0;
}

static void TestBasics(void)
{
    Wide zero = mlocWideOf(0);
    Wide max64 = mlocWideOf(UINT64_MAX);
    Wide square;
    static const uint32_t s_square[4] = {1, 0, 0xFFFFFFFE, 0xFFFFFFFF};
    CHECK(mlocWideIsZero(&zero) && zero.count == 0 && max64.count == 2 && !mlocWideIsZero(&max64),
          "values of 64-bit integers");
    CHECK(mlocWideMultiply(&square, &max64, &max64) && Equals(&square, s_square, 4),
          "(2^64 - 1)^2");
    Wide sum;
    static const uint32_t s_sum[3] = {0xFFFFFFFE, 0xFFFFFFFF, 1};
    CHECK(mlocWideAdd(&sum, &max64, &max64) && Equals(&sum, s_sum, 3), "a carry into a new limb");
    Wide difference;
    mlocWideSubtract(&difference, &sum, &max64);
    CHECK(mlocWideCompare(&difference, &max64) == 0, "a borrow across limbs");
    mlocWideSubtract(&difference, &max64, &max64);
    CHECK(mlocWideIsZero(&difference), "a difference of zero has no limbs");
    Wide one = mlocWideOf(1);
    Wide two = mlocWideOf(2);
    CHECK(mlocWideCompare(&one, &two) < 0 && mlocWideCompare(&two, &one) > 0 &&
              mlocWideCompare(&max64, &one) > 0 && mlocWideCompare(&one, &max64) < 0 &&
              mlocWideCompare(&one, &one) == 0,
          "comparison by length, then by limbs from the top");
    Wide ten = mlocWideOf(7);
    CHECK(mlocWideMultiplyAdd(&ten, 10, 3) && ten.limb[0] == 73 && ten.count == 1,
          "a small product and sum");
    Wide n = mlocWideOf(1000000007);
    CHECK(mlocWideDivideSmall(&n, 1000) == 7 && n.limb[0] == 1000000, "a small division");
}

static void TestLimits(void)
{
    Wide w = mlocWideOf(1);
    uint32_t doublings = 0;
    while (mlocWideMultiplyAdd(&w, 2, 0))
    {
        doublings++;
    }
    CHECK(doublings == 32 * wideLimbs - 1, "a value doubles up to the top bit");
    Wide top = mlocWideOf(0);
    top.limb[wideLimbs - 1] = 0x80000000;
    top.count = wideLimbs;
    Wide product;
    Wide two = mlocWideOf(2);
    Wide high = mlocWideOf(0);
    high.limb[wideLimbs / 2] = 1;
    high.count = wideLimbs / 2 + 1;
    CHECK(!mlocWideMultiply(&product, &high, &high), "a product past the width");
    CHECK(mlocWideMultiply(&product, &top, &two) == false, "a product of one limb too many");
    Wide low = mlocWideOf(0);
    low.limb[wideLimbs - 1] = 1;
    low.count = wideLimbs;
    CHECK(mlocWideMultiply(&product, &low, &two) && product.limb[wideLimbs - 1] == 2,
          "limb counts that add to one past the width, a product within it");
    Wide a = mlocWideOf(0);
    Wide b = mlocWideOf(0);
    a.limb[32] = 1;
    a.count = 33;
    b.limb[31] = 0x100;
    b.count = 32;
    CHECK(mlocWideMultiply(&product, &a, &b) && product.count == wideLimbs &&
              product.limb[wideLimbs - 1] == 0x100,
          "a product filling every limb");
    Wide all = top;
    for (uint32_t i = 0; i < wideLimbs; i++)
    {
        all.limb[i] = 0xFFFFFFFF;
    }
    Wide one = mlocWideOf(1);
    CHECK(!mlocWideAdd(&product, &all, &one) && !mlocWideMultiplyAdd(&all, 1, 1),
          "a sum past the width");
    Wide scaled = mlocWideOf(1);
    CHECK(mlocWideScale10(&scaled, 616) && !mlocWideScale10(&scaled, 1),
          "10^616 fits and 10^617 does not");
    scaled = mlocWideOf(1);
    CHECK(!mlocWideScale10(&scaled, 617), "10^617 fails in its steps of 10^9");
}

static void TestDigits(void)
{
    Wide w = mlocWideOf(0);
    CHECK(mlocWideDigitCount(&w) == 1, "zero has one digit");
    for (uint32_t k = 1; k < 600; k += 7)
    {
        Wide p = mlocWideOf(1);
        (void)mlocWideScale10(&p, k);
        Wide below = p;
        Wide one = mlocWideOf(1);
        mlocWideSubtract(&below, &p, &one);
        CHECK(mlocWideDigitCount(&p) == k + 1 && mlocWideDigitCount(&below) == k,
              "digits at a power of ten and one below");
    }
}

static void TestDivision(void)
{
    static const uint32_t s_u1[4] = {0, 0, 0x80000000, 0x7FFFFFFF};
    static const uint32_t s_v1[3] = {1, 0, 0x80000000};
    static const uint32_t s_q1[1] = {0xFFFFFFFE};
    static const uint32_t s_r1[3] = {2, 0xFFFFFFFF, 0x7FFFFFFF};
    static const uint32_t s_u2[4] = {0, 0xFFFFFFFE, 0, 0x80000000};
    static const uint32_t s_v2[2] = {0xFFFFFFFF, 0x80000000};
    static const uint32_t s_q2[2] = {7, 0xFFFFFFFE};
    static const uint32_t s_r2[2] = {7, 0x7FFFFFF5};
    static const uint32_t s_u3[3] = {3, 0, 0x80000000};
    static const uint32_t s_v3[3] = {1, 0, 0x20000000};
    static const uint32_t s_r3[3] = {0, 0, 0x20000000};
    Wide q;
    Wide r;
    Wide u = Limbs(s_u1, 4);
    Wide v = Limbs(s_v1, 3);
    mlocWideDivide(&u, &v, &q, &r);
    CHECK(Equals(&q, s_q1, 1) && Equals(&r, s_r1, 3), "an estimate the divisor is added back to");
    u = Limbs(s_u2, 4);
    v = Limbs(s_v2, 2);
    mlocWideDivide(&u, &v, &q, &r);
    CHECK(Equals(&q, s_q2, 2) && Equals(&r, s_r2, 2), "an estimate two too large");
    u = Limbs(s_u3, 3);
    v = Limbs(s_v3, 3);
    mlocWideDivide(&u, &v, &q, nullptr);
    mlocWideDivide(&u, &v, nullptr, &r);
    CHECK(q.count == 1 && q.limb[0] == 3 && Equals(&r, s_r3, 3), "either output alone");
    static const uint32_t s_u4[3] = {0, 0x80000000, 0x7FFFFFFF};
    static const uint32_t s_v4[2] = {0, 0x80000000};
    u = Limbs(s_u4, 3);
    v = Limbs(s_v4, 2);
    mlocWideDivide(&u, &v, &q, &r);
    CHECK(q.count == 1 && q.limb[0] == 0xFFFFFFFF && mlocWideIsZero(&r),
          "an estimate of exactly the largest limb");
    Wide small = mlocWideOf(5);
    Wide large = mlocWideOf(UINT64_MAX);
    mlocWideDivide(&small, &large, &q, &r);
    CHECK(mlocWideIsZero(&q) && mlocWideCompare(&r, &small) == 0, "a numerator below the divisor");
    mlocWideDivide(&large, &small, &q, &r);
    CHECK(q.limb[0] == 0x33333333 && q.limb[1] == 0x33333333 && mlocWideIsZero(&r),
          "a divisor of one limb");
    // Pseudo-random values of many lengths, each division by its identity.
    uint64_t state = 0x9E3779B97F4A7C15u;
    bool all = true;
    for (int trial = 0; trial < 2000; trial++)
    {
        uint32_t nl = 1 + (uint32_t)(trial % (wideLimbs - 1));
        uint32_t dl = 1 + (uint32_t)((trial * 7) % nl);
        uint32_t limbs[wideLimbs];
        for (uint32_t i = 0; i < nl; i++)
        {
            state = state * 6364136223846793005u + 1442695040888963407u;
            limbs[i] = (uint32_t)(state >> 32);
        }
        Wide n = Limbs(limbs, nl);
        for (uint32_t i = 0; i < dl; i++)
        {
            state = state * 6364136223846793005u + 1442695040888963407u;
            // Some divisors with a small top limb, some with a full one.
            limbs[i] = trial % 3 == 0 && i == dl - 1 ? (uint32_t)(state >> 60) | 1
                                                     : (uint32_t)(state >> 32);
        }
        Wide d = Limbs(limbs, dl);
        all &= mlocWideIsZero(&d) || Divides(&n, &d);
    }
    CHECK(all, "pseudo-random divisions satisfy their identity");
}

int main(void)
{
    TestBasics();
    TestLimits();
    TestDigits();
    TestDivision();
    return s_failures == 0 ? 0 : 1;
}
