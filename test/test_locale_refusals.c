// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A locale filled in by hand with bytes no locale has, given to every
// function that reads an mlocLocale, in each place it reads one (both
// locales of a name or a combined match, each of a match's supported and
// desired locales, its default). Each call is made once with a parsed
// locale, which must succeed, and with well-formed locales filled in by
// hand, which must not be refused; then with each malformed one, which
// must be refused with mloc_errorInvalid.

#include "test_harness.h"

#include "maul-locale/collate.h"
#include "maul-locale/currency.h"
#include "maul-locale/datetime.h"
#include "maul-locale/decimal.h"
#include "maul-locale/duration.h"
#include "maul-locale/grammar.h"
#include "maul-locale/index.h"
#include "maul-locale/list.h"
#include "maul-locale/locale.h"
#include "maul-locale/message.h"
#include "maul-locale/names.h"
#include "maul-locale/number.h"
#include "maul-locale/plural.h"
#include "maul-locale/relative.h"
#include "maul-locale/segment.h"
#include "maul-locale/spellout.h"
#include "maul-locale/unit.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char* const s_files[] = {
    "collation.mloc", "currency-names.mloc", "dates.mloc", "names.mloc",      "relative.mloc",
    "segments.mloc",  "spellout.mloc",       "units.mloc", "zone-names.mloc",
};
enum
{
    fileCount = sizeof(s_files) / sizeof(s_files[0]),
};

static mlocData s_sources[fileCount + 1];
static mlocDataSet s_data;
static mlocLocale s_en;
static mlocCollator s_root;
static mlocDecimal s_five;
static char s_text[2048];
alignas(8) static uint8_t s_index[1 << 17];

static mlocOutput Text(void)
{
    return (mlocOutput){.buffer = s_text, .capacity = sizeof(s_text)};
}

// A call made with a locale.
typedef mlocResult (*Call)(const mlocLocale* locale);

static mlocResult Collator(const mlocLocale* locale)
{
    mlocCollator collator;
    return mlocInitCollator(&s_data, locale, nullptr, &collator);
}

static mlocResult CurrencyFormatter(const mlocLocale* locale)
{
    mlocCurrencyFormatter formatter;
    return mlocInitCurrencyFormatter(&s_data, locale, "EUR", 3, nullptr, &formatter);
}

static mlocResult CurrencySymbol(const mlocLocale* locale)
{
    mlocOutput out = Text();
    return mlocGetCurrencySymbol(&s_data, locale, "EUR", 3, mloc_currencySymbol, &out);
}

static mlocResult CurrencyName(const mlocLocale* locale)
{
    mlocOutput out = Text();
    return mlocGetCurrencyName(&s_data, locale, "EUR", 3, &out);
}

static mlocResult FindCurrency(const mlocLocale* locale)
{
    char code[4];
    return mlocFindCurrency(&s_data, locale, 20000, code);
}

static mlocResult DateFormatter(const mlocLocale* locale)
{
    mlocDateFormatter formatter;
    return mlocInitDateFormatter(&s_data, locale, nullptr, &formatter);
}

static mlocResult TimeFormatter(const mlocLocale* locale)
{
    mlocTimeFormatter formatter;
    return mlocInitTimeFormatter(&s_data, locale, nullptr, &formatter);
}

static mlocResult DateTimeFormatter(const mlocLocale* locale)
{
    mlocDateTimeFormatter formatter;
    mlocDateTimeOptions options = {.date = {.style = mloc_dateStyleMedium},
                                   .time = {.style = mloc_timeStyleShort}};
    return mlocInitDateTimeFormatter(&s_data, locale, &options, &formatter);
}

static mlocResult ZoneFormatter(const mlocLocale* locale)
{
    mlocZoneFormatter formatter;
    return mlocInitZoneFormatter(&s_data, locale, mloc_zoneStyleSpecificShort, &formatter);
}

static mlocResult DurationFormatter(const mlocLocale* locale)
{
    mlocDurationFormatter formatter;
    return mlocInitDurationFormatter(&s_data, locale, nullptr, &formatter);
}

static mlocResult Inflector(const mlocLocale* locale)
{
    mlocInflector inflector;
    mlocResult r = mlocInitInflector(&s_data, locale, &inflector);
    // English has no inflector.
    return r == mloc_errorNotFound ? mloc_success : r;
}

static mlocResult Index(const mlocLocale* locale)
{
    size_t size = 0;
    return mlocBuildIndex(&s_root, &s_data, locale, nullptr, s_index, sizeof(s_index), &size);
}

static mlocResult ListFormatter(const mlocLocale* locale)
{
    mlocListFormatter formatter;
    return mlocInitListFormatter(&s_data, locale, nullptr, &formatter);
}

static mlocResult ListGender(const mlocLocale* locale)
{
    static const mlocPersonGender s_genders[] = {mloc_personMale, mloc_personFemale};
    mlocPersonGender gender;
    return mlocGetListGender(&s_data, locale, s_genders, 2, &gender);
}

static mlocResult FormatLocale(const mlocLocale* locale)
{
    mlocOutput out = Text();
    return mlocFormatLocale(locale, &out);
}

static mlocResult Maximize(const mlocLocale* locale)
{
    mlocLocale out;
    return mlocMaximizeLocale(&s_data, locale, &out);
}

static mlocResult Minimize(const mlocLocale* locale)
{
    mlocLocale out;
    return mlocMinimizeLocale(&s_data, locale, mloc_favorRegion, &out);
}

static mlocResult Direction(const mlocLocale* locale)
{
    mlocDirection direction;
    return mlocGetLocaleDirection(&s_data, locale, &direction);
}

static mlocResult Parent(const mlocLocale* locale)
{
    mlocLocale out;
    mlocResult r = mlocGetParentLocale(&s_data, mloc_componentMain, locale, &out);
    // A language with no parent but root has none.
    return r == mloc_errorNotFound ? mloc_success : r;
}

static mlocResult MatchSupported(const mlocLocale* locale)
{
    mlocLocale supported[2] = {s_en, *locale};
    mlocMatchEntry entries[2];
    return mlocPrepareMatch(&s_data, supported, 2, nullptr, entries);
}

static mlocResult MatchDefault(const mlocLocale* locale)
{
    mlocMatchEntry entries[1];
    return mlocPrepareMatch(&s_data, &s_en, 1, locale, entries);
}

static mlocResult MatchDesired(const mlocLocale* locale)
{
    mlocMatchEntry entries[1];
    mlocLocale desired[2] = {s_en, *locale};
    mlocMatch match;
    mlocResult r = mlocPrepareMatch(&s_data, &s_en, 1, nullptr, entries);
    return r != mloc_success
               ? r
               : mlocMatchLocales(&s_data, &s_en, entries, 1, desired, 2, nullptr, &match);
}

static mlocResult MatchAgainst(const mlocLocale* locale)
{
    // Entries prepared for valid locales, matched with the locale in
    // their place.
    mlocLocale supported[2] = {s_en, s_en};
    mlocMatchEntry entries[2];
    mlocMatch match;
    mlocResult r = mlocPrepareMatch(&s_data, supported, 2, nullptr, entries);
    supported[1] = *locale;
    return r != mloc_success
               ? r
               : mlocMatchLocales(&s_data, supported, entries, 2, &s_en, 1, nullptr, &match);
}

static mlocResult CombineDesired(const mlocLocale* locale)
{
    mlocLocale out;
    return mlocCombineMatch(locale, &s_en, &out);
}

static mlocResult CombineSupported(const mlocLocale* locale)
{
    mlocLocale out;
    return mlocCombineMatch(&s_en, locale, &out);
}

static mlocResult Preferences(const mlocLocale* locale)
{
    mlocLocalePreferences preferences;
    return mlocGetLocalePreferences(&s_data, locale, &preferences);
}

static mlocResult MessageFormatter(const mlocLocale* locale)
{
    mlocMessageFormatter formatter;
    return mlocInitMessageFormatter(&s_data, locale, nullptr, &formatter);
}

static mlocResult Name(const mlocLocale* locale)
{
    mlocOutput out = Text();
    return mlocGetName(&s_data, locale, mloc_nameRegion, "US", 2, nullptr, &out);
}

static mlocResult LocaleNameIn(const mlocLocale* locale)
{
    mlocOutput out = Text();
    return mlocGetLocaleName(&s_data, locale, &s_en, nullptr, &out);
}

static mlocResult LocaleNameOf(const mlocLocale* locale)
{
    mlocOutput out = Text();
    return mlocGetLocaleName(&s_data, &s_en, locale, nullptr, &out);
}

static mlocResult NumberFormatter(const mlocLocale* locale)
{
    mlocNumberFormatter formatter;
    return mlocInitNumberFormatter(&s_data, locale, nullptr, &formatter);
}

static mlocResult PluralRules(const mlocLocale* locale)
{
    mlocPluralRules rules;
    return mlocInitPluralRules(&s_data, locale, mloc_pluralCardinal, &rules);
}

static mlocResult RelativeFormatter(const mlocLocale* locale)
{
    mlocRelativeFormatter formatter;
    return mlocInitRelativeFormatter(&s_data, locale, nullptr, &formatter);
}

static mlocResult Segmenter(const mlocLocale* locale)
{
    mlocSegmenter segmenter;
    return mlocInitSegmenter(&s_data, locale, mloc_segmentWord, "a b", 3, false, &segmenter);
}

static mlocResult SpelloutFormatter(const mlocLocale* locale)
{
    mlocSpelloutFormatter formatter;
    return mlocInitSpelloutFormatter(&s_data, locale, "%spellout-numbering", 19, nullptr,
                                     &formatter);
}

static mlocResult NumberingSystemFormatter(const mlocLocale* locale)
{
    mlocSpelloutFormatter formatter;
    return mlocInitNumberingSystemFormatter(&s_data, locale, "roman", 5, nullptr, &formatter);
}

static mlocResult RuleSetCount(const mlocLocale* locale)
{
    // A count of 0 is the only refusal the call has.
    return mlocGetSpelloutRuleSetCount(&s_data, locale) > 0 ? mloc_success : mloc_errorInvalid;
}

static mlocResult RuleSet(const mlocLocale* locale)
{
    const char* name = nullptr;
    size_t length = 0;
    return mlocGetSpelloutRuleSet(&s_data, locale, 0, &name, &length);
}

static mlocResult UnitPreference(const mlocLocale* locale)
{
    mlocOutput out = Text();
    mlocUnitAmounts amounts;
    return mlocGetUnitPreference(&s_data, locale, "meter", 5, "road", 4, &s_five, nullptr, &out,
                                 &amounts);
}

static mlocResult UnitFormatter(const mlocLocale* locale)
{
    mlocUnitFormatter formatter;
    return mlocInitUnitFormatter(&s_data, locale, "meter", 5, nullptr, &formatter);
}

static mlocResult UnitDisplayName(const mlocLocale* locale)
{
    mlocOutput out = Text();
    return mlocGetUnitDisplayName(&s_data, locale, "meter", 5, mloc_unitWidthLong, &out);
}

static mlocResult UnitGender(const mlocLocale* locale)
{
    mlocGender gender;
    mlocResult r = mlocGetUnitGender(&s_data, locale, "meter", 5, &gender);
    // English units have no gender.
    return r == mloc_errorNotFound ? mloc_success : r;
}

#define CALL(name) {name, #name}

static const struct
{
    Call call;
    const char* name;
} s_calls[] = {
    CALL(Collator),
    CALL(CurrencyFormatter),
    CALL(CurrencySymbol),
    CALL(CurrencyName),
    CALL(FindCurrency),
    CALL(DateFormatter),
    CALL(TimeFormatter),
    CALL(DateTimeFormatter),
    CALL(ZoneFormatter),
    CALL(DurationFormatter),
    CALL(Inflector),
    CALL(Index),
    CALL(ListFormatter),
    CALL(ListGender),
    CALL(FormatLocale),
    CALL(Maximize),
    CALL(Minimize),
    CALL(Direction),
    CALL(Parent),
    CALL(MatchSupported),
    CALL(MatchDefault),
    CALL(MatchDesired),
    CALL(MatchAgainst),
    CALL(CombineDesired),
    CALL(CombineSupported),
    CALL(Preferences),
    CALL(MessageFormatter),
    CALL(Name),
    CALL(LocaleNameIn),
    CALL(LocaleNameOf),
    CALL(NumberFormatter),
    CALL(PluralRules),
    CALL(RelativeFormatter),
    CALL(Segmenter),
    CALL(SpelloutFormatter),
    CALL(NumberingSystemFormatter),
    CALL(RuleSetCount),
    CALL(RuleSet),
    CALL(UnitPreference),
    CALL(UnitFormatter),
    CALL(UnitDisplayName),
    CALL(UnitGender),
};

// A locale filled in field by field; each field's bytes are copied as
// given (sizeof of a literal, less its NUL), the rest left NUL.
typedef struct Fields
{
    const char* name;
    const char* language;
    size_t languageSize;
    const char* script;
    size_t scriptSize;
    const char* region;
    size_t regionSize;
    const char* tail;
    size_t tailSize;
} Fields;

#define FIELD(text) text, sizeof(text) - 1
#define LOCALE(name, language, script, region, tail)                                               \
    {name, FIELD(language), FIELD(script), FIELD(region), FIELD(tail)}

static mlocLocale Fill(const Fields* f)
{
    mlocLocale locale;
    memset(&locale, 0, sizeof(locale));
    memcpy(locale.language, f->language, f->languageSize);
    memcpy(locale.script, f->script, f->scriptSize);
    memcpy(locale.region, f->region, f->regionSize);
    memcpy(locale.tail, f->tail, f->tailSize);
    return locale;
}

// Well formed, though not all canonical or known: accepted.
static const Fields s_wellFormed[] = {
    LOCALE("a full locale", "en", "Latn", "US", "fonipa-u-ca-buddhist"),
    LOCALE("an alias", "iw", "", "419", ""),
    LOCALE("variants out of order", "sl", "", "", "rozaj-biske"),
    LOCALE("an unknown language of 8 letters", "abcdefgh", "", "", ""),
    LOCALE("one-letter private use", "en", "", "", "x-a"),
};

// Malformed: refused.
static const Fields s_malformed[] = {
    LOCALE("an empty language", "", "", "", ""),
    LOCALE("a language of 1 letter", "e", "", "", ""),
    LOCALE("a language of 4 letters", "engl", "", "", ""),
    LOCALE("an uppercase language", "EN", "", "", ""),
    LOCALE("a language with a digit", "e1", "", "", ""),
    LOCALE("a language of 8 bytes, not letters", "en-latn-", "", "", ""),
    LOCALE("a language not ASCII", "\xC3\xA9n", "", "", ""),
    LOCALE("a byte after the language's NUL", "en\0x", "", "", ""),
    LOCALE("a lowercase script", "en", "latn", "", ""),
    LOCALE("an uppercase script", "en", "LATN", "", ""),
    LOCALE("a script of 3 letters", "en", "Lat", "", ""),
    LOCALE("a byte after the script's NUL", "en", "La\0t", "", ""),
    LOCALE("a lowercase region", "en", "", "us", ""),
    LOCALE("a region of 2 digits", "en", "", "12", ""),
    LOCALE("a region of a letter and a digit", "en", "", "U1", ""),
    LOCALE("a region of 4 letters", "en", "", "USAX", ""),
    LOCALE("a region of 1 letter", "en", "", "U", ""),
    LOCALE("a tail with a leading hyphen", "en", "", "", "-fonipa"),
    LOCALE("a tail with a trailing hyphen", "en", "", "", "fonipa-"),
    LOCALE("a tail with an empty subtag", "en", "", "", "u--ca-x-"),
    LOCALE("a tail with a subtag of 9 characters", "en", "", "", "abcdefghi"),
    LOCALE("an uppercase tail", "en", "", "", "fonIPA"),
    LOCALE("a tail with control bytes", "de", "", "", "\x01\x02\x03\x04\x05"),
    LOCALE("a tail with a space", "en", "", "", "u-ca buddhist"),
    LOCALE("a tail with an underscore", "en", "", "", "u_ca"),
    LOCALE("a byte after the tail's NUL", "en", "", "", "fonipa\0x"),
};

static void TestWellFormed(void)
{
    for (size_t c = 0; c < sizeof(s_calls) / sizeof(s_calls[0]); c++)
    {
        mlocResult r = s_calls[c].call(&s_en);
        if (r < 0)
        {
            fprintf(stderr, "%s: en gives %d\n", s_calls[c].name, (int)r);
        }
        CHECK(r >= 0, "a parsed locale succeeds");
        for (size_t l = 0; l < sizeof(s_wellFormed) / sizeof(s_wellFormed[0]); l++)
        {
            mlocLocale locale = Fill(&s_wellFormed[l]);
            r = s_calls[c].call(&locale);
            if (r == mloc_errorInvalid)
            {
                fprintf(stderr, "%s: %s is refused\n", s_calls[c].name, s_wellFormed[l].name);
            }
            CHECK(r != mloc_errorInvalid, "a well-formed locale is accepted");
        }
    }
}

static void Refused(const mlocLocale* locale, const char* name)
{
    for (size_t c = 0; c < sizeof(s_calls) / sizeof(s_calls[0]); c++)
    {
        mlocResult r = s_calls[c].call(locale);
        if (r != mloc_errorInvalid)
        {
            fprintf(stderr, "%s: %s gives %d\n", s_calls[c].name, name, (int)r);
        }
        CHECK(r == mloc_errorInvalid, "a malformed locale is refused");
    }
}

static void TestMalformed(void)
{
    for (size_t l = 0; l < sizeof(s_malformed) / sizeof(s_malformed[0]); l++)
    {
        mlocLocale locale = Fill(&s_malformed[l]);
        Refused(&locale, s_malformed[l].name);
    }
    mlocLocale locale;
    memset(&locale, 0xFF, sizeof(locale));
    Refused(&locale, "every byte 0xFF");
    memset(&locale, 'a', sizeof(locale));
    Refused(&locale, "every byte a");
    // mlocFormatLocale wrote such bytes as they were.
    memset(&locale, 0xFF, sizeof(locale));
    char text[256];
    mlocOutput out = {.buffer = text, .capacity = sizeof(text)};
    CHECK(mlocFormatLocale(&locale, &out) == mloc_errorInvalid && out.length == 0,
          "0xFF is not written as a locale");
}

static bool OpenData(void)
{
    // Room for every file at once.
    alignas(8) static uint8_t s_bytes[24 << 20];
    size_t used = 0;
    mlocOpenBuiltinData(&s_sources[0]);
    for (size_t i = 0; i < fileCount; i++)
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", MLOC_GENERATED_DIR, s_files[i]);
        FILE* file = fopen(path, "rb");
        size_t size = file == nullptr ? 0 : fread(s_bytes + used, 1, sizeof(s_bytes) - used, file);
        if (file != nullptr)
        {
            fclose(file);
        }
        if (size == 0 || used + size == sizeof(s_bytes) ||
            mlocOpenData(s_bytes + used, size, &s_sources[i + 1]) != mloc_success)
        {
            fprintf(stderr, "test_locale_refusals: cannot open %s\n", path);
            return false;
        }
        used += (size + 7) & ~(size_t)7;
    }
    s_data = (mlocDataSet){s_sources, fileCount + 1};
    return true;
}

int main(void)
{
    if (!OpenData())
    {
        return 1;
    }
    mlocLocale und;
    CHECK(mlocParseLocale("en", 2, &s_en).status == mloc_success &&
              mlocParseLocale("und", 3, &und).status == mloc_success &&
              mlocInitCollator(&s_data, &und, nullptr, &s_root) == mloc_success &&
              mlocMakeDecimal(5, &s_five) == mloc_success,
          "data and objects to call with");
    TestWellFormed();
    TestMalformed();
    return s_failures == 0 ? 0 : 1;
}
