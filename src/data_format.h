// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The data file layout (record mloc-0014), shared by the library, which
// reads it, and the generator, which writes it. All numbers are
// little-endian.

#ifndef MLOC_DATA_FORMAT_H
#define MLOC_DATA_FORMAT_H

#include <stdint.h>

// The first four bytes.
#define MLOC_DATA_MAGIC "MLOC"

enum
{
    dataFormatVersion = 1,
    dataHeaderSize = 64,
    // A unit: kind, locale index, offset, size, each 32 bits.
    dataUnitSize = 16,
    // A locale: the offset and length of its id in the string pool.
    dataLocaleSize = 8,
    // A payload's offset is a multiple of this.
    dataAlignment = 8,
    // The locale index of a unit that belongs to no locale.
    dataNoLocale = 0xFFFFFFFF,
    // The longest locale id a file may hold.
    dataMaxLocaleId = 64,
};

// The header's fields, by byte offset.
enum
{
    headerMagic = 0,
    headerFormat = 4,     // 16 bits
    headerHeaderSize = 6, // 16 bits
    headerFileSize = 8,
    headerFlags = 12, // zero
    headerCldr = 16,  // a version as major << 16 | minor << 8 | patch
    headerUnicode = 20,
    headerLibrary = 24,
    headerReserved = 28, // zero
    headerTzdb = 32,     // eight ASCII bytes, NUL-padded
    headerTables = 40,   // 64 bits
    headerUnitCount = 48,
    headerLocaleCount = 52,
    headerPoolOffset = 56,
    headerPoolSize = 60,
};

// A version packed into the header's 32 bits.
static inline uint32_t mlocPackVersion(uint32_t major, uint32_t minor, uint32_t patch)
{
    return major << 16 | minor << 8 | patch;
}

#endif
