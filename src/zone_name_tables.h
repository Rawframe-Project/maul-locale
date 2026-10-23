// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The units of the zone names data file, zone-names.mloc, which the
// generator compiles from CLDR's metaZones.xml, bcp47/timezone.xml and the
// locale files' timeZoneNames. Both index the BCP 47 ids of the zone codes
// unit (zone_code_tables.h) of the same CLDR release, by their place
// there. All integers are little-endian.
//
// Metazones, a unit of no locale. The payload is a header (u32 code
// count, u32 period count, u32 metazone count, u32 preferred count),
// then these sections, back to back:
//   codes      zoneMetaCodeSize bytes each, in the zone codes unit's
//              order: the region (two upper-case ASCII letters, or two
//              zero bytes for none: TR35's first two letters of a
//              five-letter id unless a region attribute names another),
//              its flags (u8, zoneMetaCountry), its period count (u8),
//              its first period (u16), two zero bytes. A deprecated id
//              has no region and no periods.
//   periods    zoneMetaPeriodSize bytes each: from and to, seconds since
//              1970 in universal time (i64, to exclusive; INT64_MIN and
//              INT64_MAX for open ends), the metazone (u16), its flags
//              (u8, zoneMetaOffsets), a zero byte, the standard and the
//              daylight offset CLDR gives (i16 minutes each, zero unless
//              flagged). A code's periods ascend and do not overlap.
//   metazones  zoneMetaZoneSize bytes each, in the order of their CLDR
//              ids: the golden zone (u16, a code), the first preferred
//              zone (u16), the preferred count (u16), two zero bytes.
//   preferred  zoneMetaPreferredSize bytes each: the region (two
//              upper-case ASCII letters), the zone (u16, a code); a
//              metazone's ascend by region.
//
// Zone names, a unit per locale key. The payload is a header (u32 entry
// count, u32 text size), the entries' keys (u16 each, an item times 8
// plus a field, strictly ascending), their text offsets (u32 each, count
// + 1, from 0 to the text size, ascending), then the text, UTF-8 with no
// byte below 0x20. An empty text is CLDR's "∅∅∅": no name, and none
// inherited. Items: zoneNameFormats with ZoneNameFormat's fields;
// zoneNameUnknown, Etc/Unknown, and each code at zoneNameCodeBase plus
// its place, with ZoneNameField's fields; each metazone at
// zoneNameMetazoneBase plus its place, with ZoneNameField's fields but
// the city. An entry is CLDR's value resolved along the locale's chain,
// filed at a key where the library's lookup (the first key along the
// chain that files the item's field) would give another; root's unit
// files each code's city the identifier implies when CLDR names none
// (its last part, '_' as spaces, for a code with a region), as ICU4C
// derives it.

#ifndef MLOC_ZONE_NAME_TABLES_H
#define MLOC_ZONE_NAME_TABLES_H

#include "data_view.h"

#include <stdint.h>

enum
{
    zoneMetaHeaderSize = 16,
    zoneMetaCodeSize = 8,
    zoneMetaPeriodSize = 24,
    zoneMetaZoneSize = 8,
    zoneMetaPreferredSize = 4,
    // A code's flag: the location format names its region (the only
    // zone there, or its primary zone).
    zoneMetaCountry = 1,
    // A period's flag: CLDR gives its standard and daylight offsets.
    zoneMetaOffsets = 1,
    zoneNameHeaderSize = 8,
    zoneNameFormats = 0,
    zoneNameUnknown = 1,
    zoneNameCodeBase = 2,
    zoneNameMetazoneBase = 1024,
    // The most codes and metazones the keys hold.
    zoneNameMaxCodes = zoneNameMetazoneBase - zoneNameCodeBase,
    zoneNameMaxMetazones = 1024,
    zoneNameFieldsPerItem = 8,
};

// The fields of zoneNameFormats.
typedef enum ZoneNameFormat
{
    zoneFormatGmt = 0,
    zoneFormatHourPositive = 1,
    zoneFormatHourNegative = 2,
    zoneFormatGmtUnknown = 3,
    zoneFormatRegion = 4,
    zoneFormatRegionStandard = 5,
    zoneFormatRegionDaylight = 6,
    zoneFormatFallback = 7,
    zoneFormatCount = 8,
} ZoneNameFormat;

// The fields of a zone or a metazone.
typedef enum ZoneNameField
{
    zoneFieldCity = 0,
    zoneFieldLongGeneric = 1,
    zoneFieldLongStandard = 2,
    zoneFieldLongDaylight = 3,
    zoneFieldShortGeneric = 4,
    zoneFieldShortStandard = 5,
    zoneFieldShortDaylight = 6,
    zoneFieldCount = 7,
} ZoneNameField;

// Whether a payload is a valid metazone unit.
bool mlocValidZoneMeta(const DataView* view, DataSpan payload);

// Whether a payload is a valid zone names unit.
bool mlocValidZoneNames(const DataView* view, DataSpan payload);

#endif
