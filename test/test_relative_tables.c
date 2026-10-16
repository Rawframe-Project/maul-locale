// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The relative time data file's units (relative_tables.h): a unit built
// here, valid, and copies with one field spoiled; then relative.mloc:
// every unit valid, and a few locales' names and patterns as CLDR gives
// them, read along the chain as the library reads them.

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "relative_tables.h"
#include "test_harness.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// The key of a unit's width's slot.
static uint32_t Key(uint32_t unit, uint32_t width, uint32_t slot)
{
    return (unit * relativeWidthCount + width) * relativeSlots + slot;
}

enum
{
    // The unit built here: three entries, the day's long "today" and its
    // future "other", and the second's narrow past "one", which shares
    // the day's pattern.
    builtCount = 3,
    builtStrings = 2,
    builtKeys = relativeHeaderSize,
    builtIndexes = builtKeys + 2 * builtCount,
    builtOffsets = builtIndexes + 2 * builtCount,
    builtText = builtOffsets + 4 * (builtStrings + 1),
};

static const char s_builtText[] = "todayin {0} days";

alignas(8) static uint8_t s_unit[256];

static uint32_t BuildUnit(void)
{
    memset(s_unit, 0, sizeof(s_unit));
    uint32_t textSize = (uint32_t)strlen(s_builtText);
    Put(s_unit, builtCount, 4);
    Put(s_unit + 4, builtStrings, 4);
    Put(s_unit + 8, textSize, 4);
    Put(s_unit + builtKeys, Key(relativeDay, 0, 2), 2);
    Put(s_unit + builtKeys + 2, Key(relativeDay, 0, relativeFuture + 5), 2);
    Put(s_unit + builtKeys + 4, Key(relativeSecond, 2, relativePast + 1), 2);
    Put(s_unit + builtIndexes + 2, 1, 2);
    Put(s_unit + builtIndexes + 4, 1, 2);
    Put(s_unit + builtOffsets + 4, 5, 4);
    Put(s_unit + builtOffsets + 8, textSize, 4);
    memcpy(s_unit + builtText, s_builtText, textSize);
    return builtText + textSize;
}

static bool Valid(uint32_t size)
{
    DataView view = {0};
    return mlocValidRelative(&view, (DataSpan){s_unit, size});
}

// Whether the unit with bytes at an offset spoiled is refused.
static bool Refuses(uint32_t at, uint64_t value, int bytes)
{
    uint32_t size = BuildUnit();
    Put(s_unit + at, value, bytes);
    return !Valid(size);
}

// Whether the unit with its text replaced (the same length) is refused.
static bool TextRefused(const char* text)
{
    uint32_t size = BuildUnit();
    memcpy(s_unit + builtText, text, strlen(s_builtText));
    return !Valid(size);
}

static void TestUnit(void)
{
    uint32_t size = BuildUnit();
    CHECK(Valid(size), "the unit built here");
    CHECK(!Valid(size - 1) && !Valid(relativeHeaderSize + 3), "a short unit");
    alignas(8) static const uint8_t s_empty[relativeHeaderSize + 4] = {0};
    DataView view = {0};
    CHECK(mlocValidRelative(&view, (DataSpan){s_empty, sizeof(s_empty)}), "a unit of no entries");
    CHECK(Refuses(0, relativeKeyCount + 1, 4) && Refuses(4, relativeKeyCount + 1, 4) &&
              Refuses(8, 15, 4),
          "the counts");
    CHECK(Refuses(builtOffsets, 1, 4) && Refuses(builtOffsets + 8, 15, 4),
          "the offsets' first and last");
    CHECK(Refuses(builtOffsets + 4, 17, 4), "offsets out of order");
    CHECK(Refuses(builtKeys + 2, Key(relativeDay, 0, 2), 2) &&
              Refuses(builtKeys + 2, Key(relativeDay, 0, 1), 2),
          "keys not ascending");
    CHECK(Refuses(builtKeys + 4, relativeKeyCount, 2), "a key past the layout");
    CHECK(Refuses(builtIndexes + 4, builtStrings, 2), "a string past the strings");
    CHECK(Refuses(builtIndexes, 1, 2), "a name given a pattern");
    CHECK(TextRefused("t\x01"
                      "dayin {0} days") &&
              TextRefused("today{0} {0}days"),
          "a control byte, a pattern's {0} twice");
    CHECK(!TextRefused("todayin one days") && TextRefused("{0}ayin {0} days"),
          "a pattern without {0} (ar's \"in one year\"), a name with it");
}

static mlocData s_sources[2];
static mlocDataSet s_data;

static bool OpenFile(const char* path, uint8_t* buffer, size_t capacity, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    size_t read = file == nullptr ? 0 : fread(buffer, 1, capacity, file);
    if (file != nullptr)
    {
        fclose(file);
    }
    return read > 0 && read < capacity && mlocOpenData(out, buffer, read) == mloc_success;
}

// A unit's string at a key: true when the unit files the key.
static bool Filed(DataSpan unit, uint32_t key, const char** text, size_t* length)
{
    uint32_t count = mlocLoad32(unit.bytes);
    uint32_t stringCount = mlocLoad32(unit.bytes + 4);
    const uint8_t* keys = unit.bytes + relativeHeaderSize;
    const uint8_t* strings = keys + 2 * (size_t)count;
    const uint8_t* offsets = strings + 2 * (size_t)count;
    for (uint32_t i = 0; i < count; i++)
    {
        if (mlocLoad16(keys + 2 * (size_t)i) == key)
        {
            uint32_t string = mlocLoad16(strings + 2 * (size_t)i);
            uint32_t start = mlocLoad32(offsets + 4 * (size_t)string);
            *length = mlocLoad32(offsets + 4 * (size_t)string + 4) - start;
            *text = (const char*)offsets + 4 * ((size_t)stringCount + 1) + start;
            return true;
        }
    }
    return false;
}

// The library's lookup along a chain of locale ids: the key's width,
// else the next wider one's; null for none or an empty string.
static const char* Lookup(const char* const* chain, uint32_t key, size_t* length)
{
    uint32_t width = key / relativeSlots % relativeWidthCount;
    for (uint32_t w = 0; w <= width; w++)
    {
        for (const char* const* id = chain; *id != nullptr; id++)
        {
            DataSpan unit;
            const char* text = nullptr;
            if (mlocFindDataSetUnit(&s_data, kindRelative, *id, strlen(*id), &unit) &&
                Filed(unit, key - w * relativeSlots, &text, length))
            {
                return *length == 0 ? nullptr : text;
            }
        }
    }
    *length = 0;
    return nullptr;
}

static bool Is(const char* const* chain, uint32_t key, const char* want)
{
    size_t length = 0;
    const char* text = Lookup(chain, key, &length);
    bool same = want == nullptr
                    ? text == nullptr
                    : text != nullptr && length == strlen(want) && memcmp(text, want, length) == 0;
    if (!same)
    {
        printf("  key %u: got \"%.*s\", want \"%s\"\n", key, (int)length,
               text != nullptr ? text : "", want != nullptr ? want : "(none)");
    }
    return same;
}

static void TestFile(void)
{
    static const char* const s_en[] = {"en", "root", nullptr};
    static const char* const s_es[] = {"es", "root", nullptr};
    static const char* const s_gb[] = {"en_GB", "en_001", "en", "root", nullptr};
    CHECK(Is(s_en, Key(relativeDay, 0, 1), "yesterday") &&
              Is(s_en, Key(relativeDay, 0, 3), "tomorrow") &&
              Is(s_en, Key(relativeDay, 0, 0), nullptr),
          "en's day names, no -2");
    CHECK(Is(s_en, Key(relativeDay, 0, relativeFuture + 1), "in {0} day") &&
              Is(s_en, Key(relativeDay, 0, relativePast + 5), "{0} days ago") &&
              Is(s_en, Key(relativeDay, 0, relativeFuture + 3), nullptr),
          "en's day patterns, none for few");
    CHECK(Is(s_es, Key(relativeDay, 0, 0), "anteayer") &&
              Is(s_es, Key(relativeDay, 0, 4), "pasado mañana"),
          "es's -2 and 2");
    CHECK(Is(s_en, Key(relativeSecond, 0, 2), "now") &&
              Is(s_en, Key(relativeSunday + 1, 0, 3), "next Monday"),
          "en's now and next Monday");
    CHECK(Is(s_en, Key(relativeYear, 2, relativePast + 5), "{0}y ago") &&
              Is(s_gb, Key(relativeYear, 2, relativePast + 5), "{0} yr ago") &&
              Is(s_gb, Key(relativeYear, 1, relativePast + 5), "{0} yr ago") &&
              Is(s_gb, Key(relativeYear, 0, relativePast + 5), "{0} years ago"),
          "en's narrow year, and en_GB's widths along its chain (en_001's)");
}

int main(void)
{
    TestUnit();
    alignas(8) static uint8_t s_file[1 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    s_data = (mlocDataSet){s_sources, 2};
    if (!OpenFile(MLOC_RELATIVE_FILE, s_file, sizeof(s_file), &s_sources[1]))
    {
        fputs("test_relative_tables: cannot open " MLOC_RELATIVE_FILE "\n", stderr);
        return 2;
    }
    TestFile();
    return s_failures == 0 ? 0 : 1;
}
