// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// UTF-8 or UTF-16 text read as its NFD (record mloc-0066), a code point
// at a time, through Maul Unicode's decomposition and combining classes;
// ill-formed bytes read as U+FFFD, one for each maximal ill-formed
// subpart, and so does a lone surrogate. Nothing is
// allocated and nothing limits the marks after a starter.
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
