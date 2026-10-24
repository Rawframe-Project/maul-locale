// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A data file of root alone, built from chosen units, for the tests that
// give a formatter units which pass validation alone but disagree.

#ifndef MLOC_TEST_PACK_H
#define MLOC_TEST_PACK_H

#include "data_format.h"
#include "data_view.h"

#include "maul-locale/data.h"

#include <string.h>

// A unit of a pack: its kind, its bytes, and whether it is root's (else
// of no locale).
typedef struct PackUnit
{
    uint32_t kind;
    DataSpan bytes;
    bool root;
} PackUnit;

alignas(8) static uint8_t s_pack[131072];

static void PackPut(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// Opens units, in the order of their kinds, as a data file whose one
// locale is root; false when they do not fit or the file is refused.
static bool OpenRootPack(const PackUnit* units, uint32_t count, mlocData* source)
{
    memset(s_pack, 0, sizeof(s_pack));
    uint32_t at = dataHeaderSize + count * dataUnitSize + dataLocaleSize;
    for (uint32_t i = 0; i < count; i++)
    {
        at = (at + 7) / 8 * 8;
        uint8_t* unit = s_pack + dataHeaderSize + i * dataUnitSize;
        PackPut(unit, units[i].kind, 4);
        PackPut(unit + 4, units[i].root ? 0 : dataNoLocale, 4);
        PackPut(unit + 8, at, 4);
        PackPut(unit + 12, units[i].bytes.size, 4);
        if (at + units[i].bytes.size > sizeof(s_pack) - 16)
        {
            return false;
        }
        memcpy(s_pack + at, units[i].bytes.bytes, units[i].bytes.size);
        at += units[i].bytes.size;
    }
    uint32_t pool = at;
    memcpy(s_pack + at, "root", 4);
    at += 4;
    uint8_t* entry = s_pack + dataHeaderSize + count * dataUnitSize;
    PackPut(entry, 0, 4);
    PackPut(entry + 4, 4, 4);
    mlocDataVersion version;
    mlocGetDataVersion(&version);
    memcpy(s_pack, MLOC_DATA_MAGIC, 4);
    PackPut(s_pack + headerFormat, dataFormatVersion, 2);
    PackPut(s_pack + headerHeaderSize, dataHeaderSize, 2);
    PackPut(s_pack + headerFileSize, at, 4);
    PackPut(s_pack + headerCldr,
            mlocPackVersion(version.cldr.major, version.cldr.minor, version.cldr.patch), 4);
    PackPut(s_pack + headerUnicode,
            mlocPackVersion(version.unicode.major, version.unicode.minor, version.unicode.patch),
            4);
    PackPut(s_pack + headerDataFormat, version.format, 4);
    memcpy(s_pack + headerTzdb, version.tzdb, 8);
    PackPut(s_pack + headerTables, version.tables, 8);
    PackPut(s_pack + headerUnitCount, count, 4);
    PackPut(s_pack + headerLocaleCount, 1, 4);
    PackPut(s_pack + headerPoolOffset, pool, 4);
    PackPut(s_pack + headerPoolSize, 4, 4);
    return mlocOpenData(source, s_pack, at) == mloc_success;
}

#endif
