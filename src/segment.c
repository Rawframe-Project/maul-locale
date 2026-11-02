// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Segmentation (maul-locale/segment.h): a locale's tailoring found along
// its segmentation chain, set on Maul Unicode's iterator, and the filters
// over the iterator's breaks (segmenter.h).

#include "bytes.h"
#include "invariant.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "locale_keyword.h"
#include "locale_valid.h"
#include "segment_overrides.h"
#include "segmenter.h"

#include <string.h>

// The first unit along the chain that gives a type.
typedef struct Find
{
    uint32_t type;
    DataSpan unit;
} Find;

static bool TakeType(void* context, DataSpan unit, const DataView* view, int depth)
{
    (void)view;
    (void)depth;
    Find* find = context;
    if ((mlocLoad32(unit.bytes) & find->type) == 0)
    {
        return false;
    }
    find->unit = unit;
    return true;
}

static DataSpan FindType(const mlocDataSet* data, const mlocLocale* locale, uint32_t type)
{
    Find find = {type, {nullptr, 0}};
    if (!mlocVisitLocaleUnits(data, mloc_componentSegmentation, kindSegments, locale, TakeType,
                              &find))
    {
        find.unit = (DataSpan){nullptr, 0};
    }
    return find.unit;
}

static bool KeywordIs(const mlocLocale* locale, const char* key, const char* value)
{
    const char* found = nullptr;
    size_t length = mlocFindKeyword(locale, key, &found);
    return length == strlen(value) && memcmp(found, value, length) == 0;
}

// The overrides a unit's field names, in the code table: Maul Unicode
// reads them in place, and constant memory outlives every copy of a
// segmenter.
static void TakeOverrides(Segmenter* s, uint32_t field)
{
    if (field > 0)
    {
        const SegmentOverrideSet* set = &mlocSegmentOverrideSets[field - 1];
        s->overrides = mlocSegmentOverrides + set->first;
        s->overrideCount = set->count;
    }
}

// The tailoring of a word or sentence segmenter, from its type's unit.
static void TakeUnit(Segmenter* s, DataSpan unit, const mlocLocale* locale)
{
    uint32_t flags = mlocLoad32(unit.bytes + 4);
    if (s->kind == mloc_segmentWord)
    {
        TakeOverrides(s, mlocLoad32(unit.bytes + 8));
        s->filters = (flags & segmentJapaneseWords) != 0 ? segmentJapanese : 0;
        return;
    }
    TakeOverrides(s, mlocLoad32(unit.bytes + 12));
    s->backward = mlocLoad32(unit.bytes + 16);
    s->forward = mlocLoad32(unit.bytes + 20);
    s->values = unit.bytes + segmentHeaderSize;
    s->offsets = s->values + 4 * (size_t)s->backward;
    s->text = s->offsets + 4 * ((size_t)s->backward + s->forward + 1);
    if (s->backward > 0 && KeywordIs(locale, "ss", "standard"))
    {
        s->filters = segmentSuppress;
    }
}

// A line's strictness: the level of CSS's line-break -u-lb names, else
// normal. CLDR's key has strict, normal and loose, not CSS's anywhere.
static muniLineBreakStrictness LineStrictness(const mlocLocale* locale)
{
    return KeywordIs(locale, "lb", "strict")  ? muni_lineBreakStrict
           : KeywordIs(locale, "lb", "loose") ? muni_lineBreakLoose
                                              : muni_lineBreakNormal;
}

// A line's writing system: Chinese or Japanese where CLDR tailors the
// language's lines to take CJ as an ideograph, which it does for ja, zh
// and zh_Hant alone.
static muniWritingSystem LineWritingSystem(const mlocDataSet* data, const mlocLocale* locale)
{
    DataSpan unit = FindType(data, locale, segmentGivesLine);
    return unit.bytes != nullptr && (mlocLoad32(unit.bytes + 4) & segmentCjIdeographic) != 0
               ? muni_writingChineseJapanese
               : muni_writingOther;
}

static muniResult StartLineIterator(Segmenter* s, const mlocDataSet* data, const mlocLocale* locale,
                                    const char* text, size_t length, bool moreFollows)
{
    muniResult r = muniInitLineIterator(&s->iterator, text, length, moreFollows);
    if (r == muni_success)
    {
        r = muniSegmentIterator_SetLineBreakStrictness(&s->iterator, LineStrictness(locale));
    }
    return r == muni_success
               ? muniSegmentIterator_SetWritingSystem(&s->iterator, LineWritingSystem(data, locale))
               : r;
}

static muniResult StartIterator(Segmenter* s, const mlocDataSet* data, const mlocLocale* locale,
                                const char* text, size_t length, bool moreFollows)
{
    muniResult r = muni_errorInvalid;
    if (s->kind == mloc_segmentLine)
    {
        return StartLineIterator(s, data, locale, text, length, moreFollows);
    }
    uint32_t type = s->kind == mloc_segmentWord ? segmentGivesWord : segmentGivesSentence;
    DataSpan unit = FindType(data, locale, type);
    if (unit.bytes != nullptr)
    {
        TakeUnit(s, unit, locale);
    }
    r = s->kind == mloc_segmentWord
            ? muniInitWordIterator(&s->iterator, text, length, moreFollows)
            : muniInitSentenceIterator(&s->iterator, text, length, moreFollows);
    return r == muni_success && s->overrideCount > 0
               ? muniSegmentIterator_SetBreakClassOverrides(&s->iterator, s->overrides,
                                                            s->overrideCount)
               : r;
}

mlocResult mlocInitSegmenter(const mlocDataSet* data, const mlocLocale* locale,
                             mlocSegmentKind kind, const char* text, size_t length,
                             bool moreFollows, mlocSegmenter* segmenterOut)
{
    if (segmenterOut == nullptr || data == nullptr || !mlocLocaleValid(locale) ||
        (text == nullptr && length > 0) ||
        (kind != mloc_segmentWord && kind != mloc_segmentSentence && kind != mloc_segmentLine))
    {
        return mloc_errorInvalid;
    }
    if (!mlocDataSetHasKind(data, kindSegments))
    {
        return mloc_errorNotFound;
    }
    Segmenter s;
    memset(&s, 0, sizeof(s));
    s.kind = (uint8_t)kind;
    if (StartIterator(&s, data, locale, text, length, moreFollows) != muni_success)
    {
        return mloc_errorInvalid;
    }
    s.stamp = segmenterStamp;
    s.moreFollows = moreFollows;
    s.piece = text;
    s.pieceLength = length;
    memcpy(segmenterOut, &s, sizeof(s));
    return mloc_success;
}

static Segmenter* Made(mlocSegmenter* segmenter)
{
    Segmenter* s = (Segmenter*)segmenter;
    return s != nullptr && s->stamp == segmenterStamp ? s : nullptr;
}

mlocResult mlocSegmenter_SetComplexBreaker(mlocSegmenter* segmenter,
                                           mlocComplexBreakFunction breaker, void* context)
{
    Segmenter* s = Made(segmenter);
    if (s == nullptr || s->kind == mloc_segmentSentence || s->started)
    {
        return mloc_errorInvalid;
    }
    return muniSegmentIterator_SetComplexBreaker(&s->iterator, breaker, context) == muni_success
               ? mloc_success
               : mloc_errorInvalid;
}

mlocResult mlocSegmenter_Feed(mlocSegmenter* segmenter, const char* text, size_t length,
                              bool moreFollows)
{
    Segmenter* s = Made(segmenter);
    if (s == nullptr || (text == nullptr && length > 0) || !s->iteratorWaits)
    {
        return mloc_errorInvalid;
    }
    if (muniSegmentIterator_Feed(&s->iterator, text, length, moreFollows) != muni_success)
    {
        return mloc_errorInvalid;
    }
    // The last piece was let go, and pieceStart moved past it, when the
    // iterator asked for this one.
    s->iteratorWaits = false;
    s->moreFollows = moreFollows;
    s->piece = text;
    s->pieceLength = length;
    return mloc_success;
}

// The iterator's next break, into the queue when a filter waits.
static muniResult Pull(Segmenter* s, size_t* offsetOut, bool* mandatoryOut)
{
    *mandatoryOut = false;
    s->busy = true;
    muniResult r = s->kind == mloc_segmentLine
                       ? muniSegmentIterator_NextLineBreak(&s->iterator, offsetOut, mandatoryOut)
                       : muniSegmentIterator_NextBreak(&s->iterator, offsetOut);
    s->busy = false;
    s->iteratorDone = r == muni_done;
    if (r == muni_needMoreText)
    {
        // The piece is let go once the caller is asked for the next one.
        mlocSegmentLeavePiece(s);
        s->iteratorWaits = true;
    }
    return r;
}

static mlocResult Status(muniResult r)
{
    return r == muni_done           ? mloc_done
           : r == muni_needMoreText ? mloc_needMoreText
                                    : mloc_errorInvalid;
}

// Queues a break for the filters. The breaks waiting are within the
// bytes a filter may wait on, so their distances from the first fit.
static void Push(Segmenter* s, size_t offset)
{
    MLOC_ASSERT(s->queueCount < segmentQueue);
    if (s->queueCount == 0)
    {
        s->queueBase = offset;
    }
    MLOC_ASSERT(offset - s->queueBase <= UINT8_MAX);
    s->queue[s->queueCount++] = (uint8_t)(offset - s->queueBase);
}

static void Pop(Segmenter* s)
{
    s->queueCount--;
    if (s->queueCount == 0)
    {
        return;
    }
    uint8_t step = s->queue[1];
    for (uint8_t i = 0; i < s->queueCount; i++)
    {
        s->queue[i] = (uint8_t)(s->queue[i + 1] - step);
    }
    s->queueBase += step;
}

// The first queued break the filters keep, as far as the text at hand
// tells: true with it, false when the queue empties or its first break
// waits for text.
static bool Drain(Segmenter* s, size_t* offsetOut)
{
    while (s->queueCount > 0)
    {
        size_t offset = s->queueBase;
        int keeps = mlocSegmentKeeps(s, offset);
        if (keeps < 0)
        {
            return false;
        }
        Pop(s);
        if (keeps > 0)
        {
            *offsetOut = offset;
            return true;
        }
    }
    return false;
}

// The next break into offsetOut and mandatoryOut, which are the caller's
// locals: written whatever the result.
static mlocResult Next(Segmenter* s, size_t* offsetOut, bool* mandatoryOut)
{
    if (s->filters == 0)
    {
        muniResult r = Pull(s, offsetOut, mandatoryOut);
        return r == muni_success ? mloc_success : Status(r);
    }
    for (;;)
    {
        if (Drain(s, offsetOut))
        {
            return mloc_success;
        }
        if (s->queueCount == 0 && s->iteratorDone)
        {
            return mloc_done;
        }
        if (s->iteratorWaits)
        {
            return mloc_needMoreText;
        }
        // A break waits only for text not yet fed, which the end of the
        // text never leaves it.
        MLOC_ASSERT(!s->iteratorDone);
        size_t offset = 0;
        bool mandatory = false;
        muniResult r = Pull(s, &offset, &mandatory);
        if (r == muni_success)
        {
            Push(s, offset);
        }
        else if (r != muni_done && r != muni_needMoreText)
        {
            return mloc_errorInvalid;
        }
    }
}

mlocResult mlocSegmenter_NextBreak(mlocSegmenter* segmenter, size_t* offsetOut, bool* mandatoryOut)
{
    Segmenter* s = Made(segmenter);
    // A call from the complex breaker is refused before it writes
    // anything: its outputs may be the ones the call it is inside fills.
    if (s != nullptr && s->busy)
    {
        return mloc_errorInvalid;
    }
    if (s == nullptr || offsetOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    s->started = true;
    size_t offset = 0;
    bool mandatory = false;
    mlocResult result = Next(s, &offset, &mandatory);
    // No boundary until one is found: mloc_done and mloc_needMoreText
    // leave none from an earlier call; a failure leaves both outputs.
    if (result != mloc_errorInvalid)
    {
        *offsetOut = result == mloc_success ? offset : 0;
        if (mandatoryOut != nullptr)
        {
            *mandatoryOut = result == mloc_success && mandatory;
        }
    }
    return result;
}
