// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "index_build.h"

#include "bytes.h"
#include "collation_compare.h"
#include "collation_iterator.h"
#include "index_value.h"
#include "nfd_stream.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"

#include <string.h>

enum
{
    // A label with U+034F between its code points, or its NFKD, at most.
    scratchBytes = 1024,
    noDisplay = 0xFFFFFF,
    specialUnderflow = 0,
    specialInflow = 1,
    specialOverflow = 2,
};

static const uint8_t s_base[] = {0xEF, 0xB7, 0x90};
static const uint8_t s_ffff[] = {0xEF, 0xBF, 0xBF};
static const uint8_t s_strokes[] = {0xE5, 0x8A, 0x83};

typedef struct Text
{
    const uint8_t* bytes;
    uint32_t length;
} Text;

typedef struct Build
{
    const Collator* c;
    IndexCandidates* in;
    const IndexSpecials* specials;
    // The records and the pool being written, null when counting.
    uint8_t* records;
    uint8_t* pool;
    uint32_t bucketCount;
    uint32_t poolSize;
    uint32_t specialAt[3];
    // ICU4C's hasInvisibleBuckets.
    bool invisible;
    bool pinyin;
    // The ASCII and pinyin buckets of each letter, or -1.
    int32_t asciiBuckets[26];
    int32_t pinyinBuckets[26];
} Build;

static int Compare(const Build* b, Text x, Text y)
{
    return mlocCollationCompareText(&b->c->root, &b->c->settings, mlocNfdUtf8(x.bytes, x.length),
                                    mlocNfdUtf8(y.bytes, y.length));
}

static Text LabelText(const IndexText* t, uint8_t* scratch)
{
    Text text = {0};
    text.bytes = mlocIndexTextBytes(t, scratch, &text.length);
    return text;
}

static bool Starts(Text t, const uint8_t* bytes)
{
    return t.length >= 3 && memcmp(t.bytes, bytes, 3) == 0;
}

static bool Ends(Text t, const uint8_t* bytes)
{
    return t.length >= 3 && memcmp(t.bytes + t.length - 3, bytes, 3) == 0;
}

// Code point order, which UTF-8's bytes keep.
static int Order(Text x, Text y)
{
    uint32_t common = x.length < y.length ? x.length : y.length;
    int order = memcmp(x.bytes, y.bytes, common);
    return order != 0 ? order : (x.length > y.length) - (x.length < y.length);
}

static uint32_t CodePoints(Text t)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < t.length; i++)
    {
        count += (t.bytes[i] & 0xC0) != 0x80;
    }
    return count;
}

// ICU4C's hasMultiplePrimaryWeights: more than one primary above the
// variable top (0 when not shifted) in the text's elements.
static bool MultiPrimary(const Build* b, Text t)
{
    CollationIterator it;
    mlocCollationIteratorStartText(&it, &b->c->root, mlocNfdUtf8(t.bytes, t.length));
    it.tailoring = b->c->settings.tailoring;
    it.han = b->c->settings.han;
    it.numeric = b->c->settings.numeric;
    uint32_t top = b->c->settings.shifted ? b->c->settings.variableTop : 0;
    bool seen = false;
    uint64_t element = 0;
    while (mlocCollationNextElement(&it, &element))
    {
        if ((uint32_t)(element >> 32) > top)
        {
            if (seen)
            {
                return true;
            }
            seen = true;
        }
    }
    return false;
}

static int CompareTexts(const Build* b, const IndexText* x, const IndexText* y)
{
    uint8_t a[indexWordsBytes];
    uint8_t c[indexWordsBytes];
    return Compare(b, LabelText(x, a), LabelText(y, c));
}

// The empty text, whose key has no weights.
static const IndexText s_empty = {.key = {.whole = true}};

static IndexKey Key(const Build* b, Text text)
{
    IndexKey key;
    uint32_t count = 0;
    key.whole =
        mlocCollationPrimaries(&b->c->root, &b->c->settings, mlocNfdUtf8(text.bytes, text.length),
                               key.weights, indexKeyWeights, &count);
    key.count = (uint8_t)count;
    return key;
}

// A text's key, read once so that most compares are of weights alone.
static void Weigh(const Build* b, IndexText* t)
{
    uint8_t scratch[indexWordsBytes];
    t->key = Key(b, LabelText(t, scratch));
}

// Their order at the primary strength: by their keys, else by a compare.
static int Ordered(const Build* b, const IndexText* x, const IndexText* y)
{
    int order = mlocIndexKeyOrder(&x->key, &y->key, b->c->settings.caseLevel);
    return order != indexKeyUnknown ? order : CompareTexts(b, x, y);
}

// firstCharsInScripts_: sorted by the collator, stably (each inserted
// after those equal to it), the leading primary-ignorable ones dropped.
static bool SortScripts(Build* b)
{
    IndexCandidates* in = b->in;
    for (uint32_t i = 0; i < in->scriptCount; i++)
    {
        IndexText script = in->scripts[i];
        Weigh(b, &script);
        uint32_t start = 0;
        uint32_t limit = i;
        while (start < limit)
        {
            uint32_t mid = start + (limit - start) / 2;
            if (Ordered(b, &script, &in->scripts[mid]) < 0)
            {
                limit = mid;
            }
            else
            {
                start = mid + 1;
            }
        }
        memmove(in->scripts + start + 1, in->scripts + start, (i - start) * sizeof(script));
        in->scripts[start] = script;
    }
    uint32_t drop = 0;
    while (drop < in->scriptCount && Ordered(b, &in->scripts[drop], &s_empty) == 0)
    {
        drop++;
    }
    memmove(in->scripts, in->scripts + drop, (in->scriptCount - drop) * sizeof(in->scripts[0]));
    in->scriptCount -= drop;
    return in->scriptCount > 0;
}

// A label's code points joined by U+034F (ICU4C's separated).
static Text Separated(Text t, uint8_t* scratch)
{
    uint32_t length = 0;
    for (uint32_t i = 0; i < t.length; i++)
    {
        if (i > 0 && (t.bytes[i] & 0xC0) != 0x80)
        {
            scratch[length++] = 0xCD;
            scratch[length++] = 0x8F;
        }
        scratch[length++] = t.bytes[i];
    }
    return (Text){scratch, length};
}

static bool Nfkd(Text t, uint8_t* scratch, Text* out)
{
    size_t needed = 0;
    muniTextResult r = muniNormalize((const char*)t.bytes, t.length, muni_nfkd, muni_convertStrict,
                                     (char*)scratch, scratchBytes, &needed);
    *out = (Text){scratch, (uint32_t)needed};
    return r.status == muni_success;
}

// ICU4C's isOneLabelBetterThanOther, for labels equal at the primary
// level: fewer code points in NFKD, then NFKD's code point order, then
// their own. False with the place's label kept when the NFKD does not
// fit (labels the data holds are far shorter).
static bool Better(Text one, Text other)
{
    uint8_t a[scratchBytes];
    uint8_t c[scratchBytes];
    Text n1;
    Text n2;
    if (!Nfkd(one, a, &n1) || !Nfkd(other, c, &n2))
    {
        return false;
    }
    uint32_t c1 = CodePoints(n1);
    uint32_t c2 = CodePoints(n2);
    if (c1 != c2)
    {
        return c1 < c2;
    }
    int order = Order(n1, n2);
    return order != 0 ? order < 0 : Order(one, other) < 0;
}

// ICU4C's binarySearch over the sorted labels: true with the place of
// one equal to the text, else false with where to insert it.
static bool Search(const Build* b, uint32_t size, const IndexText* t, uint32_t* placeOut)
{
    uint32_t start = 0;
    uint32_t limit = size;
    while (start < limit)
    {
        uint32_t i = (start + limit) / 2;
        int order = Ordered(b, t, &b->in->labels[i]);
        if (order == 0)
        {
            *placeOut = i;
            return true;
        }
        if (order < 0)
        {
            limit = i;
        }
        else
        {
            start = i + 1;
        }
    }
    *placeOut = start;
    return false;
}

static bool Multiple(const IndexText* t)
{
    if (t->first != 0)
    {
        return true;
    }
    uint32_t cp = 0;
    size_t size = 0;
    (void)muniDecodeUtf8((const char*)t->bytes, t->length, &cp, &size);
    return size < t->length;
}

// The label ends in one "*" (a multi-code-point label's mark that it is
// kept though it sorts as its parts).
static bool Starred(const IndexText* t)
{
    if (t->first != 0)
    {
        uint32_t last = mlocLoad32(t->bytes + (size_t)(t->length - 1) * 4);
        uint32_t before =
            t->length >= 2 ? mlocLoad32(t->bytes + (size_t)(t->length - 2) * 4) : t->first;
        return last == '*' && before != '*';
    }
    return t->bytes[t->length - 1] == '*' && t->bytes[t->length - 2] != '*';
}

// Whether a candidate is kept, as initLabels keeps one: within the
// scripts, and sorting apart from its parts unless starred (the star
// dropped from the label). Its key is set.
static bool Keep(const Build* b, IndexText* item, const IndexText* first, const IndexText* last)
{
    bool distinct = false;
    if (Multiple(item))
    {
        if (Starred(item))
        {
            item->length--;
        }
        else
        {
            distinct = true;
        }
    }
    Weigh(b, item);
    if (Ordered(b, item, first) < 0 || Ordered(b, item, last) >= 0)
    {
        return false;
    }
    uint8_t scratch[indexWordsBytes];
    uint8_t separated[scratchBytes];
    Text t = LabelText(item, scratch);
    if (distinct && Compare(b, t, Separated(t, separated)) == 0)
    {
        return false;
    }
    // Unshifted, the weights are the primaries above 0; shifted, those
    // above the variable top are counted.
    item->multi =
        b->c->settings.shifted ? MultiPrimary(b, t) : !item->key.whole || item->key.count > 1;
    return true;
}

// ICU4C's cut to maxLabelCount: every n-th label dropped.
static void Thin(IndexCandidates* in, uint32_t maxLabelCount)
{
    if (in->labelCount == 0 || in->labelCount - 1 <= maxLabelCount)
    {
        return;
    }
    uint32_t size = in->labelCount - 1;
    uint32_t count = 0;
    int64_t old = -1;
    uint32_t kept = 0;
    for (uint32_t i = 0; i < in->labelCount; i++)
    {
        ++count;
        int64_t bump = (int64_t)count * maxLabelCount / size;
        if (bump != old)
        {
            old = bump;
            in->labels[kept++] = in->labels[i];
        }
    }
    in->labelCount = kept;
}

// initLabels: the labels sorted in place, each kept or the better of
// those equal to it at the primary level.
static void Filter(Build* b, uint32_t maxLabelCount)
{
    IndexCandidates* in = b->in;
    const IndexText* first = &in->scripts[0];
    const IndexText* last = &in->scripts[in->scriptCount - 1];
    uint32_t size = 0;
    for (uint32_t i = 0; i < in->labelCount; i++)
    {
        IndexText item = in->labels[i];
        if (!Keep(b, &item, first, last))
        {
            continue;
        }
        uint32_t place = 0;
        if (Search(b, size, &item, &place))
        {
            uint8_t scratch[indexWordsBytes];
            uint8_t other[indexWordsBytes];
            if (Better(LabelText(&item, scratch), LabelText(&in->labels[place], other)))
            {
                in->labels[place] = item;
            }
            continue;
        }
        memmove(in->labels + place + 1, in->labels + place, (size - place) * sizeof(item));
        in->labels[place] = item;
        size++;
    }
    in->labelCount = size;
    Thin(in, maxLabelCount);
}

static uint32_t PoolAdd(Build* b, const uint8_t* bytes, uint32_t length)
{
    uint32_t at = b->poolSize;
    if (b->pool != nullptr && length > 0)
    {
        memcpy(b->pool + at, bytes, length);
    }
    b->poolSize += length;
    return at;
}

static uint32_t Field(const Build* b, uint32_t record, uint32_t field)
{
    return mlocLoad32(b->records + (size_t)record * indexRecordSize + (size_t)field * 4);
}

static void SetField(Build* b, uint32_t record, uint32_t field, uint32_t value)
{
    mlocStore32(b->records + (size_t)record * indexRecordSize + (size_t)field * 4, value);
}

static uint32_t AddBucket(Build* b, Text label, uint32_t labelAt, Text boundary,
                          uint32_t boundaryAt, const IndexKey* key, uint32_t type, uint32_t display)
{
    uint32_t r = b->bucketCount++;
    if (b->records != nullptr)
    {
        SetField(b, r, 0, labelAt);
        SetField(b, r, 1, label.length);
        SetField(b, r, 2, boundaryAt);
        SetField(b, r, 3, boundary.length);
        SetField(b, r, 4, display << 8 | type);
        for (uint32_t i = 0; i < indexKeyWeights; i++)
        {
            SetField(b, r, 5 + i, i < key->count ? key->weights[i] : 0);
        }
        SetField(b, r, 5 + indexKeyWeights, key->count | (key->whole ? 1u : 0u) << 8);
    }
    return r;
}

static uint32_t Type(const Build* b, uint32_t record)
{
    return Field(b, record, 4) & 0xFF;
}

static uint32_t Display(const Build* b, uint32_t record)
{
    return Field(b, record, 4) >> 8;
}

static void SetDisplay(Build* b, uint32_t record, uint32_t display)
{
    SetField(b, record, 4, display << 8 | Type(b, record));
}

static Text Special(const Build* b, uint32_t which)
{
    return (Text){b->specials->text[which], b->specials->length[which]};
}

static uint32_t AddSpecial(Build* b, uint32_t which, Text boundary, const IndexKey* key,
                           uint32_t type)
{
    uint32_t at = PoolAdd(b, boundary.bytes, boundary.length);
    return AddBucket(b, Special(b, which), b->specialAt[which], boundary, at, key, type, noDisplay);
}

// fixLabel: a U+FDD0 label shown without it, a stroke count as its
// digits and 劃.
static Text Shown(Build* b, Text current, uint32_t at, uint32_t* labelAt)
{
    *labelAt = at;
    if (!Starts(current, s_base))
    {
        return current;
    }
    uint32_t rest = 0;
    size_t size = 0;
    (void)muniDecodeUtf8((const char*)current.bytes + 3, current.length - 3, &rest, &size);
    if (rest <= 0x2800 || rest > 0x28FF)
    {
        *labelAt = at + 3;
        return (Text){current.bytes + 3, current.length - 3};
    }
    uint32_t count = rest - 0x2800;
    uint8_t digits[6];
    uint32_t n = 0;
    if (count >= 100)
    {
        digits[n++] = (uint8_t)('0' + count / 100);
    }
    if (count >= 10)
    {
        digits[n++] = (uint8_t)('0' + count / 10 % 10);
    }
    digits[n++] = (uint8_t)('0' + count % 10);
    memcpy(digits + n, s_strokes, 3);
    *labelAt = PoolAdd(b, digits, n + 3);
    return (Text){nullptr, n + 3};
}

static Text BoundaryOf(const Build* b, uint32_t record)
{
    return (Text){b->pool + Field(b, record, 2), Field(b, record, 3)};
}

// After a label of several primaries, an invisible bucket bounded by it
// and U+FFFF that shows as the nearest single-primary label bucket
// before it ("Sch￿" to "S"). Counting adds one for every such
// label, the most there can be.
static void Invisible(Build* b, Text current)
{
    Text boundary = {nullptr, current.length + 3};
    if (b->records == nullptr)
    {
        b->poolSize += boundary.length;
        AddBucket(b, (Text){0}, 0, boundary, 0, &s_empty.key, mloc_indexNormal, noDisplay);
        return;
    }
    for (uint32_t j = b->bucketCount - 1; j-- > 0;)
    {
        if (Type(b, j) != mloc_indexNormal)
        {
            return;
        }
        if (Display(b, j) == noDisplay && !MultiPrimary(b, BoundaryOf(b, j)))
        {
            uint32_t at = PoolAdd(b, current.bytes, current.length);
            PoolAdd(b, s_ffff, 3);
            IndexKey key = Key(b, (Text){b->pool + at, boundary.length});
            AddBucket(b, (Text){0}, 0, boundary, at, &key, mloc_indexNormal, j);
            b->invisible = true;
            return;
        }
    }
}

static void AddLabel(Build* b, Text current, const IndexText* item)
{
    uint32_t at = PoolAdd(b, current.bytes, current.length);
    uint32_t labelAt = 0;
    Text label = Shown(b, current, at, &labelAt);
    uint32_t r = AddBucket(b, label, labelAt, current, at, &item->key, mloc_indexNormal, noDisplay);
    if (current.length == 1 && current.bytes[0] >= 'A' && current.bytes[0] <= 'Z')
    {
        b->asciiBuckets[current.bytes[0] - 'A'] = (int32_t)r;
    }
    else if (current.length == 4 && Starts(current, s_base) && current.bytes[3] >= 'A' &&
             current.bytes[3] <= 'Z')
    {
        b->pinyinBuckets[current.bytes[3] - 'A'] = (int32_t)r;
        b->pinyin = true;
    }
    if (!Starts(current, s_base) && !Ends(current, s_ffff) && item->multi)
    {
        Invisible(b, current);
    }
}

// createBucketList's walk over the sorted labels.
static void Emit(Build* b)
{
    IndexCandidates* in = b->in;
    for (uint32_t k = 0; k < 3; k++)
    {
        b->specialAt[k] = PoolAdd(b, b->specials->text[k], b->specials->length[k]);
    }
    AddSpecial(b, specialUnderflow, (Text){nullptr, 0}, &s_empty.key, mloc_indexUnderflow);
    uint32_t scriptIndex = 0;
    // The script boundary above the labels so far, its UTF-8 in scratch
    // (a contraction's always is).
    const IndexText* upperText = &s_empty;
    uint8_t upperScratch[indexWordsBytes] = {0};
    uint8_t inflowScratch[indexWordsBytes];
    Text upper = {nullptr, 0};
    bool started = false;
    for (uint32_t i = 0; i < in->labelCount; i++)
    {
        const IndexText* item = &in->labels[i];
        if (Ordered(b, item, upperText) >= 0)
        {
            memcpy(inflowScratch, upperScratch, sizeof(inflowScratch));
            Text inflow = {inflowScratch, upper.length};
            const IndexKey* inflowKey = &upperText->key;
            bool skipped = false;
            for (;;)
            {
                scriptIndex += started ? 1 : 0;
                started = true;
                upperText = &in->scripts[scriptIndex];
                upper = LabelText(upperText, upperScratch);
                if (Ordered(b, item, upperText) < 0 || scriptIndex + 1 == in->scriptCount)
                {
                    break;
                }
                skipped = true;
            }
            if (skipped && b->bucketCount > 1)
            {
                AddSpecial(b, specialInflow, inflow, inflowKey, mloc_indexInflow);
            }
        }
        uint8_t scratch[indexWordsBytes];
        AddLabel(b, LabelText(item, scratch), item);
    }
    if (b->bucketCount > 1)
    {
        AddSpecial(b, specialOverflow, upper, &upperText->key, mloc_indexOverflow);
    }
}

// The pinyin buckets shown as the ASCII ones, and with invisible
// buckets, an inflow bucket before a special one merged into it.
static void Redirect(Build* b)
{
    int32_t ascii = -1;
    for (uint32_t i = 0; b->pinyin && i < 26; i++)
    {
        ascii = b->asciiBuckets[i] >= 0 ? b->asciiBuckets[i] : ascii;
        if (b->pinyinBuckets[i] >= 0 && ascii >= 0)
        {
            SetDisplay(b, (uint32_t)b->pinyinBuckets[i], (uint32_t)ascii);
            b->invisible = true;
        }
    }
    if (!b->invisible || b->bucketCount < 2)
    {
        return;
    }
    uint32_t next = b->bucketCount - 1;
    for (uint32_t i = b->bucketCount - 1; --i > 0;)
    {
        if (Display(b, i) != noDisplay)
        {
            continue;
        }
        if (Type(b, i) == mloc_indexInflow && Type(b, next) != mloc_indexNormal)
        {
            SetDisplay(b, i, next);
            continue;
        }
        next = i;
    }
}

// The shown buckets' list, each record given the shown bucket it counts
// as; the pool moved down after the list. The size written.
static size_t Finish(Build* b, uint8_t* out)
{
    uint8_t* shown = b->records + (size_t)b->bucketCount * indexRecordSize;
    uint32_t shownCount = 0;
    for (uint32_t r = 0; r < b->bucketCount; r++)
    {
        if (Display(b, r) == noDisplay)
        {
            mlocStore32(shown + (size_t)shownCount * 4, r);
            shownCount++;
        }
    }
    for (uint32_t r = 0; r < b->bucketCount; r++)
    {
        uint32_t target = r;
        for (uint32_t steps = 0; Display(b, target) != noDisplay && steps < b->bucketCount; steps++)
        {
            target = Display(b, target);
        }
        // The shown list rises and starts with the underflow, record 0.
        uint32_t lo = 0;
        uint32_t hi = shownCount;
        while (hi - lo > 1)
        {
            uint32_t mid = lo + (hi - lo) / 2;
            if (mlocLoad32(shown + (size_t)mid * 4) <= target)
            {
                lo = mid;
            }
            else
            {
                hi = mid;
            }
        }
        SetField(b, r, 4, lo << 8 | Type(b, r));
    }
    uint8_t* pool = shown + (size_t)shownCount * 4;
    memmove(pool, b->pool, b->poolSize);
    mlocStore32(out, indexStamp);
    mlocStore32(out + 4, b->bucketCount);
    mlocStore32(out + 8, shownCount);
    mlocStore32(out + 12, b->poolSize);
    memcpy(out + indexHead, b->c, sizeof(Collator));
    return (size_t)(pool - out) + b->poolSize;
}

static void Start(Build* b, const Collator* primary, IndexCandidates* in,
                  const IndexSpecials* specials)
{
    *b = (Build){.c = primary, .in = in, .specials = specials};
    for (uint32_t i = 0; i < 26; i++)
    {
        b->asciiBuckets[i] = -1;
        b->pinyinBuckets[i] = -1;
    }
}

mlocResult mlocIndexBuild(const Collator* primary, IndexCandidates* candidates,
                          const IndexSpecials* specials, uint32_t maxLabelCount, uint8_t* out,
                          size_t capacity, size_t* sizeOut)
{
    Build b;
    Start(&b, primary, candidates, specials);
    if (!SortScripts(&b))
    {
        return mloc_errorData;
    }
    Filter(&b, maxLabelCount);
    Emit(&b);
    size_t records = (size_t)b.bucketCount * (indexRecordSize + 4);
    size_t needed = mlocIndexRecords() + records + b.poolSize;
    *sizeOut = needed;
    if (out == nullptr || capacity < needed)
    {
        return mloc_errorCapacity;
    }
    Start(&b, primary, candidates, specials);
    b.records = out + mlocIndexRecords();
    b.pool = b.records + records;
    Emit(&b);
    Redirect(&b);
    *sizeOut = Finish(&b, out);
    return mloc_success;
}
