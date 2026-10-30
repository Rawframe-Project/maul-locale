// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The data part: the data version, the compiled-in data, files built here
// by hand and opened with test kinds, the lookups over them, and a refusal
// for every field a file can get wrong.

#include "bytes.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"

#include "maul-locale/data.h"

#include <string.h>

extern const uint8_t mlocBuiltinData[];
extern const uint32_t mlocBuiltinDataSize;

enum
{
    kindWords = 7,
    kindAny = 9,
    kindUnknown = 11,
};

// Kind 7 holds 32-bit words, so its payload's size is a multiple of 4.
static bool ValidWords(const DataView* view, DataSpan payload)
{
    (void)view;
    return payload.size % 4 == 0;
}

static bool ValidAny(const DataView* view, DataSpan payload)
{
    (void)view;
    (void)payload;
    return true;
}

static const DataKind s_kinds[] = {{kindWords, ValidWords}, {kindAny, ValidAny}};

typedef struct TestUnit
{
    uint32_t kind;
    uint32_t locale;
    const char* payload;
} TestUnit;

alignas(8) static uint8_t s_file[2048];

static void Put16(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void Put32(uint8_t* p, uint32_t v)
{
    Put16(p, v);
    Put16(p + 2, v >> 16);
}

static void Put64(uint8_t* p, uint64_t v)
{
    Put32(p, (uint32_t)v);
    Put32(p + 4, (uint32_t)(v >> 32));
}

static uint32_t Align(uint32_t offset)
{
    return (offset + 7) / 8 * 8;
}

static void PutHeader(uint32_t size, uint32_t units, uint32_t locales, uint32_t pool)
{
    mlocDataVersion version;
    mlocGetDataVersion(&version);
    memcpy(s_file + headerMagic, "MLOC", 4);
    Put16(s_file + headerFormat, dataFormatVersion);
    Put16(s_file + headerHeaderSize, dataHeaderSize);
    Put32(s_file + headerFileSize, size);
    Put32(s_file + headerCldr,
          mlocPackVersion(version.cldr.major, version.cldr.minor, version.cldr.patch));
    Put32(s_file + headerUnicode,
          mlocPackVersion(version.unicode.major, version.unicode.minor, version.unicode.patch));
    Put32(s_file + headerDataFormat, version.format);
    memcpy(s_file + headerTzdb, version.tzdb, 8);
    Put64(s_file + headerTables, version.tables);
    Put32(s_file + headerUnitCount, units);
    Put32(s_file + headerLocaleCount, locales);
    Put32(s_file + headerPoolOffset, pool);
    Put32(s_file + headerPoolSize, size - pool);
}

// Writes a file into s_file in the given order, which may be wrong on
// purpose; returns its size.
static uint32_t Build(const TestUnit* units, uint32_t unitCount, const char* const* locales,
                      uint32_t localeCount)
{
    memset(s_file, 0, sizeof(s_file));
    uint32_t at = Align(dataHeaderSize + unitCount * dataUnitSize + localeCount * dataLocaleSize);
    for (uint32_t i = 0; i < unitCount; i++)
    {
        uint8_t* entry = s_file + dataHeaderSize + i * dataUnitSize;
        uint32_t size = (uint32_t)strlen(units[i].payload);
        Put32(entry, units[i].kind);
        Put32(entry + 4, units[i].locale);
        Put32(entry + 8, at);
        Put32(entry + 12, size);
        memcpy(s_file + at, units[i].payload, size);
        at = Align(at + size);
    }
    uint32_t pool = at;
    for (uint32_t i = 0; i < localeCount; i++)
    {
        uint8_t* entry = s_file + dataHeaderSize + unitCount * dataUnitSize + i * dataLocaleSize;
        uint32_t length = (uint32_t)strlen(locales[i]);
        Put32(entry, at - pool);
        Put32(entry + 4, length);
        memcpy(s_file + at, locales[i], length);
        at += length;
    }
    PutHeader(at, unitCount, localeCount, pool);
    return at;
}

static const char* const s_locales[] = {"de", "de_AT", "en", "sr_Latn"};
static const TestUnit s_units[] = {
    {kindWords, 0, "wordsde."}, {kindWords, 2, "wordsen."},  {kindWords, dataNoLocale, "root"},
    {kindAny, 1, "any de_AT"},  {kindAny, 3, "any sr_Latn"},
};

static uint32_t BuildGood(void)
{
    return Build(s_units, 5, s_locales, 4);
}

static mlocResult Open(uint32_t size)
{
    DataView view;
    return mlocOpenDataWithKinds(&view, s_file, size, s_kinds, 2);
}

static bool SameVersions(mlocVersion a, mlocVersion b)
{
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
}

static bool SameVersion(const mlocDataVersion* a, const mlocDataVersion* b)
{
    return SameVersions(a->cldr, b->cldr) && SameVersions(a->unicode, b->unicode) &&
           a->format == b->format && memcmp(a->tzdb, b->tzdb, 8) == 0 && a->tables == b->tables;
}

static void TestVersion(void)
{
    mlocDataVersion version;
    mlocGetDataVersion(&version);
    CHECK(version.cldr.major == MLOC_CLDR_VERSION_MAJOR &&
              version.format == MLOC_DATA_FORMAT_VERSION,
          "the data version names the library's CLDR and data format");
    CHECK(version.unicode.major == 18, "Unicode 18");
    CHECK(memcmp(version.tzdb, "2026e\0\0\0", 8) == 0, "the time zone release, NUL-padded");
    mlocDataVersion read;
    CHECK(mlocReadDataVersion(mlocBuiltinData, mlocBuiltinDataSize, &read) == mloc_success &&
              SameVersion(&read, &version),
          "the compiled-in data has the library's version");
    CHECK(mlocReadDataVersion(mlocBuiltinData, 63, &read) == mloc_errorFormat, "a short header");
    CHECK(mlocReadDataVersion(nullptr, 64, &read) == mloc_errorInvalid, "null bytes");
    CHECK(mlocReadDataVersion(mlocBuiltinData, 64, nullptr) == mloc_errorInvalid, "a null version");
}

static void TestBuiltin(void)
{
    mlocData data = {0};
    CHECK(mlocOpenData(mlocBuiltinData, mlocBuiltinDataSize, &data) == mloc_success,
          "the compiled-in bytes are a valid data file");
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    CHECK(memcmp(&data, &builtin, sizeof(data)) == 0, "opened alike either way");
    // The plural part files a unit under every locale id CLDR's plural
    // files name, root included; the ids are sorted.
    size_t count = mlocData_GetLocaleCount(&builtin);
    CHECK(count > 200, "the plural part's locales");
    bool sorted = true;
    bool root = false;
    const char* previous = nullptr;
    size_t previousLength = 0;
    for (size_t i = 0; i < count; i++)
    {
        const char* id = nullptr;
        size_t length = 0;
        sorted = sorted && mlocData_GetLocale(&builtin, i, &id, &length) == mloc_success;
        if (previous != nullptr)
        {
            size_t n = length < previousLength ? length : previousLength;
            int order = memcmp(previous, id, n);
            sorted = sorted && (order < 0 || (order == 0 && previousLength < length));
        }
        root = root || (length == 4 && memcmp(id, "root", 4) == 0);
        previous = id;
        previousLength = length;
    }
    CHECK(sorted && root, "sorted, with root");
}

static void TestLookups(void)
{
    uint32_t size = BuildGood();
    mlocData data = {0};
    DataView view;
    if (mlocOpenDataWithKinds(&view, s_file, size, s_kinds, 2) != mloc_success)
    {
        CHECK(false, "a file built by hand opens");
        return;
    }
    memcpy(&data, &view, sizeof(view));
    CHECK(mlocData_GetLocaleCount(&data) == 4, "its locales");
    const char* id = nullptr;
    size_t length = 0;
    CHECK(mlocData_GetLocale(&data, 3, &id, &length) == mloc_success && length == 7 &&
              memcmp(id, "sr_Latn", 7) == 0,
          "a locale by index");
    CHECK(mlocData_GetLocale(&data, 4, &id, &length) == mloc_errorInvalid, "an index past them");
    CHECK(mlocData_GetLocale(&data, 0, nullptr, &length) == mloc_errorInvalid, "a null id");
    uint32_t index = 0;
    CHECK(mlocFindDataLocale(&view, "de_AT", 5, &index) && index == 1, "a locale by id");
    CHECK(mlocFindDataLocale(&view, "de", 2, &index) && index == 0, "a prefix of another");
    CHECK(!mlocFindDataLocale(&view, "de_A", 4, &index), "a prefix that is no locale");
    CHECK(!mlocFindDataLocale(&view, "zz", 2, &index), "a locale past the last");
    CHECK(!mlocFindDataLocale(&view, "a", 1, &index), "a locale before the first");
    DataSpan span;
    CHECK(mlocFindDataUnit(&view, kindWords, 2, &span) && span.size == 8 &&
              memcmp(span.bytes, "wordsen.", 8) == 0,
          "a unit by kind and locale");
    CHECK(mlocFindDataUnit(&view, kindWords, dataNoLocale, &span) && span.size == 4,
          "a unit of no locale");
    CHECK(!mlocFindDataUnit(&view, kindWords, 1, &span), "a locale without the kind");
    CHECK(!mlocFindDataUnit(&view, kindUnknown, 0, &span), "a kind the file lacks");
}

static void TestDataSet(void)
{
    alignas(8) static uint8_t s_first[2048];
    static const TestUnit s_firstUnits[] = {{kindAny, 0, "pack tr"}};
    static const char* const s_firstLocales[] = {"tr"};
    uint32_t firstSize = Build(s_firstUnits, 1, s_firstLocales, 1);
    memcpy(s_first, s_file, firstSize);
    DataView first;
    DataView second;
    if (mlocOpenDataWithKinds(&first, s_first, firstSize, s_kinds, 2) != mloc_success)
    {
        CHECK(false, "a pack opens");
        return;
    }
    uint32_t secondSize = BuildGood();
    if (mlocOpenDataWithKinds(&second, s_file, secondSize, s_kinds, 2) != mloc_success)
    {
        CHECK(false, "a second source opens");
        return;
    }
    mlocData sources[2] = {0};
    memcpy(&sources[0], &first, sizeof(first));
    memcpy(&sources[1], &second, sizeof(second));
    mlocDataSet set = {sources, 2};
    DataSpan span;
    CHECK(mlocFindDataSetUnit(&set, kindAny, "tr", 2, &span) && span.size == 7,
          "found in the first source");
    CHECK(mlocFindDataSetUnit(&set, kindAny, "sr_Latn", 7, &span) && span.size == 11,
          "found in the second");
    CHECK(mlocFindDataSetUnit(&set, kindWords, nullptr, 0, &span) && span.size == 4,
          "a unit of no locale from the second");
    CHECK(!mlocFindDataSetUnit(&set, kindWords, "tr", 2, &span),
          "a locale one source has, without the kind");
    mlocDataSet empty = {nullptr, 0};
    CHECK(!mlocFindDataSetUnit(&empty, kindAny, "tr", 2, &span), "an empty set");
}

static void TestArguments(void)
{
    uint32_t size = BuildGood();
    mlocData data;
    CHECK(mlocOpenData(s_file, size, nullptr) == mloc_errorInvalid, "a null view");
    CHECK(mlocOpenData(nullptr, size, &data) == mloc_errorInvalid, "null bytes");
    CHECK(mlocOpenData(s_file + 1, size, &data) == mloc_errorInvalid, "unaligned bytes");
    CHECK(mlocOpenData(s_file, size, &data) == mloc_errorFormat,
          "kinds the library does not read are refused");
    CHECK(Open(63) == mloc_errorFormat, "shorter than a header");
}

static void TestVersionRefusals(void)
{
    static const uint32_t s_fields[] = {headerCldr, headerUnicode, headerDataFormat,
                                        headerTzdb, headerTables,  headerTables + 4};
    for (size_t i = 0; i < sizeof(s_fields) / sizeof(s_fields[0]); i++)
    {
        uint32_t size = BuildGood();
        s_file[s_fields[i]] ^= 1;
        CHECK(Open(size) == mloc_errorVersion, "another data version");
    }
    // The field held a packed library version before the data format
    // version: the released files' 0.0.1 and 0.1.0 never match a format.
    static const uint32_t s_released[] = {1, 256};
    for (size_t i = 0; i < 2; i++)
    {
        uint32_t size = BuildGood();
        Put32(s_file + headerDataFormat, s_released[i]);
        CHECK(Open(size) == mloc_errorVersion, "a released file's library version");
    }
    uint32_t size = BuildGood();
    s_file[headerFileSize] ^= 1;
    s_file[headerCldr] ^= 1;
    CHECK(Open(size) == mloc_errorVersion, "the version is checked before the rest");
}

// A zones file may be of another tzdb release; any other file may not,
// whatever else it holds beside zones.
static void TestZonesFiles(void)
{
    static const DataKind s_zoneKinds[] = {{kindWords, ValidWords}, {kindZones, ValidAny}};
    static const TestUnit s_zones[] = {{kindZones, dataNoLocale, "zones"}};
    static const TestUnit s_mixed[] = {{kindWords, dataNoLocale, "root"},
                                       {kindZones, dataNoLocale, "zones"}};
    DataView view;
    uint32_t size = Build(s_zones, 1, nullptr, 0);
    memcpy(s_file + headerTzdb, "1999z\0\0\0", 8);
    CHECK(mlocOpenDataWithKinds(&view, s_file, size, s_zoneKinds, 2) == mloc_success,
          "zones alone, of another release");
    size = Build(s_mixed, 2, nullptr, 0);
    memcpy(s_file + headerTzdb, "1999z\0\0\0", 8);
    CHECK(mlocOpenDataWithKinds(&view, s_file, size, s_zoneKinds, 2) == mloc_errorVersion,
          "zones beside another unit, of another release");
    size = Build(s_mixed, 2, nullptr, 0);
    CHECK(mlocOpenDataWithKinds(&view, s_file, size, s_zoneKinds, 2) == mloc_success,
          "zones beside another unit, of the library's release");
    size = Build(s_zones, 0, nullptr, 0);
    memcpy(s_file + headerTzdb, "1999z\0\0\0", 8);
    CHECK(mlocOpenDataWithKinds(&view, s_file, size, s_zoneKinds, 2) == mloc_errorVersion,
          "no unit at all, of another release");
    size = Build(s_zones, 1, nullptr, 0);
    memcpy(s_file + headerTzdb, "1999z\0\0\0", 8);
    s_file[headerDataFormat] ^= 1;
    CHECK(mlocOpenDataWithKinds(&view, s_file, size, s_zoneKinds, 2) == mloc_errorVersion,
          "zones of another data format");
}

// Builds the good file, puts a 32-bit value at an offset, and expects a
// refusal.
static void Corrupt(uint32_t offset, uint32_t value, const char* what)
{
    uint32_t size = BuildGood();
    Put32(s_file + offset, value);
    CHECK(Open(size) == mloc_errorFormat, what);
}

static void TestHeaderRefusals(void)
{
    uint32_t size = BuildGood();
    Corrupt(headerMagic, 0x434F4C4E, "the magic");
    Corrupt(headerFormat, 2 | dataHeaderSize << 16, "the format version");
    Corrupt(headerFormat, 1 | 72u << 16, "the header size");
    Corrupt(headerFileSize, size + 8, "a file size past the bytes");
    Corrupt(headerFlags, 1, "flags");
    Corrupt(headerReserved, 1, "the reserved field");
    Corrupt(headerUnitCount, 0x10000000, "a unit table past the file");
    Corrupt(headerUnitCount, 0xFFFFFFFF, "a unit count that would overflow");
    Corrupt(headerLocaleCount, 200, "a locale table past the pool");
    Corrupt(headerPoolOffset, size + 1, "a pool past the file");
    Corrupt(headerPoolSize, 3, "a pool that does not end the file");
    Corrupt(headerPoolSize, 17, "a pool said to be longer than the file");
    Corrupt(headerPoolOffset, 64, "a pool inside the tables");
    CHECK(Open(size - 1) == mloc_errorFormat, "bytes shorter than the file size");
}

static void TestLongIds(void)
{
    static const TestUnit s_longUnits[] = {{kindAny, 0, "x"}};
    char id[66];
    memset(id, 'a', 65);
    id[64] = '\0';
    const char* longest[] = {id};
    CHECK(Open(Build(s_longUnits, 1, longest, 1)) == mloc_success, "a 64-byte locale id");
    id[64] = 'a';
    id[65] = '\0';
    CHECK(Open(Build(s_longUnits, 1, longest, 1)) == mloc_errorFormat, "a 65-byte locale id");
}

static void TestTableRefusals(void)
{
    uint32_t units = dataHeaderSize;
    uint32_t locales = units + 5 * dataUnitSize;
    Corrupt(locales + 4, 0, "an empty locale id");
    Corrupt(locales + 4, 65, "a locale id past the limit");
    Corrupt(locales, 1000, "a locale id past the pool");
    Corrupt(locales + 4, 1000, "a locale id running past the pool");
    Corrupt(locales, 7, "locales out of order");
    Corrupt(locales + dataLocaleSize + 4, 2, "a locale id equal to another");
    Corrupt(units, kindAny, "units out of order");
    Corrupt(units + dataUnitSize + 4, 0, "a kind and locale twice");
    Corrupt(units + 4, 4, "a locale index past the locales");
    Corrupt(units + 8, 1, "an unaligned payload");
    Corrupt(units + 8, 177, "an unaligned payload among the payloads");
    Corrupt(units + 4 * dataUnitSize + 12, 20, "a payload running into the pool");
    Corrupt(units + 8, 8, "a payload inside the tables");
    Corrupt(units + 12, 4000, "a payload past the pool");
    Corrupt(units + 4 * dataUnitSize, kindUnknown, "an unknown kind");
    Corrupt(units + 12, 5, "a payload its kind refuses");
    uint32_t size = BuildGood();
    uint8_t* id = s_file + mlocLoad32(s_file + headerPoolOffset);
    id[0] = '-';
    CHECK(Open(size) == mloc_errorFormat, "a byte no locale id has");
}

// A failed call leaves its outputs as they were: each filled with a
// pattern before, every byte of it checked after.
static void TestFailedOutputs(void)
{
    static const uint8_t s_zeros[64] = {0};
    mlocDataVersion version;
    memset(&version, 0xA5, sizeof version);
    mlocDataVersion keptVersion = version;
    CHECK(mlocReadDataVersion(s_zeros, sizeof s_zeros, &version) == mloc_errorFormat &&
              mlocReadDataVersion(nullptr, 64, &version) == mloc_errorInvalid &&
              memcmp(&version, &keptVersion, sizeof version) == 0,
          "a version refused leaves the version");
    mlocData data;
    memset(&data, 0xA5, sizeof data);
    mlocData keptData = data;
    uint32_t size = BuildGood();
    bool refused = mlocOpenData(s_file + 1, size, &data) == mloc_errorInvalid &&
                   mlocOpenData(s_file, 63, &data) == mloc_errorFormat;
    s_file[headerCldr] ^= 1;
    refused = refused && mlocOpenData(s_file, size, &data) == mloc_errorVersion;
    CHECK(refused && memcmp(&data, &keptData, sizeof data) == 0,
          "an open refused as unaligned, malformed or of another version leaves the view");
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    const char* id = (const char*)&keptData;
    size_t length = 99;
    CHECK(mlocData_GetLocale(&builtin, mlocData_GetLocaleCount(&builtin), &id, &length) ==
                  mloc_errorInvalid &&
              id == (const char*)&keptData && length == 99,
          "an index out of range leaves the id and its length");
}

int main(void)
{
    TestVersion();
    TestBuiltin();
    TestLookups();
    TestDataSet();
    TestArguments();
    TestVersionRefusals();
    TestZonesFiles();
    TestHeaderRefusals();
    TestTableRefusals();
    TestLongIds();
    TestFailedOutputs();
    return s_failures == 0 ? 0 : 1;
}
