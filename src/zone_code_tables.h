// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone codes unit of the data file (record mloc-0041), which the
// generator compiles from CLDR's bcp47/timezone.xml and
// supplemental/windowsZones.xml: the BCP 47 time zone ids and the Windows
// zone names, keyed by identifier names so that they serve a zones unit
// of any tzdb release.
//
// The payload is a header of five 32-bit counts and four zero bytes, then
// these sections, each starting on an 8-byte boundary:
//   codes    16 bytes each, sorted by their ids' bytes: the id's offset in
//            the text (u32), the offset of the identifier it names (u32),
//            the id's length (u8), the identifier's length (u8, 0 for a
//            deprecated id), its preferred code (u16: its own index, or a
//            deprecated id's replacement), its Windows name (u16, or
//            0xFFFF for none), two zero bytes
//   aliases  8 bytes each, sorted by their bytes with ASCII letters folded
//            to lower case: the name's offset in the text (u32), its
//            length (u8), a zero byte, its code (u16)
//   windows  8 bytes each, sorted as the aliases: the name's offset
//            (u32), its length (u8), a zero byte, its first mapping (u16);
//            a name's mappings run to the next name's first
//   mappings 8 bytes each, a name's sorted by their regions' bytes: the
//            identifier's offset (u32), its length (u8), the region (three
//            bytes: "001", or two upper-case letters and a zero byte)
//   text     the ids and names, ASCII, unterminated
// An id is two to eight digits and lower-case letters; CLDR's "unk" is
// left out. An alias names every identifier CLDR lists for a code, and
// every Windows name has a mapping for region 001.

#ifndef MLOC_ZONE_CODE_TABLES_H
#define MLOC_ZONE_CODE_TABLES_H

#include "data_view.h"

#include <stdint.h>

enum
{
    zoneCodeHeaderSize = 24,
    zoneCodeSectionCount = 5,
    zoneCodeSize = 16,
    zoneAliasSize = 8,
    zoneWindowsSize = 8,
    zoneMappingSize = 8,
    zoneCodeNone = 0xFFFF,
};

// The unit's sections and their counts.
typedef struct ZoneCodeTables
{
    const uint8_t* codes;
    const uint8_t* aliases;
    const uint8_t* windows;
    const uint8_t* mappings;
    const uint8_t* text;
    uint32_t codeCount;
    uint32_t aliasCount;
    uint32_t windowsCount;
    uint32_t mappingCount;
    uint32_t textSize;
} ZoneCodeTables;

// A code read out of its 16 bytes.
typedef struct ZoneCode
{
    uint32_t id;
    uint32_t name;
    uint8_t idLength;
    uint8_t nameLength;
    uint16_t preferred;
    uint16_t windows;
} ZoneCode;

// The sections of a payload whose size their counts give; false when it
// is not.
bool mlocZoneCodeTablesOf(DataSpan payload, ZoneCodeTables* out);

// The validator of the zone codes unit: the sections, every index and
// range, ids and names in strict order, a deprecated id's replacement not
// deprecated, aliases naming ids that are not, and each Windows name's
// mappings in order with one for region 001.
bool mlocValidZoneCodes(const DataView* view, DataSpan payload);

// The tables of the first source of a set with a zone codes unit, or
// false.
bool mlocFindZoneCodeTables(const mlocDataSet* data, ZoneCodeTables* out);

ZoneCode mlocZoneCode(const ZoneCodeTables* t, uint32_t code);

// The code of an id, letters folded, or -1.
int32_t mlocZoneCodeFind(const ZoneCodeTables* t, const char* id, size_t length);

// The code whose aliases hold an identifier, letters folded, or -1.
int32_t mlocZoneCodeOfIdentifier(const ZoneCodeTables* t, const char* name, size_t length);

// The Windows name's index, letters folded, or -1.
int32_t mlocZoneWindowsFind(const ZoneCodeTables* t, const char* name, size_t length);

// A Windows name's text.
const char* mlocZoneWindowsName(const ZoneCodeTables* t, uint32_t windows, size_t* lengthOut);

// The identifier a Windows name maps to in a region (three bytes as the
// mappings store them), or in 001 when the region has none.
const char* mlocZoneWindowsIdentifier(const ZoneCodeTables* t, uint32_t windows,
                                      const uint8_t region[3], size_t* lengthOut);

#endif
