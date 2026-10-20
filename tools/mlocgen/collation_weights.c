// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_weights.h"

#include <stdlib.h>
#include <string.h>

enum
{
    mergeSeparatorByte = 0x02,
    levelSeparatorByte = 0x01,
    trailByte = 0xFF,
    compressionLow = 0x03,
    compressionHigh = 0xFF,
};

int32_t CollationWeightLength(uint32_t weight)
{
    return (weight & 0xFFFFFF) == 0 ? 1 : (weight & 0xFFFF) == 0 ? 2 : (weight & 0xFF) == 0 ? 3 : 4;
}

// The byte at position 1 to 4 (also "the trail" of a weight that long).
static uint32_t Byte(uint32_t weight, int32_t position)
{
    return position < 1 || position > 4 ? 0 : weight >> (8 * (4 - position)) & 0xFF;
}

// The weight with the byte at the position set and the bytes after it
// cleared (ICU's setWeightTrail).
static uint32_t SetTrail(uint32_t weight, int32_t position, uint32_t trail)
{
    if (position < 1 || position > 4)
    {
        return weight;
    }
    int shift = 8 * (4 - position);
    return (weight & (uint32_t)(0xFFFFFF00ULL << shift)) | trail << shift;
}

// The weight with the byte at the position set, the others kept (ICU's
// setWeightByte).
static uint32_t SetByte(uint32_t weight, int32_t position, uint32_t byte)
{
    if (position < 1 || position > 4)
    {
        return weight;
    }
    int shift = 8 * (4 - position);
    uint32_t mask = ~(0xFFU << shift);
    return (weight & mask) | byte << shift;
}

static uint32_t Truncate(uint32_t weight, int32_t length)
{
    return length == 0 ? 0 : weight & (uint32_t)(0xFFFFFFFFULL << (8 * (4 - length)));
}

// One at the byte of the length (1 to 4).
static uint32_t Unit(int32_t length)
{
    return length < 1 || length > 4 ? 0 : 1U << (8 * (4 - length));
}

static uint32_t IncrementTrail(uint32_t weight, int32_t length)
{
    return weight + Unit(length);
}

static uint32_t DecrementTrail(uint32_t weight, int32_t length)
{
    return weight - Unit(length);
}

static int32_t CountBytes(const CollationWeights* w, int32_t position)
{
    return (int32_t)(w->maxBytes[position] - w->minBytes[position] + 1);
}

void CollationWeightsForPrimary(CollationWeights* w, bool compressible)
{
    memset(w, 0, sizeof(*w));
    w->middleLength = 1;
    w->minBytes[1] = mergeSeparatorByte + 1;
    w->maxBytes[1] = trailByte;
    w->minBytes[2] = compressible ? compressionLow + 1 : 2;
    w->maxBytes[2] = compressible ? compressionHigh - 1 : 0xFF;
    w->minBytes[3] = 2;
    w->maxBytes[3] = 0xFF;
    w->minBytes[4] = 2;
    w->maxBytes[4] = 0xFF;
}

static void ForLower(CollationWeights* w, uint32_t maxByte)
{
    memset(w, 0, sizeof(*w));
    w->middleLength = 3;
    w->minBytes[3] = levelSeparatorByte + 1;
    w->maxBytes[3] = maxByte;
    w->minBytes[4] = 2;
    w->maxBytes[4] = maxByte;
}

void CollationWeightsForSecondary(CollationWeights* w)
{
    ForLower(w, 0xFF);
}

void CollationWeightsForTertiary(CollationWeights* w)
{
    ForLower(w, 0x3F);
}

// The next weight of the length, carrying into earlier bytes.
static uint32_t Increment(const CollationWeights* w, uint32_t weight, int32_t length)
{
    // Every byte at its most has no next weight; the allocation never asks.
    for (length = length > 4 ? 4 : length; length >= 1; length--)
    {
        uint32_t byte = Byte(weight, length);
        if (byte < w->maxBytes[length])
        {
            return SetByte(weight, length, byte + 1);
        }
        weight = SetByte(weight, length, w->minBytes[length]);
    }
    return 0xFFFFFFFF;
}

// The weight offset weights further on, carrying.
static uint32_t IncrementBy(const CollationWeights* w, uint32_t weight, int32_t length,
                            int32_t offset)
{
    for (length = length > 4 ? 4 : length; length >= 1; length--)
    {
        offset += (int32_t)Byte(weight, length);
        if ((uint32_t)offset <= w->maxBytes[length])
        {
            return SetByte(weight, length, (uint32_t)offset);
        }
        offset -= (int32_t)w->minBytes[length];
        weight = SetByte(weight, length,
                         w->minBytes[length] + (uint32_t)(offset % CountBytes(w, length)));
        offset /= CountBytes(w, length);
    }
    return 0xFFFFFFFF;
}

// The range's weights one byte longer, each spanning its new byte.
static void Lengthen(const CollationWeights* w, WeightRange* range)
{
    // Four bytes is the longest; the allocation stops there.
    if (range->length >= 4)
    {
        return;
    }
    int32_t length = range->length + 1;
    range->start = SetTrail(range->start, length, w->minBytes[length]);
    range->end = SetTrail(range->end, length, w->maxBytes[length]);
    range->count *= CountBytes(w, length);
    range->length = length;
}

static int ByStart(const void* x, const void* y)
{
    const WeightRange* a = x;
    const WeightRange* b = y;
    return (a->start > b->start) - (a->start < b->start);
}

// The ranges below the upper limit's bytes, as ICU's getWeightRanges
// finds them from the upper end.
static uint32_t UpperRanges(const CollationWeights* w, uint32_t upperLimit, WeightRange* upper)
{
    uint32_t weight = upperLimit;
    for (int32_t length = CollationWeightLength(upperLimit); length > w->middleLength; length--)
    {
        uint32_t trail = Byte(weight, length);
        if (trail > w->minBytes[length])
        {
            upper[length] = (WeightRange){SetTrail(weight, length, w->minBytes[length]),
                                          DecrementTrail(weight, length), length,
                                          (int32_t)(trail - w->minBytes[length])};
        }
        weight = Truncate(weight, length - 1);
    }
    return weight;
}

// Where no middle range is left, a lower and an upper range of one
// length that meet or overlap become one, and the shorter ones go.
static void Merge(const CollationWeights* w, WeightRange* lower, WeightRange* upper)
{
    for (int32_t length = 4; length > w->middleLength; length--)
    {
        if (lower[length].count <= 0 || upper[length].count <= 0)
        {
            continue;
        }
        uint32_t lowerEnd = lower[length].end;
        uint32_t upperStart = upper[length].start;
        bool merged = false;
        if (lowerEnd > upperStart)
        {
            lower[length].end = upper[length].end;
            lower[length].count = (int32_t)Byte(lower[length].end, length) -
                                  (int32_t)Byte(lower[length].start, length) + 1;
            merged = true;
        }
        else if (lowerEnd != upperStart && Increment(w, lowerEnd, length) == upperStart)
        {
            lower[length].end = upper[length].end;
            lower[length].count += upper[length].count;
            merged = true;
        }
        if (merged)
        {
            upper[length].count = 0;
            while (--length > w->middleLength)
            {
                lower[length].count = 0;
                upper[length].count = 0;
            }
            return;
        }
    }
}

static bool Ranges(CollationWeights* w, uint32_t lowerLimit, uint32_t upperLimit)
{
    // A level's middle length is 1 or 3; anything else was never set.
    if (w->middleLength < 1 || w->middleLength > 3)
    {
        return false;
    }
    int32_t lowerLength = CollationWeightLength(lowerLimit);
    int32_t upperLength = CollationWeightLength(upperLimit);
    if (lowerLimit >= upperLimit ||
        (lowerLength < upperLength && lowerLimit == Truncate(upperLimit, lowerLength)))
    {
        return false;
    }
    WeightRange lower[5];
    WeightRange upper[5];
    WeightRange middle = {0};
    memset(lower, 0, sizeof(lower));
    memset(upper, 0, sizeof(upper));
    uint32_t weight = lowerLimit;
    for (int32_t length = lowerLength; length > w->middleLength; length--)
    {
        uint32_t trail = Byte(weight, length);
        if (trail < w->maxBytes[length])
        {
            lower[length] = (WeightRange){IncrementTrail(weight, length),
                                          SetTrail(weight, length, w->maxBytes[length]), length,
                                          (int32_t)(w->maxBytes[length] - trail)};
        }
        weight = Truncate(weight, length - 1);
    }
    middle.start = weight < 0xFF000000 ? IncrementTrail(weight, w->middleLength) : 0xFFFFFFFF;
    weight = UpperRanges(w, upperLimit, upper);
    middle.end = DecrementTrail(weight, w->middleLength);
    middle.length = w->middleLength;
    if (middle.end >= middle.start)
    {
        middle.count = (int32_t)((middle.end - middle.start) / Unit(w->middleLength)) + 1;
    }
    else
    {
        Merge(w, lower, upper);
    }
    w->rangeCount = 0;
    if (middle.count > 0)
    {
        w->ranges[w->rangeCount++] = middle;
    }
    for (int32_t length = w->middleLength + 1; length <= 4; length++)
    {
        if (upper[length].count > 0)
        {
            w->ranges[w->rangeCount++] = upper[length];
        }
        if (lower[length].count > 0)
        {
            w->ranges[w->rangeCount++] = lower[length];
        }
    }
    return w->rangeCount > 0;
}

// The n weights in the ranges of the shortest length or one more, when
// they fit; the ranges used then sorted by start.
static bool InShortRanges(CollationWeights* w, int32_t n, int32_t minLength)
{
    for (int32_t i = 0; i < w->rangeCount && w->ranges[i].length <= minLength + 1; i++)
    {
        if (n <= w->ranges[i].count)
        {
            if (w->ranges[i].length > minLength)
            {
                w->ranges[i].count = n;
            }
            w->rangeCount = i + 1;
            qsort(w->ranges, (size_t)w->rangeCount, sizeof(WeightRange), ByStart);
            return true;
        }
        n -= w->ranges[i].count;
    }
    return false;
}

// The n weights in the shortest ranges, some of them lengthened by a
// byte, when that fits.
static bool InMinLengthRanges(CollationWeights* w, int32_t n, int32_t minLength)
{
    int32_t count = 0;
    int32_t ranges = 0;
    for (; ranges < w->rangeCount && w->ranges[ranges].length == minLength; ranges++)
    {
        count += w->ranges[ranges].count;
    }
    int32_t next = CountBytes(w, minLength + 1);
    if (n > count * next)
    {
        return false;
    }
    uint32_t start = w->ranges[0].start;
    uint32_t end = w->ranges[0].end;
    for (int32_t i = 1; i < ranges; i++)
    {
        start = w->ranges[i].start < start ? w->ranges[i].start : start;
        end = w->ranges[i].end > end ? w->ranges[i].end : end;
    }
    int32_t lengthened = (n - count) / (next - 1);
    int32_t kept = count - lengthened;
    if (lengthened == 0 || kept + lengthened * next < n)
    {
        lengthened++;
        kept--;
    }
    w->ranges[0].start = start;
    if (kept == 0)
    {
        w->ranges[0].end = end;
        w->ranges[0].count = count;
        Lengthen(w, &w->ranges[0]);
        w->rangeCount = 1;
        return true;
    }
    w->ranges[0].end = IncrementBy(w, start, minLength, kept - 1);
    w->ranges[0].count = kept;
    w->ranges[1] =
        (WeightRange){Increment(w, w->ranges[0].end, minLength), end, minLength, lengthened};
    Lengthen(w, &w->ranges[1]);
    w->rangeCount = 2;
    return true;
}

bool CollationWeightsAllocate(CollationWeights* w, uint32_t lowerLimit, uint32_t upperLimit,
                              int32_t n)
{
    if (!Ranges(w, lowerLimit, upperLimit))
    {
        return false;
    }
    for (;;)
    {
        int32_t minLength = w->ranges[0].length;
        if (InShortRanges(w, n, minLength))
        {
            break;
        }
        if (minLength == 4)
        {
            return false;
        }
        if (InMinLengthRanges(w, n, minLength))
        {
            break;
        }
        for (int32_t i = 0; i < w->rangeCount && w->ranges[i].length == minLength; i++)
        {
            Lengthen(w, &w->ranges[i]);
        }
    }
    w->rangeIndex = 0;
    return true;
}

uint32_t CollationWeightsNext(CollationWeights* w)
{
    if (w->rangeIndex >= w->rangeCount)
    {
        return 0xFFFFFFFF;
    }
    WeightRange* range = &w->ranges[w->rangeIndex];
    uint32_t weight = range->start;
    if (--range->count == 0)
    {
        w->rangeIndex++;
    }
    else
    {
        range->start = Increment(w, weight, range->length);
    }
    return weight;
}
