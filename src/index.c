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
#include "index_build.h"
#include "index_labels.h"
#include "index_value.h"
#include "nfd_stream.h"

#include "maul-unicode/encoding.h"

static const char s_ellipsis[] = "\xE2\x80\xA6";

// A special label as given, or "…"; false when it is no UTF-8 of at most
// 255 bytes.
static bool SpecialLabel(const char* text, size_t length, const uint8_t** textOut,
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
    return length <= indexMaxLabelBytes && muniValidateUtf8(text, length).status == muni_success;
}

static bool Specials(const mlocIndexOptions* o, IndexSpecials* out)
{
    return SpecialLabel(o->underflowLabel, o->underflowLength, &out->text[0], &out->length[0]) &&
           SpecialLabel(o->inflowLabel, o->inflowLength, &out->text[1], &out->length[1]) &&
           SpecialLabel(o->overflowLabel, o->overflowLength, &out->text[2], &out->length[2]);
}

mlocResult mlocBuildIndex(const mlocCollator* collator, const mlocDataSet* data,
                          const mlocLocale* locale, const mlocIndexOptions* options, void* buffer,
                          size_t capacity, size_t* sizeOut)
{
    mlocIndexOptions o = options != nullptr ? *options : (mlocIndexOptions){0};
    IndexSpecials specials;
    Collator c;
    if (sizeOut == nullptr || data == nullptr || locale == nullptr ||
        (buffer == nullptr && capacity > 0) || !Specials(&o, &specials) ||
        !mlocReadCollator(collator, &c))
    {
        return mloc_errorInvalid;
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
                          buffer, capacity, sizeOut);
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
    *bucketOut = (mlocIndexBucket){
        .label = (const char*)v.pool + mlocIndexField(&v, record, 0),
        .length = mlocIndexField(&v, record, 1),
        .type = (mlocIndexLabelType)(mlocIndexField(&v, record, 4) & 0xFF),
    };
    return mloc_success;
}

// getBucketIndex: the last bucket, shown or not, whose lower boundary
// the name sorts at or after, as the shown bucket it counts as.
static uint32_t Find(const IndexView* v, NfdText name)
{
    // The fast Latin compare where it serves a UTF-8 name (the boundaries
    // are UTF-8), as mlocCompare; else the name's key, read once, against
    // the boundaries' (a script's boundary is a contraction of many, slow
    // to compare); else a compare.
    Collator c;
    (void)mlocReadCollatorHead(v->bytes + indexHead, &c);
    bool fast = c.fastLatin != 0 && name.bytes != nullptr;
    if (fast)
    {
        mlocReadCollatorFast(v->bytes + indexHead, &c);
    }
    bool general = false;
    IndexKey key = {0};
    uint32_t start = 0;
    uint32_t limit = v->bucketCount;
    while (start + 1 < limit)
    {
        uint32_t i = (start + limit) / 2;
        NfdText boundary = mlocNfdUtf8(v->pool + mlocIndexField(v, i, 2), mlocIndexField(v, i, 3));
        int order = fast ? mlocFastLatinCompare(&c.fast, name.bytes, name.length, boundary.bytes,
                                                boundary.length)
                         : fastLatinBail;
        if (order == fastLatinBail)
        {
            if (!general)
            {
                mlocReadCollatorRest(v->bytes + indexHead, &c);
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
    return mlocIndexField(v, start, 4) >> 8;
}

mlocResult mlocFindIndexBucket(const void* index, const char* name, size_t length,
                               uint32_t* bucketOut)
{
    IndexView v;
    if (bucketOut == nullptr || (name == nullptr && length > 0) || !mlocReadIndex(index, &v))
    {
        return mloc_errorInvalid;
    }
    *bucketOut = Find(&v, mlocNfdUtf8((const uint8_t*)(name != nullptr ? name : ""), length));
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
    *bucketOut = Find(&v, mlocNfdUtf16(name != nullptr ? name : s_empty, length));
    return mloc_success;
}
