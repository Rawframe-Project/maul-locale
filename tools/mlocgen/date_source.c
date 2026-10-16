// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A locale's date patterns read from CLDR (date_source.h).

#include "date_source.h"

#include "day_periods_gen.h"
#include "semantic.h"
#include "xml.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxItems = 1024,
};

static const char s_calendarChain[] = "//ldml/dates/calendars/calendar[@type=\"%s\"]/%s";

// The appendItem request and the field name of each field, as ICU names
// them; "*" for none.
static const char* const s_appendRequests[skeletonFieldCount] = {
    "Era", "Year", "Quarter", "Month", "Week",   "*",      "Day-Of-Week", "*",
    "*",   "Day",  "*",       "Hour",  "Minute", "Second", "*",           "Timezone"};
static const char* const s_fieldNames[skeletonFieldCount] = {
    "era",         "year",    "quarter",   "month",          "week",
    "weekOfMonth", "weekday", "dayOfYear", "weekdayOfMonth", "day",
    "dayperiod",   "hour",    "minute",    "second",         "*",
    "zone"};

// ICU's default appendItem: "{0} ├{2}: {1}┤".
static const char s_defaultAppend[] = "{0} \xE2\x94\x9C{2}: {1}\xE2\x94\xA4";

// Formats into a buffer; a text that would not fit stops the generator,
// as no CLDR name comes near the buffers' sizes.
[[gnu::format(printf, 3, 4)]] static void Format(char* out, size_t capacity, const char* format,
                                                 ...)
{
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(out, capacity, format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= capacity)
    {
        fprintf(stderr, "mlocgen: a date chain or name is too long: %s\n", out);
        exit(1);
    }
}

static char* Copy(const char* text)
{
    size_t length = strlen(text);
    char* copy = malloc(length + 1);
    if (copy == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static void AddHours(DateSupplement* s, const XmlNode* hours)
{
    const char* preferred = XmlGetAttribute(hours, "preferred");
    const char* allowed = XmlGetAttribute(hours, "allowed");
    const char* regions = XmlGetAttribute(hours, "regions");
    if (preferred == nullptr || allowed == nullptr || regions == nullptr)
    {
        return;
    }
    char value[128];
    Format(value, sizeof(value), "%s %s", preferred, allowed);
    for (const char* at = regions; *at != '\0';)
    {
        char key[32];
        size_t length = strcspn(at, " ");
        Format(key, sizeof(key), "%.*s", (int)length, at);
        free(MapGet(&s->hours, key));
        MapPut(&s->hours, key, Copy(value));
        at += length + (at[length] == ' ');
    }
}

// calendarData's era types by calendar and era code.
static void AddEras(DateSupplement* s, const XmlNode* calendarData)
{
    for (int c = 0; calendarData != nullptr && c < calendarData->childCount; c++)
    {
        const XmlNode* calendar = calendarData->children[c];
        const XmlNode* eras = XmlFindChild(calendar, "eras");
        const char* type = XmlGetAttribute(calendar, "type");
        for (int e = 0; eras != nullptr && type != nullptr && e < eras->childCount; e++)
        {
            const char* code = XmlGetAttribute(eras->children[e], "code");
            const char* era = XmlGetAttribute(eras->children[e], "type");
            if (code != nullptr && era != nullptr)
            {
                char key[128];
                Format(key, sizeof(key), "%s %s", type, code);
                MapPut(&s->eras, key, Copy(era));
            }
        }
    }
}

int DateSupplementLoad(DateSupplement* s, const char* commonDirectory, char* errorOut,
                       size_t errorCapacity)
{
    char path[4096];
    memset(s, 0, sizeof(*s));
    Format(path, sizeof(path), "%s/supplemental/supplementalData.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    const XmlNode* timeData = root == nullptr ? nullptr : XmlFindChild(root, "timeData");
    for (int i = 0; timeData != nullptr && i < timeData->childCount; i++)
    {
        AddHours(s, timeData->children[i]);
    }
    AddEras(s, root == nullptr ? nullptr : XmlFindChild(root, "calendarData"));
    XmlFree(root);
    Format(path, sizeof(path), "%s/supplemental/likelySubtags.xml", commonDirectory);
    XmlNode* likely = timeData == nullptr ? nullptr : XmlParseFile(path, errorOut, errorCapacity);
    const XmlNode* list = likely == nullptr ? nullptr : XmlFindChild(likely, "likelySubtags");
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        const char* from = XmlGetAttribute(list->children[i], "from");
        const char* to = XmlGetAttribute(list->children[i], "to");
        if (from != nullptr && to != nullptr)
        {
            MapPut(&s->likely, from, Copy(to));
        }
    }
    XmlFree(likely);
    return list != nullptr &&
           DayPeriodRulesLoad(&s->dayPeriods, commonDirectory, errorOut, errorCapacity);
}

static void FreeValue(const char* key, void* value, void* context)
{
    (void)key;
    (void)context;
    free(value);
}

void DateSupplementFree(DateSupplement* s)
{
    MapEach(&s->hours, FreeValue, nullptr);
    MapEach(&s->likely, FreeValue, nullptr);
    MapEach(&s->eras, FreeValue, nullptr);
    MapFree(&s->hours);
    MapFree(&s->likely);
    MapFree(&s->eras);
    DayPeriodRulesFree(&s->dayPeriods);
}

// The language and region of a CLDR id, the region by likely subtags
// when the id has none, else 001.
static void LanguageRegion(const DateSupplement* s, const char* locale, char* language,
                           char* region)
{
    char script[8] = "";
    region[0] = '\0';
    Format(language, 16, "%.*s", (int)strcspn(locale, "_"), locale);
    for (const char* at = strchr(locale, '_'); at != nullptr; at = strchr(at + 1, '_'))
    {
        size_t length = strcspn(at + 1, "_");
        if (length == 4)
        {
            Format(script, sizeof(script), "%.4s", at + 1);
        }
        else if (length == 2 || length == 3)
        {
            Format(region, 4, "%.*s", (int)length, at + 1);
        }
    }
    char key[32];
    Format(key, sizeof(key), "%s_%s", language, script);
    const char* likely = script[0] != '\0' ? MapGet(&s->likely, key) : nullptr;
    likely = likely != nullptr ? likely : MapGet(&s->likely, language);
    const char* last = likely != nullptr ? strrchr(likely, '_') : nullptr;
    if (region[0] == '\0' && last != nullptr)
    {
        Format(region, 4, "%s", last + 1);
    }
    if (region[0] == '\0')
    {
        memcpy(region, "001", 4);
    }
}

// The hour letters of the locale's region, as ICU's
// getAllowedHourFormats and mapSkeletonMetacharacters read them.
static void Hours(const DateSupplement* s, const char* locale, PatternGenerator* g)
{
    char language[16];
    char region[4];
    char key[32];
    LanguageRegion(s, locale, language, region);
    Format(key, sizeof(key), "%s_%s", language, region);
    const char* hours = MapGet(&s->hours, key);
    hours = hours != nullptr ? hours : MapGet(&s->hours, region);
    g->hourChar = 'H';
    g->preferredHour = 'H';
    g->preferredPeriod = 'a';
    if (hours == nullptr)
    {
        return;
    }
    g->hourChar = strchr("hHKk", hours[0]) != nullptr ? hours[0] : 'H';
    const char* allowed = strchr(hours, ' ');
    if (allowed == nullptr)
    {
        return;
    }
    allowed++;
    g->preferredHour =
        allowed[0] == 'H' || allowed[0] == 'K' || allowed[0] == 'k' ? allowed[0] : 'h';
    bool two = allowed[1] == 'b' || allowed[1] == 'B';
    g->preferredPeriod = two && allowed[0] != 'k' ? allowed[1] : 'a';
}

static const char* Resolve(Cldr* cldr, const char* locale, const char* chain)
{
    const Leaf* leaf = CldrResolve(cldr, locale, chain, componentMain);
    if (leaf == nullptr || strcmp(leaf->text, "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85") == 0)
    {
        return nullptr;
    }
    return leaf->text;
}

static const char* CalendarValue(Cldr* cldr, const char* locale, const char* calendar,
                                 const char* rest)
{
    char chain[cldrMaxChain];
    Format(chain, sizeof(chain), s_calendarChain, calendar, rest);
    return Resolve(cldr, locale, chain);
}

static const char* const s_standardLengths[4] = {"full", "long", "medium", "short"};

typedef struct Ids
{
    const char* prefix;
    size_t prefixLength;
    char* ids[maxItems];
    int count;
} Ids;

// Collects the id of an availableFormats item without alt.
static void CollectId(const char* key, void* value, void* context)
{
    (void)value;
    Ids* ids = context;
    if (strncmp(key, ids->prefix, ids->prefixLength) != 0 || strstr(key, "[@alt=") != nullptr)
    {
        return;
    }
    // The prefix ends inside the item's first bracket.
    const char* id = strstr(key + ids->prefixLength - 1, "[@id=\"");
    if (id == nullptr || ids->count == maxItems)
    {
        return;
    }
    id += 6;
    char text[128];
    Format(text, sizeof(text), "%.*s", (int)strcspn(id, "\""), id);
    for (int i = 0; i < ids->count; i++)
    {
        if (strcmp(ids->ids[i], text) == 0)
        {
            return;
        }
    }
    ids->ids[ids->count++] = Copy(text);
}

static int CompareIds(const void* a, const void* b)
{
    return strcmp(*(char* const*)a, *(char* const*)b);
}

// One file's items, in the byte order of their ids (ICU's table order).
static void AddFileItems(Cldr* cldr, const char* locale, const LdmlFile* file, const char* prefix,
                         PatternGenerator* g)
{
    char itemPrefix[cldrMaxChain];
    Format(itemPrefix, sizeof(itemPrefix), "%s/dateFormatItem[", prefix);
    Ids* ids = calloc(1, sizeof(Ids));
    if (ids == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    ids->prefix = itemPrefix;
    ids->prefixLength = strlen(itemPrefix);
    MapEach(&file->leaves, CollectId, ids);
    qsort(ids->ids, (size_t)ids->count, sizeof(char*), CompareIds);
    for (int i = 0; i < ids->count; i++)
    {
        char chain[cldrMaxChain];
        Format(chain, sizeof(chain), "%s/dateFormatItem[@id=\"%s\"]", prefix, ids->ids[i]);
        const char* pattern = Resolve(cldr, locale, chain);
        if (pattern == nullptr)
        {
            // An item given per plural count (week of month, week of
            // year) takes its "other" form.
            Format(chain, sizeof(chain), "%s/dateFormatItem[@count=\"other\"][@id=\"%s\"]", prefix,
                   ids->ids[i]);
            pattern = Resolve(cldr, locale, chain);
        }
        if (pattern != nullptr)
        {
            PatternGeneratorAdd(g, pattern, ids->ids[i]);
        }
        free(ids->ids[i]);
    }
    free(ids);
}

// availableFormats from the locale up to root, then through root's
// alias, again from the locale, as ICU's resource fallback reads them.
static void AddItems(Cldr* cldr, const char* locale, const char* calendar, PatternGenerator* g)
{
    char prefix[cldrMaxChain];
    Format(prefix, sizeof(prefix), s_calendarChain, calendar, "dateTimeFormats/availableFormats");
    for (int depth = 0; depth < 8; depth++)
    {
        for (const char* at = locale; at != nullptr; at = CldrParent(cldr, at, componentMain))
        {
            const LdmlFile* file = CldrFile(cldr, at, componentMain);
            if (file != nullptr)
            {
                AddFileItems(cldr, locale, file, prefix, g);
            }
        }
        char target[cldrMaxChain];
        if (!CldrAliasOf(cldr, prefix, target, componentMain))
        {
            return;
        }
        memcpy(prefix, target, sizeof(prefix));
    }
}

static void AddStandard(Cldr* cldr, const char* locale, const char* calendar, PatternGenerator* g)
{
    for (int i = 0; i < 8; i++)
    {
        const char* kind = i < 4 ? "time" : "date";
        char rest[256];
        Format(rest, sizeof(rest), "%sFormats/%sFormatLength[@type=\"%s\"]/%sFormat/pattern", kind,
               kind, s_standardLengths[i % 4], kind);
        const char* pattern = CalendarValue(cldr, locale, calendar, rest);
        if (pattern != nullptr)
        {
            PatternGeneratorAdd(g, pattern, nullptr);
        }
    }
    for (int i = 0; i < 4; i++)
    {
        char rest[256];
        Format(
            rest, sizeof(rest),
            "dateTimeFormats/dateTimeFormatLength[@type=\"%s\"]/dateTimeFormat[@type=\"atTime\"]/"
            "pattern",
            s_standardLengths[i]);
        const char* glue = CalendarValue(cldr, locale, calendar, rest);
        if (glue == nullptr)
        {
            Format(rest, sizeof(rest),
                   "dateTimeFormats/dateTimeFormatLength[@type=\"%s\"]/dateTimeFormat/pattern",
                   s_standardLengths[i]);
            glue = CalendarValue(cldr, locale, calendar, rest);
        }
        g->glue[i] = Copy(glue != nullptr ? glue : "{1} {0}");
    }
}

static void AddNames(Cldr* cldr, const char* locale, const char* calendar, PatternGenerator* g)
{
    for (int i = 0; i < skeletonFieldCount; i++)
    {
        char rest[256];
        Format(rest, sizeof(rest), "dateTimeFormats/appendItems/appendItem[@request=\"%s\"]",
               s_appendRequests[i]);
        const char* item = CalendarValue(cldr, locale, calendar, rest);
        g->appendItems[i] = Copy(item != nullptr ? item : s_defaultAppend);
        char chain[256];
        Format(chain, sizeof(chain), "//ldml/dates/fields/field[@type=\"%s\"]/displayName",
               s_fieldNames[i]);
        const char* name = Resolve(cldr, locale, chain);
        char fallback[8];
        Format(fallback, sizeof(fallback), "F%d", i);
        g->fieldNames[i] = Copy(name != nullptr ? name : fallback);
    }
    const char* system = Resolve(cldr, locale, "//ldml/numbers/defaultNumberingSystem");
    char chain[256];
    Format(chain, sizeof(chain), "//ldml/numbers/symbols[@numberSystem=\"%s\"]/decimal",
           system != nullptr ? system : "latn");
    const char* decimal = Resolve(cldr, locale, chain);
    g->decimal = Copy(decimal != nullptr ? decimal : ".");
}

void DateSourceLoad(Cldr* cldr, const DateSupplement* s, const char* locale, const char* calendar,
                    PatternGenerator* g)
{
    AddStandard(cldr, locale, calendar, g);
    AddItems(cldr, locale, calendar, g);
    AddNames(cldr, locale, calendar, g);
    Hours(s, locale, g);
}

int PrintSkeletons(const char* commonDirectory, char* errorOut, size_t errorCapacity)
{
    Cldr cldr;
    DateSupplement supplement;
    if (!CldrOpen(&cldr, commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
    if (!DateSupplementLoad(&supplement, commonDirectory, errorOut, errorCapacity))
    {
        CldrClose(&cldr);
        return 0;
    }
    PatternGenerator g;
    PatternGeneratorInit(&g);
    char loaded[1024] = "";
    char line[512];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        char* calendar = strchr(line, '\t');
        char* skeleton = calendar == nullptr ? nullptr : strchr(calendar + 1, '\t');
        if (skeleton == nullptr)
        {
            puts("?");
            continue;
        }
        *calendar++ = '\0';
        *skeleton++ = '\0';
        char key[1024];
        Format(key, sizeof(key), "%s\t%s", line, calendar);
        if (strcmp(key, loaded) != 0)
        {
            PatternGeneratorFree(&g);
            PatternGeneratorInit(&g);
            DateSourceLoad(&cldr, &supplement, line, calendar, &g);
            Format(loaded, sizeof(loaded), "%s", key);
        }
        char* pattern = PatternGeneratorBest(&g, skeleton);
        puts(pattern);
        free(pattern);
    }
    PatternGeneratorFree(&g);
    DateSupplementFree(&supplement);
    CldrClose(&cldr);
    return 1;
}

const char* DateSkeletonOf(Cldr* cldr, const char* locale, const char* calendar, int length)
{
    static const char* const s_lengths[3] = {"long", "medium", "short"};
    char rest[256];
    Format(rest, sizeof(rest),
           "dateFormats/dateFormatLength[@type=\"%s\"]/dateFormat/datetimeSkeleton",
           s_lengths[length]);
    const char* skeleton = CalendarValue(cldr, locale, calendar, rest);
    return skeleton != nullptr ? skeleton : "yMd";
}

const char* DateCalendarValue(Cldr* cldr, const char* locale, const char* calendar,
                              const char* rest)
{
    return CalendarValue(cldr, locale, calendar, rest);
}

const char* DateStandardPattern(Cldr* cldr, const char* locale, const char* calendar, int length)
{
    char rest[256];
    Format(rest, sizeof(rest), "dateFormats/dateFormatLength[@type=\"%s\"]/dateFormat/pattern",
           s_standardLengths[length]);
    return CalendarValue(cldr, locale, calendar, rest);
}

// The numbers attribute along a chain as CLDR's JSON resolves it: the
// first element's that has one, inheritance markers passed through, up
// to the element that holds the value.
static const char* NumbersAlong(Cldr* cldr, const char* locale, const char* chain, bool* found)
{
    for (const char* at = locale; at != nullptr; at = CldrParent(cldr, at, componentMain))
    {
        const LdmlFile* file = CldrFile(cldr, at, componentMain);
        const Leaf* leaf = file == nullptr ? nullptr : MapGet(&file->leaves, chain);
        if (leaf == nullptr)
        {
            continue;
        }
        for (int i = 0; i < leaf->valueCount; i++)
        {
            if (strcmp(leaf->valueNames[i], "numbers") == 0)
            {
                *found = true;
                return leaf->values[i];
            }
        }
        if (strcmp(leaf->text, "\xE2\x86\x91\xE2\x86\x91\xE2\x86\x91") != 0)
        {
            *found = true;
            return nullptr;
        }
    }
    *found = false;
    return nullptr;
}

const char* DateNumbersOf(Cldr* cldr, const char* locale, const char* calendar, int length)
{
    char chain[cldrMaxChain];
    char rest[256];
    Format(rest, sizeof(rest), "dateFormats/dateFormatLength[@type=\"%s\"]/dateFormat/pattern",
           s_standardLengths[length]);
    Format(chain, sizeof(chain), s_calendarChain, calendar, rest);
    for (int depth = 0; depth < 8; depth++)
    {
        bool found = false;
        const char* numbers = NumbersAlong(cldr, locale, chain, &found);
        char target[cldrMaxChain];
        if (found || !CldrAliasOf(cldr, chain, target, componentMain))
        {
            return numbers;
        }
        memcpy(chain, target, sizeof(chain));
    }
    return nullptr;
}

// A field set written as letters: Y, M, D, E, T and Z.
static unsigned FieldsOf(const char* letters)
{
    static const char s_letters[] = "YMDETZ";
    unsigned fields = 0;
    for (const char* p = letters; *p != '\0'; p++)
    {
        const char* at = strchr(s_letters, *p);
        fields |= at != nullptr ? 1U << (at - s_letters) : 1U << 8;
    }
    return fields;
}

// Answers one line: "locale calendar fields length year-style hour-cycle
// precision zone-style", numbers as semantic.h's enumerations.
static void AnswerSemantic(Cldr* cldr, const DateSupplement* s, PatternGenerator* g, char* loaded,
                           size_t loadedCapacity, const char* line)
{
    char locale[64];
    char calendar[64];
    char fields[16];
    int length = 0;
    int year = 0;
    int hour = 0;
    int precision = 0;
    int zone = 0;
    if (sscanf(line, "%63s %63s %15s %d %d %d %d %d", locale, calendar, fields, &length, &year,
               &hour, &precision, &zone) != 8 ||
        length < 0 || length > 2 || year < 0 || year > 2 || hour < 0 || hour > 6 || precision < 0 ||
        precision > 11 || zone < 0 || zone > 3)
    {
        puts("?");
        return;
    }
    char key[160];
    Format(key, sizeof(key), "%s\t%s", locale, calendar);
    if (strcmp(key, loaded) != 0)
    {
        PatternGeneratorFree(g);
        PatternGeneratorInit(g);
        DateSourceLoad(cldr, s, locale, calendar, g);
        Format(loaded, loadedCapacity, "%s", key);
    }
    SemanticSkeleton semantic = {
        FieldsOf(fields), (SemanticLength)length, (SemanticYearStyle)year, (SemanticHourCycle)hour,
        precision,        (SemanticZoneStyle)zone};
    char classical[64];
    if (!SemanticClassical(&semantic, DateSkeletonOf(cldr, locale, calendar, length), classical,
                           sizeof(classical)))
    {
        puts("-");
        return;
    }
    char* pattern = PatternGeneratorBest(g, classical);
    printf("%s\t%s\n", classical, pattern);
    free(pattern);
}

int PrintSemantic(const char* commonDirectory, char* errorOut, size_t errorCapacity)
{
    Cldr cldr;
    DateSupplement supplement;
    if (!CldrOpen(&cldr, commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
    if (!DateSupplementLoad(&supplement, commonDirectory, errorOut, errorCapacity))
    {
        CldrClose(&cldr);
        return 0;
    }
    PatternGenerator g;
    PatternGeneratorInit(&g);
    char loaded[160] = "";
    char line[512];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        AnswerSemantic(&cldr, &supplement, &g, loaded, sizeof(loaded), line);
    }
    PatternGeneratorFree(&g);
    DateSupplementFree(&supplement);
    CldrClose(&cldr);
    return 1;
}
