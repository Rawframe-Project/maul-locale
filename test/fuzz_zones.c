// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the zones unit's validator (record mloc-0041).
// The input is a zones payload; one the validator accepts must answer
// every lookup from inside its own bytes: each identifier found by its
// name, and each zone's offsets and transitions read across Temporal's
// range. (A crafted unit whose closing rule overlaps its table answers
// inconsistently, which the generator's checks rule out for its own; it
// still reads nothing outside its bytes.)

#include "zone_rules.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
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
    ZoneTables t;
    if (!mlocValidZones(&view, span))
    {
        return 0;
    }
    Require(mlocZoneTablesOf(span, &t));
    for (uint32_t i = 0; i < t.identifierCount; i++)
    {
        size_t length = 0;
        const char* name = mlocZoneIdentifierName(&t, i, &length);
        int32_t found = mlocZoneFindIdentifier(&t, name, length);
        Require(found == (int32_t)i);
    }
    for (uint32_t z = 0; z < t.zoneCount; z++)
    {
        int64_t at = zoneMinSeconds;
        ZoneOffset o = mlocZoneOffsetAt(&t, z, at);
        for (int steps = 0; steps < 64 && mlocZoneNextTransition(&t, z, at, &at, &o); steps++)
        {
            (void)mlocZoneOffsetAt(&t, z, at);
        }
        (void)mlocZoneOffsetAt(&t, z, zoneMaxSeconds);
        (void)mlocZoneOffsetAt(&t, z, 0);
    }
    return 0;
}
