// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A zone formatter's state (maul-locale/datetime.h): the zone names units
// along the locale's chain, the metazone and zone codes units, the region
// the generic names are qualified for, the offset's digits; and the
// writing of a zone at an instant by a style, falling back as ICU4C's
// TimeZoneFormat falls back.

#ifndef MLOC_ZONE_FORMAT_H
#define MLOC_ZONE_FORMAT_H

#include "data_view.h"
#include "date_data.h"
#include "number_core.h"
#include "number_tables.h"
#include "zone_code_tables.h"
#include "zone_tables.h"

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

// The metazone unit's sections.
typedef struct ZoneMeta
{
    const uint8_t* codes;
    const uint8_t* periods;
    const uint8_t* metazones;
    const uint8_t* preferred;
    uint32_t codeCount;
    uint32_t periodCount;
    uint32_t metazoneCount;
    uint32_t preferredCount;
} ZoneMeta;

// A zone at an instant, read once for every field written of it: its
// code, metazone and offset.
typedef struct ZoneAt
{
    ZoneTables t;
    ZoneCodeTables codes;
    ZoneMeta meta;
    // The zone's place in the zones unit, for a named zone.
    uint32_t index;
    bool named;
    // Its BCP 47 id's place, or -1 for a zone CLDR does not know.
    int32_t code;
    // Its metazone at the instant, or -1.
    int32_t metazone;
    ZoneOffset offset;
    int64_t seconds;
} ZoneAt;

// Makes a zone format for a style other than none.
mlocResult mlocMakeZoneFormat(ZoneFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                              mlocZoneStyle style);

// Whether a zone format holds what the library made.
bool mlocZoneFormatSound(const ZoneFormat* f);

// Reads a zone the library made at an instant in range; false for zone
// names units that disagree with it.
bool mlocReadZoneAt(const ZoneFormat* f, const mlocTimeZone* zone, const mlocInstant* instant,
                    ZoneAt* out);

// Reads a zone at an instant as mlocReadZoneAt does, after checking both,
// and the ISO date-time there.
bool mlocReadZonedLocal(const ZoneFormat* f, const mlocTimeZone* zone, const mlocInstant* instant,
                        ZoneAt* at, mlocDateTime* localOut);

// Writes a zone read at an instant by a style other than none, as one
// part.
void mlocWriteZone(const ZoneFormat* f, mlocZoneStyle style, const ZoneAt* zone, NumberWriter* w);

// Writes a compiled pattern of literal text and zone fields, which
// mlocZoneFieldStyle names, the span's parts marked as one value's.
void mlocWriteZonePattern(const ZoneFormat* f, const uint8_t* pattern, uint32_t length,
                          const ZoneAt* zone, const PatternSpan* span, NumberWriter* w);

#endif
