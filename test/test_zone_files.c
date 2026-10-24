// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Zones files: a data file holding the zones unit alone, of another tzdb
// release than the library's, built here from the compiled-in zones unit
// with its release renamed. It opens, a data set with it first reads its
// zones and names its release, zones found in it keep reading it, and a
// zones unit whose release is malformed is refused. tools/check_zones.py
// checks a zones file mlocgen writes from an older release against that
// release's zic.

#include "bytes.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/zone.h"

#include <string.h>

enum
{
    maxFile = 1 << 20,
};

alignas(8) static uint8_t s_file[maxFile];

static void Put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
    {
        p[i] = (uint8_t)(v >> (8 * i));
    }
}

// Writes a zones file of the compiled-in zones unit, its release in the
// header and in the unit given; returns its size, or 0.
static uint32_t BuildZonesFile(const char* headerRelease, const char unitRelease[8])
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet set = {&builtin, 1};
    DataSpan zones;
    uint32_t payload = (uint32_t)(dataHeaderSize + dataUnitSize + 7) / 8 * 8;
    if (!mlocFindDataSetUnit(&set, kindZones, nullptr, 0, &zones) || payload + zones.size > maxFile)
    {
        return 0;
    }
    memset(s_file, 0, sizeof(s_file));
    uint32_t size = payload + (uint32_t)zones.size;
    mlocDataVersion version;
    mlocGetDataVersion(&version);
    memcpy(s_file + headerMagic, "MLOC", 4);
    s_file[headerFormat] = (uint8_t)dataFormatVersion;
    s_file[headerFormat + 1] = (uint8_t)(dataFormatVersion >> 8);
    s_file[headerHeaderSize] = (uint8_t)dataHeaderSize;
    s_file[headerHeaderSize + 1] = (uint8_t)(dataHeaderSize >> 8);
    Put32(s_file + headerFileSize, size);
    Put32(s_file + headerCldr,
          mlocPackVersion(version.cldr.major, version.cldr.minor, version.cldr.patch));
    Put32(s_file + headerUnicode,
          mlocPackVersion(version.unicode.major, version.unicode.minor, version.unicode.patch));
    Put32(s_file + headerLibrary,
          mlocPackVersion(version.library.major, version.library.minor, version.library.patch));
    memcpy(s_file + headerTzdb, headerRelease, strlen(headerRelease));
    Put32(s_file + headerTables, (uint32_t)version.tables);
    Put32(s_file + headerTables + 4, (uint32_t)(version.tables >> 32));
    Put32(s_file + headerUnitCount, 1);
    Put32(s_file + headerPoolOffset, size);
    uint8_t* entry = s_file + dataHeaderSize;
    Put32(entry, kindZones);
    Put32(entry + 4, dataNoLocale);
    Put32(entry + 8, payload);
    Put32(entry + 12, (uint32_t)zones.size);
    memcpy(s_file + payload, zones.bytes, zones.size);
    // The release is the zones header's last eight bytes.
    memcpy(s_file + payload + 40, unitRelease, 8);
    return size;
}

static bool ReleaseIs(const mlocDataSet* set, const char* want)
{
    char text[16];
    mlocOutput out = {.buffer = text, .capacity = sizeof text};
    return mlocGetTimeZoneRelease(set, &out) == mloc_success && out.length == strlen(want) &&
           memcmp(text, want, out.length) == 0;
}

static void TestOpenAndRead(void)
{
    uint32_t size = BuildZonesFile("1999z", "1999z\0\0");
    mlocData sources[2];
    mlocOpenBuiltinData(&sources[1]);
    if (size == 0 || mlocOpenData(&sources[0], s_file, size) != mloc_success)
    {
        CHECK(false, "a zones file of another release opens");
        return;
    }
    mlocDataSet both = {sources, 2};
    mlocDataSet builtin = {&sources[1], 1};
    mlocDataVersion own;
    mlocGetDataVersion(&own);
    CHECK(ReleaseIs(&both, "1999z") && ReleaseIs(&builtin, own.tzdb),
          "the release of the first zones in a set");
    mlocTimeZone fromFile;
    mlocTimeZone fromBuiltin;
    bool equal = false;
    CHECK(mlocFindTimeZone(&both, "Europe/Vienna", 13, &fromFile) == mloc_success &&
              mlocFindTimeZone(&builtin, "Europe/Vienna", 13, &fromBuiltin) == mloc_success &&
              (const uint8_t*)fromFile.zones >= s_file &&
              (const uint8_t*)fromFile.zones < s_file + size &&
              fromBuiltin.zones != fromFile.zones &&
              mlocTimeZonesEqual(&fromFile, &fromBuiltin, &equal) == mloc_success && equal,
          "a zone found in the file reads the file, and equals the built-in one");
    mlocInstant epoch = {0, 0};
    int32_t offset = 0;
    CHECK(mlocGetTimeZoneOffset(&fromFile, &epoch, &offset) == mloc_success && offset == 3600,
          "and gives its offsets");
}

static void TestRefusals(void)
{
    static const char s_bad[][8] = {
        {'1', '9', '9', '9', 'Z', 0, 0, 0},   {0, '9', '9', '9', 'z', 0, 0, 0},
        {'1', '9', '9', '9', 'z', 0, 'x', 0}, {0},
        {'1', '9', '9', '9', '-', 0, 0, 0},
    };
    for (size_t i = 0; i < sizeof s_bad / sizeof s_bad[0]; i++)
    {
        mlocData data;
        uint32_t size = BuildZonesFile("1999z", s_bad[i]);
        CHECK(size > 0 && mlocOpenData(&data, s_file, size) == mloc_errorData,
              "a zones unit whose release is malformed");
    }
    mlocData data;
    uint32_t size = BuildZonesFile("1999z", "abcdefgh");
    if (size == 0 || mlocOpenData(&data, s_file, size) != mloc_success)
    {
        CHECK(false, "a release of eight bytes, letters and digits");
        return;
    }
    mlocDataSet set = {&data, 1};
    mlocDataSet empty = {nullptr, 0};
    char text[8];
    mlocOutput shortOut = {.buffer = text, .capacity = 7};
    mlocOutput forged = {.buffer = nullptr, .capacity = 4};
    CHECK(ReleaseIs(&set, "abcdefgh") &&
              mlocGetTimeZoneRelease(&set, &shortOut) == mloc_errorCapacity && shortOut.length == 8,
          "eight bytes, measured past a short buffer");
    CHECK(mlocGetTimeZoneRelease(&empty, &shortOut) == mloc_errorNotFound &&
              mlocGetTimeZoneRelease(nullptr, &shortOut) == mloc_errorInvalid &&
              mlocGetTimeZoneRelease(&set, nullptr) == mloc_errorInvalid &&
              mlocGetTimeZoneRelease(&set, &forged) == mloc_errorInvalid,
          "no zones, nulls, a null buffer with a capacity");
}

int main(void)
{
    TestOpenAndRead();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
