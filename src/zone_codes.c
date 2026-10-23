// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Time zones by their BCP 47 ids and Windows names: a code names an
// identifier, which the set's zones then resolve, so the codes serve zones
// of any tzdb release.

#include "zone_code_tables.h"
#include "zone_handle.h"
#include "zone_tables.h"

#include "maul-locale/zone.h"

#include <string.h>

enum
{
    // The Etc/GMT zones CLDR gives ids: 12 hours behind to 14 ahead.
    minCodeHours = -12,
    maxCodeHours = 14,
};

static mlocResult Write(const char* text, size_t length, mlocOutput* output)
{
    output->length = length;
    if (length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    memcpy(output->buffer, text, length);
    return mloc_success;
}

// The code of a zone: its identifier's, else its primary identifier's;
// an offset zone of whole hours, its Etc/GMT zone's. -1 when none.
static int32_t CodeOfZone(const ZoneCodeTables* c, const mlocTimeZone* zone, const ZoneTables* t,
                          bool named)
{
    if (!named)
    {
        int minutes = zone->offsetMinutes;
        int hours = minutes / 60;
        if (minutes % 60 != 0 || hours < minCodeHours || hours > maxCodeHours)
        {
            return -1;
        }
        int magnitude = hours < 0 ? -hours : hours;
        char id[6] = {'u',
                      't',
                      'c',
                      hours < 0 ? 'w' : 'e',
                      (char)('0' + magnitude / 10),
                      (char)('0' + magnitude % 10)};
        return mlocZoneCodeFind(c, id, hours == 0 ? 3 : sizeof(id));
    }
    size_t length = 0;
    const char* name = mlocZoneIdentifierName(t, zone->identifier, &length);
    int32_t code = mlocZoneCodeOfIdentifier(c, name, length);
    if (code < 0)
    {
        name = mlocZoneIdentifierName(t, mlocZoneIdentifierPrimary(t, zone->identifier), &length);
        code = mlocZoneCodeOfIdentifier(c, name, length);
    }
    return code;
}

// The code of a zone a caller gives, or a result.
static mlocResult ZoneCodeOf(const mlocDataSet* data, const mlocTimeZone* zone,
                             const mlocOutput* output, ZoneCodeTables* c, ZoneCode* codeOut)
{
    ZoneTables t;
    bool named = false;
    if (data == nullptr || output == nullptr ||
        (output->buffer == nullptr && output->capacity > 0) || !mlocReadTimeZone(zone, &t, &named))
    {
        return mloc_errorInvalid;
    }
    if (!mlocFindZoneCodeTables(data, c))
    {
        return mloc_errorNotFound;
    }
    int32_t code = CodeOfZone(c, zone, &t, named);
    if (code < 0)
    {
        return mloc_errorNotFound;
    }
    *codeOut = mlocZoneCode(c, mlocZoneCode(c, (uint32_t)code).preferred);
    return mloc_success;
}

mlocResult mlocFindTimeZoneBcp47(const mlocDataSet* data, const char* id, size_t length,
                                 mlocTimeZone* zoneOut)
{
    ZoneCodeTables c;
    if (data == nullptr || (id == nullptr && length > 0) || zoneOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    int32_t code = mlocFindZoneCodeTables(data, &c) ? mlocZoneCodeFind(&c, id, length) : -1;
    if (code < 0)
    {
        return mloc_errorNotFound;
    }
    ZoneCode named = mlocZoneCode(&c, mlocZoneCode(&c, (uint32_t)code).preferred);
    return mlocFindTimeZone(data, (const char*)c.text + named.name, named.nameLength, zoneOut);
}

mlocResult mlocFormatTimeZoneBcp47(const mlocDataSet* data, const mlocTimeZone* zone,
                                   mlocOutput* output)
{
    ZoneCodeTables c;
    ZoneCode code;
    mlocResult result = ZoneCodeOf(data, zone, output, &c, &code);
    return result != mloc_success ? result
                                  : Write((const char*)c.text + code.id, code.idLength, output);
}

// A region as the mappings store it: "001" for none, two letters upper
// case and a zero byte, or three digits; false when it is none of those.
static bool RegionOf(const char* region, size_t length, uint8_t out[3])
{
    out[0] = length == 0 ? '0' : 0;
    out[1] = length == 0 ? '0' : 0;
    out[2] = length == 0 ? '1' : 0;
    if (length == 0)
    {
        return true;
    }
    bool letters = length == 2;
    bool digits = length == 3;
    for (size_t i = 0; i < length && (letters || digits); i++)
    {
        char ch = region[i];
        letters = letters && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'));
        digits = digits && ch >= '0' && ch <= '9';
        out[i] = (uint8_t)(ch >= 'a' && ch <= 'z' ? ch - 32 : ch);
    }
    return letters || digits;
}

mlocResult mlocFindTimeZoneWindows(const mlocDataSet* data, const char* name, size_t length,
                                   const char* region, size_t regionLength, mlocTimeZone* zoneOut)
{
    uint8_t key[3];
    if (data == nullptr || (name == nullptr && length > 0) ||
        (region == nullptr && regionLength > 0) || zoneOut == nullptr ||
        !RegionOf(region, regionLength, key))
    {
        return mloc_errorInvalid;
    }
    ZoneCodeTables c;
    int32_t windows = mlocFindZoneCodeTables(data, &c) ? mlocZoneWindowsFind(&c, name, length) : -1;
    if (windows < 0)
    {
        return mloc_errorNotFound;
    }
    size_t idLength = 0;
    const char* id = mlocZoneWindowsIdentifier(&c, (uint32_t)windows, key, &idLength);
    return mlocFindTimeZone(data, id, idLength, zoneOut);
}

mlocResult mlocFormatTimeZoneWindows(const mlocDataSet* data, const mlocTimeZone* zone,
                                     mlocOutput* output)
{
    ZoneCodeTables c;
    ZoneCode code;
    mlocResult result = ZoneCodeOf(data, zone, output, &c, &code);
    if (result != mloc_success)
    {
        return result;
    }
    if (code.windows == zoneCodeNone)
    {
        return mloc_errorNotFound;
    }
    size_t length = 0;
    const char* name = mlocZoneWindowsName(&c, code.windows, &length);
    return Write(name, length, output);
}
