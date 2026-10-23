// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// UTF-8 or UTF-16 text read as its NFD, a code point at a time, through
// Maul Unicode's decomposition and combining classes; ill-formed bytes
// read as U+FFFD, one for each maximal ill-formed subpart, and so does a
// lone surrogate. Nothing is allocated and nothing limits the marks after
// a starter.
//
// The stream fills a window: a segment (code points up to the next one
// whose decomposition starts with a starter), decomposed and its marks
// in canonical order. A segment longer than the window is given in
// windows of nfdWindow code points, its marks put in order by passes
// over their bytes, one for each combining class present, smallest
// first. Where the windows end depends only on the NFD, so canonically
// equal texts give the same windows.

#ifndef MLOC_SRC_NFD_STREAM_H
#define MLOC_SRC_NFD_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    nfdWindow = 64,
    // The longest canonical decomposition is 4 code points.
    nfdMaxDecomposition = 8,
};

// A text in UTF-8 (bytes) or UTF-16 (units, bytes null); its length in
// bytes or units.
typedef struct NfdText
{
    const uint8_t* bytes;
    const uint16_t* units;
    size_t length;
} NfdText;

static inline NfdText mlocNfdUtf8(const uint8_t* bytes, size_t length)
{
    return (NfdText){bytes, nullptr, length};
}

static inline NfdText mlocNfdUtf16(const uint16_t* units, size_t length)
{
    return (NfdText){nullptr, units, length};
}

typedef struct NfdStream
{
    const uint8_t* text;
    const uint16_t* units;
    size_t length;
    // The first byte or unit not yet read into a window.
    size_t at;
    // The window: code points [next, count) are still to be given.
    uint32_t window[nfdWindow];
    uint8_t classes[nfdWindow];
    // Where in the source each window code point comes from, as ICU4C's
    // FCD iterator places offsets with normalization on: where the source
    // is in canonical order, each source code point's NFD gives its first
    // code point the source's start and the rest its end; the part ICU
    // normalizes (from the start of the segment's part with the first
    // mark out of order, or a Tibetan composite vowel, to the segment's
    // end) gives its first code point the part's start and the rest the
    // segment's end. A segment longer than the window reads as normalized
    // whole.
    uint32_t from[nfdWindow];
    uint32_t count;
    uint32_t next;
    // A long segment's marks, given by class: the marks of its first
    // decomposition (head), then those of the bytes [runStart, runEnd).
    bool longRun;
    uint8_t runClass;
    uint8_t headCount;
    uint8_t headAt;
    uint32_t head[nfdMaxDecomposition];
    uint8_t headClasses[nfdMaxDecomposition];
    size_t runStart;
    size_t runEnd;
    size_t runAt;
    // The next of the decomposition at runAt.
    uint32_t runSub;
} NfdStream;

void mlocNfdStartText(NfdStream* s, NfdText text);

// The code point at a byte or unit of a text and its size there: U+FFFD
// for each maximal ill-formed UTF-8 subpart and each lone surrogate.
uint32_t mlocNfdDecodeAt(NfdText text, size_t at, size_t* sizeOut);

// A code point's full canonical decomposition (at most
// nfdMaxDecomposition code points) and its count, with the combining
// classes it starts and ends with.
uint32_t mlocNfdDecompose(uint32_t codePoint, uint32_t* out, uint8_t* leadOut, uint8_t* trailOut);

// The code point boundary before one past 0, as the decoding reads the
// text (a continuation byte alone is a code point of its own).
size_t mlocNfdBefore(NfdText text, size_t at);

static inline void mlocNfdStart(NfdStream* s, const uint8_t* text, size_t length)
{
    mlocNfdStartText(s, mlocNfdUtf8(text, length));
}

// Fills the window with the next code points when it is empty; false at
// the end of the text.
bool mlocNfdFill(NfdStream* s);

// The next code point of the NFD; false at the end.
static inline bool mlocNfdNext(NfdStream* s, uint32_t* codePointOut)
{
    if (s->next == s->count && !mlocNfdFill(s))
    {
        return false;
    }
    *codePointOut = s->window[s->next++];
    return true;
}

// The next code point of the NFD, its combining class and where in the
// source it comes from (a byte or unit offset; texts over 4 GiB give
// offsets modulo 2^32); false at the end.
static inline bool mlocNfdNextFrom(NfdStream* s, uint32_t* codePointOut, uint8_t* classOut,
                                   uint32_t* fromOut)
{
    if (s->next == s->count && !mlocNfdFill(s))
    {
        return false;
    }
    *classOut = s->classes[s->next];
    *fromOut = s->from[s->next];
    *codePointOut = s->window[s->next++];
    return true;
}

// Where the next code point comes from in the source, without reading
// it: the window's next, else where the stream reads on (the rest of a
// long segment come from its end); the text's length at its end.
static inline uint32_t mlocNfdPeekFrom(const NfdStream* s)
{
    if (s->next < s->count)
    {
        return s->from[s->next];
    }
    return (uint32_t)(s->longRun ? s->runEnd : s->at);
}

// The next code point of the NFD and its combining class; false at the
// end.
static inline bool mlocNfdNextClass(NfdStream* s, uint32_t* codePointOut, uint8_t* classOut)
{
    if (s->next == s->count && !mlocNfdFill(s))
    {
        return false;
    }
    *classOut = s->classes[s->next];
    *codePointOut = s->window[s->next++];
    return true;
}

#endif
