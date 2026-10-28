// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The family's asset container (docs/internals/data.md#asset-formats),
// which catalogs use: what a host builds once and stores, read by later
// releases. All numbers are little-endian. A 16-byte header: four bytes of
// magic, the format version (16 bits), flags (16 bits), the size in bytes
// and the number of fields. Then the fields, each a 32-bit tag, its
// payload's length (32 bits) and the payload, padded with zero bytes to
// the next multiple of 8 from the asset's start. The fields come in the
// order the format lists its tags, each at most once, and none is empty:
// a writer leaves out a field that holds its default.
//
// A reader takes its own format version alone (before 1.0.0 it changes
// whenever the bytes do), and refuses a flag or a tag it does not know
// with mloc_errorVersion, since only a later version writes one; a field
// the format requires that is absent, or a layout that does not hold, is
// mloc_errorFormat.

#ifndef MLOC_ASSET_H
#define MLOC_ASSET_H

#include "maul-locale/base.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    assetHeaderSize = 16,
    assetMagic = 0,
    assetFormat = 4, // 16 bits
    assetFlags = 6,  // 16 bits
    assetSize = 8,
    assetFields = 12,
    // A field's head: its tag and its payload's length.
    assetFieldHead = 8,
    assetAlignment = 8,
};

// A tag: four ASCII letters read as a little-endian word.
#define MLOC_ASSET_TAG(a, b, c, d)                                                                 \
    ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)

// What a format is: its magic, version, the flags it knows, and its tags
// in order, with the ones it requires.
typedef struct AssetFormat
{
    const char* magic;
    uint32_t version;
    uint32_t flags;
    const uint32_t* tags;
    uint32_t tagCount;
    // A bit per tag, by its place: set when the field is required.
    uint32_t required;
} AssetFormat;

// A field found: its payload's byte offset and length; 0 and 0 when the
// asset leaves it out.
typedef struct AssetField
{
    uint32_t start;
    uint32_t length;
} AssetField;

// The bytes a field of a payload's length takes, its head and padding
// included.
static inline uint64_t mlocAssetFieldBytes(uint64_t length)
{
    return assetFieldHead + ((length + assetAlignment - 1) & ~(uint64_t)(assetAlignment - 1));
}

// Reads an asset's header and fields: fieldsOut gets one entry per tag of
// the format, in its order, and flagsOut the header's flags.
// mloc_success; mloc_errorVersion for another format version or a flag or
// tag the format does not know; mloc_errorFormat for anything else that
// does not hold.
mlocResult mlocReadAsset(const uint8_t* bytes, size_t size, const AssetFormat* format,
                         uint32_t* flagsOut, AssetField* fieldsOut);

#endif
