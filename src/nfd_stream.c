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
static uint32_t DecodeUtf16(const NfdStream* s, size_t at, size_t* sizeOut)
{
    uint32_t u = s->units[at];
    *sizeOut = 1;
    if (u - 0xD800 >= 0x800)
    {
        return u;
    }
    uint32_t t = at + 1 < s->length ? s->units[at + 1] : 0;
    if (u < 0xDC00 && t - 0xDC00 < 0x400)
    {
        *sizeOut = 2;
        return 0x10000 + ((u - 0xD800) << 10) + (t - 0xDC00);
    }
    return 0xFFFD;
}

static uint32_t Decode(const NfdStream* s, size_t at, size_t* sizeOut)
{
    if (s->units != nullptr)
    {
        return DecodeUtf16(s, at, sizeOut);
    }
    if (s->text[at] < 0x80)
    {
        *sizeOut = 1;
        return s->text[at];
    }
    uint32_t codePoint = 0xFFFD;
    *sizeOut = 1;
    // An ill-formed sequence gives U+FFFD and its maximal subpart's size.
    (void)muniDecodeUtf8((const char*)s->text + at, s->length - at, &codePoint, sizeOut);
    return codePoint;
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

static void Put(NfdStream* s, uint32_t codePoint, uint8_t ccc)
{
    s->window[s->count] = codePoint;
    s->classes[s->count++] = ccc;
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
                Put(s, d[s->runSub], s->runClass);
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
                Put(s, s->head[s->headAt], s->runClass);
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
    uint32_t n = Decompose(Decode(s, s->at, &size), d);
    s->at += size;
    uint32_t marks = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        uint8_t ccc = Class(d[i]);
        Put(s, d[i], ccc);
        marks = ccc == 0 ? s->count : marks;
    }
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
            for (uint32_t i = 0; i < k; i++)
            {
                Put(s, d[i], Class(d[i]));
            }
        }
        Order(s, marks);
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
    return true;
}
