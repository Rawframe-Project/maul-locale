// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's currency units (currency_gen.h). Whether a symbol
// starts or ends in a symbol or a space is Maul Unicode's
// General_Category, so the library needs no property lookup to choose an
// alphaNextToNumber pattern.

#include "currency_gen.h"

#include "currency_tables.h"
#include "kinds.h"
#include "locale_tables.h"
#include "number_tables.h"
#include "table_gen.h"
#include "xml.h"

#include "maul-unicode/properties.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxFractions = 256,
    maxRegions = 512,
    maxRegionEntries = 1024,
    maxCurrencies = 512,
    maxKey = 64,
    maxSymbolPoints = 64,
    maxNamePoints = 256,
    // The fields of a currency the symbols unit holds: the four symbols,
    // the pattern, the decimal and group separators.
    fieldCount = 7,
    fieldPattern = 4,
    fieldDecimal = 5,
    fieldGroup = 6,
};

static const char s_inherit[] = "\xE2\x86\x91\xE2\x86\x91\xE2\x86\x91";
static const char s_empty[] = "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85";
static const char s_prefix[] = "//ldml/numbers/currencies/currency[@type=\"";

// The element of each name field after the currency's, in field order.
static const char* const s_names[fieldCount] = {"displayName",
                                                "displayName[@count=\"zero\"]",
                                                "displayName[@count=\"one\"]",
                                                "displayName[@count=\"two\"]",
                                                "displayName[@count=\"few\"]",
                                                "displayName[@count=\"many\"]",
                                                "displayName[@count=\"other\"]"};

// The element of each field after the currency's, in field order.
static const char* const s_fields[fieldCount] = {"symbol",
                                                 "symbol[@alt=\"narrow\"]",
                                                 "symbol[@alt=\"variant\"]",
                                                 "symbol[@alt=\"formal\"]",
                                                 "pattern",
                                                 "decimal",
                                                 "group"};

typedef struct Fraction
{
    uint16_t code;
    uint8_t values[4];
} Fraction;

// A digit count or rounding of the fractions, or -1 when the attribute is
// not a small decimal integer.
static int Small(const char* text)
{
    if (text == nullptr || text[0] < '0' || text[0] > '9' || strlen(text) > 2)
    {
        return -1;
    }
    return text[1] == '\0'                    ? text[0] - '0'
           : text[1] >= '0' && text[1] <= '9' ? (text[0] - '0') * 10 + (text[1] - '0')
                                              : -1;
}

static Fraction ReadFraction(const XmlNode* info)
{
    const char* code = XmlGetAttribute(info, "iso4217");
    if (code == nullptr)
    {
        GenDie("a currency fraction without a code", "supplementalData.xml");
    }
    Fraction f = {.code = mlocPackCurrency(code, (uint32_t)strlen(code))};
    if (strcmp(code, "DEFAULT") != 0 && f.code == 0)
    {
        GenDie("a currency fraction's code is not three letters", code);
    }
    int digits = Small(XmlGetAttribute(info, "digits"));
    int rounding = Small(XmlGetAttribute(info, "rounding"));
    const char* cashDigitsText = XmlGetAttribute(info, "cashDigits");
    const char* cashRoundingText = XmlGetAttribute(info, "cashRounding");
    int cashDigits = cashDigitsText == nullptr ? digits : Small(cashDigitsText);
    int cashRounding = cashRoundingText == nullptr ? rounding : Small(cashRoundingText);
    if (digits < 0 || digits > currencyMaxDigits || cashDigits < 0 ||
        cashDigits > currencyMaxDigits || rounding < 0 || cashRounding < 0 ||
        !mlocValidCurrencyRounding((uint32_t)rounding) ||
        !mlocValidCurrencyRounding((uint32_t)cashRounding))
    {
        GenDie("a currency fraction the library cannot round by", code);
    }
    f.values[0] = (uint8_t)digits;
    f.values[1] = (uint8_t)rounding;
    f.values[2] = (uint8_t)cashDigits;
    f.values[3] = (uint8_t)cashRounding;
    return f;
}

static int AddFractions(const XmlNode* data, DataFile* file, char* errorOut, size_t errorCapacity)
{
    const XmlNode* list = data == nullptr ? nullptr : XmlFindChild(data, "fractions");
    static Fraction s_fractions[maxFractions];
    int count = 0;
    bool hasDefault = false;
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        if (strcmp(list->children[i]->name, "info") != 0)
        {
            continue;
        }
        if (count == maxFractions)
        {
            GenDie("too many currency fractions", "supplementalData.xml");
        }
        Fraction f = ReadFraction(list->children[i]);
        hasDefault |= f.code == 0;
        // Sorted by code, the default first.
        int at = count++;
        for (; at > 0 && s_fractions[at - 1].code >= f.code; at--)
        {
            if (s_fractions[at - 1].code == f.code)
            {
                GenDie("a currency fraction given twice", "supplementalData.xml");
            }
            s_fractions[at] = s_fractions[at - 1];
        }
        s_fractions[at] = f;
    }
    if (!hasDefault)
    {
        snprintf(errorOut, errorCapacity, "supplementalData.xml has no default currency fraction");
        return 0;
    }
    size_t size = currencyFractionsHeaderSize + (size_t)count * currencyFractionSize;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, (uint64_t)count, 4);
    for (int i = 0; i < count; i++)
    {
        uint8_t* entry = unit + currencyFractionsHeaderSize + (size_t)i * currencyFractionSize;
        GenPut(entry + fractionCode, s_fractions[i].code, 2);
        memcpy(entry + fractionDigits, s_fractions[i].values, 4);
    }
    DataFileAdd(file, kindCurrencyFractions, nullptr, unit, size);
    free(unit);
    return 1;
}

// The days from the Unix epoch's to a proleptic Gregorian date (Howard
// Hinnant's days_from_civil).
static int32_t DaysFromCivil(int32_t year, int32_t month, int32_t day)
{
    year -= month <= 2;
    int32_t era = (year >= 0 ? year : year - 399) / 400;
    int32_t yearOfEra = year - era * 400;
    int32_t dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

// A from or to day: "yyyy-mm-dd", the hour absent (UTS #35's date ranges,
// which then include the day), or the open end when the attribute is.
static int32_t ReadDay(const XmlNode* currency, const char* name, int32_t open)
{
    const char* text = XmlGetAttribute(currency, name);
    if (text == nullptr)
    {
        return open;
    }
    int year = 0;
    int month = 0;
    int day = 0;
    int end = 0;
    if (sscanf(text, "%4d-%2d-%2d%n", &year, &month, &day, &end) != 3 || text[end] != '\0' ||
        year < 1000 || month < 1 || month > 12 || day < 1 || day > 31)
    {
        GenDie("a currency date that is not yyyy-mm-dd", text);
    }
    return DaysFromCivil(year, month, day);
}

typedef struct RegionEntry
{
    uint16_t code;
    uint8_t tender;
    int32_t from;
    int32_t to;
} RegionEntry;

typedef struct Region
{
    uint32_t region;
    uint32_t first;
    uint32_t count;
} Region;

static void ReadRegion(const XmlNode* node, Region* region, RegionEntry* entries,
                       uint32_t* entryCount)
{
    const char* id = XmlGetAttribute(node, "iso3166");
    region->region = id == nullptr ? 0 : mlocPackRegion(id, (uint32_t)strlen(id));
    region->first = *entryCount;
    if (region->region == 0)
    {
        GenDie("a currency region that is no region", id == nullptr ? "(none)" : id);
    }
    for (int i = 0; i < node->childCount; i++)
    {
        const XmlNode* c = node->children[i];
        if (strcmp(c->name, "currency") != 0)
        {
            continue;
        }
        const char* code = XmlGetAttribute(c, "iso4217");
        const char* tender = XmlGetAttribute(c, "tender");
        if (*entryCount == maxRegionEntries)
        {
            GenDie("too many currency region entries", id);
        }
        RegionEntry e = {
            .code = code == nullptr ? 0 : mlocPackCurrency(code, (uint32_t)strlen(code)),
            .tender = tender == nullptr || strcmp(tender, "false") != 0,
            .from = ReadDay(c, "from", INT32_MIN),
            .to = ReadDay(c, "to", INT32_MAX),
        };
        if (e.code == 0 || e.from > e.to)
        {
            GenDie("a region's currency without a code or with an empty span", id);
        }
        entries[(*entryCount)++] = e;
    }
    region->count = *entryCount - region->first;
}

static void AddRegions(const XmlNode* data, DataFile* file)
{
    static Region s_regions[maxRegions];
    static RegionEntry s_entries[maxRegionEntries];
    uint32_t regionCount = 0;
    uint32_t entryCount = 0;
    for (int i = 0; data != nullptr && i < data->childCount; i++)
    {
        if (strcmp(data->children[i]->name, "region") != 0)
        {
            continue;
        }
        if (regionCount == maxRegions)
        {
            GenDie("too many currency regions", "supplementalData.xml");
        }
        Region r;
        ReadRegion(data->children[i], &r, s_entries, &entryCount);
        if (r.count == 0)
        {
            continue;
        }
        // Sorted by region; the entries stay in CLDR's order.
        uint32_t at = regionCount++;
        for (; at > 0 && s_regions[at - 1].region >= r.region; at--)
        {
            if (s_regions[at - 1].region == r.region)
            {
                GenDie("a currency region given twice", "supplementalData.xml");
            }
            s_regions[at] = s_regions[at - 1];
        }
        s_regions[at] = r;
    }
    size_t size = currencyRegionsHeaderSize + (size_t)regionCount * currencyRegionSize +
                  (size_t)entryCount * currencyRegionEntrySize;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, regionCount, 2);
    GenPut(unit + 2, entryCount, 2);
    uint8_t* out = unit + currencyRegionsHeaderSize + (size_t)regionCount * currencyRegionSize;
    uint32_t written = 0;
    for (uint32_t i = 0; i < regionCount; i++)
    {
        uint8_t* r = unit + currencyRegionsHeaderSize + (size_t)i * currencyRegionSize;
        GenPut(r, s_regions[i].region, 2);
        GenPut(r + 2, written, 2);
        for (uint32_t j = 0; j < s_regions[i].count; j++, written++)
        {
            const RegionEntry* e = &s_entries[s_regions[i].first + j];
            uint8_t* w = out + (size_t)written * currencyRegionEntrySize;
            GenPut(w + regionEntryCode, e->code, 2);
            w[regionEntryTender] = e->tender;
            GenPut(w + regionEntryFrom, (uint32_t)e->from, 4);
            GenPut(w + regionEntryTo, (uint32_t)e->to, 4);
        }
    }
    DataFileAdd(file, kindCurrencyRegions, nullptr, unit, size);
    free(unit);
}

// The units of supplementalData.xml's currencyData: the fractions and
// the regions.
static int AddSupplemental(const char* commonDirectory, DataFile* file, char* errorOut,
                           size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/supplementalData.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    const XmlNode* data = XmlFindChild(root, "currencyData");
    int ok = AddFractions(data, file, errorOut, errorCapacity);
    if (ok)
    {
        AddRegions(data, file);
    }
    XmlFree(root);
    return ok;
}

// Decodes text's code points into out; their count, or -1 for malformed
// UTF-8, a control character or more than the room.
static int Decode(const char* text, uint32_t* out, int capacity)
{
    int count = 0;
    for (const unsigned char* c = (const unsigned char*)text; *c != '\0';)
    {
        int n = GenCharLength(c);
        if (n == 0 || *c < 0x20 || count == capacity)
        {
            return -1;
        }
        static const unsigned s_masks[5] = {0, 0x7F, 0x1F, 0x0F, 0x07};
        uint32_t cp = c[0] & s_masks[n];
        for (int i = 1; i < n; i++)
        {
            cp = cp << 6 | (c[i] & 0x3Fu);
        }
        out[count++] = cp;
        c += n;
    }
    return count;
}

// Whether a code point is neither a symbol nor a space.
static bool IsAlpha(uint32_t cp)
{
    muniGeneralCategory gc = muniGetGeneralCategory(cp);
    return gc < muni_gcSm || gc > muni_gcZp;
}

// A symbol's spacing flags (currency_tables.h).
static uint8_t SpacingFlags(const char* symbol)
{
    uint32_t points[maxSymbolPoints];
    int count = Decode(symbol, points, maxSymbolPoints);
    if (count <= 0)
    {
        GenDie("a currency symbol empty, too long or malformed", symbol);
    }
    return (uint8_t)((IsAlpha(points[0]) ? currencyStartsAlpha : 0) |
                     (IsAlpha(points[count - 1]) ? currencyEndsAlpha : 0));
}

// A field's value given by a locale's own file, or null: an inheritance
// marker or a draft below the threshold gives none. With the library's
// lookup (each field along the chain, then its lateral fallback from the
// locale asked for: a category's name to other's, then to the name
// without a count; an alternative symbol to the symbol), this is CLDR's
// resolution: it agrees with CLDR's JSON on 481,409 of 481,455 currency
// values, all but 46 Norwegian Nynorsk names.
static const char* OwnValue(Cldr* cldr, const char* locale, const char* chain)
{
    const LdmlFile* file = CldrFile(cldr, locale, componentMain);
    const Leaf* leaf = file == nullptr ? nullptr : MapGet(&file->leaves, chain);
    if (leaf == nullptr || leaf->draft > cldr->threshold || strcmp(leaf->text, s_inherit) == 0 ||
        strcmp(leaf->text, s_empty) == 0)
    {
        return nullptr;
    }
    return leaf->text;
}

// The value of a field along a locale's chain, as the library looks it
// up: each key's own entries in turn, root's last.
static const char* ChainValue(Cldr* cldr, const char* locale, const char* chain)
{
    for (const char* at = locale; at != nullptr; at = CldrParent(cldr, at, componentMain))
    {
        const char* value = OwnValue(cldr, at, chain);
        if (value != nullptr)
        {
            return value;
        }
    }
    return nullptr;
}

// A currency's fields a locale gives of its own.
typedef struct Own
{
    uint16_t code;
    const char* fields[fieldCount];
} Own;

// A locale's own currencies: the symbol fields, and the names.
typedef struct Collect
{
    const CurrencyInput* input;
    const char* locale;
    const char* parent;
    Own* own[2];
    int count[2];
} Collect;

// The index of a field's element in a list, or fieldCount.
static int FieldOf(const char* const* list, const char* field)
{
    int f = 0;
    while (f < fieldCount && strcmp(field, list[f]) != 0)
    {
        f++;
    }
    return f;
}

// Takes one chain of a locale's file: a currency field the parent's
// chain does not give alike.
static void CollectChain(const char* chain, void* value, void* context)
{
    (void)value;
    Collect* c = context;
    size_t prefix = sizeof(s_prefix) - 1;
    if (strncmp(chain, s_prefix, prefix) != 0)
    {
        return;
    }
    const char* code = chain + prefix;
    const char* field = code + 3;
    uint16_t packed = mlocPackCurrency(code, 3);
    if (packed == 0 || strncmp(field, "\"]/", 3) != 0)
    {
        GenDie("a currency whose code is not three letters", chain);
    }
    field += 3;
    int list = strncmp(field, "displayName", 11) == 0 ? 1 : 0;
    int f = FieldOf(list == 0 ? s_fields : s_names, field);
    if (f == fieldCount)
    {
        GenDie("a currency element the generator does not know", chain);
    }
    Cldr* cldr = c->input->cldr;
    const char* own = OwnValue(cldr, c->locale, chain);
    const char* inherited = c->parent == nullptr ? nullptr : ChainValue(cldr, c->parent, chain);
    if (own == nullptr || (inherited != nullptr && strcmp(own, inherited) == 0))
    {
        return;
    }
    Own* entries = c->own[list];
    int at = 0;
    while (at < c->count[list] && entries[at].code != packed)
    {
        at++;
    }
    if (at == c->count[list])
    {
        if (c->count[list] == maxCurrencies)
        {
            GenDie("too many currencies in a locale", c->locale);
        }
        entries[c->count[list]++] = (Own){.code = packed};
    }
    entries[at].fields[f] = own;
}

static int CompareOwn(const void* a, const void* b)
{
    return (int)((const Own*)a)->code - (int)((const Own*)b)->code;
}

// Whether a code point is a decimal digit or one of number parsing's
// ignorables but the bidi controls: Nd, Zs, the tab or a variation
// selector.
static bool DigitOrSpace(uint32_t cp)
{
    muniGeneralCategory gc = muniGetGeneralCategory(cp);
    bool selector = (cp >= 0x180B && cp <= 0x180D) || cp == 0x180F ||
                    (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0xE0100 && cp <= 0xE01EF);
    return gc == muni_gcNd || gc == muni_gcZs || cp == '\t' || selector;
}

// A string field's index in the unit's table, checked for the bytes a
// table may hold. A symbol or a name that parsing reads (read set) does
// not begin with a digit or an ignorable other than a bidi control (Hebrew
// writes its yuan after a mark), so that currency parsing passes those at
// once.
static uint16_t FieldString(Table* t, const char* text, const char* locale, bool read)
{
    uint32_t points[maxNamePoints];
    if (Decode(text, points, maxNamePoints) <= 0)
    {
        GenDie("a currency string empty, too long or malformed", locale);
    }
    if (read && DigitOrSpace(points[0]))
    {
        GenDie("a currency symbol or name that begins with a digit or a space", locale);
    }
    return TableAddString(t, text, strlen(text));
}

static void AddSymbolUnit(const CurrencyInput* input, Table* t, Own* own, int count,
                          const char* locale, const char* key, DataFile* file)
{
    static uint16_t s_codes[maxCurrencies];
    static uint8_t s_rows[maxCurrencies][1 + 2 * fieldCount];
    static uint8_t* s_rowPointers[maxCurrencies];
    static size_t s_rowSizes[maxCurrencies];
    static uint8_t s_flags[maxTable];
    qsort(own, (size_t)count, sizeof(Own), CompareOwn);
    for (int i = 0; i < count; i++)
    {
        uint8_t* row = s_rows[i];
        size_t size = 1;
        row[0] = 0;
        for (int f = 0; f < fieldCount; f++)
        {
            const char* text = own[i].fields[f];
            if (text == nullptr)
            {
                continue;
            }
            uint16_t index = 0;
            if (f == fieldPattern)
            {
                index = input->compile(input->context, text);
            }
            else
            {
                index = FieldString(t, text, locale, f < fieldPattern);
                s_flags[index] = SpacingFlags(text);
            }
            row[0] |= (uint8_t)(1u << f);
            GenPut(row + size, index, 2);
            size += 2;
        }
        s_codes[i] = own[i].code;
        s_rowPointers[i] = row;
        s_rowSizes[i] = size;
    }
    TableAddSparseUnit(t, s_codes, s_rowPointers, s_rowSizes, count, s_flags, kindCurrencySymbols,
                       key, file);
    TableClear(t);
}

static void AddNameUnit(Table* t, Own* own, int count, const char* locale, const char* key,
                        DataFile* file)
{
    static uint16_t s_codes[maxCurrencies];
    static uint8_t s_rows[maxCurrencies][1 + 2 * fieldCount];
    static uint8_t* s_rowPointers[maxCurrencies];
    static size_t s_rowSizes[maxCurrencies];
    qsort(own, (size_t)count, sizeof(Own), CompareOwn);
    for (int i = 0; i < count; i++)
    {
        uint8_t* row = s_rows[i];
        size_t size = 1;
        row[0] = 0;
        for (int f = 0; f < fieldCount; f++)
        {
            if (own[i].fields[f] != nullptr)
            {
                row[0] |= (uint8_t)(1u << f);
                GenPut(row + size, FieldString(t, own[i].fields[f], locale, true), 2);
                size += 2;
            }
        }
        s_codes[i] = own[i].code;
        s_rowPointers[i] = row;
        s_rowSizes[i] = size;
    }
    TableAddSparseUnit(t, s_codes, s_rowPointers, s_rowSizes, count, nullptr, kindCurrencyNames,
                       key, file);
    TableClear(t);
}

int GenerateCurrencyUnits(const CurrencyInput* input, DataFile* file, char* errorOut,
                          size_t errorCapacity)
{
    if (!AddSupplemental(input->commonDirectory, file, errorOut, errorCapacity))
    {
        return 0;
    }
    static Table s_table;
    static Own s_own[maxCurrencies];
    static Own s_nameEntries[maxCurrencies];
    s_table = (Table){0};
    for (int i = 0; i < input->count; i++)
    {
        const char* locale = input->ids[i];
        const LdmlFile* own = CldrFile(input->cldr, locale, componentMain);
        if (own == nullptr)
        {
            continue;
        }
        Collect c = {
            .input = input,
            .locale = locale,
            .parent = CldrParent(input->cldr, locale, componentMain),
            .own = {s_own, s_nameEntries},
        };
        MapEach(&own->leaves, CollectChain, &c);
        if (c.count[0] == 0 && c.count[1] == 0)
        {
            continue;
        }
        char key[maxKey];
        char parentKey[maxKey] = "";
        KeyOf(input->keys, locale, key, sizeof(key));
        if (c.parent != nullptr)
        {
            KeyOf(input->keys, c.parent, parentKey, sizeof(parentKey));
        }
        if (strcmp(key, parentKey) == 0)
        {
            GenDie("a locale under its parent's key with currencies of its own", locale);
        }
        if (c.count[0] > 0)
        {
            AddSymbolUnit(input, &s_table, s_own, c.count[0], locale, key, file);
        }
        if (c.count[1] > 0)
        {
            AddNameUnit(&s_table, s_nameEntries, c.count[1], locale, key, input->names);
        }
    }
    return 1;
}
