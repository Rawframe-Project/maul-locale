// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The public zone values checked before use (record mloc-0041): a zone
// the library made, an instant in Temporal's range, and a zone's offset
// at a time whether it is named or an offset.

#ifndef MLOC_ZONE_HANDLE_H
#define MLOC_ZONE_HANDLE_H

#include "zone_tables.h"

#include "maul-locale/zone.h"

#include <stdint.h>

// Whether an instant lies in Temporal's range, its nanoseconds within a
// second.
bool mlocValidInstant(const mlocInstant* instant);

// A zone's tables when it is named, after checking that the library made
// it: an offset zone has no unit and an offset within a day, a named one
// an identifier of its unit and no offset.
bool mlocReadTimeZone(const mlocTimeZone* zone, ZoneTables* t, bool* namedOut);

// A zone's offset in seconds at a time, for a zone mlocReadTimeZone read.
int32_t mlocTimeZoneOffsetAt(const mlocTimeZone* zone, const ZoneTables* t, bool named,
                             int64_t seconds);

#endif
