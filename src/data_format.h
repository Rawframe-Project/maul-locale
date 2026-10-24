// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The data file layout, shared by the library, which reads it, and the
// generator, which writes it. All numbers are little-endian.
//
// A data layout: tools/check_data_format.py watches this header.

#ifndef MLOC_DATA_FORMAT_H
#define MLOC_DATA_FORMAT_H

#include <stdint.h>

// The first four bytes.
#define MLOC_DATA_MAGIC "MLOC"

// Unsigned constants, so that comparisons with the file's 32-bit fields
// keep one type on every compiler (an enum holding 0xFFFFFFFF is signed
// under MSVC's rules).
static const uint32_t dataFormatVersion = 1;
static const uint32_t dataHeaderSize = 64;
// A unit: kind, locale index, offset, size, each 32 bits.
static const uint32_t dataUnitSize = 16;
// A locale: the offset and length of its id in the string pool.
static const uint32_t dataLocaleSize = 8;
// A payload's offset is a multiple of this.
static const uint32_t dataAlignment = 8;
// The locale index of a unit that belongs to no locale.
static const uint32_t dataNoLocale = 0xFFFFFFFF;
// The longest locale id a file may hold.
static const uint32_t dataMaxLocaleId = 64;

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
    headerDataFormat = 24, // MLOC_DATA_FORMAT_VERSION
    headerReserved = 28,   // zero
    headerTzdb = 32,       // eight ASCII bytes, NUL-padded
    headerTables = 40,     // 64 bits
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
