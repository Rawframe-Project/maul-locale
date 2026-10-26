// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generators refuse a token longer than the buffer they read it into,
// with the file and the reason, instead of cutting it short: each site is
// given a token one byte longer than it holds, in a CLDR common directory
// this suite writes under the directory its argument names. A token that
// just fits is taken whole, by GenCopy and by a site.

#include "datafile.h"
#include "day_periods_gen.h"
#include "extension_gen.h"
#include "map.h"
#include "names_gen.h"
#include "parents_gen.h"
#include "plural_gen.h"
#include "table_gen.h"
#include "test_harness.h"
#include "zone_codes_gen.h"
#include "zone_names_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_common;
static char s_error[1024];
static char s_long[9000];

// A file of the common directory with this text; false when it cannot be
// written.
static bool Put(const char* name, const char* text)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", s_common, name);
    FILE* file = fopen(path, "wb");
    if (file == nullptr)
    {
        printf("  cannot write %s\n", path);
        return false;
    }
    bool written = fputs(text, file) >= 0;
    return fclose(file) == 0 && written;
}

// Text in s_long: a prefix, then a token of length bytes of one letter,
// then a suffix.
static const char* Long(const char* prefix, size_t length, const char* suffix)
{
    size_t start = strlen(prefix);
    snprintf(s_long, sizeof(s_long), "%s", prefix);
    memset(s_long + start, 'a', length);
    snprintf(s_long + start + length, sizeof(s_long) - start - length, "%s", suffix);
    return s_long;
}

// Whether a generator's answer is a refusal for this reason.
static bool Refused(int ok, const char* reason)
{
    if (ok || strstr(s_error, reason) == nullptr)
    {
        printf("  %s, want \"%s\"\n", ok ? "taken" : s_error, reason);
        return false;
    }
    return true;
}

static void TestCopy(void)
{
    char out[8] = "xxxxxxx";
    CHECK(GenCopy(out, sizeof(out), "abcdefgh", 7) && strcmp(out, "abcdefg") == 0,
          "GenCopy takes a token of capacity - 1 bytes whole, NUL-ended");
    CHECK(!GenCopy(out, sizeof(out), "abcdefgh", 8) && out[0] == '\0',
          "and refuses one of capacity bytes, leaving the buffer empty");
}

static void TestPrint(void)
{
    char out[8] = "xxxxxxx";
    size_t size = 2;
    memcpy(out, "ab", 2);
    CHECK(GenPrint(out, sizeof(out), &size, "%s%d", "cd", 345) && size == 7 &&
              strcmp(out, "abcd345") == 0,
          "GenPrint appends a text that leaves capacity - 1 bytes in use, NUL-ended");
    size = 2;
    CHECK(!GenPrint(out, sizeof(out), &size, "%s%d", "cd", 3456) && size == 2 &&
              strcmp(out, "ab") == 0,
          "and refuses one that would fill the capacity, keeping the text before it");
    size = sizeof(out);
    CHECK(!GenPrint(out, sizeof(out), &size, "%s", ""), "and refuses any text at a full capacity");
}

// dayPeriods.xml with root and a locale of this many bytes in one list.
static bool PutDayPeriods(size_t length)
{
    char text[512];
    snprintf(text, sizeof(text),
             "<supplementalData><dayPeriodRuleSet><dayPeriodRules locales=\"root %s\">"
             "<dayPeriodRule type=\"morning1\" from=\"00:00\" before=\"24:00\"/>"
             "</dayPeriodRules></dayPeriodRuleSet></supplementalData>",
             Long("", length, ""));
    return Put("supplemental/dayPeriods.xml", text);
}

static void TestDayPeriods(void)
{
    Map rules = {0};
    CHECK(PutDayPeriods(31) && DayPeriodRulesLoad(&rules, s_common, s_error, sizeof(s_error)) &&
              MapGet(&rules, Long("", 31, "")) != nullptr,
          "dayPeriods.xml: a locale of 31 bytes is taken, under its whole key");
    DayPeriodRulesFree(&rules);
    rules = (Map){0};
    CHECK(PutDayPeriods(32) &&
              Refused(DayPeriodRulesLoad(&rules, s_common, s_error, sizeof(s_error)),
                      "a locale longer than the generator holds"),
          "dayPeriods.xml: a locale of 32 bytes");
    DayPeriodRulesFree(&rules);
}

static void TestExtensions(void)
{
    static const char* const s_files[] = {
        "calendar",
        "collation",
        "currency",
        "measure",
        "number",
        "segmentation",
        "timezone",
        "transform",
        "transform_hybrid",
        "transform_ime",
        "transform_keyboard",
        "transform_mt",
        "transform_private_use",
        "transform-destination",
        "variant",
    };
    bool put = true;
    for (size_t i = 0; i < sizeof(s_files) / sizeof(s_files[0]); i++)
    {
        char name[64];
        snprintf(name, sizeof(name), "bcp47/%s.xml", s_files[i]);
        // One alias, as CLDR's files give some: a writer of no pairs is not
        // what is checked here.
        put = put && Put(name, i > 0 ? "<ldmlBCP47><keyword/></ldmlBCP47>"
                                     : "<ldmlBCP47><keyword><key name=\"ca\"><type "
                                       "name=\"islamic-civil\" alias=\"islamicc\"/></key>"
                                       "</keyword></ldmlBCP47>");
    }
    put = put && Put("supplemental/supplementalMetadata.xml",
                     Long("<supplementalData><metadata><alias><subdivisionAlias type=\"usqq\" "
                          "replacement=\"",
                          64, " usny\"/></alias></metadata></supplementalData>"));
    DataFile file = {0};
    CHECK(put && Refused(GenerateExtensionUnit(s_common, &file, s_error, sizeof(s_error)),
                         "a subdivision replacement longer than the generator holds"),
          "supplementalMetadata.xml: a replacement's first subtag of 64 bytes");
    DataFileFree(&file);
}

// Whether supplementalData.xml with this parentLocales list is refused for
// this reason.
static bool ParentsRefused(const char* locales, const char* reason)
{
    size_t size = strlen(locales) + 256;
    char* text = malloc(size);
    if (text == nullptr)
    {
        return false;
    }
    snprintf(text, size,
             "<supplementalData><parentLocales><parentLocale parent=\"en_001\" locales=\"%s\"/>"
             "</parentLocales></supplementalData>",
             locales);
    DataFile file = {0};
    bool refused = Put("supplemental/supplementalData.xml", text) &&
                   Refused(GenerateParentUnit(s_common, &file, s_error, sizeof(s_error)), reason);
    DataFileFree(&file);
    free(text);
    return refused;
}

static void TestParents(void)
{
    // Two-letter ids, each with a space after it, to 8192 bytes.
    char list[8193];
    for (size_t i = 0; i < 8192; i++)
    {
        list[i] = i % 3 == 2 ? ' ' : 'a';
    }
    list[8192] = '\0';
    CHECK(Put("supplemental/likelySubtags.xml",
              "<supplementalData><likelySubtags/></supplementalData>") &&
              ParentsRefused(list, "a locale list longer than the generator holds"),
          "supplementalData.xml: a list of 8192 bytes");
    CHECK(
        ParentsRefused(Long("en_AU ", 64, " en_GB"), "a locale id longer than the generator holds"),
        "supplementalData.xml: a child locale id of 64 bytes");
}

// Whether plurals.xml with this pluralRules element is refused for this
// reason.
static bool PluralsRefused(const char* rules, const char* reason)
{
    size_t size = strlen(rules) + 128;
    char* text = malloc(size);
    if (text == nullptr)
    {
        return false;
    }
    snprintf(text, size,
             "<supplementalData><plurals type=\"cardinal\">%s</plurals></supplementalData>", rules);
    bool refused = Put("supplemental/plurals.xml", text) &&
                   Put("supplemental/ordinals.xml",
                       "<supplementalData><plurals type=\"ordinal\"/></supplementalData>") &&
                   Refused(PrintPluralSamples(s_common, s_error, sizeof(s_error)), reason);
    free(text);
    return refused;
}

static void TestPlurals(void)
{
    CHECK(PluralsRefused(
              Long("<pluralRules locales=\"en ", 64,
                   "\"><pluralRule count=\"other\"> @integer 0</pluralRule></pluralRules>"),
              "a locale id longer than the generator holds"),
          "plurals.xml: a locale id of 64 bytes");
    // Samples of 4096 bytes: "1, " 1365 times and a space.
    char samples[4200] = "<pluralRules locales=\"en\"><pluralRule count=\"other\"> @integer";
    size_t at = strlen(samples);
    for (int i = 0; i < 1365; i++)
    {
        memcpy(samples + at, " 1,", 3);
        at += 3;
    }
    snprintf(samples + at, sizeof(samples) - at, " </pluralRule></pluralRules>");
    CHECK(PluralsRefused(samples, "samples longer than the generator holds"),
          "plurals.xml: samples of 4096 bytes");
}

static void TestZones(void)
{
    // The tz key with one live id, whose aliases end with one of 64 bytes.
    CHECK(
        Put("bcp47/timezone.xml", Long("<ldmlBCP47><keyword><key name=\"tz\"><type name=\"usnyc\" "
                                       "alias=\"America/New_York ",
                                       64, "\"/></key></keyword></ldmlBCP47>")),
        "timezone.xml with a long alias");
    NamesInput input = {.commonDirectory = s_common};
    DataFile file = {0};
    CHECK(Refused(GenerateZoneNames(&input, &file, s_error, sizeof(s_error)),
                  "an alias longer than the generator holds"),
          "the zone names' timezone.xml: an alias of 64 bytes");
    DataFileFree(&file);
    // The codes read every alias as one of a tz id's; a mapZone then lists
    // an identifier of 64 bytes.
    CHECK(Put("bcp47/timezone.xml", "<ldmlBCP47><keyword><key name=\"tz\"><type name=\"usnyc\" "
                                    "alias=\"America/New_York\"/></key></keyword></ldmlBCP47>") &&
              Put("supplemental/windowsZones.xml",
                  Long("<supplementalData><windowsZones><mapTimezones><mapZone "
                       "other=\"Eastern Standard Time\" territory=\"001\" type=\"",
                       64, "\"/></mapTimezones></windowsZones></supplementalData>")),
          "windowsZones.xml with a long identifier");
    file = (DataFile){0};
    CHECK(Refused(GenerateZoneCodes(s_common, &file, s_error, sizeof(s_error)),
                  "an identifier longer than the generator holds"),
          "windowsZones.xml: an identifier of 64 bytes");
    DataFileFree(&file);
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("usage: test_gentokens <directory with supplemental/ and bcp47/>\n");
        return 2;
    }
    s_common = argv[1];
    TestCopy();
    TestPrint();
    TestDayPeriods();
    TestExtensions();
    TestParents();
    TestPlurals();
    TestZones();
    return s_failures == 0 ? 0 : 1;
}
