// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_iterator.h"

#include "bytes.h"
#include "code_point_trie.h"
#include "invariant.h"

#include "maul-unicode/properties.h"

static_assert((int)collationNumericElements > (int)collationMaxExpansion,
              "an expansion fits the elements");

void mlocCollationIteratorStartText(CollationIterator* it, const CollationRoot* root, NfdText text)
{
    it->root = root;
    it->tailoring = nullptr;
    it->han = nullptr;
    it->numeric = false;
    it->ceCount = 0;
    it->ceNext = 0;
    it->upcomingCount = 0;
    it->historyAt = 0;
    it->historyCount = 0;
    it->high = 0;
    it->taken = 0;
    it->droppedCount = 0;
    it->scanEnd = 0;
    it->scanMax = 0;
    mlocNfdStartText(&it->nfd, text);
}

// Whether a contraction took the code point the stream gives at the
// count; taken out of the list when reading on (drop).
static bool Dropped(CollationIterator* it, uint32_t at, bool drop)
{
    for (uint32_t i = 0; i < it->droppedCount; i++)
    {
        if (it->dropped[i] == at)
        {
            if (drop)
            {
                it->dropped[i] = it->dropped[--it->droppedCount];
            }
            return true;
        }
    }
    return false;
}

// The next code point of a stream that no contraction took, its count
// advanced past it (*atOut, its own count).
static bool Read(CollationIterator* it, NfdStream* s, uint32_t* count, bool drop, uint32_t* c,
                 uint8_t* k, uint32_t* from, uint32_t* atOut)
{
    for (;;)
    {
        if (!mlocNfdNextFrom(s, c, k, from))
        {
            return false;
        }
        uint32_t at = (*count)++;
        if (!Dropped(it, at, drop))
        {
            *atOut = at;
            return true;
        }
    }
}

// The upcoming code point at the index, read from the stream when it is
// not yet; false past the end or the lookahead.
static bool Upcoming(CollationIterator* it, uint32_t index)
{
    while (it->upcomingCount <= index && it->upcomingCount < collationLookahead)
    {
        uint32_t c = 0;
        uint8_t k = 0;
        uint32_t from = 0;
        uint32_t at = 0;
        if (!Read(it, &it->nfd, &it->taken, true, &c, &k, &from, &at))
        {
            return false;
        }
        it->upcoming[it->upcomingCount] = c;
        it->upcomingClasses[it->upcomingCount] = k;
        it->upcomingAt[it->upcomingCount] = at;
        it->upcomingFrom[it->upcomingCount++] = from;
    }
    return index < it->upcomingCount;
}

static void Remove(CollationIterator* it, uint32_t index)
{
    MLOC_ASSERT(index < it->upcomingCount);
    for (uint32_t i = index + 1; i < it->upcomingCount; i++)
    {
        it->upcoming[i - 1] = it->upcoming[i];
        it->upcomingClasses[i - 1] = it->upcomingClasses[i];
        it->upcomingFrom[i - 1] = it->upcomingFrom[i];
        it->upcomingAt[i - 1] = it->upcomingAt[i];
    }
    it->upcomingCount--;
}

void mlocCollationIteratorRemember(CollationIterator* it, uint32_t codePoint)
{
    it->history[it->historyAt] = codePoint;
    it->historyAt = (it->historyAt + 1) % collationMaxContext;
    it->historyCount += it->historyCount < collationMaxContext;
}

// A code point's CE32 and the unit that gave it: the tailoring's, or
// the root's where the tailoring falls back, Han's from the radical-stroke
// table when the collator reads one.
static uint32_t Value(const CollationIterator* it, uint32_t codePoint,
                      const CollationRoot** unitOut)
{
    if (it->tailoring != nullptr)
    {
        uint32_t ce32 = mlocTrieValue(&it->tailoring->trie, codePoint);
        if (ce32 != collationFallbackCe32)
        {
            *unitOut = it->tailoring;
            return ce32;
        }
    }
    *unitOut = it->root;
    uint32_t primary = 0;
    if (it->han != nullptr && mlocCollationHanPrimary(it->han, codePoint, &primary))
    {
        // A long primary: the three bytes, the secondary and tertiary
        // common.
        return primary | collationSpecialByte | collationTagLongPrimary;
    }
    return mlocTrieValue(&it->root->trie, codePoint);
}

static bool IsContext(uint32_t ce32, uint32_t tag)
{
    return mlocCe32IsSpecial(ce32) && ce32 != collationImplicitCe32 && mlocCe32Tag(ce32) == tag;
}

// The code point given back steps before the latest, back 0 the latest.
static uint32_t Before(const CollationIterator* it, uint32_t back)
{
    return it->history[(it->historyAt + collationMaxContext - 1 - back) % collationMaxContext];
}

// The CE32 of the longest prefix entry the history spells, or the list's
// default.
static uint32_t MatchPrefix(const CollationIterator* it, const CollationRoot* r, uint32_t number)
{
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

// A match being grown: the code points after the first, where each was
// among the upcoming ones (from the lookahead on, read through the copy)
// and its count in the stream; where the last ends in the source.
typedef struct Match
{
    uint32_t text[collationMaxContext];
    uint32_t from[collationMaxContext];
    uint32_t at[collationMaxContext];
    uint8_t classes[collationMaxContext];
    uint32_t length;
    uint32_t end;
} Match;

// The code points past the lookahead, read from a copy of the stream, so
// that a discontiguous contraction sees past any number of marks, as
// ICU4C's skipped buffer does.
typedef struct Ahead
{
    NfdStream stream;
    uint32_t count;
    bool started;
    // The copy was not read: too many code points wait to be dropped.
    bool limited;
} Ahead;

// The upcoming code point at the index (one more than the last asked)
// and its class: from the lookahead, else from the copy; *atOut its
// count in the stream past the lookahead.
static bool Next(CollationIterator* it, Ahead* a, uint32_t index, uint32_t* c, uint8_t* k,
                 uint32_t* atOut)
{
    if (Upcoming(it, index))
    {
        *c = it->upcoming[index];
        *k = it->upcomingClasses[index];
        *atOut = it->upcomingAt[index];
        return true;
    }
    a->limited = it->upcomingCount == collationLookahead &&
                 it->droppedCount + collationMaxContext > collationMaxDropped;
    if (it->upcomingCount < collationLookahead || a->limited)
    {
        return false;
    }
    if (!a->started)
    {
        a->stream = it->nfd;
        a->count = it->taken;
        a->started = true;
    }
    uint32_t from = 0;
    return Read(it, &a->stream, &a->count, false, c, k, &from, atOut);
}

// Where the upcoming code point at the index, the last one read, ends
// in the source.
static uint32_t EndOf(CollationIterator* it, const Ahead* a, uint32_t index)
{
    if (index < it->upcomingCount)
    {
        return Upcoming(it, index + 1) ? it->upcomingFrom[index + 1] : mlocNfdPeekFrom(&it->nfd);
    }
    return mlocNfdPeekFrom(&a->stream);
}

// Takes a match's code points out: from the lookahead, or by their
// counts past it; the marks it skipped in the lookahead are given from
// its end.
static void TakeOut(CollationIterator* it, const Match* best)
{
    uint32_t last = best->from[best->length - 1];
    if (last >= best->length)
    {
        // Marks skipped: as ICU4C's iterator, the source is read to the
        // end of the last code point matched, and the skipped marks are
        // given from there.
        for (uint32_t i = 0; i < last && i < it->upcomingCount; i++)
        {
            it->upcomingFrom[i] = best->end;
        }
    }
    for (uint32_t j = best->length; j-- > 0;)
    {
        if (best->from[j] < it->upcomingCount)
        {
            Remove(it, best->from[j]);
        }
        else
        {
            // A match reaches past the lookahead only with room left to
            // drop its whole context (the limit set where it was taken).
            MLOC_ASSERT(it->droppedCount < collationMaxDropped);
            it->dropped[it->droppedCount++] = best->at[j];
        }
    }
}

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
// Remembers a scan that stopped at a starter (the count) or at the end:
// the highest class among the code points it leaves (those it skipped
// and those it grew past the best match).
static void RememberScan(CollationIterator* it, uint32_t end, uint8_t skippedMax,
                         const Match* grown, uint32_t kept)
{
    uint8_t high = skippedMax;
    for (uint32_t j = kept; j < grown->length; j++)
    {
        high = grown->classes[j] > high ? grown->classes[j] : high;
    }
    it->scanEnd = end;
    it->scanMax = high;
}

// A contraction scan's state: the match grown and the best one, the copy
// of the stream past the lookahead, the class last skipped and the
// highest, where the scan stopped (0 for nowhere RememberScan keeps) and
// the CE32 found.
typedef struct Scan
{
    Match grown;
    Match best;
    Ahead ahead;
    uint8_t skipped;
    uint8_t skippedMax;
    uint32_t stop;
    uint32_t ce32;
} Scan;

static void Skip(Scan* s, uint8_t k)
{
    s->skippedMax = k > s->skippedMax ? k : s->skippedMax;
}

// One step of the scan over the upcoming code point at the index; false
// when the scan ends.
static bool Step(CollationIterator* it, const CollationRoot* r, uint32_t number, Scan* s,
                 uint32_t i)
{
    uint32_t c = 0;
    uint8_t k = 0;
    uint32_t at = 0;
    if (!Next(it, &s->ahead, i, &c, &k, &at))
    {
        s->stop = s->ahead.limited ? 0 : UINT32_MAX;
        return false;
    }
    if (s->skipped != 0 && at < it->scanEnd && s->skipped >= it->scanMax)
    {
        // Inside the last scan: nothing past here can extend.
        return false;
    }
    if (k == 0 && s->skipped != 0)
    {
        s->stop = at;
        return false;
    }
    if (k != 0 && k <= s->skipped)
    {
        // Blocked by a skipped mark of the same class.
        Skip(s, k);
        return true;
    }
    Match* g = &s->grown;
    g->text[g->length] = c;
    g->at[g->length] = at;
    g->classes[g->length] = k;
    g->from[g->length++] = i;
    bool exact = false;
    uint32_t entry = 0;
    if (!Lookup(r, number, g, &exact, &entry))
    {
        g->length--;
        if (k == 0)
        {
            s->stop = at;
            return false;
        }
        s->skipped = k;
        Skip(s, k);
        return true;
    }
    if (exact)
    {
        s->best = *g;
        s->best.end = EndOf(it, &s->ahead, i);
        s->ce32 = entry;
    }
    return true;
}

static uint32_t MatchContraction(CollationIterator* it, const CollationRoot* r, uint32_t number)
{
    Scan s = {.grown = {.length = 0},
              .best = {.length = 0},
              .ahead = {.started = false, .limited = false},
              .ce32 = mlocCollationContext(r, mlocCollationListStart(r, number))};
    uint32_t i = 0;
    while (s.grown.length < collationMaxContext && Step(it, r, number, &s, i))
    {
        i++;
    }
    if (s.stop != 0 && s.skipped != 0)
    {
        RememberScan(it, s.stop, s.skippedMax, &s.grown, s.best.length);
    }
    if (s.best.length > 0)
    {
        TakeOut(it, &s.best);
    }
    for (uint32_t j = 0; j < s.best.length; j++)
    {
        mlocCollationIteratorRemember(it, s.best.text[j]);
    }
    return s.ce32;
}

// Puts the elements of a CE32 that is not a context in the buffer.
static void Expand(CollationIterator* it, const CollationRoot* r, uint32_t codePoint, uint32_t ce32)
{
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
    // An expansion's elements fit the buffer: the length field holds 31.
    MLOC_ASSERT(length <= collationNumericElements);
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

static uint64_t NumericElement(uint32_t primary)
{
    return (uint64_t)primary << 32 | collationCommonSecondaryTertiary;
}

// A value below 74 + 40 * 254 + 16 * 254 * 254 in one element: one
// byte below 74, two below 74 + 40 * 254, else three.
static void ShortSegment(CollationIterator* it, uint32_t value)
{
    uint32_t lead = it->root->numericPrimary;
    if (value < 74)
    {
        it->ces[0] = NumericElement(lead | (2 + value) << 16);
        return;
    }
    value -= 74;
    if (value < 40 * 254)
    {
        it->ces[0] = NumericElement(lead | (76 + value / 254) << 16 | (2 + value % 254) << 8);
        return;
    }
    value -= 40 * 254;
    uint32_t primary = lead | (2 + value % 254);
    value /= 254;
    primary |= (2 + value % 254) << 8;
    value /= 254;
    it->ces[0] = NumericElement(primary | (116 + value % 254) << 16);
}

// A segment's elements, as ICU4C's appendNumericSegmentCEs: values below
// 74 + 40 * 254 + 16 * 254 * 254 in one element, others a length byte of
// 128 plus the pairs, then the pairs (11 + 2 * pair, the last odd), with
// trailing zero pairs dropped, three bytes an element.
static void NumericSegment(CollationIterator* it, const uint8_t* digits, uint32_t length)
{
    it->ceCount = 1;
    it->ceNext = 0;
    if (length <= 7)
    {
        uint32_t value = 0;
        for (uint32_t i = 0; i < length; i++)
        {
            value = value * 10 + digits[i];
        }
        if (value < 74 + 40 * 254 + 16 * 254 * 254)
        {
            ShortSegment(it, value);
            return;
        }
    }
    uint32_t lead = it->root->numericPrimary;
    uint32_t primary = lead | (128 + (length + 1) / 2) << 16;
    while (digits[length - 1] == 0 && digits[length - 2] == 0)
    {
        length -= 2;
    }
    uint32_t pos = length % 2 == 1 ? 1 : 2;
    uint32_t pair = length % 2 == 1 ? digits[0] : digits[0] * 10U + digits[1];
    uint32_t shift = 8;
    pair = 11 + 2 * pair;
    while (pos < length)
    {
        if (shift == 0)
        {
            // 254 digits are 127 pairs: a length byte and two pairs, then
            // three pairs an element.
            MLOC_ASSERT(it->ceCount < collationNumericElements);
            it->ces[it->ceCount - 1] = NumericElement(primary | pair);
            it->ceCount++;
            primary = lead;
            shift = 16;
        }
        else
        {
            primary |= pair << shift;
            shift -= 8;
        }
        pair = 11 + 2 * (digits[pos] * 10U + digits[pos + 1]);
        pos += 2;
    }
    it->ces[it->ceCount - 1] = NumericElement(primary | (pair - 1) << shift);
}

// A decimal digit's value under numeric, or -1: every digit, its prefix
// or contraction lists passed over, as ICU4C's builder gives each mapped
// digit the digit tag (setDigitTags) and its iterator reads that tag
// before any list.
static int32_t Digit(const CollationIterator* it, uint32_t codePoint)
{
    return it->numeric ? muniGetDecimalDigitValue(codePoint) : -1;
}

// A run of digits from the first, as ICU4C's appendNumericCEs: leading
// zeros dropped (one kept when all are), a segment ending at 254 digits;
// the next segment is the next call's.
static void Numeric(CollationIterator* it, int32_t first)
{
    uint8_t digits[collationNumericDigits];
    uint32_t length = 1;
    digits[0] = (uint8_t)first;
    while (length < collationNumericDigits && Upcoming(it, 0))
    {
        uint32_t c = it->upcoming[0];
        int32_t digit = Digit(it, c);
        if (digit < 0)
        {
            break;
        }
        Remove(it, 0);
        mlocCollationIteratorRemember(it, c);
        if (length == 1 && digits[0] == 0)
        {
            length = 0;
        }
        digits[length++] = (uint8_t)digit;
    }
    NumericSegment(it, digits, length);
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
        int32_t digit = Digit(it, c);
        if (digit >= 0)
        {
            mlocCollationIteratorRemember(it, c);
            Numeric(it, digit);
            *elementOut = it->ces[it->ceNext++];
            return true;
        }
        const CollationRoot* unit = nullptr;
        uint32_t ce32 = Value(it, c, &unit);
        if (IsContext(ce32, collationTagPrefix))
        {
            ce32 = MatchPrefix(it, unit, mlocCe32Index(ce32));
        }
        mlocCollationIteratorRemember(it, c);
        // A list's default or entry naming an earlier list: the validator
        // lets a contraction name only a list before its own.
        while (IsContext(ce32, collationTagContraction))
        {
            uint32_t list = mlocCe32Index(ce32);
            ce32 = MatchContraction(it, unit, list);
            MLOC_ASSERT(!IsContext(ce32, collationTagContraction) || mlocCe32Index(ce32) < list);
        }
        Expand(it, unit, c, ce32);
    }
    MLOC_ASSERT(it->ceNext < it->ceCount);
    *elementOut = it->ces[it->ceNext++];
    return true;
}

bool mlocCollationNextElementAt(CollationIterator* it, uint64_t* elementOut, uint32_t* lowOut,
                                uint32_t* highOut)
{
    if (it->ceNext < it->ceCount)
    {
        *lowOut = it->high;
        *highOut = it->high;
        *elementOut = it->ces[it->ceNext++];
        return true;
    }
    if (!Upcoming(it, 0))
    {
        return false;
    }
    *lowOut = it->upcomingFrom[0];
    (void)mlocCollationNextElement(it, elementOut);
    it->high = Upcoming(it, 0) ? it->upcomingFrom[0] : (uint32_t)it->nfd.length;
    *highOut = it->high;
    return true;
}
