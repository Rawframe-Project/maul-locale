// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A segmenter's state (maul-locale/segment.h): Maul Unicode's iterator,
// the tailoring's class overrides and suppressions, and the filters that
// take breaks away: the suppressions of a sentence segmenter, which keep a
// sentence going after an abbreviation, and Japanese's rules 13.3 and 13.4
// in a word segmenter, which keep a run of Hiragana, or of ideographs, one
// word.
//
// A filter reads the text around a break: the suppressions up to
// segmentMaxSuppression + 2 bytes before it and as many after an entry's
// start, rules 13.3 and 13.4 the code point after it and the last one
// before it that is not Extend, Format or ZWJ. The text comes in pieces, and Maul
// Unicode's iterator may hold a break back until it has read far past it
// (a sentence after "Mr. (1, 2, 3" waits for a letter). So the segmenter
// keeps the last segmentWindow bytes of the earlier pieces, and when it
// lets a piece go, it settles what a break held back that far would need:
// - A sentence break held back past the window is the end of the last
//   run of a terminator, closing punctuation and spaces, since no other
//   break can wait for text after a terminator. The suppression of that
//   one position is decided then and kept.
// - A word break held back past the window comes before a mid-word
//   character (WB6, WB7b, WB12) with Extend or Format characters after it,
//   so it is never between two Hiragana or two ideographs, and rules 13.3
//   and 13.4 keep it.
// A break a filter decides only with text not yet fed waits in a queue,
// while the iterator goes on to the end of its piece; only then does the
// segmenter ask for the next one, so that no piece is needed once it has.

#ifndef MLOC_SEGMENTER_H
#define MLOC_SEGMENTER_H

#include "data_view.h"
#include "segment_tables.h"

#include "maul-locale/segment.h"
#include "maul-unicode/segment.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    segmenterStamp = 0x47455353,
    // The bytes of the earlier pieces kept: what a filter reads around a
    // break the iterator may still report, at most 2 * 34 + 4.
    segmentWindow = 72,
    // The breaks a queue holds: one for each byte a filter may wait on.
    segmentQueue = 40,
    // The filters.
    segmentSuppress = 1u << 0,
    segmentJapanese = 1u << 1,
    // Rules 13.3 and 13.4's classes of a code point.
    japaneseOther = 0,
    japaneseHiragana = 1,
    japaneseIdeographic = 2,
};

typedef struct Segmenter
{
    uint32_t stamp;
    uint8_t kind;
    uint8_t filters;
    // Whether a break was asked for; whether the iterator asked for the
    // next piece; whether more pieces follow the last one fed; whether the
    // iterator reported its last break; whether the iterator is finding a
    // break, so that the complex breaker runs.
    bool started;
    bool iteratorWaits;
    bool moreFollows;
    bool iteratorDone;
    bool busy;
    // Rules 13.3 and 13.4: the class of the last code point before cursor
    // that is not Extend, Format or ZWJ.
    uint8_t cursorClass;
    // The kept answer for the held sentence break.
    bool heldSuppressed;
    bool heldValid;
    // The terminator run the scan is in: 0 none, 1 the terminator and
    // closing punctuation, 2 the spaces after them.
    uint8_t scanRun;
    uint8_t queueCount;
    uint16_t windowLength;
    uint32_t backward;
    uint32_t forward;
    uint32_t overrideCount;
    // The current piece, from pieceStart in the whole text.
    const char* piece;
    size_t pieceLength;
    size_t pieceStart;
    // Rule 13.3's place; the sentence scan's place, and the end of the
    // last terminator run it passed; the held break.
    size_t cursor;
    size_t scanned;
    size_t runEnd;
    size_t held;
    // The suppressions, in the data the segmenter was made from.
    const uint8_t* values;
    const uint8_t* offsets;
    const uint8_t* text;
    // The class overrides, in the code table (segment_overrides.h): a
    // segmenter holds no pointer into itself, so it may be copied.
    const muniBreakClassOverride* overrides;
    // The queue's first break, and each one's distance from it.
    size_t queueBase;
    uint8_t queue[segmentQueue];
    // The bytes before pieceStart.
    uint8_t window[segmentWindow];
    muniSegmentIterator iterator;
} Segmenter;

static_assert(sizeof(Segmenter) <= sizeof(mlocSegmenter), "a segmenter fits its value");

// Whether a filter keeps a break: 1 when it does, 0 when it takes it
// away, -1 when it needs text not yet fed.
int mlocSegmentKeeps(Segmenter* s, size_t offset);

// Before the segmenter lets its piece go: keeps its last bytes and settles
// what a break the iterator holds back past them needs.
void mlocSegmentLeavePiece(Segmenter* s);

#endif
