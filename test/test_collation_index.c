// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The index labels in the data (record mloc-0075) on the CLDR fixture
// (test/cldr/common): an index set taken as written, a standard set made
// into labels (a to z, Hangul and Ethiopic cut, upper-cased), an
// unconfirmed index set passed over, and locales that match the next
// keyed one filing nothing; the set pattern reader's escapes, ranges and
// strings; the validator refusing crafted units.
// conform_index_labels checks every CLDR locale against ICU4C's sets.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "cldr.h"
#include "collation_index.h"
#include "datafile.h"
#include "index_gen.h"
#include "keys.h"
#include "kinds.h"
#include "set_pattern.h"
#include "table_gen.h"
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Cldr s_cldr;

typedef struct Read
{
    char text[256];
    size_t length;
} Read;

static void Append(Read* r, const char* text)
{
    size_t n = strlen(text);
    if (r->length + n < sizeof(r->text))
    {
        memcpy(r->text + r->length, text, n + 1);
        r->length += n;
    }
}

static void TakeRange(void* context, uint32_t first, uint32_t last)
{
    char item[32];
    snprintf(item, sizeof(item), "%X-%X ", (unsigned)first, (unsigned)last);
    Append(context, item);
}

static void TakeString(void* context, const char* text, size_t length)
{
    char item[64];
    snprintf(item, sizeof(item), "{%.*s} ", (int)length, text);
    Append(context, item);
}

static bool Reads(const char* pattern, const char* expected)
{
    Read r = {0};
    GenReadSetPattern(pattern, &(SetSink){TakeRange, TakeString, &r});
    return strcmp(r.text, expected) == 0;
}

static void TestPattern(void)
{
    CHECK(Reads("[a \\u0062 e-g \\- z]", "61-61 62-62 65-67 2D-2D 7A-7A "),
          "characters, a \\u escape, a range and an escaped hyphen");
    CHECK(Reads("[{c\\:d} {x\\}y} \\U0001F600 - ]", "{c:d} {x}y} 1F600-1F600 2D-2D "),
          "strings with escapes, a \\U escape and a hyphen at the end");
    CHECK(Reads("[- a - c]", "2D-2D 61-63 "), "a hyphen first, a range written with spaces");
    CHECK(Reads("[a {bc} d]", "61-61 {bc} 64-64 "), "a character before a string kept");
    CHECK(Reads("[]", ""), "the empty set");
}

// The unit filed under a key, or null.
static const DataUnit* UnitOf(const DataFile* file, const char* key)
{
    for (size_t i = 0; i < file->unitCount; i++)
    {
        if (file->units[i].kind == kindCollationIndex && strcmp(file->units[i].locale, key) == 0)
        {
            return &file->units[i];
        }
    }
    return nullptr;
}

// Whether a key's unit holds these labels, given as UTF-8 apart by
// spaces, and validates.
static bool Holds(const DataFile* file, const char* key, const char* labels)
{
    const DataUnit* unit = UnitOf(file, key);
    if (unit == nullptr ||
        !mlocValidIndexLabels(nullptr, (DataSpan){unit->payload, (uint32_t)unit->size}))
    {
        return false;
    }
    IndexLabels l = {unit->payload, (uint32_t)unit->size, 0};
    char joined[512] = "";
    size_t used = 0;
    uint32_t at = 4;
    const uint8_t* text = nullptr;
    uint32_t length = 0;
    uint32_t count = 0;
    while (mlocNextIndexLabel(&l, &at, &text, &length) && used + length + 1 < sizeof(joined))
    {
        if (used > 0)
        {
            joined[used++] = ' ';
        }
        memcpy(joined + used, text, length);
        used += length;
        joined[used] = '\0';
        count++;
    }
    uint32_t stored = (uint32_t)unit->payload[0] | (uint32_t)unit->payload[1] << 8 |
                      (uint32_t)unit->payload[2] << 16 | (uint32_t)unit->payload[3] << 24;
    return stored == count && strcmp(joined, labels) == 0;
}

static void TestUnits(const char* commonDirectory)
{
    char error[256] = "";
    Keys keys;
    if (!KeysOpen(&keys, commonDirectory, error, sizeof(error)) ||
        !CldrOpen(&s_cldr, commonDirectory, error, sizeof(error)))
    {
        CHECK(false, error);
        return;
    }
    int count = 0;
    char** ids = GenListXml(commonDirectory, "main", &count);
    DataFile file = {0};
    GenerateIndexUnits(&(IndexInput){&s_cldr, &keys, (const char* const*)ids, count}, &file);
    CHECK(Holds(&file, "root", "A B C D E F G H I J K L M N O P Q R S T U V W X Y Z"),
          "root's empty standard set makes A to Z");
    CHECK(Holds(&file, "xx", "A B C CH D E F \xC4\x90"),
          "an index set as written, its string, escape and range, in code point order");
    CHECK(Holds(&file, "xx_Cyrl",
                "SS \xD0\x90 \xD0\x94\xD0\x97 \xD0\x96 \xE1\x88\x80 \xE1\x88\x88 \xEA\xB0\x80 "
                "\xEB\x82\x98 \xEB\x8B\xA4 \xEB\x9D\xBC \xEB\xA7\x88 \xEB\xB0\x94 \xEC\x82\xAC "
                "\xEC\x95\x84 \xEC\x9E\x90 \xEC\xB0\xA8 \xEC\xB9\xB4 \xED\x83\x80 \xED\x8C\x8C "
                "\xED\x95\x98"),
          "a standard set made into labels: upper-cased in full (ß to SS), the 14 Hangul "
          "initials for any syllable, the Ethiopic bases it holds, no a to z");
    CHECK(UnitOf(&file, "xx_YY") == nullptr && UnitOf(&file, "xx_ZZ") == nullptr &&
              file.unitCount == 3,
          "an unconfirmed index set passed over, and sets equal to the next keyed one's not "
          "filed");
    DataFileFree(&file);
    GenFreeList(ids, count);
    CldrClose(&s_cldr);
    KeysClose(&keys);
}

static bool Valid(const uint8_t* bytes, uint32_t size)
{
    return mlocValidIndexLabels(nullptr, (DataSpan){bytes, size});
}

static void TestValidator(void)
{
    static const uint8_t s_good[] = {2, 0, 0, 0, 1, 'A', 2, 'A', 'B'};
    static const uint8_t s_empty[] = {0, 0, 0, 0};
    CHECK(Valid(s_good, sizeof(s_good)) && Valid(s_empty, sizeof(s_empty)),
          "two labels in order, and none");
    static const uint8_t s_counted[] = {3, 0, 0, 0, 1, 'A', 2, 'A', 'B'};
    static const uint8_t s_short[] = {1, 0, 0, 0, 1, 'A', 1};
    static const uint8_t s_zero[] = {2, 0, 0, 0, 1, 'A', 0};
    static const uint8_t s_past[] = {1, 0, 0, 0, 3, 'A', 'B'};
    static const uint8_t s_ill[] = {1, 0, 0, 0, 2, 0xC0, 0x80};
    static const uint8_t s_order[] = {2, 0, 0, 0, 1, 'B', 1, 'A'};
    static const uint8_t s_twice[] = {2, 0, 0, 0, 1, 'A', 1, 'A'};
    static const uint8_t s_prefix[] = {2, 0, 0, 0, 2, 'A', 'B', 1, 'A'};
    static const uint8_t s_head[] = {0, 0, 0};
    CHECK(!Valid(s_counted, sizeof(s_counted)) && !Valid(s_short, sizeof(s_short)) &&
              !Valid(s_zero, sizeof(s_zero)) && !Valid(s_past, sizeof(s_past)) &&
              !Valid(s_ill, sizeof(s_ill)) && !Valid(s_order, sizeof(s_order)) &&
              !Valid(s_twice, sizeof(s_twice)) && !Valid(s_prefix, sizeof(s_prefix)) &&
              !Valid(s_head, sizeof(s_head)),
          "a count past the labels, bytes past the count, a zero length, a length past the "
          "end, ill-formed UTF-8, labels out of order, twice or after a longer one, no count");
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fputs("usage: test_collation_index <CLDR fixture common directory>\n", stderr);
        return 2;
    }
    TestPattern();
    TestUnits(argv[1]);
    TestValidator();
    return s_failures == 0 ? 0 : 1;
}
