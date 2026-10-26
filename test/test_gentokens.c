// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generators refuse a token longer than the buffer they read it into,
// with the file and the reason, instead of cutting it short: each site is
// given a token one byte longer than it holds, in a CLDR common directory
// this suite writes under the directory its argument names. A token that
// just fits is taken whole, by GenCopy and by a site.
//
// A site that stops the generator rather than returning is a case of its
// own: "test_gentokens <directory> <case>" runs it alone, and CTest passes
// it only on the line the generator prints as it stops (CMakeLists.txt
// names each case and its line). A case whose generator returns prints
// "returned" instead.

#include "alias_gen.h"
#include "collation_catalog.h"
#include "datafile.h"
#include "day_periods_gen.h"
#include "extension_gen.h"
#include "map.h"
#include "match_gen.h"
#include "names_gen.h"
#include "number_gen.h"
#include "parents_gen.h"
#include "plural_gen.h"
#include "table_gen.h"
#include "test_harness.h"
#include "unit_names_model.h"
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

// The common/bcp47 files the extension and names generators read, each an
// empty keyword but calendar.xml, which has this text.
static bool PutKeys(const char* calendar)
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
        put = put && Put(name, i > 0 ? "<ldmlBCP47><keyword/></ldmlBCP47>" : calendar);
    }
    return put;
}

static void TestExtensions(void)
{
    // One alias, as CLDR's files give some: a writer of no pairs is not
    // what is checked here.
    bool put = PutKeys("<ldmlBCP47><keyword><key name=\"ca\"><type name=\"islamic-civil\" "
                       "alias=\"islamicc\"/></key></keyword></ldmlBCP47>");
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
// supplementalData.xml with one parentLocales element of these
// attributes, listing locales under en_001, made into the parent unit.
static int Parents(const char* attributes, const char* locales)
{
    size_t size = strlen(attributes) + strlen(locales) + 256;
    char* text = malloc(size);
    if (text == nullptr)
    {
        return false;
    }
    snprintf(text, size,
             "<supplementalData><parentLocales%s><parentLocale parent=\"en_001\" "
             "locales=\"%s\"/></parentLocales></supplementalData>",
             attributes, locales);
    DataFile file = {0};
    int ok = Put("supplemental/supplementalData.xml", text) &&
             GenerateParentUnit(s_common, &file, s_error, sizeof(s_error));
    DataFileFree(&file);
    free(text);
    return ok;
}

static bool ParentsRefused(const char* locales, const char* reason)
{
    return Refused(Parents("", locales), reason);
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
    // A component list of 256 bytes: spaces, then a component.
    char attributes[300];
    snprintf(attributes, sizeof(attributes), " component=\"%*s\"", 256, "plurals");
    CHECK(Refused(Parents(attributes, "en_AU"), "a component list longer than the generator holds"),
          "supplementalData.xml: a component list of 256 bytes");
    snprintf(attributes, sizeof(attributes), " component=\"%*s\"", 255, "plurals");
    CHECK(Parents(attributes, "en_AU"), "and one of 255 bytes is taken");
    // A list naming no component the generator knows files no parent.
    CHECK(Parents(" component=\"unknown\"", "en_AU"), "a unit of no parent is made");
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

// timezone.xml with a live id of 15 bytes, the longest the codes hold,
// and one more type; windowsZones.xml maps to the live id's alias.
static int ZoneCodesWith(const char* type)
{
    char text[512];
    snprintf(text, sizeof(text),
             "<ldmlBCP47><keyword><key name=\"tz\">"
             "<type name=\"aaaaaaaaaaaaaaa\" alias=\"America/New_York\"/>%s"
             "</key></keyword></ldmlBCP47>",
             type);
    DataFile file = {0};
    int ok = Put("bcp47/timezone.xml", text) &&
             Put("supplemental/windowsZones.xml",
                 "<supplementalData><windowsZones><mapTimezones><mapZone "
                 "other=\"Eastern Standard Time\" territory=\"001\" type=\"America/New_York\"/>"
                 "</mapTimezones></windowsZones></supplementalData>") &&
             GenerateZoneCodes(s_common, &file, s_error, sizeof(s_error));
    DataFileFree(&file);
    return ok;
}

static void TestZoneCodeNames(void)
{
    char type[256];
    snprintf(type, sizeof(type), "<type name=\"usold\" deprecated=\"true\" preferred=\"%s\"/>",
             Long("", 64, ""));
    CHECK(Refused(ZoneCodesWith(type), "a preferred id longer than the generator holds"),
          "timezone.xml: a preferred id of 64 bytes");
    snprintf(type, sizeof(type), "<type name=\"usold\" deprecated=\"true\" preferred=\"%s\"/>",
             Long("", 16, ""));
    CHECK(Refused(ZoneCodesWith(type), "a replacement missing or deprecated"),
          "timezone.xml: a preferred id of 16 bytes is no id, not the 15 its first bytes spell");
    snprintf(type, sizeof(type), "<type name=\"usold\" deprecated=\"true\" preferred=\"%s\"/>",
             Long("", 15, ""));
    CHECK(ZoneCodesWith(type), "timezone.xml: a preferred id of 15 bytes is the live id");
    snprintf(type, sizeof(type), "<type name=\"usbos\" iana=\"%s\" alias=\"America/Boston\"/>",
             Long("", 64, ""));
    CHECK(Refused(ZoneCodesWith(type), "an identifier longer than the generator holds"),
          "timezone.xml: an IANA name of 64 bytes");
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
    TestZoneCodeNames();
}

// A collation file of the common directory read into a catalog with
// these attributes on its collation, and this default; the reader's
// answer.
static bool ReadCollation(CollationCatalog* catalog, const char* name, const char* attributes,
                          const char* defaultType)
{
    char relative[128];
    char path[4096];
    char text[1024];
    snprintf(relative, sizeof(relative), "supplemental/%s.xml", name);
    snprintf(path, sizeof(path), "%s/%s", s_common, relative);
    snprintf(text, sizeof(text),
             "<ldml><collations><defaultCollation>%s</defaultCollation><collation%s>"
             "<cr><![CDATA[&a<b]]></cr></collation></collations></ldml>",
             defaultType, attributes);
    return Put(relative, text) && CollationCatalogRead(catalog, path, s_error, sizeof(s_error));
}

static void TestCollationCatalog(void)
{
    CollationCatalog catalog = {0};
    char attributes[128];
    snprintf(attributes, sizeof(attributes), " type=\"%s\"", Long("", 64, ""));
    CHECK(!ReadCollation(&catalog, "de", attributes, "standard") &&
              strstr(s_error, "a collation type or alt longer than the generator holds") != nullptr,
          "a collation type of 64 bytes");
    snprintf(attributes, sizeof(attributes), " type=\"standard\" alt=\"%s\"", Long("", 32, ""));
    CHECK(!ReadCollation(&catalog, "de", attributes, "standard") &&
              strstr(s_error, "a collation type or alt longer than the generator holds") != nullptr,
          "a collation alt of 32 bytes");
    CHECK(!ReadCollation(&catalog, "de", "", Long("", 64, "")) &&
              strstr(s_error, "a default collation longer than the generator holds") != nullptr,
          "a default collation of 64 bytes");
    CollationCatalogFree(&catalog);
    // A file whose name is 63 bytes, the most a name holds: an import of
    // it finds it, and one of a tag a byte longer finds nothing rather
    // than it.
    catalog = (CollationCatalog){0};
    char name[64];
    snprintf(name, sizeof(name), "%s", Long("", 63, ""));
    CHECK(ReadCollation(&catalog, name, "", "standard"), "a collation file of a 63-byte name");
    CollationCatalogSort(&catalog);
    CHECK(CollationCatalogImport(&catalog, Long("", 63, "")) != nullptr,
          "an import of the 63-byte name finds it");
    CHECK(CollationCatalogImport(&catalog, Long("", 64, "")) == nullptr,
          "an import of 64 bytes finds nothing, not the name its first bytes spell");
    // de with a type of 63 bytes, the most a type holds: an import of it
    // finds it, and one of a type a byte longer finds nothing.
    snprintf(attributes, sizeof(attributes), " type=\"%s\"", Long("", 63, ""));
    CHECK(ReadCollation(&catalog, "de", attributes, "standard"), "a collation type of 63 bytes");
    CollationCatalogSort(&catalog);
    CHECK(CollationCatalogImport(&catalog, Long("de-u-co-", 63, "")) != nullptr,
          "an import of the 63-byte type finds it");
    CHECK(CollationCatalogImport(&catalog, Long("de-u-co-", 64, "")) == nullptr,
          "an import of a 64-byte type finds nothing, not the type its first bytes spell");
    CollationCatalogFree(&catalog);
}

// The cases that stop the generator.

static void RunZoneRegion(void)
{
    NamesInput input = {.commonDirectory = s_common};
    DataFile file = {0};
    if (Put("bcp47/timezone.xml", "<ldmlBCP47><keyword><key name=\"tz\"><type name=\"usnyc\" "
                                  "alias=\"America/New_York\" region=\"USA\"/></key></keyword>"
                                  "</ldmlBCP47>"))
    {
        (void)GenerateZoneNames(&input, &file, s_error, sizeof(s_error));
    }
}

static void RunExtensionAliases(void)
{
    char text[1024];
    snprintf(text, sizeof(text),
             "<ldmlBCP47><keyword><key name=\"ca\"><type name=\"gregorian\" alias=\"%s\"/></key>"
             "</keyword></ldmlBCP47>",
             Long("", 512, ""));
    DataFile file = {0};
    if (PutKeys(text) && Put("supplemental/supplementalMetadata.xml",
                             "<supplementalData><metadata><alias/></metadata></supplementalData>"))
    {
        (void)GenerateExtensionUnit(s_common, &file, s_error, sizeof(s_error));
    }
}

static void RunNameWord(void)
{
    char text[1024];
    snprintf(text, sizeof(text),
             "<ldmlBCP47><keyword><key name=\"ca\"><type name=\"gregorian\" alias=\"%s\"/></key>"
             "</keyword></ldmlBCP47>",
             Long("", 256, ""));
    NamesInput input = {.commonDirectory = s_common};
    DataFile file = {0};
    DataFile subdivisions = {0};
    if (PutKeys(text))
    {
        (void)GenerateNames(&input, &file, &subdivisions, s_error, sizeof(s_error));
    }
}

static void RunAliases(const char* aliases)
{
    char text[2048];
    snprintf(text, sizeof(text),
             "<supplementalData><metadata><alias>%s</alias></metadata>"
             "</supplementalData>",
             aliases);
    DataFile file = {0};
    if (Put("supplemental/supplementalMetadata.xml", text))
    {
        (void)GenerateAliasUnit(s_common, &file, s_error, sizeof(s_error));
    }
}

static void RunAliasId(void)
{
    char alias[512];
    snprintf(alias, sizeof(alias), "<languageAlias type=\"%s\" replacement=\"en\"/>",
             Long("", 128, ""));
    RunAliases(alias);
}

static void RunAliasReplacements(void)
{
    // US, then spaces, so that a list cut at 255 bytes drops CA whole.
    char alias[512];
    snprintf(alias, sizeof(alias), "<territoryAlias type=\"QQ\" replacement=\"US%*sCA\"/>", 254,
             " ");
    RunAliases(alias);
}

static void RunNumberSystem(void)
{
    DataFile file = {0};
    if (Put("supplemental/numberingSystems.xml",
            "<supplementalData><numberingSystems><numberingSystem id=\"abcdefghi\" "
            "type=\"numeric\" digits=\"0123456789\"/></numberingSystems></supplementalData>"))
    {
        (void)GenerateNumberUnits(s_common, &file, nullptr, s_error, sizeof(s_error));
    }
}

// The match generator's three files, with languageInfo.xml's written_new
// matches holding this text.
static void RunMatches(const char* matches)
{
    char text[2048];
    snprintf(text, sizeof(text),
             "<supplementalData><languageMatching><languageMatches type=\"written_new\">%s"
             "</languageMatches></languageMatching></supplementalData>",
             matches);
    DataFile file = {0};
    if (Put("supplemental/supplementalData.xml",
            "<supplementalData><territoryContainment/></supplementalData>") &&
        Put("supplemental/likelySubtags.xml",
            "<supplementalData><likelySubtags/></supplementalData>") &&
        Put("supplemental/languageInfo.xml", text))
    {
        (void)GenerateMatchUnit(s_common, &file, s_error, sizeof(s_error));
    }
}

static void RunMatchParadigm(void)
{
    char matches[256];
    snprintf(matches, sizeof(matches), "<paradigmLocales locales=\"%s en\"/>", Long("", 48, ""));
    RunMatches(matches);
}

// GeneratePluralUnits on plurals.xml with these rules, and no ordinals or
// ranges.
static void RunPlurals(const char* rules)
{
    size_t size = strlen(rules) + 128;
    char* text = malloc(size);
    DataFile file = {0};
    char* source = nullptr;
    size_t sourceSize = 0;
    if (text != nullptr)
    {
        snprintf(text, size,
                 "<supplementalData><plurals type=\"cardinal\">%s</plurals></supplementalData>",
                 rules);
    }
    if (text != nullptr && Put("supplemental/plurals.xml", text) &&
        Put("supplemental/ordinals.xml",
            "<supplementalData><plurals type=\"ordinal\"/></supplementalData>") &&
        Put("supplemental/pluralRanges.xml", "<supplementalData><plurals/></supplementalData>"))
    {
        (void)GeneratePluralUnits(s_common, &file, &source, &sourceSize, s_error, sizeof(s_error));
    }
    free(text);
}

static void RunPluralLocales(void)
{
    RunPlurals(
        Long("<pluralRules locales=\"", 4096, "\"><pluralRule count=\"other\"/></pluralRules>"));
}

static void RunPluralLocaleId(void)
{
    RunPlurals(
        Long("<pluralRules locales=\"en ", 64, "\"><pluralRule count=\"other\"/></pluralRules>"));
}

// plurals.xml whose rule for one is this condition.
static void RunPluralCondition(const char* condition)
{
    char rules[2048];
    snprintf(rules, sizeof(rules),
             "<pluralRules locales=\"en\"><pluralRule count=\"one\">%s</pluralRule>"
             "<pluralRule count=\"other\"/></pluralRules>",
             condition);
    RunPlurals(rules);
}

static void RunPluralConditionWords(void)
{
    // 131 words: 33 terms "n = 1" and the 32 "or"s between them.
    char condition[512] = "n = 1";
    size_t at = 5;
    for (int i = 1; i < 33; i++)
    {
        memcpy(condition + at, " or n = 1", 9);
        at += 9;
    }
    condition[at] = '\0';
    RunPluralCondition(condition);
}

static void RunFileName(void)
{
    char name[128];
    snprintf(name, sizeof(name), "main/%s.xml", Long("", 64, ""));
    int count = 0;
    if (Put(name, "<ldml/>"))
    {
        GenFreeList(GenListXml(s_common, "main", &count), count);
    }
}

// The names generator on a common directory of its own: a cut of CLDR's
// DTD, root's nested brackets, empty keys, and this text in root's
// localeDisplayNames.
static void RunNames(const char* names)
{
    char root[2048];
    snprintf(root, sizeof(root),
             "<ldml><identity><version number=\"1\"/><language type=\"root\"/></identity>"
             "<localeDisplayNames>%s</localeDisplayNames><characters>"
             "<nestedBracketReplacement bracket=\"(\">[</nestedBracketReplacement>"
             "<nestedBracketReplacement bracket=\")\">]</nestedBracketReplacement>"
             "<nestedBracketReplacement bracket=\"\xEF\xBC\x88\">\xEF\xBC\xBB"
             "</nestedBracketReplacement>"
             "<nestedBracketReplacement bracket=\"\xEF\xBC\x89\">\xEF\xBC\xBD"
             "</nestedBracketReplacement></characters></ldml>",
             names);
    bool put =
        Put("dtd/ldml.dtd", "<!ELEMENT ldml (identity, localeDisplayNames?, characters?) >\n"
                            "<!ELEMENT identity (version, language) >\n"
                            "<!ELEMENT version EMPTY >\n"
                            "<!ATTLIST version number CDATA #REQUIRED >\n"
                            "    <!--@METADATA-->\n"
                            "<!ELEMENT localeDisplayNames (languages?, territories?) >\n"
                            "<!ELEMENT languages (language*) >\n"
                            "<!ELEMENT language ( #PCDATA ) >\n"
                            "<!ATTLIST language type NMTOKEN #REQUIRED >\n"
                            "<!ATTLIST language alt NMTOKENS #IMPLIED >\n"
                            "<!ELEMENT territories (territory*) >\n"
                            "<!ELEMENT territory ( #PCDATA ) >\n"
                            "<!ATTLIST territory type NMTOKEN #REQUIRED >\n"
                            "<!ATTLIST territory alt NMTOKENS #IMPLIED >\n"
                            "<!ELEMENT characters (nestedBracketReplacement*) >\n"
                            "<!ELEMENT nestedBracketReplacement ( #PCDATA ) >\n"
                            "<!ATTLIST nestedBracketReplacement bracket CDATA #REQUIRED >\n") &&
        Put("supplemental/supplementalData.xml", "<supplementalData/>") &&
        Put("supplemental/likelySubtags.xml",
            "<supplementalData><likelySubtags/></supplementalData>") &&
        Put("main/root.xml", root) && PutKeys("<ldmlBCP47><keyword/></ldmlBCP47>");
    static Cldr s_cldr;
    static Keys s_keys;
    if (!put || !CldrOpen(&s_cldr, s_common, s_error, sizeof(s_error)) ||
        !KeysOpen(&s_keys, s_common, s_error, sizeof(s_error)))
    {
        printf("%s\n", s_error);
        return;
    }
    const char* const ids[] = {"root"};
    NamesInput input = {
        .commonDirectory = s_common, .cldr = &s_cldr, .keys = &s_keys, .ids = ids, .count = 1};
    DataFile file = {0};
    DataFile subdivisions = {0};
    (void)GenerateNames(&input, &file, &subdivisions, s_error, sizeof(s_error));
}

static void RunNameLanguage(void)
{
    // "l:" and 127 bytes, the longest type an attribute holds.
    char names[256];
    snprintf(names, sizeof(names), "<languages><language type=\"%s\">x</language></languages>",
             Long("", 127, ""));
    RunNames(names);
}

static void RunNameTerritory(void)
{
    char names[256];
    snprintf(names, sizeof(names),
             "<territories><territory type=\"%s\">x</territory></territories>", Long("", 127, ""));
    RunNames(names);
}

static void RunNameAlt(void)
{
    // "r:US", "%" and 124 bytes.
    char names[256];
    snprintf(names, sizeof(names),
             "<territories><territory type=\"US\" alt=\"%s\">x</territory></territories>",
             Long("", 124, ""));
    RunNames(names);
}

static void RunUnitChain(void)
{
    UnitName name = {.kind = unitIdUnit};
    char chain[128];
    (void)NamesChainOf(&name, Long("", 128, ""), nullptr, widthLong, unitNameOther, chain,
                       sizeof(chain));
}

typedef struct DeathCase
{
    const char* name;
    void (*run)(void);
} DeathCase;

static const DeathCase s_deathCases[] = {
    {"zone_region", RunZoneRegion},
    {"extension_aliases", RunExtensionAliases},
    {"name_word", RunNameWord},
    {"alias_id", RunAliasId},
    {"alias_replacements", RunAliasReplacements},
    {"number_system", RunNumberSystem},
    {"match_paradigm", RunMatchParadigm},
    {"plural_locales", RunPluralLocales},
    {"plural_locale_id", RunPluralLocaleId},
    {"plural_condition_words", RunPluralConditionWords},
    {"file_name", RunFileName},
    {"name_language", RunNameLanguage},
    {"name_territory", RunNameTerritory},
    {"name_alt", RunNameAlt},
    {"unit_chain", RunUnitChain},
};

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3)
    {
        printf("usage: test_gentokens <directory with supplemental/ and bcp47/> [case]\n");
        return 2;
    }
    s_common = argv[1];
    if (argc == 3)
    {
        for (size_t i = 0; i < sizeof(s_deathCases) / sizeof(s_deathCases[0]); i++)
        {
            if (strcmp(argv[2], s_deathCases[i].name) == 0)
            {
                s_deathCases[i].run();
                printf("returned\n");
                return 0;
            }
        }
        printf("no case %s\n", argv[2]);
        return 2;
    }
    TestCopy();
    TestPrint();
    TestDayPeriods();
    TestExtensions();
    TestParents();
    TestPlurals();
    TestZones();
    TestCollationCatalog();
    return s_failures == 0 ? 0 : 1;
}
