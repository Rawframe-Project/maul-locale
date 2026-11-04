// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An index whose stamp is right but whose bytes were changed after
// mlocBuildIndex wrote them (as for a bound message: the
// index's header is its extent): counts that do not fit each other, a
// collator that is not one or whose bool bytes are forged, a shown entry past the buckets, a label
// or boundary past the pool, a bucket counted as no shown bucket, and a type that is none, are
// refused by every call that reads them; records reversed still end every find; the index as built
// answers, before and after.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "bytes.h"
#include "index_value.h"
#include "test_harness.h"

#include "maul-locale/collate.h"
#include "maul-locale/data.h"
#include "maul-locale/index.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};
alignas(8) static uint8_t s_built[1 << 16];
alignas(8) static uint8_t s_index[1 << 16];
static size_t s_size;

static bool Build(void)
{
    mlocLocale locale;
    mlocCollator collator;
    return mlocParseLocale("en", 2, &locale).status == mloc_success &&
           mlocInitCollator(&s_data, &locale, nullptr, &collator) == mloc_success &&
           mlocBuildIndex(&collator, &s_data, &locale, nullptr, s_built, sizeof(s_built),
                          &s_size) == mloc_success;
}

// Whether each call gives the result on the index: its count, its first
// bucket, and a name found in UTF-8 and UTF-16.
static bool AllGive(mlocResult expected)
{
    static const uint16_t units[] = {'b', 'o', 'b'};
    uint32_t count = 0;
    uint32_t bucket = 0;
    mlocIndexBucket first;
    mlocResult r[] = {
        mlocGetIndexBucketCount(s_index, &count),
        mlocGetIndexBucket(s_index, 0, &first),
        mlocFindIndexBucket(s_index, "bob", 3, &bucket),
        mlocFindIndexBucketUtf16(s_index, units, 3, &bucket),
    };
    bool all = true;
    for (size_t i = 0; i < sizeof(r) / sizeof(r[0]); i++)
    {
        all = all && r[i] == expected;
    }
    return all;
}

static IndexView Fresh(void)
{
    memcpy(s_index, s_built, s_size);
    IndexView v = {0};
    (void)mlocReadIndex(s_index, &v);
    return v;
}

// The pool's size, from the header (offset 12).
static uint32_t PoolSize(void)
{
    return mlocLoad32(s_index + 12);
}

static uint8_t* Field(const IndexView* v, uint32_t record, uint32_t field)
{
    return (uint8_t*)v->records + (size_t)record * indexRecordSize + (size_t)field * 4;
}

// Sets a field of every record.
static void EveryRecord(const IndexView* v, uint32_t field, uint32_t value)
{
    for (uint32_t i = 0; i < v->bucketCount; i++)
    {
        mlocStore32(Field(v, i, field), value);
    }
}

static void TestHeader(void)
{
    IndexView v = Fresh();
    CHECK(v.bucketCount > 1 && AllGive(mloc_success), "the index as built answers");
    v = Fresh();
    mlocStore32(s_index + 4, 0);
    CHECK(AllGive(mloc_errorInvalid), "no buckets is refused");
    v = Fresh();
    mlocStore32(s_index + 8, 0);
    CHECK(AllGive(mloc_errorInvalid), "no shown buckets is refused");
    v = Fresh();
    mlocStore32(s_index + 8, v.bucketCount + 1);
    CHECK(AllGive(mloc_errorInvalid), "more shown buckets than buckets is refused");
    v = Fresh();
    mlocStore32(s_index + indexHead, 0);
    CHECK(AllGive(mloc_errorInvalid), "a collator that is not one is refused");
}

// The collator's bool bytes, which a find reads: a settings bool byte of
// 2 is refused by a find through the general path (UTF-16), a fast Latin
// bool byte of 2 by one through the fast path (UTF-8); a count and a
// bucket read neither.
static void TestCollatorBytes(void)
{
    static const uint16_t units[] = {'b', 'o', 'b'};
    uint32_t bucket = 0;
    uint32_t count = 0;
    (void)Fresh();
    s_index[indexHead + offsetof(Collator, settings.shifted)] = 2;
    CHECK(mlocFindIndexBucketUtf16(s_index, units, 3, &bucket) == mloc_errorInvalid &&
              mlocGetIndexBucketCount(s_index, &count) == mloc_success,
          "a settings bool byte of 2 is refused by a find");
    (void)Fresh();
    s_index[indexHead + offsetof(Collator, fast.backwards)] = 2;
    CHECK(mlocFindIndexBucket(s_index, "bob", 3, &bucket) == mloc_errorInvalid &&
              mlocGetIndexBucketCount(s_index, &count) == mloc_success,
          "a fast Latin bool byte of 2 is refused by a find");
    (void)Fresh();
    CHECK(AllGive(mloc_success), "put back, the index answers again");
}

static void TestRecords(void)
{
    mlocIndexBucket bucket;
    uint32_t found = 0;
    IndexView v = Fresh();
    mlocStore32((uint8_t*)v.shown, v.bucketCount);
    CHECK(mlocGetIndexBucket(s_index, 0, &bucket) == mloc_errorInvalid,
          "a shown entry past the buckets is refused");
    v = Fresh();
    mlocStore32(Field(&v, mlocLoad32(v.shown), 0), PoolSize());
    CHECK(mlocGetIndexBucket(s_index, 0, &bucket) == mloc_errorInvalid,
          "a label past the pool is refused");
    v = Fresh();
    mlocStore32(Field(&v, mlocLoad32(v.shown), 1), UINT32_MAX);
    CHECK(mlocGetIndexBucket(s_index, 0, &bucket) == mloc_errorInvalid,
          "a label longer than the pool is refused");
    v = Fresh();
    mlocStore32(Field(&v, mlocLoad32(v.shown), 4), mloc_indexOverflow + 1);
    CHECK(mlocGetIndexBucket(s_index, 0, &bucket) == mloc_errorInvalid,
          "a type that is none is refused");
    v = Fresh();
    EveryRecord(&v, 2, PoolSize());
    CHECK(mlocFindIndexBucket(s_index, "bob", 3, &found) == mloc_errorInvalid,
          "a boundary past the pool is refused");
    v = Fresh();
    EveryRecord(&v, 4, v.shownCount << 8);
    CHECK(mlocFindIndexBucket(s_index, "bob", 3, &found) == mloc_errorInvalid,
          "a bucket counted as no shown bucket is refused");
    v = Fresh();
    mlocStore32(Field(&v, mlocLoad32(v.shown), 0), PoolSize());
    mlocStore32(Field(&v, mlocLoad32(v.shown), 1), 0);
    CHECK(mlocGetIndexBucket(s_index, 0, &bucket) == mloc_success && bucket.length == 0,
          "an empty label at the pool's end is read");
    (void)Fresh();
    CHECK(AllGive(mloc_success), "put back, the index answers again");
}

// Records whose order was changed: the search halves its range whatever a
// compare answers, so every find ends, with a shown bucket or a refusal.
static void TestReversed(void)
{
    IndexView v = Fresh();
    uint8_t record[indexRecordSize];
    for (uint32_t i = 0, j = v.bucketCount - 1; i < j; i++, j--)
    {
        memcpy(record, Field(&v, i, 0), indexRecordSize);
        memcpy(Field(&v, i, 0), Field(&v, j, 0), indexRecordSize);
        memcpy(Field(&v, j, 0), record, indexRecordSize);
    }
    static const char* const names[] = {"", "a", "bob", "Zoe", "\xCE\xB1", "\xE4\xB8\x80", "9"};
    bool ended = true;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        uint32_t found = UINT32_MAX;
        mlocResult r = mlocFindIndexBucket(s_index, names[i], strlen(names[i]), &found);
        ended = ended && ((r == mloc_success && found < v.shownCount) || r == mloc_errorInvalid);
    }
    CHECK(ended, "with the records reversed, every find ends in a shown bucket or a refusal");
}

int main(void)
{
    FILE* f = fopen(MLOC_COLLATION_FILE, "rb");
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocOpenBuiltinData(&s_sources[0]);
    if (size == 0 || size == sizeof(s_file) ||
        mlocOpenData(s_file, size, &s_sources[1]) != mloc_success || !Build())
    {
        fputs("test_forged_index: cannot open " MLOC_COLLATION_FILE " or build\n", stderr);
        return 2;
    }
    TestHeader();
    TestCollatorBytes();
    TestRecords();
    TestReversed();
    return s_failures == 0 ? 0 : 1;
}
