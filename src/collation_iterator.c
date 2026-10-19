// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_iterator.h"

#include "bytes.h"
#include "code_point_trie.h"

void mlocCollationIteratorStart(CollationIterator* it, const CollationRoot* root,
                                const uint8_t* text, size_t length)
{
    it->root = root;
    it->ceCount = 0;
    it->ceNext = 0;
    it->upcomingCount = 0;
    it->historyAt = 0;
    it->historyCount = 0;
    mlocNfdStart(&it->nfd, text, length);
}

// The upcoming code point at the index, read from the stream when it is
// not yet; false past the end or the lookahead.
static bool Upcoming(CollationIterator* it, uint32_t index)
{
    while (it->upcomingCount <= index && it->upcomingCount < collationLookahead)
    {
        uint32_t c = 0;
        uint8_t k = 0;
        if (!mlocNfdNextClass(&it->nfd, &c, &k))
        {
            return false;
        }
        it->upcoming[it->upcomingCount] = c;
        it->upcomingClasses[it->upcomingCount++] = k;
    }
    return index < it->upcomingCount;
}

static void Remove(CollationIterator* it, uint32_t index)
{
    for (uint32_t i = index + 1; i < it->upcomingCount; i++)
    {
        it->upcoming[i - 1] = it->upcoming[i];
        it->upcomingClasses[i - 1] = it->upcomingClasses[i];
    }
    it->upcomingCount--;
}

static void Remember(CollationIterator* it, uint32_t codePoint)
{
    it->history[it->historyAt] = codePoint;
    it->historyAt = (it->historyAt + 1) % collationMaxContext;
    it->historyCount += it->historyCount < collationMaxContext;
}

// The code point given back steps before the latest, back 0 the latest.
static uint32_t Before(const CollationIterator* it, uint32_t back)
{
    return it->history[(it->historyAt + collationMaxContext - 1 - back) % collationMaxContext];
}

// The CE32 of the longest prefix entry the history spells, or the list's
// default.
static uint32_t MatchPrefix(const CollationIterator* it, uint32_t number)
{
    const CollationRoot* r = it->root;
    uint32_t at = mlocCollationListStart(r, number);
    uint32_t ce32 = mlocCollationContext(r, at);
    uint32_t count = mlocCollationContext(r, at + 1);
    uint32_t best = 0;
    at += 2;
    for (uint32_t e = 0; e < count; e++)
    {
        uint32_t length = mlocCollationContext(r, at);
        bool same = length <= it->historyCount && length > best;
        for (uint32_t k = 0; same && k < length; k++)
        {
            same = mlocCollationContext(r, at + 1 + k) == Before(it, k);
        }
        if (same)
        {
            best = length;
            ce32 = mlocCollationContext(r, at + 1 + length);
        }
        at += 2 + length;
    }
    return ce32;
}

// A match being grown: the code points after the first, and where each
// was among the upcoming ones.
typedef struct Match
{
    uint32_t text[collationMaxContext];
    uint32_t from[collationMaxContext];
    uint32_t length;
} Match;

// Whether some entry starts with the match's text; *exactOut set to the
// CE32 of the entry that is the text, when one is.
static bool Lookup(const CollationRoot* r, uint32_t number, const Match* m, bool* exactOut,
                   uint32_t* ce32Out)
{
    uint32_t at = mlocCollationListStart(r, number);
    uint32_t count = mlocCollationContext(r, at + 1);
    bool found = false;
    *exactOut = false;
    at += 2;
    for (uint32_t e = 0; e < count; e++)
    {
        uint32_t length = mlocCollationContext(r, at);
        bool starts = length >= m->length;
        for (uint32_t k = 0; starts && k < m->length; k++)
        {
            starts = mlocCollationContext(r, at + 1 + k) == m->text[k];
        }
        if (starts && length == m->length)
        {
            *exactOut = true;
            *ce32Out = mlocCollationContext(r, at + 1 + length);
        }
        found = found || starts;
        at += 2 + length;
    }
    return found;
}

// The CE32 of the longest contraction entry the upcoming code points
// spell, contiguous or not, its code points taken out and remembered;
// the list's default when none matches.
static uint32_t MatchContraction(CollationIterator* it, uint32_t number)
{
    const CollationRoot* r = it->root;
    uint32_t ce32 = mlocCollationContext(r, mlocCollationListStart(r, number));
    Match grown = {.length = 0};
    Match best = {.length = 0};
    uint8_t skipped = 0;
    for (uint32_t i = 0; grown.length < collationMaxContext && Upcoming(it, i); i++)
    {
        uint8_t k = it->upcomingClasses[i];
        if (k == 0 && skipped != 0)
        {
            break;
        }
        if (k != 0 && k <= skipped)
        {
            // Blocked by a skipped mark of the same class.
            continue;
        }
        grown.text[grown.length] = it->upcoming[i];
        grown.from[grown.length++] = i;
        bool exact = false;
        uint32_t entry = 0;
        if (!Lookup(r, number, &grown, &exact, &entry))
        {
            grown.length--;
            if (k == 0)
            {
                break;
            }
            skipped = k;
            continue;
        }
        if (exact)
        {
            best = grown;
            ce32 = entry;
        }
    }
    for (uint32_t j = best.length; j-- > 0;)
    {
        Remove(it, best.from[j]);
    }
    for (uint32_t j = 0; j < best.length; j++)
    {
        Remember(it, best.text[j]);
    }
    return ce32;
}

// Puts the elements of a CE32 that is not a context in the buffer.
static void Expand(CollationIterator* it, uint32_t codePoint, uint32_t ce32)
{
    const CollationRoot* r = it->root;
    it->ceCount = 1;
    it->ceNext = 0;
    if (!mlocCe32IsSpecial(ce32))
    {
        it->ces[0] = mlocCe32Element(ce32);
        return;
    }
    if (ce32 == collationImplicitCe32)
    {
        it->ces[0] = (uint64_t)mlocCollationUnassignedPrimary(codePoint) << 32 |
                     collationCommonSecondaryTertiary;
        return;
    }
    uint32_t index = mlocCe32Index(ce32);
    uint32_t length = mlocCe32Length(ce32);
    switch (mlocCe32Tag(ce32))
    {
    case collationTagExpansion32:
        for (uint32_t i = 0; i < length; i++)
        {
            it->ces[i] = mlocCe32Element(mlocCollationCe32(r, index + i));
        }
        it->ceCount = length;
        break;
    case collationTagExpansion:
        for (uint32_t i = 0; i < length; i++)
        {
            it->ces[i] = mlocCollationCe(r, index + i);
        }
        it->ceCount = length;
        break;
    case collationTagOffset:
    {
        uint64_t element = mlocCollationOffsetElement(mlocCollationCe(r, index), codePoint);
        it->ces[0] = element != 0 ? element
                                  : (uint64_t)mlocCollationUnassignedPrimary(codePoint) << 32 |
                                        collationCommonSecondaryTertiary;
        break;
    }
    default:
        // Long primaries and secondaries; the validator lets no other tag
        // reach here.
        it->ces[0] = mlocCe32Element(ce32);
        break;
    }
}

bool mlocCollationNextElement(CollationIterator* it, uint64_t* elementOut)
{
    if (it->ceNext == it->ceCount)
    {
        if (!Upcoming(it, 0))
        {
            return false;
        }
        uint32_t c = it->upcoming[0];
        Remove(it, 0);
        uint32_t ce32 = mlocTrieValue(&it->root->trie, c);
        bool special = mlocCe32IsSpecial(ce32) && ce32 != collationImplicitCe32;
        if (special && mlocCe32Tag(ce32) == collationTagPrefix)
        {
            ce32 = MatchPrefix(it, mlocCe32Index(ce32));
        }
        Remember(it, c);
        special = mlocCe32IsSpecial(ce32) && ce32 != collationImplicitCe32;
        if (special && mlocCe32Tag(ce32) == collationTagContraction)
        {
            ce32 = MatchContraction(it, mlocCe32Index(ce32));
        }
        Expand(it, c, ce32);
    }
    *elementOut = it->ces[it->ceNext++];
    return true;
}
