// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for what making a collator trusts: a collation unit
// that its validator passes. The generated collation file is opened once;
// each input chooses a unit (the root, or a locale's unit of types),
// overwrites bytes inside it, and seals its hash again, so that only the
// values decide. When the unit's validator passes it, as opening the file
// would, the locale's collators are made (its default type and each named
// one) and compare and write sort keys; none may read outside the file.
// The unit is put back after each input.
//
// The input: a byte choosing the unit, then patches of a 32-bit offset
// into the unit, a length of up to eight bytes, and those bytes.

#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"

#include "maul-locale/collate.h"
#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

enum
{
    maxPatches = 16,
};

alignas(8) static uint8_t s_file[1 << 22];
static size_t s_size;
static mlocData s_source;
static DataView s_view;

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static bool Load(void)
{
    static int s_state;
    if (s_state == 0)
    {
        s_state = -1;
        FILE* f = fopen(MLOC_COLLATION_FILE, "rb");
        s_size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
        if (f != nullptr)
        {
            fclose(f);
        }
        if (s_size > 0 && s_size < sizeof(s_file) &&
            mlocOpenData(&s_source, s_file, s_size) == mloc_success)
        {
            s_view = mlocViewOf(&s_source);
            s_state = 1;
        }
    }
    return s_state == 1;
}

// The units of the two collation kinds, in the file's order.
static uint32_t CollationUnits(uint32_t* units, uint32_t capacity)
{
    uint32_t n = 0;
    for (uint32_t u = 0; u < s_view.unitCount && n < capacity; u++)
    {
        uint32_t kind = mlocLoad32(s_file + dataHeaderSize + (size_t)u * dataUnitSize);
        if (kind == kindCollationRoot || kind == kindCollationTailorings)
        {
            units[n++] = u;
        }
    }
    return n;
}

static void Seal(uint8_t* unit, uint32_t size, uint32_t hashAt)
{
    uint64_t hash = mlocCollationUnitHash((DataSpan){unit, size}, hashAt);
    for (int i = 0; i < 8; i++)
    {
        unit[hashAt + i] = (uint8_t)(hash >> (8 * i));
    }
}

// Makes a collator for the tag and uses it; any status is fine.
static void Use(const char* tag, size_t length)
{
    static const char* const s_texts[] = {
        "", "a", "Ab", "\xC3\xA4", "ch", "\xE4\xB8\xAD", "\xEA\xB0\x80", "1 2", "a\xCC\x88", "-x"};
    mlocDataSet set = {&s_source, 1};
    mlocLocale locale;
    mlocCollator collator;
    if (mlocParseLocale(tag, length, &locale).status != mloc_success ||
        mlocInitCollator(&collator, &set, &locale, nullptr) != mloc_success)
    {
        return;
    }
    size_t count = sizeof(s_texts) / sizeof(s_texts[0]);
    for (size_t i = 0; i < count; i++)
    {
        uint8_t key[256];
        size_t keyLength = 0;
        (void)mlocGetSortKey(&collator, s_texts[i], strlen(s_texts[i]), key, sizeof(key),
                             &keyLength);
        for (size_t j = 0; j < count; j++)
        {
            int order = 0;
            Require(mlocCompare(&collator, s_texts[i], strlen(s_texts[i]), s_texts[j],
                                strlen(s_texts[j]), &order) == mloc_success);
        }
    }
}

// The collators of a unit of types: its locale's default and each type.
static void UseTypes(DataSpan unit, uint32_t localeIndex)
{
    const char* id = nullptr;
    size_t length = 0;
    if (localeIndex == dataNoLocale ||
        mlocGetDataLocale(&s_source, localeIndex, &id, &length) != mloc_success || length > 64)
    {
        Use("und", 3);
        return;
    }
    char tag[160];
    memcpy(tag, id, length);
    Use(tag, length);
    CollationTypes t = mlocReadCollationTypes(unit);
    uint32_t name = t.defaultLength;
    for (uint32_t i = 0; i < t.count && i < 8; i++)
    {
        const uint8_t* entry = unit.bytes + collationTypesHead + (size_t)i * collationTypeSize;
        uint32_t nameLength = mlocLoad32(entry + 4);
        if (nameLength <= 64)
        {
            memcpy(tag + length, "-u-co-", 6);
            memcpy(tag + length + 6, t.names + name, nameLength);
            Use(tag, length + 6 + nameLength);
        }
        name += nameLength;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Require(Load());
    if (size < 1)
    {
        return 0;
    }
    uint32_t units[512];
    uint32_t n = CollationUnits(units, 512);
    Require(n > 0);
    const uint8_t* record = s_file + dataHeaderSize + (size_t)units[data[0] % n] * dataUnitSize;
    bool root = mlocLoad32(record) == kindCollationRoot;
    uint32_t localeIndex = mlocLoad32(record + 4);
    uint8_t* unit = s_file + mlocLoad32(record + 8);
    uint32_t unitSize = mlocLoad32(record + 12);
    uint32_t hashAt = root ? collationRootHashAt : collationTypesHashAt;
    // The patches, each with the bytes it covers kept to put back.
    uint32_t at[maxPatches];
    uint8_t length[maxPatches];
    uint8_t saved[maxPatches][8];
    uint8_t savedHash[8];
    memcpy(savedHash, unit + hashAt, 8);
    uint32_t patches = 0;
    for (size_t p = 1; p + 5 <= size && patches < maxPatches;)
    {
        uint32_t offset = (uint32_t)data[p] | (uint32_t)data[p + 1] << 8 |
                          (uint32_t)data[p + 2] << 16 | (uint32_t)data[p + 3] << 24;
        uint8_t n8 = (uint8_t)(data[p + 4] % 9);
        p += 5;
        if (n8 == 0 || p + n8 > size || unitSize < n8)
        {
            break;
        }
        offset %= unitSize - n8 + 1;
        at[patches] = offset;
        length[patches] = n8;
        memcpy(saved[patches], unit + offset, n8);
        memcpy(unit + offset, data + p, n8);
        patches++;
        p += n8;
    }
    Seal(unit, unitSize, hashAt);
    DataSpan span = {unit, unitSize};
    bool valid =
        root ? mlocValidCollationRoot(&s_view, span) : mlocValidCollationTypes(&s_view, span);
    if (valid)
    {
        if (root)
        {
            Use("und", 3);
            Use("de", 2);
        }
        else
        {
            UseTypes(span, localeIndex);
        }
    }
    // Put back in reverse, so that overlapping patches restore the start.
    for (uint32_t i = patches; i-- > 0;)
    {
        memcpy(unit + at[i], saved[i], length[i]);
    }
    memcpy(unit + hashAt, savedHash, 8);
    return 0;
}
