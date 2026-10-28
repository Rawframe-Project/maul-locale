// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Reading the asset container (asset.h).

#include "asset.h"

#include "bytes.h"

#include <stdbool.h>
#include <string.h>

// A tag's place among the format's, or tagCount for one it does not know.
static uint32_t PlaceOf(const AssetFormat* format, uint32_t tag)
{
    uint32_t place = 0;
    while (place < format->tagCount && format->tags[place] != tag)
    {
        place++;
    }
    return place;
}

// Whether the bytes from at to end are zero.
static bool Zeros(const uint8_t* bytes, uint64_t at, uint64_t end)
{
    for (; at < end; at++)
    {
        if (bytes[at] != 0)
        {
            return false;
        }
    }
    return true;
}

// The header: the magic, then the format version and the flags, which
// another version may give another meaning, before the size, which it
// may place elsewhere.
static mlocResult ReadHeader(const uint8_t* bytes, size_t size, const AssetFormat* format)
{
    if (size < assetHeaderSize || memcmp(bytes, format->magic, 4) != 0)
    {
        return mloc_errorFormat;
    }
    if (mlocLoad16(bytes + assetFormat) != format->version ||
        (mlocLoad16(bytes + assetFlags) & ~format->flags) != 0)
    {
        return mloc_errorVersion;
    }
    return mlocLoad32(bytes + assetSize) == size ? mloc_success : mloc_errorFormat;
}

mlocResult mlocReadAsset(const uint8_t* bytes, size_t size, const AssetFormat* format,
                         uint32_t* flagsOut, AssetField* fieldsOut)
{
    mlocResult result = ReadHeader(bytes, size, format);
    if (result != mloc_success)
    {
        return result;
    }
    memset(fieldsOut, 0, sizeof *fieldsOut * format->tagCount);
    uint32_t fields = mlocLoad32(bytes + assetFields);
    uint64_t at = assetHeaderSize;
    uint32_t next = 0;
    uint32_t present = 0;
    // Each field takes 8 bytes or more, so the walk ends within the size.
    for (uint32_t i = 0; i < fields; i++)
    {
        if (size - at < assetFieldHead)
        {
            return mloc_errorFormat;
        }
        uint32_t place = PlaceOf(format, mlocLoad32(bytes + at));
        uint32_t length = mlocLoad32(bytes + at + 4);
        if (place == format->tagCount)
        {
            return mloc_errorVersion;
        }
        uint64_t payload = at + assetFieldHead;
        uint64_t end = at + mlocAssetFieldBytes(length);
        if (place < next || length == 0 || end > size || !Zeros(bytes, payload + length, end))
        {
            return mloc_errorFormat;
        }
        fieldsOut[place] = (AssetField){(uint32_t)payload, length};
        present |= 1u << place;
        next = place + 1;
        at = end;
    }
    if (at != size || (format->required & ~present) != 0)
    {
        return mloc_errorFormat;
    }
    *flagsOut = mlocLoad16(bytes + assetFlags);
    return mloc_success;
}
