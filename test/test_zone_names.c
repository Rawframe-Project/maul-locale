// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone names data file's units (zone_name_tables.h): a metazone unit
// and a zone names unit built here, valid, and copies with one field
// spoiled; then zone-names.mloc: every unit valid, the metazone unit's
// codes those of the zone codes unit, and a few zones' regions, flags and
// metazone periods as CLDR gives them; and a qualified generic name whose
// city a copy of the file empties.

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"
#include "zone_code_tables.h"
#include "zone_name_tables.h"

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"
#include "maul-locale/zone.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

enum
{
    // The metazone unit built here: two codes, two periods, one
    // metazone, one preferred zone.
    metaCodes = zoneMetaHeaderSize,
    metaPeriods = metaCodes + 2 * zoneMetaCodeSize,
    metaZones = metaPeriods + 2 * zoneMetaPeriodSize,
    metaPreferred = metaZones + zoneMetaZoneSize,
    metaSize = metaPreferred + zoneMetaPreferredSize,
};

alignas(8) static uint8_t s_meta[metaSize + 8];

// US with two periods of metazone 0, the second with offsets; a code of
// no region; the metazone's golden zone code 0, preferred code 1 in CA.
static void BuildMeta(void)
{
    memset(s_meta, 0, sizeof(s_meta));
    Put(s_meta, 2, 4);
    Put(s_meta + 4, 2, 4);
    Put(s_meta + 8, 1, 4);
    Put(s_meta + 12, 1, 4);
    memcpy(s_meta + metaCodes, "US", 2);
    s_meta[metaCodes + 2] = zoneMetaCountry;
    s_meta[metaCodes + 3] = 2;
    Put(s_meta + metaPeriods + 4, 0x80000000, 4);
    Put(s_meta + metaPeriods + 8, 1000, 4);
    Put(s_meta + metaPeriods + zoneMetaPeriodSize, 1000, 4);
    Put(s_meta + metaPeriods + zoneMetaPeriodSize + 8, UINT32_MAX, 4);
    Put(s_meta + metaPeriods + zoneMetaPeriodSize + 12, INT32_MAX, 4);
    s_meta[metaPeriods + zoneMetaPeriodSize + 18] = zoneMetaOffsets;
    Put(s_meta + metaPeriods + zoneMetaPeriodSize + 20, (uint16_t)-420, 2);
    Put(s_meta + metaPeriods + zoneMetaPeriodSize + 22, (uint16_t)-360, 2);
    Put(s_meta + metaZones + 4, 1, 2);
    memcpy(s_meta + metaPreferred, "CA", 2);
    Put(s_meta + metaPreferred + 2, 1, 2);
}

static bool MetaValid(uint32_t size)
{
    DataView view = {0};
    return mlocValidZoneMeta(&view, (DataSpan){s_meta, size});
}

// Whether the unit with bytes at an offset spoiled is refused.
static bool MetaRefuses(uint32_t at, uint64_t value, int bytes)
{
    BuildMeta();
    Put(s_meta + at, value, bytes);
    bool refused = !MetaValid(metaSize);
    BuildMeta();
    return refused;
}

static void TestMeta(void)
{
    BuildMeta();
    CHECK(MetaValid(metaSize), "a metazone unit");
    CHECK(!MetaValid(metaSize + 1) && !MetaValid(metaSize - 1) && !MetaValid(8),
          "a size other than the counts give");
    CHECK(MetaRefuses(0, zoneNameMaxCodes + 1, 4) && MetaRefuses(8, zoneNameMaxMetazones + 1, 4),
          "more codes or metazones than the keys hold");
    alignas(8) static uint8_t s_most[zoneMetaHeaderSize + zoneNameMaxCodes * zoneMetaCodeSize];
    memset(s_most, 0, sizeof(s_most));
    Put(s_most, zoneNameMaxCodes, 4);
    DataView view = {0};
    CHECK(mlocValidZoneMeta(&view, (DataSpan){s_most, sizeof(s_most)}) &&
              mlocValidZoneMeta(&view, (DataSpan){s_most + sizeof(s_most) - zoneMetaHeaderSize,
                                                  zoneMetaHeaderSize}),
          "the most codes the keys hold, and a unit of none");
    CHECK(MetaRefuses(metaCodes, 'u', 1) && MetaRefuses(metaCodes + 1, 0, 1) &&
              MetaRefuses(metaCodes, '@', 1) && MetaRefuses(metaCodes + 1, '[', 1),
          "a region not two upper-case letters");
    CHECK(MetaRefuses(metaCodes + 2, 2, 1) && MetaRefuses(metaCodes + 6, 1, 2),
          "a code's unknown flag or a nonzero pad");
    CHECK(MetaRefuses(metaCodes + 3, 3, 1) && MetaRefuses(metaCodes + 4, 1, 2) &&
              MetaRefuses(metaCodes + zoneMetaCodeSize + 4, 1, 2),
          "periods past the section, or a first period without any");
    CHECK(MetaRefuses(metaPeriods, 1000, 8) && MetaRefuses(metaPeriods + 8, 1001, 4),
          "a period ending before it starts, or overlapping the next");
    CHECK(MetaRefuses(metaPeriods + 16, 1, 2) && MetaRefuses(metaPeriods + 18, 2, 1) &&
              MetaRefuses(metaPeriods + 19, 1, 1),
          "a period's metazone past the metazones, an unknown flag, a nonzero pad");
    CHECK(MetaRefuses(metaPeriods + 20, 60, 2) && MetaRefuses(metaPeriods + 22, 60, 2),
          "offsets on a period without the flag");
    CHECK(MetaRefuses(metaPeriods + zoneMetaPeriodSize + 20, 18 * 60 + 1, 2) &&
              MetaRefuses(metaPeriods + zoneMetaPeriodSize + 22, (uint16_t)(-18 * 60 - 1), 2),
          "an offset past 18 hours");
    BuildMeta();
    Put(s_meta + metaPeriods + zoneMetaPeriodSize + 20, 18 * 60, 2);
    Put(s_meta + metaPeriods + zoneMetaPeriodSize + 22, (uint16_t)(-18 * 60), 2);
    CHECK(MetaValid(metaSize), "offsets of 18 hours");
    CHECK(MetaRefuses(metaZones, 2, 2) && MetaRefuses(metaZones + 6, 1, 2) &&
              MetaRefuses(metaZones + 4, 2, 2) && MetaRefuses(metaZones + 2, 1, 2),
          "a golden zone past the codes, a pad, preferred zones past the section");
    CHECK(MetaRefuses(metaPreferred, 'c', 1) && MetaRefuses(metaPreferred, 0, 2) &&
              MetaRefuses(metaPreferred + 2, 2, 2),
          "a preferred zone's region not two letters, or its code past the codes");
}

// Two preferred zones of one metazone in a region's order or not.
static void TestPreferredOrder(void)
{
    alignas(8) static uint8_t
        s_two[zoneMetaHeaderSize + zoneMetaCodeSize + zoneMetaZoneSize + 2 * zoneMetaPreferredSize];
    memset(s_two, 0, sizeof(s_two));
    Put(s_two, 1, 4);
    Put(s_two + 8, 1, 4);
    Put(s_two + 12, 2, 4);
    uint8_t* zone = s_two + zoneMetaHeaderSize + zoneMetaCodeSize;
    Put(zone + 4, 2, 2);
    memcpy(zone + zoneMetaZoneSize, "CA", 2);
    memcpy(zone + zoneMetaZoneSize + zoneMetaPreferredSize, "MX", 2);
    DataView view = {0};
    DataSpan span = {s_two, sizeof(s_two)};
    CHECK(mlocValidZoneMeta(&view, span), "preferred zones by region");
    memcpy(zone + zoneMetaZoneSize + zoneMetaPreferredSize, "CA", 2);
    CHECK(!mlocValidZoneMeta(&view, span), "a region twice");
    memcpy(zone + zoneMetaZoneSize + zoneMetaPreferredSize, "AR", 2);
    CHECK(!mlocValidZoneMeta(&view, span), "regions out of order");
}

alignas(8) static uint8_t s_names[256];

// The gmt format, code 0's city, metazone 0's empty long generic name.
static uint32_t BuildNames(void)
{
    static const char s_text[] = "GMT{0}Los Angeles";
    static const uint16_t s_keys[3] = {zoneNameFormats * zoneNameFieldsPerItem + zoneFormatGmt,
                                       zoneNameCodeBase * zoneNameFieldsPerItem + zoneFieldCity,
                                       zoneNameMetazoneBase * zoneNameFieldsPerItem +
                                           zoneFieldLongGeneric};
    static const uint32_t s_offsets[4] = {0, 6, 17, 17};
    memset(s_names, 0, sizeof(s_names));
    Put(s_names, 3, 4);
    Put(s_names + 4, 17, 4);
    for (uint32_t i = 0; i < 3; i++)
    {
        Put(s_names + zoneNameHeaderSize + 2 * i, s_keys[i], 2);
    }
    for (uint32_t i = 0; i < 4; i++)
    {
        Put(s_names + zoneNameHeaderSize + 6 + 4 * i, s_offsets[i], 4);
    }
    memcpy(s_names + zoneNameHeaderSize + 22, s_text, 17);
    return zoneNameHeaderSize + 22 + 17;
}

static bool NamesValid(uint32_t size)
{
    DataView view = {0};
    return mlocValidZoneNames(&view, (DataSpan){s_names, size});
}

static bool NamesRefuse(uint32_t at, uint32_t value, int bytes)
{
    uint32_t size = BuildNames();
    Put(s_names + at, value, bytes);
    return !NamesValid(size);
}

// Whether a unit holding one format, its text as given, is valid.
static bool FormatValid(ZoneNameFormat field, const char* text)
{
    uint32_t length = (uint32_t)strlen(text);
    memset(s_names, 0, sizeof(s_names));
    Put(s_names, 1, 4);
    Put(s_names + 4, length, 4);
    Put(s_names + zoneNameHeaderSize, zoneNameFormats * zoneNameFieldsPerItem + field, 2);
    Put(s_names + zoneNameHeaderSize + 6, length, 4);
    memcpy(s_names + zoneNameHeaderSize + 10, text, length);
    return NamesValid(zoneNameHeaderSize + 10 + length);
}

static void TestFormats(void)
{
    CHECK(FormatValid(zoneFormatGmt, "GMT{0}") && FormatValid(zoneFormatGmt, "{0} UTC") &&
              FormatValid(zoneFormatGmt, "") && !FormatValid(zoneFormatGmt, "GMT") &&
              !FormatValid(zoneFormatGmt, "GMT{0") && !FormatValid(zoneFormatGmt, "GMT{1}"),
          "the GMT format holds {0}");
    CHECK(FormatValid(zoneFormatHourPositive, "+HH:mm") &&
              FormatValid(zoneFormatHourNegative, "-H.mm") &&
              !FormatValid(zoneFormatHourPositive, "+mm:HH") &&
              !FormatValid(zoneFormatHourNegative, "-HH:m") &&
              !FormatValid(zoneFormatHourPositive, "+HH"),
          "an hour format holds an H before its first mm");
    CHECK(FormatValid(zoneFormatRegion, "x") && FormatValid(zoneFormatFallback, "y"),
          "the other formats are text");
}

static void TestNames(void)
{
    uint32_t size = BuildNames();
    CHECK(NamesValid(size), "a zone names unit, an empty name among them");
    CHECK(!NamesValid(size + 1) && !NamesValid(size - 1) && !NamesValid(4),
          "a size other than the counts give");
    memset(s_names, 0, zoneNameHeaderSize + 4);
    CHECK(NamesValid(zoneNameHeaderSize + 4) && !NamesValid(zoneNameHeaderSize + 3),
          "a unit without entries, and one without its offset");
    CHECK(NamesRefuse(zoneNameHeaderSize + 6, 1, 4) && NamesRefuse(zoneNameHeaderSize + 18, 16, 4),
          "a first offset not 0, or a last not the text's size");
    CHECK(NamesRefuse(zoneNameHeaderSize + 10, 18, 4) && NamesRefuse(zoneNameHeaderSize + 14, 5, 4),
          "an offset past the text or before the one before");
    CHECK(NamesRefuse(zoneNameHeaderSize + 2, 0, 2) &&
              NamesRefuse(zoneNameHeaderSize + 4, zoneNameCodeBase * zoneNameFieldsPerItem, 2),
          "keys not strictly ascending");
    CHECK(NamesRefuse(zoneNameHeaderSize + 2,
                      zoneNameCodeBase * zoneNameFieldsPerItem + zoneFieldCount, 2),
          "a zone field past the fields");
    CHECK(NamesRefuse(zoneNameHeaderSize + 4, zoneNameMetazoneBase * zoneNameFieldsPerItem, 2) &&
              NamesRefuse(zoneNameHeaderSize + 4,
                          zoneNameMetazoneBase * zoneNameFieldsPerItem + zoneFieldCount, 2),
          "a metazone's city, or a field past the fields");
    CHECK(NamesRefuse(zoneNameHeaderSize + 4,
                      (zoneNameMetazoneBase + zoneNameMaxMetazones) * zoneNameFieldsPerItem + 1, 2),
          "a metazone past the most the keys hold");
    size = BuildNames();
    Put(s_names + zoneNameHeaderSize + 4,
        (zoneNameMetazoneBase + zoneNameMaxMetazones - 1) * zoneNameFieldsPerItem +
            zoneFieldShortDaylight,
        2);
    CHECK(NamesValid(size), "the last metazone's last field");
    CHECK(NamesRefuse(zoneNameHeaderSize + 22, 0x1F, 1) &&
              NamesRefuse(zoneNameHeaderSize + 22, 0xC3, 1),
          "a control byte or malformed UTF-8 in the text");
}

static mlocData s_sources[2];
static mlocDataSet s_data;
alignas(8) static uint8_t s_file[1 << 23];
static size_t s_fileSize;

static bool OpenFile(const char* path, uint8_t* buffer, size_t capacity, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    size_t read = file == nullptr ? 0 : fread(buffer, 1, capacity, file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_fileSize = read;
    return read > 0 && read < capacity && mlocOpenData(buffer, read, out) == mloc_success;
}

// A code's record in the file's metazone unit, by its BCP 47 id.
static const uint8_t* CodeRecord(DataSpan meta, const ZoneCodeTables* t, const char* id)
{
    int32_t code = mlocZoneCodeFind(t, id, strlen(id));
    return code < 0 ? nullptr : meta.bytes + zoneMetaHeaderSize + (size_t)code * zoneMetaCodeSize;
}

static void TestFile(void)
{
    DataSpan meta;
    ZoneCodeTables t;
    CHECK(mlocFindDataSetUnit(&s_data, kindZoneMeta, nullptr, 0, &meta) &&
              mlocFindZoneCodeTables(&s_data, &t),
          "the metazone unit and the zone codes");
    CHECK(mlocLoad32(meta.bytes) == t.codeCount, "a record for each BCP 47 id");
    const uint8_t* la = CodeRecord(meta, &t, "uslax");
    const uint8_t* shanghai = CodeRecord(meta, &t, "cnsha");
    const uint8_t* rome = CodeRecord(meta, &t, "itrom");
    const uint8_t* utc = CodeRecord(meta, &t, "utc");
    CHECK(la != nullptr && memcmp(la, "US", 2) == 0 && la[2] == 0 && la[3] == 1,
          "Los Angeles: US, one of many, one metazone period");
    CHECK(shanghai != nullptr && memcmp(shanghai, "CN", 2) == 0 && shanghai[2] == zoneMetaCountry,
          "Shanghai: CN's primary zone");
    CHECK(rome != nullptr && memcmp(rome, "IT", 2) == 0 && rome[2] == zoneMetaCountry,
          "Rome: IT's only zone");
    CHECK(utc != nullptr && utc[0] == 0 && utc[2] == 0, "UTC: no region");
    uint32_t metazones = mlocLoad32(meta.bytes + 8);
    const uint8_t* periods =
        meta.bytes + zoneMetaHeaderSize + (size_t)t.codeCount * zoneMetaCodeSize;
    const uint8_t* zones = periods + (size_t)mlocLoad32(meta.bytes + 4) * zoneMetaPeriodSize;
    const uint8_t* period =
        la != nullptr ? periods + (size_t)mlocLoad16(la + 4) * zoneMetaPeriodSize : nullptr;
    uint32_t pacific = period != nullptr ? mlocLoad16(period + 16) : metazones;
    CHECK(pacific < metazones && (int64_t)mlocLoad64(period) == INT64_MIN &&
              (int64_t)mlocLoad64(period + 8) == INT64_MAX,
          "Los Angeles's period open at both ends");
    CHECK(pacific < metazones && mlocLoad16(zones + (size_t)pacific * zoneMetaZoneSize) ==
                                     (uint32_t)mlocZoneCodeFind(&t, "uslax", 5),
          "Los Angeles golden for its metazone");
}

// Empties a zone names unit's entry, as CLDR's "∅∅∅" would: its end is
// set to its start, so that the next entry starts there too and holds
// both texts. 1 when emptied, 0 for a unit without the key, -1 for the
// unit's last entry.
static int EmptyEntry(uint8_t* unit, uint32_t key)
{
    uint32_t count = mlocLoad32(unit);
    const uint8_t* keys = unit + zoneNameHeaderSize;
    uint8_t* offsets = unit + zoneNameHeaderSize + 2 * (size_t)count;
    for (uint32_t i = 0; i < count; i++)
    {
        if (mlocLoad16(keys + 2 * (size_t)i) == key)
        {
            if (i + 1 == count)
            {
                return -1;
            }
            Put(offsets + 4 * ((size_t)i + 1), mlocLoad32(offsets + 4 * (size_t)i), 4);
            return 1;
        }
    }
    return 0;
}

// Punta Arenas's generic name, in July, is qualified by its city: Chile's
// preferred zone for the US is another, of another offset. A file whose
// first unit along English's chain to file that city empties it
// qualifies the name by the zone's identifier.
static void TestNoCity(void)
{
    const char* id = "America/Punta_Arenas";
    mlocLocale en;
    mlocTimeZone zone;
    mlocZoneFormatter formatter;
    mlocInstant july = {1784116800, 0};
    char text[64];
    mlocOutput output = {text, sizeof text, 0, nullptr, nullptr};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocFindTimeZone(&s_data, id, strlen(id), &zone) == mloc_success &&
              mlocInitZoneFormatter(&s_data, &en, mloc_zoneStyleGenericLong, &formatter) ==
                  mloc_success &&
              mlocZoneFormatter_Format(&formatter, &zone, &july, &output) == mloc_success &&
              output.length == 25 && memcmp(text, "Chile Time (Punta Arenas)", 25) == 0,
          "Punta Arenas qualified by its city");
    ZoneCodeTables t;
    int32_t code = mlocFindZoneCodeTables(&s_data, &t) ? mlocZoneCodeFind(&t, "clpuq", 5) : -1;
    uint32_t key = (zoneNameCodeBase + (uint32_t)code) * zoneNameFieldsPerItem + zoneFieldCity;
    static const char* const chain[] = {"en", "root"};
    int emptied = 0;
    for (size_t c = 0; c < sizeof chain / sizeof chain[0] && emptied == 0 && code >= 0; c++)
    {
        DataSpan unit;
        if (mlocFindDataSetUnit(&s_data, kindZoneNames, chain[c], strlen(chain[c]), &unit))
        {
            emptied = EmptyEntry(s_file + (unit.bytes - s_file), key);
        }
    }
    CHECK(emptied == 1 && mlocOpenData(s_file, s_fileSize, &s_sources[1]) == mloc_success,
          "a copy of the file without Punta Arenas's city opens");
    output.length = 0;
    CHECK(mlocInitZoneFormatter(&s_data, &en, mloc_zoneStyleGenericLong, &formatter) ==
                  mloc_success &&
              mlocZoneFormatter_Format(&formatter, &zone, &july, &output) == mloc_success &&
              output.length == 33 && memcmp(text, "Chile Time (America/Punta_Arenas)", 33) == 0,
          "without its city, Punta Arenas qualified by its identifier");
}

int main(void)
{
    TestMeta();
    TestPreferredOrder();
    TestNames();
    TestFormats();
    mlocOpenBuiltinData(&s_sources[0]);
    s_data = (mlocDataSet){s_sources, 2};
    if (!OpenFile(MLOC_ZONE_NAMES_FILE, s_file, sizeof(s_file), &s_sources[1]))
    {
        fputs("test_zone_names: cannot open " MLOC_ZONE_NAMES_FILE "\n", stderr);
        return 2;
    }
    TestFile();
    TestNoCity();
    return s_failures == 0 ? 0 : 1;
}
