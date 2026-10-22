// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "search_find.h"

#include "search_boundary.h"

#include <string.h>

static_assert(sizeof(Searcher) <= sizeof(mlocSearcher), "a searcher fits its value");
static_assert(offsetof(Searcher, stamp) == 0, "the stamp is read first, alone");

const Cei* mlocSearchRingGet(Ring* r, uint32_t index)
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
    // The ring holds size elements: the oldest goes when one more comes.
    r->limit++;
    if (r->limit - r->first > r->size)
    {
        r->first++;
    }
    if (r->back == nullptr)
    {
        cei->ce = mlocSearchNextElement(&r->e, &cei->low, &cei->high);
        return cei;
    }
    cei->ce = mlocSearchPreviousElement(r->back, &cei->low, &cei->high);
    return r->back->full ? nullptr : cei;
}

uint32_t mlocSearchLevel(uint64_t ce, int level)
{
    return (uint32_t)(ce >> (64 - 16 * level)) & 0xFFFF;
}

int mlocSearchCompare(uint64_t text, uint64_t pattern, uint32_t comparison)
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
        uint32_t t = mlocSearchLevel(text, level);
        uint32_t p = mlocSearchLevel(pattern, level);
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
    uint32_t t = mlocSearchLevel(text, 3);
    uint32_t p = mlocSearchLevel(pattern, 3);
    return t == p || p == levelBase || (any && t == levelBase) ? ceMatch : ceNoMatch;
}

const Cei* mlocSearchAt(Find* f, uint32_t index)
{
    const Cei* cei = mlocSearchRingGet(&f->ring, index);
    f->full = f->full || cei == nullptr;
    return cei;
}

bool mlocSearchMidCluster(const Find* f, const Cei* last, const Cei* next, uint32_t maxLimit)
{
    if (f->text.length <= maxLimit || mlocSearchLevel(next->ce, 1) == 0 || maxLimit < last->high ||
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

bool mlocSearchIdentical(const Find* f, size_t start, size_t end)
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

size_t mlocSearchAlign(NfdText t, size_t at)
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

mlocResult mlocSearchOpenFind(Find* f, const mlocSearcher* searcher, NfdText text, size_t from,
                              const size_t* startOut, const size_t* endOut)
{
    uint32_t stamp = 0;
    // Elements place their ranges in 32 bits.
    if (searcher == nullptr || startOut == nullptr || endOut == nullptr || from > text.length ||
        text.length > UINT32_MAX)
    {
        return mloc_errorInvalid;
    }
    memcpy(&stamp, searcher, sizeof(stamp));
    if (stamp != searcherStamp)
    {
        return mloc_errorInvalid;
    }
    const Searcher* s = (const Searcher*)searcher;
    f->s = s;
    f->text = text;
    f->collator = s->collator;
    mlocPointCollator(&f->collator);
    f->ring.size = s->ring;
    f->ring.first = 0;
    f->ring.limit = 0;
    f->ring.back = nullptr;
    f->full = false;
    return mloc_success;
}
