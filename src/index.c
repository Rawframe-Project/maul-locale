// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The alphabetic index: built over a copy of the collator at primary
// strength (index_build.c), its buckets read and a name's found from the
// value's bytes (index_value.h).

#include "maul-locale/index.h"

#include "collation_compare.h"
#include "collation_fast_compare.h"
#include "collator.h"
#include "data_view.h"
#include "index_build.h"
#include "index_labels.h"
#include "index_value.h"
#include "locale_valid.h"
#include "nfd_stream.h"

#include "maul-unicode/encoding.h"

#include <assert.h>

static_assert(indexMaxLabelBytes == MLOC_MAX_INDEX_LABEL,
              "a special label holds what a label of the data holds");

static const char s_ellipsis[] = "\xE2\x80\xA6";

// A special label as given, or "…"; false when it is no UTF-8 (its length
// is checked apart).
MLOC_NODISCARD static bool SpecialLabel(const char* text, size_t length, const uint8_t** textOut,
                                        uint32_t* lengthOut)
{
    if (text == nullptr)
    {
        *textOut = (const uint8_t*)s_ellipsis;
        *lengthOut = 3;
        return length == 0;
    }
    *textOut = (const uint8_t*)text;
    *lengthOut = (uint32_t)length;
    return muniValidateUtf8(text, length).status == muni_success;
}

MLOC_NODISCARD static bool Specials(const mlocIndexOptions* o, IndexSpecials* out)
{
    return SpecialLabel(o->underflowLabel, o->underflowLength, &out->text[0], &out->length[0]) &&
           SpecialLabel(o->inflowLabel, o->inflowLength, &out->text[1], &out->length[1]) &&
           SpecialLabel(o->overflowLabel, o->overflowLength, &out->text[2], &out->length[2]);
}

mlocResult mlocBuildIndex(const mlocCollator* collator, const mlocDataSet* data,
                          const mlocLocale* locale, const mlocIndexOptions* options,
                          void* bufferOut, size_t capacity, size_t* sizeOut)
{
    mlocIndexOptions o = options != nullptr ? *options : (mlocIndexOptions){0};
    IndexSpecials specials;
    Collator c;
    if (sizeOut == nullptr || !mlocValidDataSet(data) || !mlocLocaleValid(locale) ||
        (bufferOut == nullptr && capacity > 0) || !Specials(&o, &specials) ||
        !mlocReadCollator(collator, &c))
    {
        return mloc_errorInvalid;
    }
    if (o.underflowLength > MLOC_MAX_INDEX_LABEL || o.inflowLength > MLOC_MAX_INDEX_LABEL ||
        o.overflowLength > MLOC_MAX_INDEX_LABEL)
    {
        return mloc_errorLimit;
    }
    // ICU4C's collatorPrimaryOnly_: the collator's settings at primary
    // strength, with the fast Latin options for it.
    c.settings.strength = collationPrimary;
    mlocSetCollatorFastLatin(&c);
    // Not cleared: the gathering sets the counts and each entry it adds.
    IndexCandidates candidates;
    mlocResult r = mlocIndexGather(&c, data, locale, &candidates);
    if (r != mloc_success)
    {
        return r;
    }
    return mlocIndexBuild(&c, &candidates, &specials, o.maxLabelCount > 0 ? o.maxLabelCount : 99,
                          bufferOut, capacity, sizeOut);
}

mlocResult mlocGetIndexBucketCount(const void* index, uint32_t* countOut)
{
    IndexView v;
    if (countOut == nullptr || !mlocReadIndex(index, &v))
    {
        return mloc_errorInvalid;
    }
    *countOut = v.shownCount;
    return mloc_success;
}

mlocResult mlocGetIndexBucket(const void* index, uint32_t bucket, mlocIndexBucket* bucketOut)
{
    IndexView v;
    if (bucketOut == nullptr || !mlocReadIndex(index, &v) || bucket >= v.shownCount)
    {
        return mloc_errorInvalid;
    }
    uint32_t record = mlocLoad32(v.shown + (size_t)bucket * 4);
    const uint8_t* label = nullptr;
    uint32_t type = record < v.bucketCount ? mlocIndexField(&v, record, 4) & 0xFF : UINT32_MAX;
    if (type > mloc_indexOverflow ||
        !mlocIndexSpan(&v, mlocIndexField(&v, record, 0), mlocIndexField(&v, record, 1), &label))
    {
        return mloc_errorInvalid;
    }
    *bucketOut = (mlocIndexBucket){
        .label = (const char*)label,
        .length = mlocIndexField(&v, record, 1),
        .type = (mlocIndexLabelType)type,
    };
    return mloc_success;
}

// getBucketIndex: the last bucket, shown or not, whose lower boundary
// the name sorts at or after, as the shown bucket it counts as; false when
// a record it reads passes the pool or names no shown bucket.
MLOC_NODISCARD static bool Find(const IndexView* v, NfdText name, uint32_t* bucketOut)
{
    // The fast Latin compare where it serves a UTF-8 name (the boundaries
    // are UTF-8), as mlocCollator_Compare; else the name's key, read once, against
    // the boundaries' (a script's boundary is a contraction of many, slow
    // to compare); else a compare.
    Collator c;
    // mlocReadIndex read the head already.
    (void)mlocReadCollatorHead(v->bytes + indexHead, &c);
    bool fast = c.fastLatin != 0 && name.bytes != nullptr;
    if (fast && !mlocReadCollatorFast(v->bytes + indexHead, &c))
    {
        return false;
    }
    bool general = false;
    IndexKey key = {0};
    uint32_t start = 0;
    uint32_t limit = v->bucketCount;
    while (start + 1 < limit)
    {
        uint32_t i = (start + limit) / 2;
        const uint8_t* bytes = nullptr;
        uint32_t length = mlocIndexField(v, i, 3);
        if (!mlocIndexSpan(v, mlocIndexField(v, i, 2), length, &bytes))
        {
            return false;
        }
        NfdText boundary = mlocNfdUtf8(bytes, length);
        int order = fast ? mlocFastLatinCompare(&c.fast, name.bytes, name.length, boundary.bytes,
                                                boundary.length)
                         : fastLatinBail;
        if (order == fastLatinBail)
        {
            if (!general)
            {
                if (!mlocReadCollatorRest(v->bytes + indexHead, &c))
                {
                    return false;
                }
                uint32_t count = 0;
                key.whole = mlocCollationPrimaries(&c.root, &c.settings, name, key.weights,
                                                   indexKeyWeights, &count);
                key.count = (uint8_t)count;
                general = true;
            }
            IndexKey other = mlocIndexBoundaryKey(v, i);
            order = mlocIndexKeyOrder(&key, &other, c.settings.caseLevel);
            if (order == indexKeyUnknown)
            {
                order = mlocCollationCompareText(&c.root, &c.settings, name, boundary);
            }
        }
        if (order < 0)
        {
            limit = i;
        }
        else
        {
            start = i;
        }
    }
    *bucketOut = mlocIndexField(v, start, 4) >> 8;
    return *bucketOut < v->shownCount;
}

mlocResult mlocFindIndexBucket(const void* index, const char* name, size_t length,
                               uint32_t* bucketOut)
{
    IndexView v;
    if (bucketOut == nullptr || (name == nullptr && length > 0) || !mlocReadIndex(index, &v))
    {
        return mloc_errorInvalid;
    }
    uint32_t bucket = 0;
    if (!Find(&v, mlocNfdUtf8((const uint8_t*)(name != nullptr ? name : ""), length), &bucket))
    {
        return mloc_errorInvalid;
    }
    *bucketOut = bucket;
    return mloc_success;
}

mlocResult mlocFindIndexBucketUtf16(const void* index, const uint16_t* name, size_t length,
                                    uint32_t* bucketOut)
{
    static const uint16_t s_empty[1] = {0};
    IndexView v;
    if (bucketOut == nullptr || (name == nullptr && length > 0) || !mlocReadIndex(index, &v))
    {
        return mloc_errorInvalid;
    }
    uint32_t bucket = 0;
    if (!Find(&v, mlocNfdUtf16(name != nullptr ? name : s_empty, length), &bucket))
    {
        return mloc_errorInvalid;
    }
    *bucketOut = bucket;
    return mloc_success;
}
