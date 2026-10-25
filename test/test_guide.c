// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The C snippets of docs/user/, each as written there (tools/check_guide.py
// checks it), run on the compiled-in data and the data files: each
// function gives a snippet what it uses and checks what it keeps.

#include "test_harness.h"

#include "maul-locale/calendar.h"
#include "maul-locale/collate.h"
#include "maul-locale/currency.h"
#include "maul-locale/datetime.h"
#include "maul-locale/grammar.h"
#include "maul-locale/list.h"
#include "maul-locale/locale.h"
#include "maul-locale/message.h"
#include "maul-locale/names.h"
#include "maul-locale/number.h"
#include "maul-locale/plural.h"
#include "maul-locale/relative.h"
#include "maul-locale/search.h"
#include "maul-locale/segment.h"
#include "maul-locale/spellout.h"
#include "maul-locale/unit.h"
#include "maul-locale/zone.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mlocData s_builtin;

// Every data file the snippets read, then the compiled-in data.
static const char* const s_files[] = {
    "dates.mloc", "zone-names.mloc", "names.mloc",    "subdivisions.mloc", "currency-names.mloc",
    "units.mloc", "spellout.mloc",   "relative.mloc", "collation.mloc",    "segments.mloc"};
enum
{
    fileCount = sizeof(s_files) / sizeof(s_files[0])
};
static mlocData s_sources[fileCount + 1];
static void* s_bytes[fileCount];
static mlocDataSet s_all = {s_sources, fileCount + 1};

// A locale from its id, which the test knows to be valid.
static mlocLocale Locale(const char* id)
{
    mlocLocale locale = {0};
    CHECK(mlocCanonicalizeLocale(&s_all, id, strlen(id), &locale, nullptr).status == mloc_success,
          id);
    return locale;
}

// A data file of src/generated/, read whole into memory from malloc,
// which is 8-byte aligned; null when it cannot be read. The files stay
// loaded for the whole run, as a program keeps the data it uses.
static void* LoadFile(const char* name, size_t* sizeOut)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", MLOC_DATA_DIR, name);
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return nullptr;
    }
    void* bytes = nullptr;
    long size = 0;
    if (fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 && fseek(file, 0, SEEK_SET) == 0)
    {
        bytes = malloc((size_t)size);
        if (bytes != nullptr && fread(bytes, 1, (size_t)size, file) != (size_t)size)
        {
            free(bytes);
            bytes = nullptr;
        }
    }
    fclose(file);
    *sizeOut = (size_t)size;
    return bytes;
}

// Whether a written text is the one expected.
static bool Wrote(const mlocOutput* output, const char* expected)
{
    return output->length == strlen(expected) &&
           memcmp(output->buffer, expected, output->length) == 0;
}

static void GuideFirstNumber(void)
{
    mlocData builtin = s_builtin;
    mlocDataSet set = {&builtin, 1};
    mlocLocale locale;
    mlocTextResult parsed = mlocParseLocale("de-CH", 5, &locale);
    mlocNumberFormatter formatter;
    mlocDecimal value;
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (parsed.status == mloc_success &&
        mlocInitNumberFormatter(&formatter, &set, &locale, nullptr) >= 0 &&
        mlocMakeShortestDecimal(1234567.891, &value) == mloc_success &&
        mlocFormatNumber(&formatter, &value, &output) == mloc_success)
    {
        // text holds output.length bytes: "1'234'567.891"
    }
    CHECK(Wrote(&output, "1'234'567.891"), "Swiss German groups with an apostrophe");
}

static void GuideOpenFile(void)
{
    size_t size = 0;
    void* bytes = LoadFile("units.mloc", &size);
    CHECK(bytes != nullptr, "units.mloc reads");
    mlocData sources[2];
    mlocDataSet set = {sources, 2};
    if (mlocOpenData(&sources[0], bytes, size) == mloc_success)
    {
        mlocOpenBuiltinData(&sources[1]);
        // set finds what the file has in it first, the rest built in
    }
    CHECK(mlocGetDataLocaleCount(&sources[0]) == 219 && mlocGetDataLocaleCount(&sources[1]) == 597,
          "the locales with unit names and with built-in data of their own");
    (void)set;
    free(bytes);
}

static void GuideCanonicalize(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale;
    mlocLocaleChanges changes = 0;
    mlocTextResult result = mlocCanonicalizeLocale(&set, "iw_IL", 5, &locale, &changes);
    if (result.status == mloc_success)
    {
        // locale is he-IL; changes has mloc_changedLanguage
    }
    mlocLocale expected = Locale("he-IL");
    CHECK(memcmp(&locale, &expected, sizeof(locale)) == 0 && changes == mloc_changedLanguage,
          "iw_IL is he-IL");
}

static void GuideParents(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("es-MX");
    mlocLocale current = locale;
    mlocLocale parent;
    while (mlocGetParentLocale(&set, mloc_componentMain, &current, &parent) == mloc_success)
    {
        // look the program's own resource up for current, then go on
        current = parent;
    }
    mlocLocale root = Locale("und");
    mlocLocale latinAmerica = Locale("es-419");
    CHECK(memcmp(&current, &root, sizeof(root)) == 0, "the chain ends at root");
    CHECK(mlocGetParentLocale(&set, mloc_componentMain, &locale, &parent) == mloc_success &&
              memcmp(&parent, &latinAmerica, sizeof(parent)) == 0,
          "es-MX inherits from es-419");
}

static void GuideMatch(void)
{
    mlocDataSet set = s_all;
    mlocLocale supported[3] = {Locale("en"), Locale("de"), Locale("fr-CA")};
    mlocLocale desired[2] = {Locale("de-AT"), Locale("en")};
    // supported: what the program has translations for, the default first
    mlocMatchEntry entries[3];
    mlocMatch match = {-1, -1};
    mlocLocale chosen = supported[0];
    mlocLocale use = supported[0];
    if (mlocPrepareMatch(&set, supported, 3, &supported[0], entries) == mloc_success &&
        mlocMatchLocales(&set, supported, entries, 3, desired, 2, nullptr, &match) ==
            mloc_success &&
        match.supported >= 0)
    {
        chosen = supported[match.supported];
        if (mlocCombineMatch(&desired[match.desired], &chosen, &use) != mloc_success)
        {
            use = chosen;
        }
    }
    // chosen picks the translations, use the formats
    mlocLocale austrian = Locale("de-AT");
    CHECK(memcmp(&chosen, &supported[1], sizeof(chosen)) == 0,
          "de-AT picks the German translations");
    CHECK(memcmp(&use, &austrian, sizeof(use)) == 0, "and formats as in Austria");
}

static void GuidePreferredList(void)
{
    mlocDataSet set = s_all;
    const char* list = "iw-IL,en_US-x-!,de-AT,,en";
    size_t length = strlen(list);
    // list: the user's languages, most preferred first, comma-separated, as
    // Maul Window's mwinGetPreferredLocales gives them
    mlocLocale desired[8];
    size_t count = 0;
    for (size_t start = 0; start < length && count < 8;)
    {
        const char* comma = memchr(list + start, ',', length - start);
        size_t end = comma != nullptr ? (size_t)(comma - list) : length;
        mlocTextResult parsed =
            mlocCanonicalizeLocale(&set, list + start, end - start, &desired[count], nullptr);
        if (parsed.status == mloc_success)
        {
            count++; // a tag that does not parse is skipped
        }
        start = end + 1;
    }
    // count 0: the platform did not say, so the program's default stays;
    // else match desired[0] to desired[count - 1] as above
    CHECK(count == 3, "three of the five entries are locales");
    mlocLocale supported[3] = {Locale("en"), Locale("de"), Locale("fr-CA")};
    mlocMatchEntry entries[3];
    mlocMatch match = {-1, -1};
    CHECK(mlocPrepareMatch(&set, supported, 3, &supported[0], entries) == mloc_success &&
              mlocMatchLocales(&set, supported, entries, 3, desired, count, nullptr, &match) ==
                  mloc_success &&
              match.supported == 1 && match.desired == 1,
          "Hebrew has no translations, so de-AT picks German");
}

static void GuidePreferences(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en-GB");
    mlocLocalePreferences preferences;
    mlocResult found = mlocGetLocalePreferences(&set, &locale, &preferences);
    if (found == mloc_success)
    {
        // en-GB: preferences.hourCycle is mloc_hourFormatH23,
        // preferences.firstDay 1 (Monday), measurementSystem mloc_measurementUk
    }
    CHECK(found == mloc_success && preferences.hourCycle == mloc_hourFormatH23 &&
              preferences.firstDay == 1 && preferences.measurementSystem == mloc_measurementUk,
          "the United Kingdom's preferences");
}

static void GuideLocaleName(void)
{
    mlocDataSet set = s_all;
    mlocLocale displayLocale = Locale("fr");
    mlocLocale named = Locale("en-GB");
    char text[128];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocLocaleNameOptions options = {.capitalization = mloc_capitalizationListOrMenu};
    if (mlocGetLocaleName(&set, &displayLocale, &named, &options, &output) == mloc_success)
    {
        // displayLocale fr, named en-GB: "Anglais britannique"
    }
    CHECK(Wrote(&output, "Anglais britannique"), "French names British English in a menu");
}

static void GuidePlural(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("pl");
    mlocDecimal value;
    CHECK(mlocMakeDecimal(5, &value) == mloc_success, "5");
    mlocPluralRules rules;
    mlocPluralCategory category = mloc_pluralOther;
    if (mlocInitPluralRules(&rules, &set, &locale, mloc_pluralCardinal) == mloc_success &&
        mlocSelectPlural(&rules, &value, &category) == mloc_success)
    {
        // pl, 5: mloc_pluralMany ("5 plików"); 2: mloc_pluralFew ("2 pliki")
    }
    CHECK(category == mloc_pluralMany, "Polish 5 is many");
    CHECK(mlocMakeDecimal(2, &value) == mloc_success &&
              mlocSelectPlural(&rules, &value, &category) == mloc_success &&
              category == mloc_pluralFew,
          "Polish 2 is few");
}

static void GuideCompact(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en");
    mlocDecimal value;
    CHECK(mlocMakeDecimal(1234567, &value) == mloc_success, "1234567");
    mlocNumberOptions options = {.notation = mloc_notationCompactShort};
    mlocNumberFormatter formatter;
    char text[32];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocInitNumberFormatter(&formatter, &set, &locale, &options) >= 0 &&
        mlocFormatNumber(&formatter, &value, &output) == mloc_success)
    {
        // en, 1234567: "1.2M"; ja: "123万"
    }
    CHECK(Wrote(&output, "1.2M"), "English short compact");
    locale = Locale("ja");
    output.length = 0;
    CHECK(mlocInitNumberFormatter(&formatter, &set, &locale, &options) >= 0 &&
              mlocFormatNumber(&formatter, &value, &output) == mloc_success &&
              Wrote(&output, "123\xE4\xB8\x87"),
          "Japanese counts by ten thousands");
}

static void GuideCurrency(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("de-AT");
    mlocDecimal value;
    CHECK(mlocParseDecimal("1234.5", 6, &value).status == mloc_success, "1234.5");
    mlocCurrencyFormatter formatter;
    char text[32];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocInitCurrencyFormatter(&formatter, &set, &locale, "EUR", 3, nullptr) >= 0 &&
        mlocFormatCurrency(&formatter, &value, &output) == mloc_success)
    {
        // de-AT, 1234.5: "€ 1.234,50"; fr: "1 234,50 €"
    }
    CHECK(Wrote(&output, "\xE2\x82\xAC\xC2\xA0"
                         "1.234,50"),
          "Austria puts the euro first");
    locale = Locale("fr");
    CHECK(mlocInitCurrencyFormatter(&formatter, &set, &locale, "EUR", 3, nullptr) >= 0 &&
              mlocFormatCurrency(&formatter, &value, &output) == mloc_success &&
              Wrote(&output, "1\xE2\x80\xAF"
                             "234,50\xC2\xA0\xE2\x82\xAC"),
          "France puts it last");
}

static void GuideUnit(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en");
    mlocDecimal value;
    CHECK(mlocMakeDecimal(120, &value) == mloc_success, "120");
    mlocUnitOptions options = {.width = mloc_unitWidthLong};
    mlocUnitFormatter formatter;
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocInitUnitFormatter(&formatter, &set, &locale, "kilometer-per-hour", 18, &options) >= 0 &&
        mlocFormatUnit(&formatter, &value, &output) == mloc_success)
    {
        // en, 120: "120 kilometers per hour"
    }
    CHECK(Wrote(&output, "120 kilometers per hour"), "a compound unit's long name");
}

static void GuideSpellout(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en");
    mlocDecimal value;
    CHECK(mlocMakeDecimal(1234, &value) == mloc_success, "1234");
    mlocSpelloutFormatter formatter;
    char text[128];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocInitSpelloutFormatter(&formatter, &set, &locale, "%spellout-cardinal", 18, nullptr) >=
            0 &&
        mlocFormatSpellout(&formatter, &value, &output) == mloc_success)
    {
        // en, 1234: "one thousand two hundred thirty-four"
    }
    CHECK(Wrote(&output, "one thousand two hundred thirty-four"), "English in words");
}

static void GuideParseNumber(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("de");
    mlocNumberFormatter formatter;
    CHECK(mlocInitNumberFormatter(&formatter, &set, &locale, nullptr) >= 0, "a German formatter");
    mlocDecimal parsed;
    size_t end = 0;
    mlocResult read =
        mlocParseNumber(&formatter, "1.234,5 kg", 10, mloc_parseLenient, &parsed, &end);
    if (read == mloc_success)
    {
        // de: parsed is 1234.5; end is 7, before " kg"
    }
    mlocDecimal expected;
    CHECK(read == mloc_success && end == 7 &&
              mlocParseDecimal("1234.5", 6, &expected).status == mloc_success &&
              memcmp(&parsed, &expected, sizeof(parsed)) == 0,
          "German separators, the unit left");
}

static void GuideCalendar(void)
{
    mlocDateInput input = {.year = 2026, .month = 10, .day = 15};
    mlocDate date;
    mlocDateFields fields;
    if (mlocMakeDate(mloc_calendarGregorian, &input, mloc_overflowReject, &date) == mloc_success &&
        mlocMakeDateFromDay(mloc_calendarHebrew, date.day, &date) == mloc_success &&
        mlocGetDateFields(&date, &fields) == mloc_success)
    {
        // fields: year 5787, the second month (Cheshvan), day 4
    }
    CHECK(fields.year == 5787 && fields.monthCodeLength == 3 &&
              memcmp(fields.monthCode, "M02", 3) == 0 && fields.day == 4,
          "October 15, 2026 is 4 Cheshvan 5787");
}

static void GuideZone(void)
{
    mlocDataSet set = s_all;
    mlocInstant instant = {.seconds = 1792069380};
    mlocTimeZone zone;
    mlocDateTime local;
    if (mlocFindTimeZone(&set, "Europe/Istanbul", 15, &zone) == mloc_success &&
        mlocGetDateTimeFor(&zone, &instant, mloc_calendarIso8601, &local) == mloc_success)
    {
        // 2026-10-15T13:03:00Z: local is 2026-10-15T16:03 (UTC+3)
    }
    CHECK(local.date.calendar == mloc_calendarIso8601 && local.time.hour == 16 &&
              local.time.minute == 3,
          "Istanbul is three hours ahead");
    mlocDateFields fields;
    CHECK(mlocGetDateFields(&local.date, &fields) == mloc_success && fields.year == 2026 &&
              fields.month == 10 && fields.day == 15,
          "on the same day");
}

static void GuideDateTime(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en-US");
    mlocTimeZone zone;
    CHECK(mlocFindTimeZone(&set, "America/Los_Angeles", 19, &zone) == mloc_success, "Los Angeles");
    mlocInstant instant = {.seconds = 1792105380};
    mlocDateTimeOptions options = {
        .date = {.fields = mloc_dateFieldSetYearMonthDay, .length = mloc_dateLengthMedium},
        .time = {.precision = mloc_timePrecisionMinute, .zone = mloc_zoneStyleSpecificShort},
    };
    mlocDateTimeFormatter formatter;
    char text[96];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocInitDateTimeFormatter(&formatter, &set, &locale, &options) >= 0 &&
        mlocFormatDateTimeInZone(&formatter, &zone, &instant, &output) == mloc_success)
    {
        // en-US, America/Los_Angeles, 2026-10-15T23:03:00Z: "Oct 15, 2026, 4:03 PM PDT"
    }
    CHECK(Wrote(&output, "Oct 15, 2026, 4:03\xE2\x80\xAFPM PDT"), "a date-time in its zone");
}

static void GuideRelative(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en");
    mlocDecimal amount;
    CHECK(mlocMakeDecimal(1, &amount) == mloc_success, "1");
    mlocRelativeOptions options = {.numeric = mloc_relativeNumericAuto};
    mlocRelativeFormatter formatter;
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocInitRelativeFormatter(&formatter, &set, &locale, &options) >= 0 &&
        mlocFormatRelative(&formatter, &amount, mloc_relativeDay, &output) == mloc_success)
    {
        // en, 1: "tomorrow"; 3: "in 3 days"; -1: "yesterday"
    }
    CHECK(Wrote(&output, "tomorrow"), "one day ahead");
    output.length = 0;
    CHECK(mlocMakeDecimal(3, &amount) == mloc_success &&
              mlocFormatRelative(&formatter, &amount, mloc_relativeDay, &output) == mloc_success &&
              Wrote(&output, "in 3 days"),
          "three days ahead");
}

static void GuideMessage(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en");
    static const char text[] = ".input {$count :integer}\n"
                               ".match $count\n"
                               "one {{You have {$count} new message.}}\n"
                               "* {{You have {$count} new messages.}}";
    uint8_t compiled[512];
    uint8_t bound[2048];
    size_t compiledSize = 0;
    size_t boundSize = 0;
    mlocMessageError error = mloc_messageErrorNone;
    mlocMessageFormatter formatter;
    mlocTextResult result = mlocCompileMessage(text, sizeof(text) - 1, compiled, sizeof(compiled),
                                               &compiledSize, &error);
    if (result.status == mloc_success &&
        mlocInitMessageFormatter(&formatter, &set, &locale, nullptr) == mloc_success &&
        mlocBindMessage(&formatter, compiled, compiledSize, bound, sizeof(bound), &boundSize) ==
            mloc_success)
    {
        mlocMessageArgument count = {
            "count", 5, {.kind = mloc_messageValueInteger, .as.integer = 3}};
        char out[128];
        mlocOutput output = {.buffer = out, .capacity = sizeof(out)};
        mlocResult formatted = mlocFormatMessage(bound, &count, 1, &output, nullptr);
        // formatted is mloc_success; out holds "You have 3 new messages."
        (void)formatted;
    }
    CHECK(result.status == mloc_success && boundSize > 0, "compiled and bound");
    mlocMessageArgument one = {"count", 5, {.kind = mloc_messageValueInteger, .as.integer = 1}};
    char out[128];
    mlocOutput output = {.buffer = out, .capacity = sizeof(out)};
    CHECK(mlocFormatMessage(bound, &one, 1, &output, nullptr) == mloc_success &&
              Wrote(&output, "You have 1 new message."),
          "the one variant");
    mlocMessageArgument three = {"count", 5, {.kind = mloc_messageValueInteger, .as.integer = 3}};
    output.length = 0;
    CHECK(mlocFormatMessage(bound, &three, 1, &output, nullptr) == mloc_success &&
              Wrote(&output, "You have 3 new messages."),
          "the other variant");
}

static void GuideList(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en");
    mlocListItem items[3] = {{"Ana", 3}, {"Bo", 2}, {"Cy", 2}};
    mlocListFormatter formatter;
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocInitListFormatter(&formatter, &set, &locale, nullptr) == mloc_success &&
        mlocFormatList(&formatter, items, 3, &output) == mloc_success)
    {
        // en: "Ana, Bo, and Cy"; de: "Ana, Bo und Cy"
    }
    CHECK(Wrote(&output, "Ana, Bo, and Cy"), "an English list");
    locale = Locale("de");
    CHECK(mlocInitListFormatter(&formatter, &set, &locale, nullptr) == mloc_success &&
              mlocFormatList(&formatter, items, 3, &output) == mloc_success &&
              Wrote(&output, "Ana, Bo und Cy"),
          "a German list");
}

static void GuideInflect(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("tr");
    mlocInflector inflector;
    mlocInflection dative = {.grammaticalCase = mloc_caseDative};
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocInitInflector(&inflector, &set, &locale) == mloc_success &&
        mlocInflectText(&inflector, "Ayşe", 5, &dative, &output) == mloc_success)
    {
        // tr: "Ayşe'ye"
    }
    CHECK(Wrote(&output, "Ay\xC5\x9F"
                         "e'ye"),
          "a Turkish dative");
    output.length = 0;
    CHECK(mlocInflectText(&inflector, "ABD", 3, &dative, &output) == mloc_success &&
              Wrote(&output, "ABD'ye"),
          "an abbreviation by its letters' names");
}

static void GuideCompare(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("de");
    mlocCollator collator;
    int order = 0;
    if (mlocInitCollator(&collator, &set, &locale, nullptr) == mloc_success &&
        mlocCompare(&collator, "öl", 3, "zebra", 5, &order) == mloc_success)
    {
        // de: order is -1, "öl" first; sv: 1, "ö" sorts after "z"
    }
    CHECK(order == -1, "German sorts o umlaut with o");
    locale = Locale("sv");
    CHECK(mlocInitCollator(&collator, &set, &locale, nullptr) == mloc_success &&
              mlocCompare(&collator, "\xC3\xB6l", 3, "zebra", 5, &order) == mloc_success &&
              order == 1,
          "Swedish sorts it after z");
}

static void GuideSearch(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en");
    const char* text = "My R\xC3\xA9sum\xC3\xA9.pdf";
    size_t length = strlen(text);
    mlocCollatorOptions loose = {.strength = mloc_strengthPrimary};
    mlocCollator collator;
    mlocSearcher searcher;
    size_t start = 0;
    size_t end = 0;
    if (mlocInitCollator(&collator, &set, &locale, &loose) == mloc_success &&
        mlocInitSearcher(&searcher, &collator, "resume", 6, nullptr) == mloc_success &&
        mlocFindNext(&searcher, text, length, 0, &start, &end) == mloc_success)
    {
        // text "My Résumé.pdf": start 3, end 11
    }
    CHECK(start == 3 && end == 11, "a primary match across the accents");
}

static void GuideSentences(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en-u-ss-standard");
    const char* text = "Mr. Smith left. He came.";
    mlocSegmenter segmenter;
    size_t end = 0;
    if (mlocInitSegmenter(&segmenter, &set, &locale, mloc_segmentSentence, text, strlen(text),
                          false) == mloc_success)
    {
        while (mlocNextSegmentBreak(&segmenter, &end, nullptr) == mloc_success)
        {
            // en-u-ss-standard: 16, then 24; en: 4 (after "Mr. ") first
        }
    }
    CHECK(end == 24, "the last sentence ends the text");
    CHECK(mlocInitSegmenter(&segmenter, &set, &locale, mloc_segmentSentence, text, strlen(text),
                            false) == mloc_success &&
              mlocNextSegmentBreak(&segmenter, &end, nullptr) == mloc_success && end == 16,
          "no break after Mr.");
    locale = Locale("en");
    CHECK(mlocInitSegmenter(&segmenter, &set, &locale, mloc_segmentSentence, text, strlen(text),
                            false) == mloc_success &&
              mlocNextSegmentBreak(&segmenter, &end, nullptr) == mloc_success && end == 4,
          "without -u-ss-standard, a break after Mr.");
}

static void GuidePieces(void)
{
    mlocDataSet set = s_all;
    mlocLocale locale = Locale("en");
    const char* first = "A line in two pie";
    const char* second = "ces.";
    mlocSegmenter segmenter;
    size_t end = 0;
    mlocResult r =
        mlocInitSegmenter(&segmenter, &set, &locale, mloc_segmentLine, first, strlen(first), true);
    while (r == mloc_success)
    {
        r = mlocNextSegmentBreak(&segmenter, &end, nullptr);
        if (r == mloc_needMoreText)
        {
            r = mlocFeedSegmenter(&segmenter, second, strlen(second), false);
        }
    }
    CHECK(r == mloc_done && end == 21, "the last opportunity is the end of both pieces");
}

int main(void)
{
    mlocOpenBuiltinData(&s_builtin);
    for (size_t i = 0; i < fileCount; i++)
    {
        size_t size = 0;
        s_bytes[i] = LoadFile(s_files[i], &size);
        CHECK(s_bytes[i] != nullptr &&
                  mlocOpenData(&s_sources[i], s_bytes[i], size) == mloc_success,
              s_files[i]);
    }
    s_sources[fileCount] = s_builtin;
    GuideFirstNumber();
    GuideOpenFile();
    GuideCanonicalize();
    GuideParents();
    GuideMatch();
    GuidePreferredList();
    GuidePreferences();
    GuideLocaleName();
    GuidePlural();
    GuideCompact();
    GuideCurrency();
    GuideUnit();
    GuideSpellout();
    GuideParseNumber();
    GuideCalendar();
    GuideZone();
    GuideDateTime();
    GuideRelative();
    GuideMessage();
    GuideList();
    GuideInflect();
    GuideCompare();
    GuideSearch();
    GuideSentences();
    GuidePieces();
    for (size_t i = 0; i < fileCount; i++)
    {
        free(s_bytes[i]);
    }
    return s_failures == 0 ? 0 : 1;
}
