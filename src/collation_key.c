// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_key.h"

#include "collation_iterator.h"
#include "nfd_stream.h"

// ICU's NO_CE, the element after the last: primary 1, secondary and
// tertiary 0x0100, which each level writes as its final 01.
constexpr uint64_t noElement = 0x0000000101000100ULL;

enum
{
    levelSeparator = 0x01,
    mergeSeparatorByte = 0x02,
    compressionLow = 0x03,
    compressionHigh = 0xFF,
    common16 = 0x0500,
    noWeight16 = 0x0100,
    // CollationKeys' runs of common weights.
    secLow = 0x05,
    secMiddle = 0x25,
    secHigh = 0x45,
    secMax = 0x21,
    caseLowerLow = 1,
    caseLowerMiddle = 7,
    caseLowerHigh = 13,
    caseLowerMax = 7,
    caseUpperLow = 3,
    caseUpperHigh = 15,
    caseUpperMax = 13,
    terOnlyLow = 0x05,
    terOnlyMiddle = 0x65,
    terOnlyHigh = 0xC5,
    terOnlyMax = 0x61,
    terLowerLow = 0x05,
    terLowerMiddle = 0x25,
    terLowerHigh = 0x45,
    terLowerMax = 0x21,
    terUpperLow = 0x85,
    terUpperMiddle = 0xA5,
    terUpperHigh = 0xC5,
    terUpperMax = 0x21,
    quatLow = 0x1C,
    quatMiddle = 0x8C,
    quatHigh = 0xFC,
    quatMax = 0x71,
    quatShiftedLimit = 0x1B,
};

// The caller's bytes; length counts past the capacity.
typedef struct Sink
{
    uint8_t* out;
    size_t capacity;
    size_t length;
} Sink;

static void Put(Sink* s, uint32_t byte)
{
    if (s->length < s->capacity)
    {
        s->out[s->length] = (uint8_t)byte;
    }
    s->length++;
}

// A 16-bit weight, its low byte when not 0.
static void Put16(Sink* s, uint32_t w)
{
    Put(s, w >> 8);
    if ((w & 0xFF) != 0)
    {
        Put(s, w & 0xFF);
    }
}

// A 32-bit weight up to its first zero byte.
static void Put32(Sink* s, uint32_t w)
{
    Put(s, w >> 24);
    for (int shift = 16; shift >= 0 && (w >> shift & 0xFF) != 0; shift -= 8)
    {
        Put(s, w >> shift & 0xFF);
    }
}

// The elements of one level's pass, as ICU's loop takes them: shifted
// variable elements given apart, the primary ignorables after them left
// out, and noElement once at the end.
typedef struct Stream
{
    CollationIterator it;
    const CollationSettings* settings;
    bool ended;
    bool afterVariable;
} Stream;

static void Open(Stream* s, const CollationRoot* root, const CollationSettings* settings,
                 const uint8_t* text, size_t length)
{
    mlocCollationIteratorStart(&s->it, root, text, length);
    s->it.numeric = settings->numeric;
    s->settings = settings;
    s->ended = false;
    s->afterVariable = false;
}

// The next element and whether it is variable; false after noElement.
static bool Next(Stream* s, uint64_t* elementOut, bool* variable)
{
    for (;;)
    {
        if (s->ended)
        {
            return false;
        }
        uint64_t e = 0;
        if (!mlocCollationNextElement(&s->it, &e))
        {
            s->ended = true;
            e = noElement;
        }
        uint32_t p = (uint32_t)(e >> 32);
        *variable =
            s->settings->shifted && p > collationMergeSeparator && p <= s->settings->variableTop;
        if (!*variable && p == 0 && s->afterVariable)
        {
            continue;
        }
        s->afterVariable = *variable;
        *elementOut = e;
        return true;
    }
}

static uint32_t Reordered(const CollationSettings* s, uint32_t p)
{
    return s->reorder == nullptr ? p : mlocCollationReorderPrimary(s->reorder, p);
}

static void Primaries(Stream* st, const CollationRoot* root, Sink* sink)
{
    uint32_t previous = 0;
    uint64_t e = 0;
    bool variable = false;
    while (Next(st, &e, &variable))
    {
        uint32_t p = (uint32_t)(e >> 32);
        if (variable || p <= 1)
        {
            continue;
        }
        bool compressible = mlocCollationCompressible(root, p >> 24);
        p = Reordered(st->settings, p);
        uint32_t p1 = p >> 24;
        if (!compressible || p1 != previous >> 24)
        {
            if (previous != 0 && p < previous)
            {
                // A run that goes down ends low, unless at the separator.
                if (p1 > mergeSeparatorByte)
                {
                    Put(sink, compressionLow);
                }
            }
            else if (previous != 0)
            {
                Put(sink, compressionHigh);
            }
            Put(sink, p1);
            previous = compressible ? p : 0;
        }
        if ((p >> 16 & 0xFF) != 0)
        {
            Put32(sink, p << 8);
        }
    }
}

// A run of common weights before a weight below (low) or above them.
static void Commons(Sink* sink, uint32_t* count, uint32_t max, uint32_t middle, uint32_t low,
                    uint32_t high, bool below)
{
    if (*count == 0)
    {
        return;
    }
    uint32_t n = *count - 1;
    for (; n >= max; n -= max)
    {
        Put(sink, middle);
    }
    Put(sink, below ? low + n : high - n);
    *count = 0;
}

// Reverses the bytes [from, to) where they were written.
static void Reverse(Sink* sink, size_t from, size_t to)
{
    if (to > sink->capacity)
    {
        return;
    }
    for (size_t i = from, j = to; i + 1 < j; i++, j--)
    {
        uint8_t b = sink->out[i];
        sink->out[i] = sink->out[j - 1];
        sink->out[j - 1] = b;
    }
}

// A backwards run of common secondaries before a weight, as ICU writes
// it into a segment that is reversed later.
static void BackwardsCommons(Sink* sink, uint32_t* count, uint32_t previous)
{
    if (*count == 0)
    {
        return;
    }
    uint32_t n = *count - 1;
    uint32_t remainder = n % secMax;
    Put(sink, previous < common16 ? secLow + remainder : secHigh - remainder);
    for (n -= remainder; n > 0; n -= secMax)
    {
        Put(sink, secMiddle);
    }
    *count = 0;
}

static void Secondaries(Stream* st, Sink* sink)
{
    bool backwards = st->settings->backwards;
    uint32_t commons = 0;
    uint32_t previous = 0;
    size_t segment = sink->length;
    uint64_t e = 0;
    bool variable = false;
    while (Next(st, &e, &variable))
    {
        uint32_t p = (uint32_t)(e >> 32);
        uint32_t s = (uint32_t)e >> 16;
        if (variable || (uint32_t)e == 0 || s == 0)
        {
            continue;
        }
        if (s == common16 && (!backwards || p != collationMergeSeparator))
        {
            commons++;
        }
        else if (!backwards)
        {
            Commons(sink, &commons, secMax, secMiddle, secLow, secHigh, s < common16);
            Put16(sink, s);
        }
        else
        {
            BackwardsCommons(sink, &commons, previous);
            if (p != 0 && p <= collationMergeSeparator)
            {
                Reverse(sink, segment, sink->length);
                Put(sink, p == 1 ? levelSeparator : mergeSeparatorByte);
                previous = 0;
                segment = sink->length;
            }
            else
            {
                // ICU's appendReverseWeight16: low byte first, when not 0.
                if ((s & 0xFF) != 0)
                {
                    Put(sink, s & 0xFF);
                }
                Put(sink, s >> 8);
                previous = s;
            }
        }
    }
}

// The case level's nibbles, packed two a byte; the last nibble given
// (noElement's) is held back and dropped.
typedef struct Nibbles
{
    Sink* sink;
    uint32_t held;
    uint32_t pending;
    uint32_t count;
} Nibbles;

static void Nibble(Nibbles* n, uint32_t c)
{
    if (n->count > 0)
    {
        if (n->pending == 0)
        {
            n->pending = n->held;
        }
        else
        {
            Put(n->sink, n->pending | n->held >> 4);
            n->pending = 0;
        }
    }
    n->held = c;
    n->count++;
}

// A run of common cases before a case byte c, as ICU writes it for lower
// first (the run kept when it starts the level and c ends it) and upper
// first; then c in ICU's nibble.
static void CaseByte(Nibbles* n, uint32_t* commons, uint32_t c, bool upperFirst)
{
    if (!upperFirst && *commons != 0 && (c > levelSeparator || n->count > 0))
    {
        uint32_t k = *commons - 1;
        for (; k >= caseLowerMax; k -= caseLowerMax)
        {
            Nibble(n, caseLowerMiddle << 4);
        }
        Nibble(n, (c <= levelSeparator ? caseLowerLow + k : caseLowerHigh - k) << 4);
        *commons = 0;
    }
    else if (upperFirst && *commons != 0)
    {
        uint32_t k = *commons - 1;
        for (; k >= caseUpperMax; k -= caseUpperMax)
        {
            Nibble(n, caseUpperLow << 4);
        }
        Nibble(n, (caseUpperLow + k) << 4);
        *commons = 0;
    }
    uint32_t mixed = upperFirst ? (caseUpperLow - (c >> 6)) << 4 : (caseLowerHigh + (c >> 6)) << 4;
    Nibble(n, c > levelSeparator ? mixed : c);
}

static void Cases(Stream* st, Sink* sink)
{
    const CollationSettings* set = st->settings;
    bool upperFirst = set->caseFirst == collationUpperFirst;
    Nibbles n = {.sink = sink};
    uint32_t commons = 0;
    uint64_t e = 0;
    bool variable = false;
    while (Next(st, &e, &variable))
    {
        uint32_t lower = (uint32_t)e;
        bool skipped = set->strength == collationPrimary ? e >> 32 == 0 : lower <= 0xFFFF;
        if (variable || lower == 0 || skipped)
        {
            continue;
        }
        uint32_t c = lower >> 8 & 0xFF;
        if ((c & 0xC0) == 0 && c > levelSeparator)
        {
            commons++;
            continue;
        }
        CaseByte(&n, &commons, c, upperFirst);
    }
    if (n.pending != 0)
    {
        Put(sink, n.pending);
    }
}

// A tertiary as ICU writes it under upper first.
static uint32_t UpperFirst(uint32_t t, uint32_t lower)
{
    if (t <= noWeight16)
    {
        return t;
    }
    if (lower > 0xFFFF)
    {
        t ^= 0xC000;
        return t < terUpperHigh << 8 ? t - 0x4000 : t;
    }
    return t + 0x4000;
}

static void Tertiaries(Stream* st, Sink* sink)
{
    const CollationSettings* set = st->settings;
    bool withCase = set->caseFirst != collationCaseFirstOff && !set->caseLevel;
    bool upperFirst = set->caseFirst == collationUpperFirst;
    uint32_t mask = withCase ? 0xFF3F : 0x3F3F;
    uint32_t commons = 0;
    uint64_t e = 0;
    bool variable = false;
    while (Next(st, &e, &variable))
    {
        uint32_t lower = (uint32_t)e;
        uint32_t t = lower & mask;
        if (variable || lower == 0)
        {
            continue;
        }
        if (t == common16)
        {
            commons++;
        }
        else if (!withCase)
        {
            Commons(sink, &commons, terOnlyMax, terOnlyMiddle, terOnlyLow, terOnlyHigh,
                    t < common16);
            Put16(sink, t > common16 ? t + 0xC000 : t);
        }
        else if (!upperFirst)
        {
            Commons(sink, &commons, terLowerMax, terLowerMiddle, terLowerLow, terLowerHigh,
                    t < common16);
            Put16(sink, t > common16 ? t + 0x4000 : t);
        }
        else
        {
            t = UpperFirst(t, lower);
            Commons(sink, &commons, terUpperMax, terUpperMiddle, terUpperLow, terUpperHigh,
                    t < terUpperLow << 8);
            Put16(sink, t);
        }
    }
}

static void Quaternaries(Stream* st, Sink* sink)
{
    const CollationSettings* set = st->settings;
    uint32_t commons = 0;
    size_t start = sink->length;
    uint64_t e = 0;
    bool variable = false;
    while (Next(st, &e, &variable))
    {
        uint32_t lower = (uint32_t)e;
        if (variable)
        {
            Commons(sink, &commons, quatMax, quatMiddle, quatLow, quatHigh, true);
            uint32_t p = Reordered(set, (uint32_t)(e >> 32));
            if (p >> 24 >= quatShiftedLimit)
            {
                Put(sink, quatShiftedLimit);
            }
            Put32(sink, p);
            continue;
        }
        uint32_t q = lower & 0xFFFF;
        if (lower == 0)
        {
            continue;
        }
        if ((q & 0xC0) == 0 && q > noWeight16)
        {
            commons++;
        }
        else if (q == noWeight16 && !set->shifted && sink->length == start)
        {
            Put(sink, levelSeparator);
        }
        else
        {
            q = q == noWeight16 ? levelSeparator : 0xFC + (q >> 6 & 3);
            Commons(sink, &commons, quatMax, quatMiddle, quatLow, quatHigh, q < quatLow);
            Put(sink, q);
        }
    }
}

enum
{
    slopeMin = 3,
    slopeMax = 0xFF,
    slopeMiddle = 0x81,
    slopeTails = slopeMax - slopeMin + 1,
    slopeSingle = 80,
    slopeLead2 = 42,
    slopeLead3 = 3,
    reachPos2 = slopeLead2 * slopeTails + (slopeLead2 - 1),
    reachNeg2 = -reachPos2 - 1,
    reachPos3 =
        slopeLead3 * slopeTails * slopeTails + (slopeLead3 - 1) * slopeTails + (slopeTails - 1),
    reachNeg3 = -reachPos3 - 1,
    startPos2 = slopeMiddle + slopeSingle + 1,
    startPos3 = startPos2 + slopeLead2,
    startNeg2 = slopeMiddle - slopeSingle,
    startNeg3 = startNeg2 - slopeLead2,
};

// The next trail byte from the end, dividing diff down (NEGDIVMOD's
// floor division for a negative diff).
static uint8_t Trail(int32_t* diff)
{
    int32_t m = *diff % slopeTails;
    *diff /= slopeTails;
    if (m < 0)
    {
        (*diff)--;
        m += slopeTails;
    }
    return (uint8_t)(slopeMin + m);
}

// The bytes BOCSU writes a difference in, one to four.
static int DifferenceBytes(int32_t diff)
{
    if (diff >= -slopeSingle && diff <= slopeSingle)
    {
        return 1;
    }
    if (diff > 0)
    {
        return diff <= reachPos2 ? 2 : diff <= reachPos3 ? 3 : 4;
    }
    return diff >= reachNeg2 ? 2 : diff >= reachNeg3 ? 3 : 4;
}

// ICU's u_writeDiff: a code point's difference from the one before, in
// one to four bytes of BOCSU: the trail bytes from the end, then a lead
// byte from what is left.
static void Difference(Sink* sink, int32_t diff)
{
    int n = DifferenceBytes(diff);
    if (n == 1)
    {
        Put(sink, (uint32_t)(slopeMiddle + diff));
        return;
    }
    bool up = diff > 0;
    uint8_t b[4];
    for (int i = n - 1; i > 0; i--)
    {
        b[i] = Trail(&diff);
    }
    static const int32_t upLeads[] = {0, 0, startPos2, startPos3};
    static const int32_t downLeads[] = {0, 0, startNeg2, startNeg3};
    int32_t lead = n == 4 ? (up ? slopeMax : slopeMin) : (up ? upLeads[n] : downLeads[n]) + diff;
    Put(sink, (uint32_t)lead);
    for (int i = 1; i < n; i++)
    {
        Put(sink, b[i]);
    }
}

static void Identical(Sink* sink, const uint8_t* text, size_t length)
{
    NfdStream nfd;
    mlocNfdStart(&nfd, text, length);
    int32_t previous = 0;
    uint32_t c = 0;
    while (mlocNfdNext(&nfd, &c))
    {
        if (previous < 0x4E00 || previous >= 0xA000)
        {
            previous = (previous & ~0x7F) + slopeSingle;
        }
        else
        {
            previous = 0x9FFF - reachPos2;
        }
        if (c == 0xFFFE)
        {
            Put(sink, mergeSeparatorByte);
            previous = 0;
        }
        else
        {
            Difference(sink, (int32_t)c - previous);
            previous = (int32_t)c;
        }
    }
}

size_t mlocCollationSortKey(const CollationRoot* root, const CollationSettings* settings,
                            const uint8_t* text, size_t length, uint8_t* out, size_t capacity)
{
    Sink sink = {out, capacity, 0};
    Stream st;
    Open(&st, root, settings, text, length);
    Primaries(&st, root, &sink);
    uint32_t strength = settings->strength;
    void (*const passes[])(Stream*, Sink*) = {Secondaries, Cases, Tertiaries, Quaternaries};
    bool wanted[] = {strength >= collationSecondary, settings->caseLevel,
                     strength >= collationTertiary, strength >= collationQuaternary};
    for (size_t i = 0; i < 4; i++)
    {
        if (!wanted[i])
        {
            continue;
        }
        Put(&sink, levelSeparator);
        Open(&st, root, settings, text, length);
        passes[i](&st, &sink);
        // Each level but the case level ends with noElement's 01, which
        // ICU leaves out.
        sink.length -= i == 1 ? 0 : 1;
    }
    if (strength == collationIdentical)
    {
        Put(&sink, levelSeparator);
        Identical(&sink, text, length);
    }
    Put(&sink, 0);
    return sink.length;
}
