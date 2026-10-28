// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation search: ICU4C 79's usearch_search over the processed elements
// (search_elements.h), its match bounds on grapheme clusters
// (search_boundary.h).

#include "maul-locale/search.h"

#include "invariant.h"
#include "search_boundary.h"
#include "search_find.h"

// ICU4C's MIGHT_BE_JAMO_L, on a UTF-16 unit.
static bool MightBeJamoL(uint32_t u)
{
    return (u >= 0x1100 && u <= 0x115E) || (u >= 0x3131 && u <= 0x314E) ||
           (u >= 0x3165 && u <= 0x3186);
}

// The text's elements ICU4C's buffer holds for a pattern.
static uint32_t RingSize(const Searcher* s, NfdText pattern)
{
    size_t size = (size_t)s->count + searchExtra;
    for (size_t at = 0; s->comparison != mloc_searchExact && at < pattern.length;)
    {
        size_t step = 0;
        uint32_t c = mlocNfdDecodeAt(pattern, at, &step);
        size += c >= 0x10000 ? 6 : MightBeJamoL(c) ? 8 : 3;
        at += step;
    }
    return size > searchRing ? searchRing + 1 : (uint32_t)size;
}

// The pattern's NFD for the identical check; false when it does not fit.
static bool PatternNfd(Searcher* s, NfdText pattern)
{
    NfdStream n;
    mlocNfdStartText(&n, pattern);
    uint32_t c = 0;
    s->nfdCount = 0;
    while (mlocNfdNext(&n, &c))
    {
        if (s->nfdCount == searchMaxElements)
        {
            return false;
        }
        s->nfd[s->nfdCount++] = c;
    }
    return true;
}

static mlocResult Init(mlocSearcher* searcherOut, const mlocCollator* collator, NfdText pattern,
                       const mlocSearchOptions* options)
{
    mlocSearchOptions o = options != nullptr ? *options : (mlocSearchOptions){0};
    static Searcher s_zero;
    Searcher* s = (Searcher*)searcherOut;
    if (searcherOut == nullptr || (uint32_t)o.comparison > mloc_searchAnyBaseWildcard)
    {
        return mloc_errorInvalid;
    }
    *s = s_zero;
    if (!mlocReadCollator(collator, &s->collator))
    {
        return mloc_errorInvalid;
    }
    s->comparison = (uint32_t)o.comparison;
    // ICU4C refuses numeric collators (U_UNSUPPORTED_ERROR) and patterns
    // with no element its getCE keeps.
    if (s->collator.settings.numeric ||
        !mlocSearchHasElements(&s->collator.root, &s->collator.settings, pattern))
    {
        return mloc_errorInvalid;
    }
    SearchElements e;
    mlocSearchElementsStart(&e, &s->collator.root, &s->collator.settings, pattern);
    uint32_t low = 0;
    uint32_t high = 0;
    for (uint64_t p = mlocSearchNextElement(&e, &low, &high); p != searchEnd;
         p = mlocSearchNextElement(&e, &low, &high))
    {
        if (s->count == searchMaxElements)
        {
            return mloc_errorLimit;
        }
        s->elements[s->count++] = p;
    }
    if (s->count == 0)
    {
        return mloc_errorInvalid;
    }
    s->ring = RingSize(s, pattern);
    if (s->ring > searchRing ||
        (s->collator.settings.strength == collationIdentical && !PatternNfd(s, pattern)))
    {
        return mloc_errorLimit;
    }
    s->stamp = searcherStamp;
    return mloc_success;
}

mlocResult mlocInitSearcher(mlocSearcher* searcherOut, const mlocCollator* collator,
                            const char* pattern, size_t length, const mlocSearchOptions* options)
{
    if (pattern == nullptr && length > 0)
    {
        return mloc_errorInvalid;
    }
    return Init(searcherOut, collator,
                mlocNfdUtf8((const uint8_t*)(pattern != nullptr ? pattern : ""), length), options);
}

mlocResult mlocInitSearcherUtf16(mlocSearcher* searcherOut, const mlocCollator* collator,
                                 const uint16_t* pattern, size_t length,
                                 const mlocSearchOptions* options)
{
    static const uint16_t s_empty[1] = {0};
    if (pattern == nullptr && length > 0)
    {
        return mloc_errorInvalid;
    }
    return Init(searcherOut, collator, mlocNfdUtf16(pattern != nullptr ? pattern : s_empty, length),
                options);
}

// Whether the pattern's elements match from an index, and where in the
// text's elements the match ends (one past its last); the text element
// compared last (searchEnd when none was).
static bool Match(Find* f, uint32_t index, uint32_t* endOut, uint64_t* lastOut,
                  uint64_t* patternOut)
{
    int64_t offset = 0;
    const Cei* t = nullptr;
    *patternOut = 0;
    for (int64_t p = 0; p < f->s->count; p++)
    {
        *patternOut = f->s->elements[p];
        // A skipped text element keeps the pattern's place and a skipped
        // pattern element the text's, so the text's index never falls.
        MLOC_ASSERT(p + offset >= 0);
        t = mlocSearchAt(f, (uint32_t)(index + p + offset));
        if (t == nullptr)
        {
            *lastOut = searchEnd;
            return false;
        }
        int m = mlocSearchCompare(t->ce, *patternOut, f->s->comparison);
        if (m == ceNoMatch)
        {
            *lastOut = t->ce;
            return false;
        }
        if (m == ceSkipText)
        {
            p--;
            offset++;
        }
        else if (m == ceSkipPattern)
        {
            offset--;
        }
    }
    *lastOut = t != nullptr ? t->ce : searchEnd;
    *endOut = (uint32_t)(offset + f->s->count);
    return true;
}

// Where a match may end: the element after it, past the text's
// ignorables a wildcard comparison takes; false when the match ends
// inside an expansion or before an ignorable it cannot take.
static bool After(Find* f, uint32_t index, uint32_t* end, uint64_t pattern, Cei* nextOut,
                  bool* heldOut)
{
    if (f->s->comparison == mloc_searchExact)
    {
        const Cei* next = mlocSearchAt(f, index + *end);
        *heldOut = next != nullptr;
        if (next == nullptr)
        {
            return false;
        }
        *nextOut = *next;
        return !(next->low == next->high && next->ce != searchEnd);
    }
    for (;; ++*end)
    {
        const Cei* next = mlocSearchAt(f, index + *end);
        *heldOut = next != nullptr;
        if (next == nullptr)
        {
            return false;
        }
        *nextOut = *next;
        if (next->ce == searchEnd)
        {
            return true;
        }
        if (mlocSearchLevel(next->ce, 1) == 0)
        {
            int m = mlocSearchCompare(next->ce, pattern, f->s->comparison);
            if (m == ceNoMatch || m == ceSkipPattern)
            {
                return false;
            }
        }
        else
        {
            return next->low != next->high;
        }
    }
}

// The match's end from its elements' bounds, as usearch_search places it:
// the next grapheme boundary after its last element's start, unless that
// passes the element after it, or the midcluster rule allows less.
static uint32_t Limit(Find* f, const Cei* last, uint32_t minLimit, uint32_t maxLimit, bool mid)
{
    uint32_t limit = maxLimit;
    if (minLimit < maxLimit)
    {
        if (minLimit == last->high && mlocSearchIsBoundary(f->text, minLimit))
        {
            limit = minLimit;
        }
        else
        {
            size_t following = mlocSearchFollowing(f->text, minLimit);
            if (following >= last->high && (!mid || following < maxLimit))
            {
                limit = (uint32_t)following;
            }
        }
    }
    return limit;
}

// Whether the match from an index, its elements matched up to end, has
// bounds usearch_search accepts; its bounds.
static bool Bounds(Find* f, uint32_t index, uint32_t end, uint64_t pattern, size_t* startOut,
                   size_t* endOut)
{
    // The elements are copied: fetching more may reuse their places.
    const Cei* held = mlocSearchAt(f, index);
    if (held == nullptr)
    {
        return false;
    }
    Cei first = *held;
    held = mlocSearchAt(f, index + end - 1);
    if (held == nullptr)
    {
        return false;
    }
    Cei last = *held;
    Cei next = {0};
    bool nextHeld = false;
    bool found = After(f, index, &end, pattern, &next, &nextHeld);
    if (!nextHeld)
    {
        return false;
    }
    uint32_t start = first.low;
    uint32_t minLimit = last.low;
    uint32_t maxLimit = next.low;
    found = found && mlocSearchIsBoundary(f->text, start) && start != first.high;
    bool mid = mlocSearchMidCluster(f, &last, &next, maxLimit);
    uint32_t limit = Limit(f, &last, minLimit, maxLimit, mid);
    if (!mid)
    {
        found = found && limit <= maxLimit && mlocSearchIsBoundary(f->text, limit);
    }
    // A pattern element skipped against the text can put the last element
    // before the first: no match then, and no text between to compare.
    found = found && start <= limit && mlocSearchIdentical(f, start, limit);
    *startOut = start;
    *endOut = limit;
    return found;
}

static mlocResult FindNext(const mlocSearcher* searcher, NfdText text, size_t from,
                           size_t* startOut, size_t* endOut)
{
    Find* f = &(Find){0};
    mlocResult opened = mlocSearchOpenFind(f, searcher, text, from, startOut, endOut);
    if (opened != mloc_success)
    {
        return opened;
    }
    mlocSearchElementsStartAt(&f->ring.e, &f->collator.root, &f->collator.settings, text, from);
    for (uint32_t index = 0;; index++)
    {
        uint32_t end = 0;
        uint64_t last = 0;
        uint64_t pattern = 0;
        const Cei* first = mlocSearchAt(f, index);
        if (first == nullptr)
        {
            return mloc_errorLimit;
        }
        if (first->ce == searchEnd)
        {
            // No match starts at the end (its start would be its first
            // element's end). ICU4C goes on here for ever when a wildcard
            // comparison skips the pattern's elements against the end.
            return mloc_errorNotFound;
        }
        bool matched = Match(f, index, &end, &last, &pattern);
        if (f->full)
        {
            return mloc_errorLimit;
        }
        if (!matched)
        {
            if (last == searchEnd)
            {
                return mloc_errorNotFound;
            }
            continue;
        }
        bool found = Bounds(f, index, end, pattern, startOut, endOut);
        if (f->full)
        {
            return mloc_errorLimit;
        }
        if (found)
        {
            return mloc_success;
        }
    }
}

mlocResult mlocFindNext(const mlocSearcher* searcher, const char* text, size_t length, size_t from,
                        size_t* startOut, size_t* endOut)
{
    if (text == nullptr && length > 0)
    {
        return mloc_errorInvalid;
    }
    NfdText t = mlocNfdUtf8((const uint8_t*)(text != nullptr ? text : ""), length);
    return FindNext(searcher, t, mlocSearchAlign(t, from), startOut, endOut);
}

mlocResult mlocFindNextUtf16(const mlocSearcher* searcher, const uint16_t* text, size_t length,
                             size_t from, size_t* startOut, size_t* endOut)
{
    static const uint16_t s_empty[1] = {0};
    if (text == nullptr && length > 0)
    {
        return mloc_errorInvalid;
    }
    NfdText t = mlocNfdUtf16(text != nullptr ? text : s_empty, length);
    return FindNext(searcher, t, mlocSearchAlign(t, from), startOut, endOut);
}
