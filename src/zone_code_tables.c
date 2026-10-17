// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone codes unit (zone_code_tables.h): its sections, its validator,
// and its lookups.

#include "zone_code_tables.h"

#include "bytes.h"
#include "kinds.h"

static uint64_t Align(uint64_t at)
{
    return (at + 7) & ~(uint64_t)7;
}

bool mlocZoneCodeTablesOf(DataSpan payload, ZoneCodeTables* out)
{
    if (payload.size < zoneCodeHeaderSize)
    {
        return false;
    }
    uint32_t counts[zoneCodeSectionCount];
    for (int i = 0; i < zoneCodeSectionCount; i++)
    {
        counts[i] = mlocLoad32(payload.bytes + 4 * i);
    }
    static const uint32_t s_sizes[zoneCodeSectionCount] = {zoneCodeSize, zoneAliasSize,
                                                           zoneWindowsSize, zoneMappingSize, 1};
    const uint8_t** sections[zoneCodeSectionCount] = {&out->codes, &out->aliases, &out->windows,
                                                      &out->mappings, &out->text};
    uint64_t at = zoneCodeHeaderSize;
    for (int i = 0; i < zoneCodeSectionCount; i++)
    {
        at = Align(at);
        *sections[i] = payload.bytes + (at <= payload.size ? at : 0);
        at += (uint64_t)counts[i] * s_sizes[i];
    }
    out->codeCount = counts[0];
    out->aliasCount = counts[1];
    out->windowsCount = counts[2];
    out->mappingCount = counts[3];
    out->textSize = counts[4];
    return at == payload.size;
}

ZoneCode mlocZoneCode(const ZoneCodeTables* t, uint32_t code)
{
    const uint8_t* p = t->codes + (size_t)code * zoneCodeSize;
    return (ZoneCode){.id = mlocLoad32(p),
                      .name = mlocLoad32(p + 4),
                      .idLength = p[8],
                      .nameLength = p[9],
                      .preferred = mlocLoad16(p + 10),
                      .windows = mlocLoad16(p + 12)};
}

static int Folded(uint8_t c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

// The order of two names, letters folded, known to share their first
// from bytes: negative, zero or positive, and the length they share.
static int OrderFrom(const uint8_t* a, size_t aLength, const char* b, size_t bLength, size_t from,
                     size_t* sharedOut)
{
    size_t i = from;
    while (i < aLength && i < bLength && Folded(a[i]) == Folded((uint8_t)b[i]))
    {
        i++;
    }
    *sharedOut = i;
    if (i < aLength && i < bLength)
    {
        return Folded(a[i]) - Folded((uint8_t)b[i]);
    }
    return aLength < bLength ? -1 : aLength > bLength;
}

// The order of two names, letters folded: negative, zero or positive.
static int Order(const uint8_t* a, size_t aLength, const char* b, size_t bLength)
{
    size_t shared = 0;
    return OrderFrom(a, aLength, b, bLength, 0, &shared);
}

// Whether a name lies in the text and is printable ASCII, spaces allowed
// or not.
static bool InText(const ZoneCodeTables* t, uint32_t offset, uint32_t length, bool spaces)
{
    if (length == 0 || (uint64_t)offset + length > t->textSize)
    {
        return false;
    }
    for (uint32_t i = 0; i < length; i++)
    {
        uint8_t c = t->text[offset + i];
        if (c > '~' || c < (spaces ? ' ' : '!'))
        {
            return false;
        }
    }
    return true;
}

static bool ValidId(const ZoneCodeTables* t, const ZoneCode* c)
{
    if (c->idLength < 2 || c->idLength > 8 || !InText(t, c->id, c->idLength, false))
    {
        return false;
    }
    for (uint32_t i = 0; i < c->idLength; i++)
    {
        uint8_t ch = t->text[c->id + i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')))
        {
            return false;
        }
    }
    return true;
}

static bool ValidCode(const ZoneCodeTables* t, uint32_t code)
{
    ZoneCode c = mlocZoneCode(t, code);
    if (!ValidId(t, &c) || mlocLoad16(t->codes + (size_t)code * zoneCodeSize + 14) != 0)
    {
        return false;
    }
    if (code > 0)
    {
        ZoneCode before = mlocZoneCode(t, code - 1);
        if (Order(t->text + before.id, before.idLength, (const char*)t->text + c.id, c.idLength) >=
            0)
        {
            return false;
        }
    }
    if (c.nameLength == 0)
    {
        // A deprecated id: its replacement is not, and it has no name.
        return c.name == 0 && c.preferred != code && c.preferred < t->codeCount &&
               mlocZoneCode(t, c.preferred).nameLength != 0 && c.windows == zoneCodeNone;
    }
    return c.preferred == code && InText(t, c.name, c.nameLength, false) &&
           (c.windows == zoneCodeNone || c.windows < t->windowsCount);
}

// An alias or a Windows name: a name in order, a zero byte, and its
// 16-bit field below a limit.
static bool ValidEntry(const ZoneCodeTables* t, const uint8_t* entries, uint32_t index, bool spaces,
                       uint32_t limit)
{
    const uint8_t* p = entries + (size_t)index * 8;
    if (p[5] != 0 || mlocLoad16(p + 6) >= limit || !InText(t, mlocLoad32(p), p[4], spaces))
    {
        return false;
    }
    if (index == 0)
    {
        return true;
    }
    const uint8_t* before = p - 8;
    return Order(t->text + mlocLoad32(before), before[4], (const char*)t->text + mlocLoad32(p),
                 p[4]) < 0;
}

static bool ValidRegion(const uint8_t* r)
{
    if (r[0] == '0' && r[1] == '0' && r[2] == '1')
    {
        return true;
    }
    return r[0] >= 'A' && r[0] <= 'Z' && r[1] >= 'A' && r[1] <= 'Z' && r[2] == 0;
}

// A Windows name's mappings: in order of their regions, one for 001.
static bool ValidMappings(const ZoneCodeTables* t, uint32_t first, uint32_t end)
{
    bool world = false;
    for (uint32_t m = first; m < end; m++)
    {
        const uint8_t* p = t->mappings + (size_t)m * zoneMappingSize;
        if (!InText(t, mlocLoad32(p), p[4], false) || !ValidRegion(p + 5) ||
            (m > first && Order(p - 3, 3, (const char*)p + 5, 3) >= 0))
        {
            return false;
        }
        world = world || p[5] == '0';
    }
    return world;
}

static uint32_t FirstMapping(const ZoneCodeTables* t, uint32_t windows)
{
    return windows < t->windowsCount
               ? mlocLoad16(t->windows + (size_t)windows * zoneWindowsSize + 6)
               : t->mappingCount;
}

bool mlocValidZoneCodes(const DataView* view, DataSpan payload)
{
    (void)view;
    ZoneCodeTables t;
    if (!mlocZoneCodeTablesOf(payload, &t) || mlocLoad32(payload.bytes + 20) != 0 ||
        t.codeCount == 0 || t.codeCount >= zoneCodeNone || t.aliasCount > UINT16_MAX ||
        t.windowsCount >= zoneCodeNone || t.mappingCount > UINT16_MAX || FirstMapping(&t, 0) != 0)
    {
        return false;
    }
    for (uint32_t i = 0; i < t.codeCount; i++)
    {
        if (!ValidCode(&t, i))
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < t.aliasCount; i++)
    {
        if (!ValidEntry(&t, t.aliases, i, false, t.codeCount) ||
            mlocZoneCode(&t, mlocLoad16(t.aliases + (size_t)i * zoneAliasSize + 6)).nameLength == 0)
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < t.windowsCount; i++)
    {
        if (!ValidEntry(&t, t.windows, i, true, t.mappingCount) ||
            FirstMapping(&t, i + 1) <= FirstMapping(&t, i) ||
            !ValidMappings(&t, FirstMapping(&t, i), FirstMapping(&t, i + 1)))
        {
            return false;
        }
    }
    return true;
}

bool mlocFindZoneCodeTables(const mlocDataSet* data, ZoneCodeTables* out)
{
    DataSpan span;
    return mlocFindDataSetUnit(data, kindZoneCodes, nullptr, 0, &span) &&
           mlocZoneCodeTablesOf(span, out);
}

// The entry of a sorted section of 8-byte or 16-byte entries whose name
// (at the entry's start, its length at byte 4 or 8) equals one, letters
// folded; -1 when none does.
// Every entry between the bounds shares with the name the shorter of
// the prefixes the bounds share with it, so a comparison starts there.
static int32_t Search(const ZoneCodeTables* t, const uint8_t* entries, uint32_t count,
                      uint32_t size, uint32_t lengthAt, const char* name, size_t length)
{
    uint32_t low = 0;
    uint32_t high = count;
    size_t lowShared = 0;
    size_t highShared = 0;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* p = entries + (size_t)middle * size;
        size_t shared = 0;
        int order = OrderFrom(t->text + mlocLoad32(p), p[lengthAt], name, length,
                              lowShared < highShared ? lowShared : highShared, &shared);
        if (order == 0)
        {
            return (int32_t)middle;
        }
        if (order < 0)
        {
            low = middle + 1;
            lowShared = shared;
        }
        else
        {
            high = middle;
            highShared = shared;
        }
    }
    return -1;
}

int32_t mlocZoneCodeFind(const ZoneCodeTables* t, const char* id, size_t length)
{
    return Search(t, t->codes, t->codeCount, zoneCodeSize, 8, id, length);
}

int32_t mlocZoneCodeOfIdentifier(const ZoneCodeTables* t, const char* name, size_t length)
{
    int32_t alias = Search(t, t->aliases, t->aliasCount, zoneAliasSize, 4, name, length);
    return alias < 0 ? -1 : mlocLoad16(t->aliases + (size_t)alias * zoneAliasSize + 6);
}

int32_t mlocZoneWindowsFind(const ZoneCodeTables* t, const char* name, size_t length)
{
    return Search(t, t->windows, t->windowsCount, zoneWindowsSize, 4, name, length);
}

const char* mlocZoneWindowsName(const ZoneCodeTables* t, uint32_t windows, size_t* lengthOut)
{
    const uint8_t* p = t->windows + (size_t)windows * zoneWindowsSize;
    *lengthOut = p[4];
    return (const char*)t->text + mlocLoad32(p);
}

const char* mlocZoneWindowsIdentifier(const ZoneCodeTables* t, uint32_t windows,
                                      const uint8_t region[3], size_t* lengthOut)
{
    // A name's mappings are in order of their regions, and the validator
    // requires one for 001, which sorts before every letter: the first.
    uint32_t first = FirstMapping(t, windows);
    const uint8_t* found = t->mappings + (size_t)first * zoneMappingSize;
    for (uint32_t m = first + 1; m < FirstMapping(t, windows + 1); m++)
    {
        const uint8_t* p = t->mappings + (size_t)m * zoneMappingSize;
        if (p[5] == region[0] && p[6] == region[1] && p[7] == region[2])
        {
            found = p;
            break;
        }
    }
    *lengthOut = found[4];
    return (const char*)t->text + mlocLoad32(found);
}
