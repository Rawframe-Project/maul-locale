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
#include "number_tables.h"
#include "table_gen.h"
#include "xml.h"

#include "maul-unicode/properties.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxFractions = 256,
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

static int AddFractions(const char* commonDirectory, DataFile* file, char* errorOut,
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
            GenDie("too many currency fractions", path);
        }
        Fraction f = ReadFraction(list->children[i]);
        hasDefault |= f.code == 0;
        // Sorted by code, the default first.
        int at = count++;
        for (; at > 0 && s_fractions[at - 1].code >= f.code; at--)
        {
            if (s_fractions[at - 1].code == f.code)
            {
                GenDie("a currency fraction given twice", path);
            }
            s_fractions[at] = s_fractions[at - 1];
        }
        s_fractions[at] = f;
    }
    XmlFree(root);
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

// A string field's index in the unit's table, checked for the bytes a
// table may hold.
static uint16_t FieldString(Table* t, const char* text, const char* locale)
{
    uint32_t points[maxNamePoints];
    if (Decode(text, points, maxNamePoints) <= 0)
    {
        GenDie("a currency string empty, too long or malformed", locale);
    }
    return TableAddString(t, text, strlen(text));
}

// Writes a unit of the sparse shape (currency_tables.h): the table's
// strings, and per entry its code and row; with flags, a byte per string
// after the text.
static void AddSparseUnit(const Table* t, const uint16_t* codes, uint8_t* const* rows,
                          const size_t* rowSizes, int count, const uint8_t* flags, uint32_t kind,
                          const char* key, DataFile* file)
{
    size_t text = 0;
    for (int i = 0; i < t->stringCount; i++)
    {
        text += t->lengths[i];
    }
    size_t rowBytes = 0;
    for (int i = 0; i < count; i++)
    {
        rowBytes += rowSizes[i];
    }
    if (text > 0xFFFF || rowBytes > 0xFFFF)
    {
        GenDie("a currency unit too large", key);
    }
    size_t strings = 2 * ((size_t)t->stringCount + 1);
    size_t entries = 2 * (size_t)count + 2 * ((size_t)count + 1);
    size_t size = sparseHeaderSize + strings + entries + rowBytes + text +
                  (flags != nullptr ? (size_t)t->stringCount : 0);
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, (uint64_t)t->stringCount, 2);
    GenPut(unit + 2, (uint64_t)count, 2);
    GenPut(unit + 4, text, 4);
    GenPut(unit + 8, rowBytes, 4);
    uint8_t* at = unit + sparseHeaderSize;
    uint8_t* textAt = at + strings + entries + rowBytes;
    size_t offset = 0;
    for (int i = 0; i < t->stringCount; i++)
    {
        GenPut(at + 2 * (size_t)i, offset, 2);
        memcpy(textAt + offset, t->texts[i], t->lengths[i]);
        offset += t->lengths[i];
    }
    GenPut(at + 2 * (size_t)t->stringCount, offset, 2);
    at += strings;
    uint8_t* rowOffsets = at + 2 * (size_t)count;
    uint8_t* rowAt = rowOffsets + 2 * ((size_t)count + 1);
    offset = 0;
    for (int i = 0; i < count; i++)
    {
        GenPut(at + 2 * (size_t)i, codes[i], 2);
        GenPut(rowOffsets + 2 * (size_t)i, offset, 2);
        memcpy(rowAt + offset, rows[i], rowSizes[i]);
        offset += rowSizes[i];
    }
    GenPut(rowOffsets + 2 * (size_t)count, offset, 2);
    if (flags != nullptr)
    {
        memcpy(textAt + text, flags, (size_t)t->stringCount);
    }
    DataFileAdd(file, kind, key, unit, size);
    free(unit);
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
                index = FieldString(t, text, locale);
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
    AddSparseUnit(t, s_codes, s_rowPointers, s_rowSizes, count, s_flags, kindCurrencySymbols, key,
                  file);
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
                GenPut(row + size, FieldString(t, own[i].fields[f], locale), 2);
                size += 2;
            }
        }
        s_codes[i] = own[i].code;
        s_rowPointers[i] = row;
        s_rowSizes[i] = size;
    }
    AddSparseUnit(t, s_codes, s_rowPointers, s_rowSizes, count, nullptr, kindCurrencyNames, key,
                  file);
    TableClear(t);
}

int GenerateCurrencyUnits(const CurrencyInput* input, DataFile* file, char* errorOut,
                          size_t errorCapacity)
{
    if (!AddFractions(input->commonDirectory, file, errorOut, errorCapacity))
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
