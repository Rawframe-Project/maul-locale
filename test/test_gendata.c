// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generator's data file writer against the library's reader: what the
// writer makes opens, finds every unit it was given, stores identical
// payloads once, and gives the same bytes whatever the order the units
// came in.

#include "data_format.h"
#include "data_view.h"
#include "datafile.h"
#include "test_harness.h"

#include <stdlib.h>
#include <string.h>

static bool ValidAny(const DataView* view, DataSpan payload)
{
    (void)view;
    (void)payload;
    return true;
}

static const DataKind s_kinds[] = {{3, ValidAny}, {5, ValidAny}};

typedef struct Addition
{
    uint32_t kind;
    const char* locale;
    const char* payload;
} Addition;

static const Addition s_additions[] = {
    {5, "tr", "same payload"}, {3, "en", "three en"},      {3, nullptr, "three root"},
    {5, "en", "same payload"}, {3, "sr_Latn", "three sr"}, {5, "de_AT", "five de_AT"},
};

enum
{
    additionCount = sizeof(s_additions) / sizeof(s_additions[0]),
};

// Writes the additions in the order given by an index list.
static uint8_t* Write(const size_t* order, size_t* sizeOut)
{
    DataFile file = {0};
    // Pooled strings first, in one order, as a generator adds them.
    DataFileAddString(&file, "pooled", 6);
    for (size_t i = 0; i < additionCount; i++)
    {
        const Addition* a = &s_additions[order[i]];
        DataFileAdd(&file, a->kind, a->locale, a->payload, strlen(a->payload));
    }
    mlocDataVersion own;
    mlocGetDataVersion(&own);
    DataFileVersion version = {
        .cldr = mlocPackVersion(own.cldr.major, own.cldr.minor, own.cldr.patch),
        .unicode = mlocPackVersion(own.unicode.major, own.unicode.minor, own.unicode.patch),
        .format = own.format,
        .tables = own.tables,
    };
    memcpy(version.tzdb, own.tzdb, sizeof(version.tzdb));
    uint8_t* bytes = DataFileWrite(&file, &version, sizeOut);
    DataFileFree(&file);
    return bytes;
}

static void TestRoundTrip(void)
{
    static const size_t s_forward[additionCount] = {0, 1, 2, 3, 4, 5};
    static const size_t s_backward[additionCount] = {5, 4, 3, 2, 1, 0};
    size_t size = 0;
    size_t otherSize = 0;
    uint8_t* bytes = Write(s_forward, &size);
    uint8_t* other = Write(s_backward, &otherSize);
    CHECK(size == otherSize && memcmp(bytes, other, size) == 0,
          "the same bytes whatever the order of the units");
    DataView view;
    CHECK(mlocOpenDataWithKinds(&view, bytes, size, s_kinds, 2) == mloc_success,
          "the written file opens");
    CHECK(view.localeCount == 4 && view.unitCount == additionCount, "its locales and units");
    for (size_t i = 0; i < additionCount; i++)
    {
        const Addition* a = &s_additions[i];
        uint32_t locale = dataNoLocale;
        bool found = a->locale == nullptr ||
                     mlocFindDataLocale(&view, a->locale, strlen(a->locale), &locale);
        DataSpan span = {0};
        found = found && mlocFindDataUnit(&view, a->kind, locale, &span);
        CHECK(found && span.size == strlen(a->payload) &&
                  memcmp(span.bytes, a->payload, span.size) == 0,
              "every unit is found with its payload");
    }
    uint32_t en = 0;
    uint32_t tr = 0;
    DataSpan first = {0};
    DataSpan second = {0};
    CHECK(mlocFindDataLocale(&view, "en", 2, &en) && mlocFindDataLocale(&view, "tr", 2, &tr) &&
              mlocFindDataUnit(&view, 5, en, &first) && mlocFindDataUnit(&view, 5, tr, &second) &&
              first.bytes == second.bytes,
          "an identical payload is stored once");
    const uint8_t* pool = view.bytes + view.poolOffset;
    CHECK(view.poolSize >= 6 && memcmp(pool, "pooled", 6) == 0, "a pooled string keeps its offset");
    free(bytes);
    free(other);
}

static void TestStrings(void)
{
    DataFile file = {0};
    uint32_t a = DataFileAddString(&file, "abc", 3);
    uint32_t b = DataFileAddString(&file, "de", 2);
    uint32_t again = DataFileAddString(&file, "abc", 3);
    CHECK(a == 0 && b == 3 && again == 0 && file.poolSize == 5, "a string is pooled once");
    DataFileFree(&file);
}

int main(void)
{
    TestRoundTrip();
    TestStrings();
    return s_failures == 0 ? 0 : 1;
}
