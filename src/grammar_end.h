// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The end of a text the inflector reads: its kind and span, found by
// the letters, digits, closing marks and period before it.

#ifndef MLOC_GRAMMAR_END_H
#define MLOC_GRAMMAR_END_H

#include "invariant.h"

#include "maul-locale/grammar.h"
#include "maul-unicode/encoding.h"
#include "maul-unicode/properties.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// What a text's end is.
typedef enum EndKind
{
    endWord,
    endLetters,
    endNumber,
    endOrdinal,
} EndKind;

// A text's end: the span read, its kind, and what stands after it.
typedef struct End
{
    size_t start;
    size_t end;
    EndKind kind;
    // A closing quotation mark or bracket after it, or a period after
    // letters: no apostrophe then.
    bool closed;
    bool period;
    // Its letters are all capitals.
    bool capitals;
} End;

// Decodes the code point that ends at text[end], valid UTF-8; its first
// byte's index in startOut.
static inline uint32_t Before(const uint8_t* text, size_t end, size_t* startOut)
{
    MLOC_ASSERT(end > 0);
    size_t at = end - 1;
    if (text[at] < 0x80)
    {
        *startOut = at;
        return text[at];
    }
    while (at > 0 && (text[at] & 0xC0) == 0x80)
    {
        at--;
    }
    *startOut = at;
    uint32_t c = text[at];
    size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    // Valid UTF-8 (the text was checked), read from a code point's end:
    // the sequence ends where the read began.
    MLOC_ASSERT(at + extra + 1 == end);
    c &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
    for (size_t i = 1; i <= extra; i++)
    {
        c = c << 6 | (text[at + i] & 0x3F);
    }
    return c;
}

// A code point's general category, ASCII's letters and digits without a
// lookup.
static inline muniGeneralCategory CategoryOf(uint32_t c)
{
    return c >= 'a' && c <= 'z'   ? muni_gcLl
           : c >= 'A' && c <= 'Z' ? muni_gcLu
           : c >= '0' && c <= '9' ? muni_gcNd
                                  : muniGetGeneralCategory(c);
}

static inline bool IsLetterCategory(muniGeneralCategory g)
{
    return g == muni_gcLu || g == muni_gcLl || g == muni_gcLt || g == muni_gcLm || g == muni_gcLo ||
           g == muni_gcMn || g == muni_gcMc;
}

static inline bool IsLetter(uint32_t c)
{
    return IsLetterCategory(CategoryOf(c));
}

static inline bool IsDigit(uint32_t c)
{
    return c >= '0' && c <= '9';
}

// A closing quotation mark or bracket: what TDK writes suffixes after
// without an apostrophe.
static inline bool IsCloser(uint32_t c)
{
    muniGeneralCategory g = CategoryOf(c);
    return g == muni_gcPe || g == muni_gcPf || c == '"' || c == '\'';
}

// The separators a written number holds between its digits.
static inline bool IsNumberSeparator(uint32_t c)
{
    return c == '.' || c == ',' || c == 0xA0 || c == 0x202F || c == '\'';
}

// Finds the digits that end a text at end: digits with separators
// between them.
static inline size_t NumberStart(const uint8_t* text, size_t end)
{
    size_t start = end;
    size_t at = end;
    while (at > 0)
    {
        size_t previous = 0;
        uint32_t c = Before(text, at, &previous);
        if (IsDigit(c))
        {
            start = previous;
        }
        else if (!IsNumberSeparator(c) || previous == 0 || at != start)
        {
            break;
        }
        at = previous;
    }
    return start;
}

// Finds the letters that end a text at end, and whether they are all
// capitals.
static inline size_t WordStart(const uint8_t* text, size_t end, bool* capitalsOut)
{
    size_t at = end;
    size_t letters = 0;
    bool capitals = true;
    while (at > 0)
    {
        size_t previous = 0;
        muniGeneralCategory g = CategoryOf(Before(text, at, &previous));
        if (!IsLetterCategory(g))
        {
            break;
        }
        capitals = capitals && (g == muni_gcLu || g == muni_gcMn || g == muni_gcMc);
        letters++;
        at = previous;
    }
    *capitalsOut = capitals && letters > 0;
    return at;
}

// Finds and classifies a text's end; false when nothing ends it that can
// be read.
static inline bool FindEnd(bool periodOrdinals, const uint8_t* text, size_t length,
                           mlocInflectionReading reading, End* out)
{
    *out = (End){0};
    size_t end = length;
    size_t previous = 0;
    while (end > 0 && IsCloser(Before(text, end, &previous)))
    {
        out->closed = true;
        end = previous;
    }
    bool dot = end > 0 && text[end - 1] == '.';
    size_t last = dot ? end - 1 : end;
    if (last == 0)
    {
        return false;
    }
    uint32_t c = Before(text, last, &previous);
    out->end = last;
    if (IsDigit(c))
    {
        out->start = NumberStart(text, last);
        out->kind = dot && periodOrdinals ? endOrdinal : endNumber;
        out->period = dot && !periodOrdinals;
        return true;
    }
    if (!IsLetter(c))
    {
        return false;
    }
    bool capitals = false;
    out->start = WordStart(text, last, &capitals);
    out->period = dot;
    out->capitals = capitals;
    out->kind = reading == mloc_readingLetters                    ? endLetters
                : reading == mloc_readingAuto && capitals && !dot ? endLetters
                                                                  : endWord;
    return true;
}

#endif
