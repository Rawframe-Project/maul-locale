// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation search backward: ICU4C 79's usearch_searchBackwards over the
// text's processed elements walking back (search_back.h). The walk's index
// 0 is the element nearest the position; a match's elements are compared
// from the pattern's last.

#include "invariant.h"
#include "search_boundary.h"
#include "search_find.h"

#include "maul-locale/search.h"

// Whether the pattern's elements match with its last at an index of the
// walk; the index of the element its first matched, and the element
// compared last (null when the ring ran out).
static bool MatchBack(Find* f, uint32_t index, uint32_t* firstOut, const Cei** comparedOut)
{
    int64_t offset = 0;
    int64_t count = f->s->count;
    const Cei* t = nullptr;
    for (int64_t p = count - 1; p >= 0; p--)
    {
        // As in Match: the walk's index never falls.
        MLOC_ASSERT(count - 1 - p + offset >= 0);
        t = mlocSearchAt(f, (uint32_t)(index + (count - 1 - p) + offset));
        *comparedOut = t;
        if (t == nullptr)
        {
            return false;
        }
        int m = mlocSearchCompare(t->ce, f->s->elements[p], f->s->comparison);
        if (m == ceNoMatch)
        {
            return false;
        }
        if (m == ceSkipText)
        {
            p++;
            offset++;
        }
        else if (m == ceSkipPattern)
        {
            offset--;
        }
    }
    *firstOut = (uint32_t)(index + (count - 1) + offset);
    return true;
}

// Whether the match whose last element is at an index has bounds
// usearch_searchBackwards accepts; its bounds.
static bool BoundsBack(Find* f, uint32_t index, uint32_t firstIndex, size_t from, size_t* startOut,
                       size_t* endOut)
{
    // The elements are copied: fetching more may reuse their places.
    const Cei* held = mlocSearchAt(f, firstIndex);
    if (held == nullptr)
    {
        return false;
    }
    Cei first = *held;
    held = mlocSearchAt(f, index);
    if (held == nullptr)
    {
        return false;
    }
    Cei last = *held;
    uint32_t start = first.low;
    bool found = mlocSearchIsBoundary(f->text, start) && start != first.high;
    uint32_t minLimit = last.low;
    uint32_t limit = 0;
    if (index > 0)
    {
        held = mlocSearchAt(f, index - 1);
        if (held == nullptr)
        {
            return false;
        }
        Cei next = *held;
        found = found && !(next.low == next.high && next.ce != searchEnd);
        uint32_t maxLimit = next.low;
        limit = maxLimit;
        bool mid = mlocSearchMidCluster(f, &last, &next, maxLimit);
        if (minLimit < maxLimit)
        {
            size_t following = mlocSearchFollowing(f->text, minLimit);
            if (following >= last.high && (!mid || following < maxLimit))
            {
                limit = (uint32_t)following;
            }
        }
        if (!mid)
        {
            found = found && limit <= maxLimit && mlocSearchIsBoundary(f->text, limit);
        }
    }
    else
    {
        // Nothing after the match: it ends at the boundary after its last
        // element's start, or at the position when that is nearer.
        size_t following = mlocSearchFollowing(f->text, minLimit);
        limit = (uint32_t)(from > following ? following : from);
    }
    found = found && start <= limit && mlocSearchIdentical(f, start, limit);
    *startOut = start;
    *endOut = limit;
    return found;
}

// The walk's index of the first element starting before the position:
// those of the cluster at it come first. False when the ring ran out.
static bool PastCluster(Find* f, size_t from, uint32_t* indexOut)
{
    uint32_t index = 0;
    for (;; index++)
    {
        const Cei* cei = mlocSearchAt(f, index);
        if (cei == nullptr)
        {
            return false;
        }
        if (cei->ce == searchEnd || cei->low < from)
        {
            *indexOut = index;
            return true;
        }
    }
}

static mlocResult FindPrevious(const mlocSearcher* searcher, NfdText text, size_t from,
                               size_t* startOut, size_t* endOut)
{
    Find* f = &(Find){0};
    mlocResult opened = mlocSearchOpenFind(f, searcher, text, from, startOut, endOut);
    if (opened != mloc_success)
    {
        return opened;
    }
    SearchBack back;
    // At the end the walk starts there: the boundary after it and its
    // start are the end.
    size_t walk = mlocSearchStartPosition(&f->collator.root, &f->collator.settings, text,
                                          mlocSearchFollowing(text, from));
    mlocSearchBackStart(&back, &f->collator.root, &f->collator.settings, text, walk);
    f->ring.back = &back;
    uint32_t index = 0;
    if (from < text.length && !PastCluster(f, from, &index))
    {
        return mloc_errorCapacity;
    }
    for (;; index++)
    {
        const Cei* last = mlocSearchAt(f, index);
        if (last == nullptr)
        {
            return mloc_errorCapacity;
        }
        if (last->ce == searchEnd)
        {
            // No match ends at the start. ICU4C goes on here for ever when
            // a wildcard comparison skips the pattern's elements against
            // it.
            return mloc_errorNotFound;
        }
        uint32_t firstIndex = 0;
        const Cei* compared = nullptr;
        bool matched = MatchBack(f, index, &firstIndex, &compared);
        if (f->full)
        {
            return mloc_errorCapacity;
        }
        if (!matched)
        {
            if (compared->ce == searchEnd)
            {
                return mloc_errorNotFound;
            }
            continue;
        }
        bool found = BoundsBack(f, index, firstIndex, from, startOut, endOut);
        if (f->full)
        {
            return mloc_errorCapacity;
        }
        if (found)
        {
            return mloc_success;
        }
    }
}

mlocResult mlocFindPrevious(const mlocSearcher* searcher, const char* text, size_t length,
                            size_t from, size_t* startOut, size_t* endOut)
{
    if (text == nullptr && length > 0)
    {
        return mloc_errorInvalid;
    }
    NfdText t = mlocNfdUtf8((const uint8_t*)(text != nullptr ? text : ""), length);
    return FindPrevious(searcher, t, mlocSearchAlign(t, from), startOut, endOut);
}

mlocResult mlocFindPreviousUtf16(const mlocSearcher* searcher, const uint16_t* text, size_t length,
                                 size_t from, size_t* startOut, size_t* endOut)
{
    static const uint16_t s_empty[1] = {0};
    if (text == nullptr && length > 0)
    {
        return mloc_errorInvalid;
    }
    NfdText t = mlocNfdUtf16(text != nullptr ? text : s_empty, length);
    return FindPrevious(searcher, t, mlocSearchAlign(t, from), startOut, endOut);
}
