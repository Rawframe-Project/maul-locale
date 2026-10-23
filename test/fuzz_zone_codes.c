// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the zone codes unit's validator. The input is a
// zone codes payload; one the validator accepts must answer every lookup
// from inside its own bytes: each id, alias and Windows name found by its
// own text, a deprecated id's replacement not deprecated, and each Windows
// name's identifier read for every region its mappings give and for 001.

#include "bytes.h"
#include "zone_code_tables.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static void CheckCodes(const ZoneCodeTables* t)
{
    for (uint32_t i = 0; i < t->codeCount; i++)
    {
        ZoneCode c = mlocZoneCode(t, i);
        Require(mlocZoneCodeFind(t, (const char*)t->text + c.id, c.idLength) == (int32_t)i);
        Require(mlocZoneCode(t, c.preferred).nameLength != 0);
    }
    for (uint32_t i = 0; i < t->aliasCount; i++)
    {
        const uint8_t* p = t->aliases + (size_t)i * zoneAliasSize;
        int32_t code = mlocZoneCodeOfIdentifier(t, (const char*)t->text + mlocLoad32(p), p[4]);
        Require(code == (int32_t)mlocLoad16(p + 6));
    }
}

static void CheckWindows(const ZoneCodeTables* t)
{
    for (uint32_t w = 0; w < t->windowsCount; w++)
    {
        size_t length = 0;
        const char* name = mlocZoneWindowsName(t, w, &length);
        Require(mlocZoneWindowsFind(t, name, length) == (int32_t)w);
        static const uint8_t s_world[3] = {'0', '0', '1'};
        Require(mlocZoneWindowsIdentifier(t, w, s_world, &length) != nullptr && length > 0);
    }
    for (uint32_t w = 0; w < t->windowsCount; w++)
    {
        uint32_t first = mlocLoad16(t->windows + (size_t)w * zoneWindowsSize + 6);
        uint32_t end = w + 1 < t->windowsCount
                           ? mlocLoad16(t->windows + (size_t)(w + 1) * zoneWindowsSize + 6)
                           : t->mappingCount;
        for (uint32_t m = first; m < end; m++)
        {
            const uint8_t* p = t->mappings + (size_t)m * zoneMappingSize;
            size_t length = 0;
            const char* name = mlocZoneWindowsIdentifier(t, w, p + 5, &length);
            Require(name == (const char*)t->text + mlocLoad32(p) && length == p[4]);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Aligned, as a payload in a data file is.
    static uint64_t s_bytes[1024];
    if (size > sizeof(s_bytes))
    {
        return 0;
    }
    memcpy(s_bytes, data, size);
    DataSpan span = {(const uint8_t*)s_bytes, (uint32_t)size};
    DataView view = {0};
    ZoneCodeTables t;
    if (!mlocValidZoneCodes(&view, span))
    {
        return 0;
    }
    Require(mlocZoneCodeTablesOf(span, &t));
    CheckCodes(&t);
    CheckWindows(&t);
    return 0;
}
