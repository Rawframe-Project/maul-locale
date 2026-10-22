// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "search_back.h"

void mlocSearchBackStart(SearchBack* b, const CollationRoot* root,
                         const CollationSettings* settings, NfdText text, size_t from)
{
    b->root = root;
    b->settings = settings;
    b->text = text;
    b->afterShifted = false;
    b->start = from;
    b->halfCount = 0;
    b->processedCount = 0;
    b->full = false;
    mlocSearchElementsStart(&b->piece, root, settings, mlocNfdUtf8(nullptr, 0));
}

// The previous raw half, reading the piece before when this one is
// spent; false at the text's start or when a piece does not fit.
static bool PreviousHalf(SearchBack* b, SearchHalf* out)
{
    while (b->halfCount == 0)
    {
        if (b->start == 0 || b->full)
        {
            return false;
        }
        size_t end = b->start;
        b->start = mlocSearchPieceStart(b->root, b->settings, b->text, end);
        mlocSearchElementsStartPiece(&b->piece, b->root, b->settings, b->text, b->start, end);
        SearchHalf h;
        while (mlocSearchNextHalf(&b->piece, &h.half, &h.low, &h.high))
        {
            if (b->halfCount == searchBackHalves)
            {
                b->full = true;
                return false;
            }
            h.low += (uint32_t)b->start;
            h.high += (uint32_t)b->start;
            b->halves[b->halfCount++] = h;
        }
    }
    *out = b->halves[--b->halfCount];
    return true;
}

uint64_t mlocSearchPreviousElement(SearchBack* b, uint32_t* lowOut, uint32_t* highOut)
{
    while (b->processedCount == 0)
    {
        // Back to a half with a primary that is not a continuation.
        uint32_t count = 0;
        SearchHalf h;
        while (PreviousHalf(b, &h))
        {
            if (count == searchBackHalves)
            {
                b->full = true;
                break;
            }
            b->chunk[count++] = h;
            if ((h.half & 0xFFFF0000) != 0 && (h.half & 0xC0) != 0xC0)
            {
                break;
            }
        }
        if (count == 0 || b->full)
        {
            *lowOut = 0;
            *highOut = 0;
            return searchEnd;
        }
        // Forward over the chunk, the state as the chunk after left it.
        b->piece.afterShifted = b->afterShifted;
        while (count-- > 0)
        {
            uint64_t element = mlocSearchProcess(&b->piece, b->chunk[count].half);
            if (element != 0)
            {
                b->processed[b->processedCount++] =
                    (SearchProcessed){element, b->chunk[count].low, b->chunk[count].high};
            }
        }
        b->afterShifted = b->piece.afterShifted;
    }
    SearchProcessed p = b->processed[--b->processedCount];
    *lowOut = p.low;
    *highOut = p.high;
    return p.element;
}
