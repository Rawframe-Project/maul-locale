// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A zone formatter's state (maul-locale/datetime.h, record mloc-0042):
// the zone names units along the locale's chain, the metazone and zone
// codes units, the region the generic names are qualified for, the
// offset's digits; and the writing of a zone at an instant by a style,
// falling back as ICU4C's TimeZoneFormat falls back.

#ifndef MLOC_ZONE_FORMAT_H
#define MLOC_ZONE_FORMAT_H

#include "data_view.h"
#include "number_core.h"
#include "number_tables.h"

#include "maul-locale/datetime.h"

enum
{
    // The zone names units a formatter keeps, the locale's chain's.
    zoneChainMost = 8,
};

typedef struct ZoneFormat
{
    DataSpan names[zoneChainMost];
    DataSpan meta;
    DataSpan codes;
    // The data and locale the region names are read from.
    const mlocDataSet* data;
    mlocLocale locale;
    uint8_t digits[10 * numberDigitSize];
    uint8_t digitLength;
    uint8_t nameCount;
    uint8_t style;
    // The region whose preferred zones qualify generic names, "001" for
    // none.
    char region[4];
    uint8_t stamp;
} ZoneFormat;

// Makes a zone format for a style other than none.
mlocResult mlocMakeZoneFormat(ZoneFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                              mlocZoneStyle style);

// Whether a zone format holds what the library made.
bool mlocZoneFormatSound(const ZoneFormat* f);

// Writes a zone the library made at an instant in range by a style other
// than none, as one part.
void mlocWriteZone(const ZoneFormat* f, mlocZoneStyle style, const mlocTimeZone* zone,
                   const mlocInstant* instant, NumberWriter* w);

// The style of a pattern's zone field (zzzz, z, vvvv, v, VVVV, VVV, OOOO,
// O; zz and zzz as z), or none for another letter or width.
mlocZoneStyle mlocZoneFieldStyle(uint8_t letter, uint32_t width);

// Writes a compiled pattern of literal text and zone fields, which
// mlocZoneFieldStyle names.
void mlocWriteZonePattern(const ZoneFormat* f, const uint8_t* pattern, uint32_t length,
                          const mlocTimeZone* zone, const mlocInstant* instant, NumberWriter* w);

#endif
