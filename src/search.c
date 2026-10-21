// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation search (record mloc-0074): ICU4C 79's usearch_search over
// the processed elements (search_elements.h), its match bounds on
// grapheme clusters (search_boundary.h).

#include "maul-locale/search.h"

#include "collator.h"
#include "search_boundary.h"
#include "search_elements.h"

#include <string.h>

enum
{
    searcherStamp = 0x53,
    // The pattern's processed elements, and the text's held at once
    // (ICU4C's CEIBuffer, sized as it sizes it: the pattern's elements,
    // 32 more, and with a wildcard comparison 3 for each UTF-16 unit of
    // the pattern, 8 for one that may be a leading jamo).
    searchMaxElements = 480,
    searchRing = 512,
    searchExtra = 32,
    // ICU4C's compareCE64s results.
    ceMatch = -1,
    ceNoMatch = 0,
    ceSkipText = 1,
    ceSkipPattern = 2,
    // ICU4C's U_CE_LEVEL2_BASE and U_CE_LEVEL3_BASE: a common weight.
    levelBase = 5,
};

typedef struct Searcher
{
    uint32_t stamp;
    uint32_t comparison;
    uint32_t count;
    uint32_t ring;
    // The pattern's NFD, for the identical strength; 0 code points below.
    uint32_t nfdCount;
    Collator collator;
    uint64_t elements[searchMaxElements];
    uint32_t nfd[searchMaxElements];
} Searcher;

static_assert(sizeof(Searcher) <= sizeof(mlocSearcher), "a searcher fits its value");
static_assert(offsetof(Searcher, stamp) == 0, "the stamp is read first, alone");

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
            return mloc_errorCapacity;
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
        return mloc_errorCapacity;
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

// An element of the text and its range (ICU4C's CEI).
typedef struct Cei
{
    uint64_t ce;
    uint32_t low;
    uint32_t high;
} Cei;

// ICU4C's CEIBuffer: the text's elements by index, the last size - 1
// held.
typedef struct Ring
{
    Cei buf[searchRing];
    uint32_t size;
    uint32_t first;
    uint32_t limit;
    SearchElements e;
} Ring;

// The element at an index: one held, or the next; null for one no
// longer held or not next.
static const Cei* Get(Ring* r, uint32_t index)
{
    Cei* cei = &r->buf[index % r->size];
    if (index >= r->first && index < r->limit)
    {
        return cei;
    }
    if (index != r->limit)
    {
        return nullptr;
    }
    r->limit++;
    if (r->limit - r->first >= r->size)
    {
        r->first++;
    }
    cei->ce = mlocSearchNextElement(&r->e, &cei->low, &cei->high);
    return cei;
}

static uint32_t Level(uint64_t ce, int level)
{
    return (uint32_t)(ce >> (64 - 16 * level)) & 0xFFFF;
}

// ICU4C's compareCE64s.
static int Compare(uint64_t text, uint64_t pattern, uint32_t comparison)
{
    if (text == pattern)
    {
        return ceMatch;
    }
    if (comparison == mloc_searchExact)
    {
        return ceNoMatch;
    }
    bool any = comparison == mloc_searchAnyBaseWildcard;
    for (int level = 1; level <= 2; level++)
    {
        uint32_t t = Level(text, level);
        uint32_t p = Level(pattern, level);
        if (t != p)
        {
            if (t == 0)
            {
                return ceSkipText;
            }
            if (p == 0 && any)
            {
                return ceSkipPattern;
            }
            if (level == 1)
            {
                return ceNoMatch;
            }
            return p == levelBase || (any && t == levelBase) ? ceMatch : ceNoMatch;
        }
    }
    uint32_t t = Level(text, 3);
    uint32_t p = Level(pattern, 3);
    return t == p || p == levelBase || (any && t == levelBase) ? ceMatch : ceNoMatch;
}

// A text's view for a find: the text, the searcher's collator pointed at
// its own parts.
typedef struct Find
{
    const Searcher* s;
    Collator collator;
    NfdText text;
    Ring ring;
    bool full;
} Find;

static const Cei* At(Find* f, uint32_t index)
{
    const Cei* cei = Get(&f->ring, index);
    f->full = f->full || cei == nullptr;
    return cei;
}

// Whether the pattern's elements match from an index, and where in the
// text's elements the match ends (one past its last); the text element
// compared last.
static bool Match(Find* f, uint32_t index, uint32_t* endOut, const Cei** lastOut,
                  uint64_t* patternOut)
{
    int64_t offset = 0;
    const Cei* t = nullptr;
    *patternOut = 0;
    for (int64_t p = 0; p < f->s->count; p++)
    {
        *patternOut = f->s->elements[p];
        t = At(f, (uint32_t)(index + p + offset));
        if (t == nullptr)
        {
            *lastOut = nullptr;
            return false;
        }
        int m = Compare(t->ce, *patternOut, f->s->comparison);
        if (m == ceNoMatch)
        {
            *lastOut = t;
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
    *lastOut = t;
    *endOut = (uint32_t)(offset + f->s->count);
    return true;
}

// Where a match may end: the element after it, past the text's
// ignorables a wildcard comparison takes; false when the match ends
// inside an expansion or before an ignorable it cannot take.
static bool After(Find* f, uint32_t index, uint32_t* end, uint64_t pattern, const Cei** nextOut)
{
    if (f->s->comparison == mloc_searchExact)
    {
        const Cei* next = At(f, index + *end);
        *nextOut = next;
        return next != nullptr && !(next->low == next->high && next->ce != searchEnd);
    }
    for (;; ++*end)
    {
        const Cei* next = At(f, index + *end);
        *nextOut = next;
        if (next == nullptr)
        {
            return false;
        }
        if (next->ce == searchEnd)
        {
            return true;
        }
        if (Level(next->ce, 1) == 0)
        {
            int m = Compare(next->ce, pattern, f->s->comparison);
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

// ICU4C's midcluster rule: the next element starts a primary inside its
// code point's range, at a normalization boundary.
static bool MidCluster(const Find* f, const Cei* last, const Cei* next, uint32_t maxLimit)
{
    if (f->text.length <= maxLimit || Level(next->ce, 1) == 0 || maxLimit < last->high ||
        next->high <= maxLimit)
    {
        return false;
    }
    size_t size = 0;
    uint32_t d[nfdMaxDecomposition];
    uint8_t lead = 0;
    uint8_t trail = 0;
    (void)mlocNfdDecompose(mlocNfdDecodeAt(f->text, maxLimit, &size), d, &lead, &trail);
    if (lead == 0)
    {
        return true;
    }
    if (maxLimit == 0)
    {
        return true;
    }
    (void)mlocNfdDecompose(mlocNfdDecodeAt(f->text, mlocSearchBefore(f->text, maxLimit), &size), d,
                           &lead, &trail);
    return trail == 0 || (trail == 1 && lead == 0);
}

// Whether the text between the bounds has the pattern's NFD, at the
// identical strength.
static bool Identical(const Find* f, size_t start, size_t end)
{
    if (f->collator.settings.strength != collationIdentical)
    {
        return true;
    }
    NfdText part = f->text.units != nullptr ? mlocNfdUtf16(f->text.units + start, end - start)
                                            : mlocNfdUtf8(f->text.bytes + start, end - start);
    NfdStream n;
    mlocNfdStartText(&n, part);
    uint32_t c = 0;
    uint32_t i = 0;
    while (mlocNfdNext(&n, &c))
    {
        if (i == f->s->nfdCount || f->s->nfd[i] != c)
        {
            return false;
        }
        i++;
    }
    return i == f->s->nfdCount;
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
    const Cei* first = At(f, index);
    const Cei* last = At(f, index + end - 1);
    const Cei* next = nullptr;
    bool found = first != nullptr && last != nullptr && After(f, index, &end, pattern, &next);
    if (first == nullptr || last == nullptr || next == nullptr)
    {
        return false;
    }
    uint32_t start = first->low;
    uint32_t minLimit = last->low;
    uint32_t maxLimit = next->low;
    found = found && mlocSearchIsBoundary(f->text, start) && start != first->high;
    bool mid = MidCluster(f, last, next, maxLimit);
    uint32_t limit = Limit(f, last, minLimit, maxLimit, mid);
    if (!mid)
    {
        found = found && limit <= maxLimit && mlocSearchIsBoundary(f->text, limit);
    }
    found = found && Identical(f, start, limit);
    *startOut = start;
    *endOut = limit;
    return found;
}

static mlocResult FindNext(const mlocSearcher* searcher, NfdText text, size_t from,
                           size_t* startOut, size_t* endOut)
{
    const Searcher* s = (const Searcher*)searcher;
    uint32_t stamp = 0;
    if (searcher == nullptr || startOut == nullptr || endOut == nullptr || from > text.length)
    {
        return mloc_errorInvalid;
    }
    memcpy(&stamp, searcher, sizeof(stamp));
    if (stamp != searcherStamp)
    {
        return mloc_errorInvalid;
    }
    Find* f = &(Find){.s = s, .text = text};
    f->collator = s->collator;
    mlocPointCollator(&f->collator);
    f->ring.size = s->ring;
    mlocSearchElementsStartAt(&f->ring.e, &f->collator.root, &f->collator.settings, text, from);
    for (uint32_t index = 0;; index++)
    {
        uint32_t end = 0;
        const Cei* last = nullptr;
        uint64_t pattern = 0;
        const Cei* first = At(f, index);
        if (first == nullptr)
        {
            return mloc_errorCapacity;
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
            return mloc_errorCapacity;
        }
        if (!matched)
        {
            if (last->ce == searchEnd)
            {
                return mloc_errorNotFound;
            }
            continue;
        }
        bool found = Bounds(f, index, end, pattern, startOut, endOut);
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

// A position inside a code point moved to its start.
static size_t Align(NfdText t, size_t at)
{
    if (at == 0 || at >= t.length)
    {
        return at;
    }
    if (t.units != nullptr)
    {
        bool trail = t.units[at] - 0xDC00u < 0x400 && t.units[at - 1] - 0xD800u < 0x400;
        return trail ? at - 1 : at;
    }
    size_t q = at;
    while (q > 0 && at - q < 3 && (t.bytes[q] & 0xC0) == 0x80)
    {
        q--;
    }
    size_t size = 0;
    (void)mlocNfdDecodeAt(t, q, &size);
    return q + size > at ? q : at;
}

mlocResult mlocFindNext(const mlocSearcher* searcher, const char* text, size_t length, size_t from,
                        size_t* startOut, size_t* endOut)
{
    if (text == nullptr && length > 0)
    {
        return mloc_errorInvalid;
    }
    NfdText t = mlocNfdUtf8((const uint8_t*)(text != nullptr ? text : ""), length);
    return FindNext(searcher, t, Align(t, from), startOut, endOut);
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
    return FindNext(searcher, t, Align(t, from), startOut, endOut);
}
