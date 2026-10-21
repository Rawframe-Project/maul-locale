// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "nfd_stream.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"

// Below U+00C0 nothing decomposes and every combining class is 0.
enum
{
    nfdPlain = 0xC0,
};

// A UTF-16 code point and its units; U+FFFD for a lone surrogate.
static uint32_t DecodeUtf16(const uint16_t* units, size_t length, size_t at, size_t* sizeOut)
{
    uint32_t u = units[at];
    *sizeOut = 1;
    if (u - 0xD800 >= 0x800)
    {
        return u;
    }
    uint32_t t = at + 1 < length ? units[at + 1] : 0;
    if (u < 0xDC00 && t - 0xDC00 < 0x400)
    {
        *sizeOut = 2;
        return 0x10000 + ((u - 0xD800) << 10) + (t - 0xDC00);
    }
    return 0xFFFD;
}

uint32_t mlocNfdDecodeAt(NfdText text, size_t at, size_t* sizeOut)
{
    if (text.units != nullptr)
    {
        return DecodeUtf16(text.units, text.length, at, sizeOut);
    }
    if (text.bytes[at] < 0x80)
    {
        *sizeOut = 1;
        return text.bytes[at];
    }
    uint32_t codePoint = 0xFFFD;
    *sizeOut = 1;
    // An ill-formed sequence gives U+FFFD and its maximal subpart's size.
    (void)muniDecodeUtf8((const char*)text.bytes + at, text.length - at, &codePoint, sizeOut);
    return codePoint;
}

static uint32_t Decode(const NfdStream* s, size_t at, size_t* sizeOut)
{
    return mlocNfdDecodeAt((NfdText){s->text, s->units, s->length}, at, sizeOut);
}

// The full canonical decomposition, one level at a time.
static uint32_t Decompose(uint32_t codePoint, uint32_t* out)
{
    if (codePoint < nfdPlain)
    {
        out[0] = codePoint;
        return 1;
    }
    uint32_t stack[nfdMaxDecomposition];
    uint32_t depth = 0;
    uint32_t count = 0;
    stack[depth++] = codePoint;
    while (depth > 0 && count < nfdMaxDecomposition)
    {
        uint32_t c = stack[--depth];
        uint32_t first = 0;
        uint32_t second = 0;
        if (depth + 2 <= nfdMaxDecomposition && muniDecomposePair(c, &first, &second))
        {
            if (second != 0)
            {
                stack[depth++] = second;
            }
            stack[depth++] = first;
        }
        else
        {
            out[count++] = c;
        }
    }
    return count;
}

static uint8_t Class(uint32_t codePoint)
{
    return codePoint < 0x300 ? 0 : muniGetCombiningClass(codePoint);
}

void mlocNfdStartText(NfdStream* s, NfdText text)
{
    *s = (NfdStream){.text = text.bytes, .units = text.units, .length = text.length};
}

// Whether the code point at the byte is a mark whose decomposition holds
// no starter, its decomposition and size given.
static bool Mark(const NfdStream* s, size_t at, uint32_t* d, uint32_t* n, size_t* size)
{
    *n = Decompose(Decode(s, at, size), d);
    for (uint32_t i = 0; i < *n; i++)
    {
        if (Class(d[i]) == 0)
        {
            return false;
        }
    }
    return true;
}

static void Put(NfdStream* s, uint32_t codePoint, uint8_t ccc, size_t from)
{
    s->window[s->count] = codePoint;
    s->classes[s->count] = ccc;
    s->from[s->count++] = (uint32_t)from;
}

// The window as one changed segment: its first code point from start,
// the rest from end.
static void Changed(NfdStream* s, size_t start, size_t end)
{
    for (uint32_t i = 0; i < s->count; i++)
    {
        s->from[i] = (uint32_t)(i == 0 ? start : end);
    }
}

// The smallest class above the current one among a long run's marks, or
// 0 when none is left.
static uint8_t NextClass(const NfdStream* s)
{
    uint32_t best = 256;
    for (uint32_t i = 0; i < s->headCount; i++)
    {
        uint32_t c = s->headClasses[i];
        best = c > s->runClass && c < best ? c : best;
    }
    uint32_t d[nfdMaxDecomposition];
    uint32_t n = 0;
    size_t size = 0;
    for (size_t at = s->runStart; at < s->runEnd; at += size)
    {
        (void)Mark(s, at, d, &n, &size);
        for (uint32_t i = 0; i < n; i++)
        {
            uint32_t c = Class(d[i]);
            best = c > s->runClass && c < best ? c : best;
        }
    }
    return best == 256 ? 0 : (uint8_t)best;
}

// Gives the current class's marks of a long run's bytes from where the
// last window stopped, until the window is full or the bytes are done.
static void FillFromRun(NfdStream* s)
{
    uint32_t d[nfdMaxDecomposition];
    uint32_t n = 0;
    size_t size = 0;
    while (s->runAt < s->runEnd && s->count < nfdWindow)
    {
        (void)Mark(s, s->runAt, d, &n, &size);
        for (; s->runSub < n && s->count < nfdWindow; s->runSub++)
        {
            if (Class(d[s->runSub]) == s->runClass)
            {
                Put(s, d[s->runSub], s->runClass, s->runEnd);
            }
        }
        if (s->runSub == n)
        {
            s->runAt += size;
            s->runSub = 0;
        }
    }
}

// Gives a long run's marks of the current class, then of the next, until
// the window is full or the run is done: the first decomposition's
// marks, then those of the bytes after it.
static void FillLong(NfdStream* s)
{
    while (s->count < nfdWindow)
    {
        if (s->headAt == s->headCount && s->runAt == s->runEnd)
        {
            s->runClass = NextClass(s);
            if (s->runClass == 0)
            {
                s->longRun = false;
                return;
            }
            s->headAt = 0;
            s->runAt = s->runStart;
            s->runSub = 0;
        }
        for (; s->headAt < s->headCount && s->count < nfdWindow; s->headAt++)
        {
            if (s->headClasses[s->headAt] == s->runClass)
            {
                Put(s, s->head[s->headAt], s->runClass, s->runEnd);
            }
        }
        FillFromRun(s);
    }
}

// Puts the marks from the first in canonical order: a stable sort by
// class.
static void Order(NfdStream* s, uint32_t first)
{
    for (uint32_t i = first + 1; i < s->count; i++)
    {
        uint32_t c = s->window[i];
        uint8_t k = s->classes[i];
        uint32_t j = i;
        for (; j > first && s->classes[j - 1] > k; j--)
        {
            s->window[j] = s->window[j - 1];
            s->classes[j] = s->classes[j - 1];
        }
        s->window[j] = c;
        s->classes[j] = k;
    }
}

// A segment's source code points, at most one for each window code
// point: where each starts and its first window code point (after the
// last, the segment's end and the window's), and the combining classes
// its NFD starts and ends with.
typedef struct Sources
{
    uint32_t count;
    size_t start[nfdWindow + 1];
    uint32_t first[nfdWindow + 1];
    uint8_t lead[nfdWindow];
    uint8_t trail[nfdWindow];
} Sources;

static void AddSource(Sources* q, const NfdStream* s, size_t start, const uint32_t* d, uint32_t n)
{
    q->start[q->count] = start;
    q->first[q->count] = s->count;
    q->lead[q->count] = Class(d[0]);
    q->trail[q->count++] = Class(d[n - 1]);
}

// The first source code point of the part ICU4C's FCD iterator
// normalizes (with normalization on), or the count when it normalizes
// none: a part starts after a code point whose NFD ends with a starter;
// it is normalized from its start when a code point's NFD starts with a
// class lower than the last's ends with, or is a Tibetan composite vowel
// (CollationFCD's lead 129, trail 130 or 132).
static uint32_t Normalized(const Sources* q)
{
    uint32_t part = 0;
    uint8_t last = 0;
    for (uint32_t j = 0; j < q->count; j++)
    {
        uint8_t lead = q->lead[j];
        bool tibetan = lead == 129 && (q->trail[j] == 130 || q->trail[j] == 132);
        if (lead != 0 && (last > lead || tibetan))
        {
            return part;
        }
        last = q->trail[j];
        part = last == 0 ? j + 1 : part;
    }
    return q->count;
}

// Where each window code point comes from: in the part normalized, the
// first from its start and the rest from the segment's end; before it,
// each source code point's first from its start and the rest of its NFD
// from its end.
static void Place(NfdStream* s, Sources* q, uint32_t normalized)
{
    q->first[q->count] = s->count;
    for (uint32_t j = 0; j < normalized; j++)
    {
        for (uint32_t i = q->first[j]; i < q->first[j + 1]; i++)
        {
            s->from[i] = (uint32_t)(i == q->first[j] ? q->start[j] : q->start[j + 1]);
        }
    }
    if (normalized < q->count)
    {
        for (uint32_t i = q->first[normalized]; i < s->count; i++)
        {
            s->from[i] =
                (uint32_t)(i == q->first[normalized] ? q->start[normalized] : q->start[q->count]);
        }
    }
}

bool mlocNfdFill(NfdStream* s)
{
    s->count = 0;
    s->next = 0;
    if (s->longRun)
    {
        FillLong(s);
        if (s->count > 0)
        {
            return true;
        }
    }
    if (s->at >= s->length)
    {
        return false;
    }
    uint32_t d[nfdMaxDecomposition];
    size_t size = 0;
    size_t start = s->at;
    Sources q = {.count = 0};
    uint32_t n = Decompose(Decode(s, s->at, &size), d);
    AddSource(&q, s, start, d, n);
    uint32_t marks = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        uint8_t ccc = Class(d[i]);
        Put(s, d[i], ccc, s->at);
        marks = ccc == 0 ? s->count : marks;
    }
    s->at += size;
    // The marks that follow, counted before they are read.
    uint32_t total = s->count;
    size_t end = s->at;
    for (uint32_t k = 0; end < s->length && Mark(s, end, d, &k, &size); end += size)
    {
        total += k;
    }
    if (total <= nfdWindow)
    {
        for (uint32_t k = 0; s->at < end; s->at += size)
        {
            (void)Mark(s, s->at, d, &k, &size);
            AddSource(&q, s, s->at, d, k);
            for (uint32_t i = 0; i < k; i++)
            {
                Put(s, d[i], Class(d[i]), s->at);
            }
        }
        q.start[q.count] = end;
        Order(s, marks);
        Place(s, &q, Normalized(&q));
        return true;
    }
    s->headCount = (uint8_t)(s->count - marks);
    for (uint32_t i = 0; i < s->headCount; i++)
    {
        s->head[i] = s->window[marks + i];
        s->headClasses[i] = s->classes[marks + i];
    }
    s->count = marks;
    s->longRun = true;
    s->runClass = 0;
    s->headAt = s->headCount;
    s->runStart = s->at;
    s->runEnd = end;
    s->runAt = end;
    s->runSub = 0;
    s->at = end;
    FillLong(s);
    Changed(s, start, end);
    return true;
}
