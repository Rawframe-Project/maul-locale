// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "search_boundary.h"

#include "invariant.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/properties.h"
#include "maul-unicode/segment.h"

enum
{
    // UTF-8 bytes of UTF-16 text handed to the iterator at a time.
    pieceBytes = 128,
};

// Whether no grapheme rule reads back past a code point.
static bool Safe(uint32_t c)
{
    muniGraphemeBreak g = muniGetGraphemeBreak(c);
    return g <= muni_gcbControl && !muniIsExtendedPictographic(c) &&
           muniGetIndicConjunctBreak(c) == muni_incbNone;
}

// A safe code point boundary before one, or 0.
static size_t SafeStart(NfdText t, size_t at)
{
    size_t q = at;
    while (q > 0)
    {
        q = mlocNfdBefore(t, q);
        size_t size = 0;
        if (Safe(mlocNfdDecodeAt(t, q, &size)))
        {
            return q;
        }
    }
    return 0;
}

// UTF-16 text handed to the iterator as UTF-8 a piece at a time, with
// the unit each code point of the last two pieces starts at.
typedef struct Pieces
{
    NfdText text;
    size_t next;
    char bytes[pieceBytes];
    size_t base;
    size_t count;
    size_t unitAt[pieceBytes + 1];
    size_t lastBase;
    size_t lastCount;
    size_t lastUnitAt[pieceBytes + 1];
} Pieces;

// The next piece's bytes, from the unit after the last piece.
static void Fill(Pieces* p)
{
    p->lastBase = p->base;
    p->lastCount = p->count;
    for (size_t i = 0; i <= p->count; i++)
    {
        p->lastUnitAt[i] = p->unitAt[i];
    }
    p->base += p->count;
    p->count = 0;
    while (p->next < p->text.length && p->count + 4 <= pieceBytes)
    {
        size_t units = 0;
        uint32_t c = mlocNfdDecodeAt(p->text, p->next, &units);
        size_t size = 0;
        (void)muniEncodeUtf8(c, p->bytes + p->count, &size);
        for (size_t i = 0; i < size; i++)
        {
            p->unitAt[p->count + i] = p->next;
        }
        p->count += size;
        p->next += units;
    }
    p->unitAt[p->count] = p->next;
}

// The unit a boundary of the UTF-8 form falls on.
static size_t UnitOf(const Pieces* p, size_t byte)
{
    if (byte >= p->base)
    {
        MLOC_ASSERT(byte - p->base <= p->count);
        return p->unitAt[byte - p->base];
    }
    MLOC_ASSERT(byte >= p->lastBase && byte - p->lastBase <= p->lastCount);
    return p->lastUnitAt[byte - p->lastBase];
}

// The first boundary from a safe start at or after at (after: past it),
// over UTF-16 text.
static size_t FirstUtf16(NfdText t, size_t start, size_t at, bool after)
{
    Pieces p = {.text = t, .next = start};
    Fill(&p);
    muniSegmentIterator g;
    (void)muniInitGraphemeIterator(&g, p.bytes, p.count, p.next < t.length);
    for (;;)
    {
        size_t offset = 0;
        muniResult r = muniNextSegmentBreak(&g, &offset);
        if (r == muni_needMoreText)
        {
            Fill(&p);
            (void)muniFeedSegmentIterator(&g, p.bytes, p.count, p.next < t.length);
            continue;
        }
        if (r != muni_success)
        {
            return t.length;
        }
        size_t unit = UnitOf(&p, offset);
        if (unit > at || (unit == at && !after))
        {
            return unit;
        }
    }
}

// The first boundary at or after at (after: past it), from a safe start
// before it.
static size_t First(NfdText t, size_t at, bool after)
{
    size_t start = SafeStart(t, at);
    if (t.units != nullptr)
    {
        return FirstUtf16(t, start, at, after);
    }
    muniSegmentIterator g;
    (void)muniInitGraphemeIterator(&g, (const char*)t.bytes + start, t.length - start, false);
    size_t offset = 0;
    while (muniNextSegmentBreak(&g, &offset) == muni_success)
    {
        if (start + offset > at || (start + offset == at && !after))
        {
            return start + offset;
        }
    }
    return t.length;
}

bool mlocSearchIsBoundary(NfdText text, size_t at)
{
    return at == 0 || at >= text.length || First(text, at, false) == at;
}

size_t mlocSearchFollowing(NfdText text, size_t at)
{
    return at >= text.length ? text.length : First(text, at, true);
}
