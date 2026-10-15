// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Time zones by BCP 47 id and Windows name (record mloc-0041): the
// compiled-in zone codes unit, its four functions, and its validator
// over a small unit with one field changed at a time.
// tools/check_zone_codes.py compares every id and name with CLDR's files.

#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"
#include "zone_code_tables.h"

#include "maul-locale/data.h"
#include "maul-locale/zone.h"

#include <string.h>

static mlocData s_builtin;
static mlocDataSet s_data = {&s_builtin, 1};

static bool IdIs(const mlocTimeZone* zone, const char* want)
{
    char text[64];
    mlocOutput out = {.buffer = text, .capacity = sizeof text};
    return mlocFormatTimeZoneId(zone, &out) == mloc_success && out.length == strlen(want) &&
           memcmp(text, want, out.length) == 0;
}

static bool FindsBcp47(const char* id, const char* want)
{
    mlocTimeZone zone;
    return mlocFindTimeZoneBcp47(&s_data, id, strlen(id), &zone) == mloc_success &&
           IdIs(&zone, want);
}

static bool FindsWindows(const char* name, const char* region, const char* want)
{
    mlocTimeZone zone;
    size_t regionLength = region == nullptr ? 0 : strlen(region);
    return mlocFindTimeZoneWindows(&s_data, name, strlen(name), region, regionLength, &zone) ==
               mloc_success &&
           IdIs(&zone, want);
}

// The result of formatting a zone named or an offset, and the text.
static mlocResult Format(const char* zoneId, bool windows, char* text, size_t capacity)
{
    mlocTimeZone zone;
    mlocOutput out = {.buffer = text, .capacity = capacity};
    if (mlocFindTimeZone(&s_data, zoneId, strlen(zoneId), &zone) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    mlocResult result = windows ? mlocFormatTimeZoneWindows(&s_data, &zone, &out)
                                : mlocFormatTimeZoneBcp47(&s_data, &zone, &out);
    if (result == mloc_success)
    {
        text[out.length] = '\0';
    }
    return result;
}

static bool Formats(const char* zoneId, bool windows, const char* want)
{
    char text[64];
    return Format(zoneId, windows, text, sizeof text - 1) == mloc_success &&
           strcmp(text, want) == 0;
}

static void TestBcp47(void)
{
    CHECK(FindsBcp47("usnyc", "America/New_York") && FindsBcp47("USNYC", "America/New_York"),
          "an id, letters folded");
    CHECK(FindsBcp47("camtr", "America/Toronto") && FindsBcp47("utcw05", "Etc/GMT+5") &&
              FindsBcp47("utc", "Etc/UTC") && FindsBcp47("gmt", "Etc/GMT"),
          "a deprecated id finds its replacement's zone; the Etc/GMT ids");
    mlocTimeZone zone;
    CHECK(mlocFindTimeZoneBcp47(&s_data, "unk", 3, &zone) == mloc_errorNotFound &&
              mlocFindTimeZoneBcp47(&s_data, "", 0, &zone) == mloc_errorNotFound &&
              mlocFindTimeZoneBcp47(&s_data, "usnycx", 6, &zone) == mloc_errorNotFound,
          "unk, empty and unknown ids");
    CHECK(Formats("Asia/Calcutta", false, "inccu") && Formats("Asia/Kolkata", false, "inccu") &&
              Formats("America/New_York", false, "usnyc") && Formats("UTC", false, "utc"),
          "a zone's id, from any of its names");
    CHECK(Formats("-05:00", false, "utcw05") && Formats("+00:00", false, "utc") &&
              Formats("+14:00", false, "utce14") && Formats("-12:00", false, "utcw12"),
          "an offset zone of whole hours");
    char text[16];
    CHECK(Format("+05:30", false, text, sizeof text) == mloc_errorNotFound &&
              Format("+15:00", false, text, sizeof text) == mloc_errorNotFound &&
              Format("-13:00", false, text, sizeof text) == mloc_errorNotFound,
          "offsets CLDR gives no id");
}

// Every id finds a zone whose id is its own, or its replacement's.
static void TestEveryCode(void)
{
    ZoneCodeTables t;
    CHECK(mlocFindZoneCodeTables(&s_data, &t) && t.codeCount > 450 && t.windowsCount > 100,
          "the compiled-in unit");
    bool all = true;
    for (uint32_t i = 0; i < t.codeCount && all; i++)
    {
        ZoneCode code = mlocZoneCode(&t, i);
        ZoneCode want = mlocZoneCode(&t, code.preferred);
        mlocTimeZone zone;
        char text[16];
        mlocOutput out = {.buffer = text, .capacity = sizeof text};
        all = mlocFindTimeZoneBcp47(&s_data, (const char*)t.text + code.id, code.idLength, &zone) ==
                  mloc_success &&
              mlocFormatTimeZoneBcp47(&s_data, &zone, &out) == mloc_success &&
              out.length == want.idLength && memcmp(text, t.text + want.id, out.length) == 0;
    }
    CHECK(all, "every id round trips");
}

static void TestWindows(void)
{
    CHECK(FindsWindows("Eastern Standard Time", nullptr, "America/New_York") &&
              FindsWindows("eastern standard time", "001", "America/New_York"),
          "a Windows name, letters folded");
    CHECK(FindsWindows("Eastern Standard Time", "CA", "America/Toronto") &&
              FindsWindows("Eastern Standard Time", "ca", "America/Toronto") &&
              FindsWindows("Eastern Standard Time", "FR", "America/New_York") &&
              FindsWindows("Eastern Standard Time", "419", "America/New_York"),
          "a region's mapping, or 001's");
    mlocTimeZone zone;
    CHECK(mlocFindTimeZoneWindows(&s_data, "Eastern Standard Time", 21, "U", 1, &zone) ==
                  mloc_errorInvalid &&
              mlocFindTimeZoneWindows(&s_data, "Eastern Standard Time", 21, "1A", 2, &zone) ==
                  mloc_errorInvalid &&
              mlocFindTimeZoneWindows(&s_data, "Eastern Standard Tim", 20, nullptr, 0, &zone) ==
                  mloc_errorNotFound,
          "regions that are not, and a name CLDR does not map");
    CHECK(Formats("America/New_York", true, "Eastern Standard Time") &&
              Formats("America/Detroit", true, "Eastern Standard Time") &&
              Formats("Asia/Calcutta", true, "India Standard Time") &&
              Formats("UTC", true, "UTC") && Formats("-05:00", true, "SA Pacific Standard Time"),
          "a zone's Windows name");
    char text[8];
    CHECK(Format("+05:30", true, text, sizeof text) == mloc_errorNotFound &&
              Format("America/New_York", true, text, sizeof text) == mloc_errorCapacity,
          "no name, and a short buffer");
}

static void TestRefusals(void)
{
    mlocTimeZone zone;
    CHECK(mlocFindTimeZone(&s_data, "Europe/Vienna", 13, &zone) == mloc_success, "a zone");
    char text[32];
    mlocOutput out = {.buffer = text, .capacity = sizeof text};
    mlocOutput forged = {.buffer = nullptr, .capacity = 4};
    mlocDataSet empty = {nullptr, 0};
    mlocTimeZone bad = {.offsetMinutes = 1440};
    CHECK(mlocFindTimeZoneBcp47(nullptr, "usnyc", 5, &zone) == mloc_errorInvalid &&
              mlocFindTimeZoneBcp47(&s_data, nullptr, 5, &zone) == mloc_errorInvalid &&
              mlocFindTimeZoneBcp47(&s_data, "usnyc", 5, nullptr) == mloc_errorInvalid &&
              mlocFindTimeZoneBcp47(&empty, "usnyc", 5, &zone) == mloc_errorNotFound,
          "finding an id: nulls and no data");
    CHECK(
        mlocFindTimeZoneWindows(nullptr, "UTC", 3, nullptr, 0, &zone) == mloc_errorInvalid &&
            mlocFindTimeZoneWindows(&s_data, nullptr, 3, nullptr, 0, &zone) == mloc_errorInvalid &&
            mlocFindTimeZoneWindows(&s_data, "UTC", 3, nullptr, 2, &zone) == mloc_errorInvalid &&
            mlocFindTimeZoneWindows(&s_data, "UTC", 3, nullptr, 0, nullptr) == mloc_errorInvalid &&
            mlocFindTimeZoneWindows(&empty, "UTC", 3, nullptr, 0, &zone) == mloc_errorNotFound,
        "finding a Windows name: nulls and no data");
    CHECK(mlocFormatTimeZoneBcp47(nullptr, &zone, &out) == mloc_errorInvalid &&
              mlocFormatTimeZoneBcp47(&s_data, nullptr, &out) == mloc_errorInvalid &&
              mlocFormatTimeZoneBcp47(&s_data, &zone, nullptr) == mloc_errorInvalid &&
              mlocFormatTimeZoneBcp47(&s_data, &zone, &forged) == mloc_errorInvalid &&
              mlocFormatTimeZoneBcp47(&s_data, &bad, &out) == mloc_errorInvalid &&
              mlocFormatTimeZoneBcp47(&empty, &zone, &out) == mloc_errorNotFound &&
              mlocFormatTimeZoneWindows(&s_data, &bad, &out) == mloc_errorInvalid &&
              mlocFormatTimeZoneWindows(&empty, &zone, &out) == mloc_errorNotFound,
          "formatting: nulls, a forged zone and no data");
}

// A small unit: ids aaaa (A/X, Windows name W), bbbb (deprecated for
// aaaa) and cccc (C/Y), their aliases, and W mapped in 001.
enum
{
    smallCodes = 24,
    smallAliases = 72,
    smallWindows = 88,
    smallMappings = 96,
    smallText = 104,
    smallSize = 123,
};

alignas(8) static uint8_t s_small[128];

static void Put(size_t at, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        s_small[at + (size_t)i] = (uint8_t)(value >> (8 * i));
    }
}

static DataSpan SmallUnit(void)
{
    memset(s_small, 0, sizeof(s_small));
    static const uint32_t s_counts[5] = {3, 2, 1, 1, 19};
    for (int i = 0; i < 5; i++)
    {
        Put((size_t)i * 4, s_counts[i], 4);
    }
    // Codes: id, name, id length, name length, preferred, Windows name.
    static const uint32_t s_codes[3][6] = {
        {0, 12, 4, 3, 0, 0}, {4, 0, 4, 0, 0, 0xFFFF}, {8, 15, 4, 3, 2, 0xFFFF}};
    static const int s_widths[6] = {4, 4, 1, 1, 2, 2};
    for (size_t c = 0; c < 3; c++)
    {
        for (size_t f = 0, at = smallCodes + c * 16; f < 6; at += (size_t)s_widths[f], f++)
        {
            Put(at, s_codes[c][f], s_widths[f]);
        }
    }
    Put(smallAliases, 12, 4);
    Put(smallAliases + 4, 3, 1);
    Put(smallAliases + 8, 15, 4);
    Put(smallAliases + 12, 3, 1);
    Put(smallAliases + 14, 2, 2);
    Put(smallWindows, 18, 4);
    Put(smallWindows + 4, 1, 1);
    Put(smallMappings, 12, 4);
    Put(smallMappings + 4, 3, 1);
    memcpy(s_small + smallMappings + 5, "001", 3);
    memcpy(s_small + smallText, "aaaabbbbccccA/XC/YW", 19);
    return (DataSpan){s_small, smallSize};
}

typedef struct Edit
{
    size_t at[4];
    uint32_t value[4];
    int bytes[4];
    bool valid;
    const char* what;
} Edit;

static void TestValidator(void)
{
    DataView view = {0};
    CHECK(mlocValidZoneCodes(&view, SmallUnit()), "the small unit");
    static const Edit s_edits[] = {
        {{20}, {1}, {4}, false, "the header's zero word"},
        {{16}, {20}, {4}, false, "a text past the payload"},
        {{smallText}, {'A'}, {1}, false, "an id with an upper-case letter"},
        {{smallCodes + 8}, {1}, {1}, false, "an id of one letter"},
        {{smallCodes}, {8}, {4}, false, "ids out of order"},
        {{smallCodes + 14}, {1}, {2}, false, "a code's zero bytes"},
        {{smallCodes + 12}, {1}, {2}, false, "a Windows name past the count"},
        {{smallCodes + 26}, {1}, {2}, false, "a replacement of its own"},
        {{smallCodes + 26}, {2}, {2}, true, "another replacement"},
        {{smallCodes + 28}, {0}, {2}, false, "a deprecated id with a Windows name"},
        {{smallCodes + 42}, {0}, {2}, false, "an id that is not its own preferred"},
        {{smallCodes + 36, smallCodes + 41, smallCodes + 42, smallAliases + 14},
         {0, 0, 0, 0},
         {4, 1, 2, 2},
         true,
         "a second deprecated id"},
        {{smallCodes + 36, smallCodes + 41, smallCodes + 42, smallAliases + 14},
         {0, 0, 1, 0},
         {4, 1, 2, 2},
         false,
         "a replacement that is deprecated"},
        {{smallAliases + 14}, {1}, {2}, false, "an alias of a deprecated id"},
        {{smallAliases + 5}, {1}, {1}, false, "an alias's zero byte"},
        {{smallAliases, smallAliases + 8}, {15, 12}, {4, 4}, false, "aliases out of order"},
        {{smallText + 13}, {' '}, {1}, false, "an identifier with a space"},
        {{smallWindows + 6}, {1}, {2}, false, "a first mapping past the count"},
        {{smallMappings + 5}, {'X' | 'X' << 8}, {3}, false, "no mapping for 001"},
        {{smallMappings + 5}, {'0' | 'A' << 8 | '1' << 16}, {3}, false, "a region that is not"},
        {{smallMappings + 4}, {0}, {1}, false, "an empty identifier"},
    };
    for (size_t i = 0; i < sizeof(s_edits) / sizeof(s_edits[0]); i++)
    {
        const Edit* e = &s_edits[i];
        DataSpan span = SmallUnit();
        for (int k = 0; k < 4 && e->bytes[k] != 0; k++)
        {
            Put(e->at[k], e->value[k], e->bytes[k]);
        }
        CHECK(mlocValidZoneCodes(&view, span) == e->valid, e->what);
    }
    DataSpan cut = SmallUnit();
    cut.size -= 1;
    CHECK(!mlocValidZoneCodes(&view, cut), "a payload cut short");
}

int main(void)
{
    mlocOpenBuiltinData(&s_builtin);
    TestBcp47();
    TestEveryCode();
    TestWindows();
    TestRefusals();
    TestValidator();
    return s_failures == 0 ? 0 : 1;
}
