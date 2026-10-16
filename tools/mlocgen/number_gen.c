// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's units (number_gen.h). Each locale's numbers are
// resolved for every numeric numbering system by CLDR's inheritance
// (cldr.h), so root's aliases from a system to latn take the locale's
// own latn data. Patterns are compiled here once: the library never
// reads a pattern string.

#include "number_gen.h"

#include "cldr.h"
#include "context_gen.h"
#include "currency_gen.h"
#include "currency_tables.h"
#include "date_gen.h"
#include "keys.h"
#include "kinds.h"
#include "list_gen.h"
#include "map.h"
#include "names_gen.h"
#include "number_tables.h"
#include "parse_gen.h"
#include "table_gen.h"
#include "unit_names_gen.h"
#include "unit_patterns_gen.h"
#include "xml.h"
#include "zone_names_gen.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

enum
{
    // A compact slot's pattern index where CLDR names none, and where it
    // names the pattern "0" (no compacting).
    numberCompactAbsent = 0xFFFF,
    numberCompactNone = 0xFFFE,
    maxSystems = 96,
    maxLocales = 2048,
    maxKey = 64,
    maxAffix = 128,
};

static const char* const s_emptyMarker = "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85";

// The CLDR element of each symbol, in NumberSymbol order.
static const char* const s_symbolNames[numberSymbolCount] = {
    "decimal",           "group",
    "minusSign",         "plusSign",
    "percentSign",       "perMille",
    "exponential",       "superscriptingExponent",
    "infinity",          "nan",
    "approximatelySign", "currencyDecimal",
    "currencyGroup",     "(range pattern)"};

typedef struct System
{
    uint64_t id;
    char name[9];
    uint8_t digits[10][numberDigitSize];
} System;

// A numbering system's data in a locale.
typedef struct Entry
{
    // The system's index.
    uint16_t system;
    uint16_t symbols;
    uint16_t decimal;
    uint16_t percent;
    uint16_t compactShort;
    uint16_t compactLong;
    // Its currency pattern set, and the compact currency tables the set
    // holds.
    uint16_t currency;
    uint16_t currencyCompact;
    uint16_t currencyAlpha;
} Entry;

// A locale's resolved numbers.
typedef struct Numbers
{
    // System indexes, or numberNoSystem or numberAlgorithmic.
    int defaultSystem;
    int native;
    int traditional;
    int finance;
    int minGrouping;
    int entryCount;
    Entry entries[maxSystems];
} Numbers;

typedef struct Generator
{
    Cldr cldr;
    Keys keys;
    System systems[maxSystems];
    int systemCount;
    Table symbols;
    Table patterns;
    Table compactPatterns;
    Table currencySets;
    // The distinct compact tables, each of its own size.
    uint8_t* tables[maxTable];
    size_t tableSizes[maxTable];
    int tableCount;
    Map tableIndex;
    // Each file's numbering systems with decimal formats of its own, as a
    // bool per system, by locale id.
    Map ownFormats;
} Generator;

// The index of a compact table, added when new.
static uint16_t AddBlob(Generator* g, const uint8_t* bytes, size_t size)
{
    char key[2 * numberCompactRows * (2 + 2 * numberCompactSlots) + 1];
    for (size_t i = 0; i < size; i++)
    {
        snprintf(key + 2 * i, 3, "%02X", bytes[i]);
    }
    key[2 * size] = '\0';
    void* found = MapGet(&g->tableIndex, key);
    if (found != nullptr)
    {
        return (uint16_t)((uintptr_t)found - 1);
    }
    if (g->tableCount == maxTable)
    {
        GenDie("too many compact tables", "tables");
    }
    g->tables[g->tableCount] = GenAllocate(size);
    memcpy(g->tables[g->tableCount], bytes, size);
    g->tableSizes[g->tableCount] = size;
    MapPut(&g->tableIndex, key, (void*)(uintptr_t)(g->tableCount + 1));
    return (uint16_t)g->tableCount++;
}

static void AddCompactUnit(Generator* g, DataFile* file)
{
    size_t data = 0;
    for (int i = 0; i < g->tableCount; i++)
    {
        data += g->tableSizes[i];
    }
    size_t offsets = 4 * (size_t)(g->tableCount + 1);
    size_t size = numberCompactHeaderSize + offsets + data;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, (uint64_t)g->tableCount, 4);
    size_t at = 0;
    for (int i = 0; i < g->tableCount; i++)
    {
        GenPut(unit + numberCompactHeaderSize + 4 * (size_t)i, at, 4);
        memcpy(unit + numberCompactHeaderSize + offsets + at, g->tables[i], g->tableSizes[i]);
        at += g->tableSizes[i];
        free(g->tables[i]);
    }
    GenPut(unit + numberCompactHeaderSize + 4 * (size_t)g->tableCount, at, 4);
    DataFileAdd(file, kindNumberCompact, nullptr, unit, size);
    free(unit);
    MapFree(&g->tableIndex);
}

static int ReadSystems(Generator* g, const char* commonDirectory, char* errorOut,
                       size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/numberingSystems.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    const XmlNode* list = XmlFindChild(root, "numberingSystems");
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        const XmlNode* node = list->children[i];
        const char* type = XmlGetAttribute(node, "type");
        const char* id = XmlGetAttribute(node, "id");
        const char* digits = XmlGetAttribute(node, "digits");
        if (strcmp(node->name, "numberingSystem") != 0 || type == nullptr ||
            strcmp(type, "numeric") != 0)
        {
            continue;
        }
        if (id == nullptr || digits == nullptr || g->systemCount == maxSystems ||
            g->systemCount == numberAlgorithmic)
        {
            GenDie("a numeric system the table cannot hold", id == nullptr ? "(no id)" : id);
        }
        System* s = &g->systems[g->systemCount++];
        s->id = mlocPackSystem(id, (uint32_t)strlen(id));
        snprintf(s->name, sizeof(s->name), "%s", id);
        const unsigned char* at = (const unsigned char*)digits;
        int length = 0;
        for (int d = 0; d < 10; d++)
        {
            int n = GenCharLength(at);
            if (n == 0 || (d > 0 && n != length))
            {
                GenDie("digits of unequal or malformed UTF-8", id);
            }
            length = n;
            memcpy(s->digits[d], at, (size_t)n);
            at += n;
        }
        if (s->id == 0 || *at != '\0')
        {
            GenDie("a numeric system not of ten digits", id);
        }
    }
    XmlFree(root);
    if (g->systemCount == 0)
    {
        snprintf(errorOut, errorCapacity, "numberingSystems.xml has no numeric system");
        return 0;
    }
    // Sorted by packed id, as the library searches them.
    for (int i = 1; i < g->systemCount; i++)
    {
        for (int j = i; j > 0 && g->systems[j - 1].id > g->systems[j].id; j--)
        {
            System t = g->systems[j];
            g->systems[j] = g->systems[j - 1];
            g->systems[j - 1] = t;
        }
    }
    return 1;
}

static void AddSystemUnit(const Generator* g, DataFile* file)
{
    size_t size = numberSystemsHeaderSize + (size_t)g->systemCount * numberSystemSize;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, (uint64_t)g->systemCount, 4);
    for (int i = 0; i < g->systemCount; i++)
    {
        uint8_t* entry = unit + numberSystemsHeaderSize + (size_t)i * numberSystemSize;
        GenPut(entry, g->systems[i].id, 8);
        memcpy(entry + 8, g->systems[i].digits, sizeof(g->systems[i].digits));
    }
    DataFileAdd(file, kindNumberSystems, nullptr, unit, size);
    free(unit);
}

// A resolved value, or null when no locale has it or it is explicitly
// empty.
static const char* Value(Generator* g, const char* locale, const char* chain)
{
    const Leaf* leaf = CldrResolve(&g->cldr, locale, chain, componentMain);
    return leaf == nullptr || strcmp(leaf->text, s_emptyMarker) == 0 ? nullptr : leaf->text;
}

// The index of the system a chain names, numberNoSystem when none does,
// numberAlgorithmic for one without digits.
static int SystemOf(Generator* g, const char* locale, const char* chain)
{
    const char* name = Value(g, locale, chain);
    if (name == nullptr)
    {
        return numberNoSystem;
    }
    uint64_t id = mlocPackSystem(name, (uint32_t)strlen(name));
    if (id == 0)
    {
        GenDie("not a numbering system id", name);
    }
    for (int i = 0; i < g->systemCount; i++)
    {
        if (g->systems[i].id == id)
        {
            return i;
        }
    }
    return numberAlgorithmic;
}

// An affix of a pattern: its literal text and placeholders, with quotes
// removed.
typedef struct Affix
{
    char bytes[maxAffix];
    size_t length;
} Affix;

static void AffixPut(Affix* a, const char* bytes, size_t length, const char* pattern)
{
    if (a->length + length > sizeof(a->bytes))
    {
        GenDie("a pattern affix too long", pattern);
    }
    memcpy(a->bytes + a->length, bytes, length);
    a->length += length;
}

// Reads an affix from *at up to the number part or the end, as UTS #35
// reads one: quotes make literals, '' is a quote, and the special
// characters become placeholders.
static void ReadAffix(const char** at, Affix* a, bool prefix, const char* pattern)
{
    bool quoted = false;
    while (**at != '\0' && (quoted || **at != ';'))
    {
        const char* c = *at;
        if (*c == '\'')
        {
            if (c[1] == '\'')
            {
                AffixPut(a, "'", 1, pattern);
                *at += 2;
            }
            else
            {
                quoted = !quoted;
                *at += 1;
            }
            continue;
        }
        if (!quoted && prefix && strchr("#0123456789@,.", *c) != nullptr)
        {
            return;
        }
        if (quoted)
        {
            AffixPut(a, c, 1, pattern);
            *at += 1;
            continue;
        }
        char code = 0;
        size_t advance = 1;
        if (*c == '-')
        {
            code = numberAffixMinus;
        }
        else if (*c == '+')
        {
            code = numberAffixPlus;
        }
        else if (*c == '%')
        {
            code = numberAffixPercent;
        }
        else if (strncmp(c, "\xE2\x80\xB0", 3) == 0)
        {
            code = numberAffixPerMille;
            advance = 3;
        }
        else if (strncmp(c, "\xC2\xA4", 2) == 0)
        {
            int run = 0;
            while (strncmp(c + 2 * run, "\xC2\xA4", 2) == 0)
            {
                run++;
            }
            static const char s_codes[6] = {
                0, numberAffixCurrency,      numberAffixCurrencyCode, numberAffixCurrencyName,
                0, numberAffixCurrencyNarrow};
            if (run > 5 || s_codes[run] == 0)
            {
                GenDie("a currency sign run the library has no meaning for", pattern);
            }
            code = s_codes[run];
            advance = 2 * (size_t)run;
        }
        else if ((unsigned char)*c < 0x20)
        {
            GenDie("a control character in a pattern", pattern);
        }
        if (code != 0)
        {
            AffixPut(a, &code, 1, pattern);
        }
        else
        {
            AffixPut(a, c, advance, pattern);
        }
        *at += advance;
    }
    if (quoted)
    {
        GenDie("an unended quote in a pattern", pattern);
    }
}

// Compiles a pattern into the bytes of a pattern record; a pattern
// without digits (a compact one for exactly 1) only where allowed.
static void CompilePattern(Table* strings, const char* pattern, bool allowNoNumber, uint8_t* record)
{
    const char* at = pattern;
    Affix prefix = {0};
    Affix suffix = {0};
    ReadAffix(&at, &prefix, true, pattern);
    int integer = 0;
    int minInteger = 0;
    int minFraction = 0;
    int maxFraction = 0;
    int sinceComma = -1;
    int previousGroup = -1;
    int exponentDigits = 0;
    bool fraction = false;
    bool exponentPlus = false;
    for (; *at != '\0' && strchr("#0,.E", *at) != nullptr; at++)
    {
        char c = *at;
        if (c == 'E')
        {
            at++;
            if (*at == '+')
            {
                exponentPlus = true;
                at++;
            }
            while (*at == '0')
            {
                exponentDigits++;
                at++;
            }
            if (exponentDigits == 0)
            {
                GenDie("an exponent without digits", pattern);
            }
            break;
        }
        if (c == '.')
        {
            if (fraction)
            {
                GenDie("two decimal points in a pattern", pattern);
            }
            fraction = true;
        }
        else if (c == ',')
        {
            if (fraction)
            {
                GenDie("a group separator in a fraction", pattern);
            }
            previousGroup = sinceComma;
            sinceComma = 0;
        }
        else if (fraction)
        {
            maxFraction++;
            minFraction += c == '0';
        }
        else
        {
            integer++;
            minInteger += c == '0';
            if (sinceComma >= 0)
            {
                sinceComma++;
            }
        }
    }
    if (*at != '\0' && *at != ';' && strchr("123456789@", *at) != nullptr)
    {
        GenDie("a pattern with significant digits or an increment", pattern);
    }
    bool noNumber = integer == 0 && !fraction;
    if (noNumber && !allowNoNumber)
    {
        GenDie("a pattern without a number part", pattern);
    }
    ReadAffix(&at, &suffix, false, pattern);
    Affix negativePrefix = {0};
    Affix negativeSuffix = {0};
    if (*at == ';')
    {
        at++;
        ReadAffix(&at, &negativePrefix, true, pattern);
        while (*at != '\0' && strchr("#0123456789@,.E+", *at) != nullptr)
        {
            at++;
        }
        ReadAffix(&at, &negativeSuffix, false, pattern);
    }
    else
    {
        // UTS #35: the minus sign before the positive pattern's prefix.
        char minus = numberAffixMinus;
        AffixPut(&negativePrefix, &minus, 1, pattern);
        AffixPut(&negativePrefix, prefix.bytes, prefix.length, pattern);
        AffixPut(&negativeSuffix, suffix.bytes, suffix.length, pattern);
    }
    if (*at != '\0')
    {
        GenDie("text after a pattern", pattern);
    }
    int primary = sinceComma > 0 ? sinceComma : 0;
    int secondary = previousGroup > 0 ? previousGroup : primary;
    if (primary > numberMaxGroup || secondary > numberMaxGroup || exponentDigits > numberMaxGroup ||
        minInteger > numberMaxPatternDigits || maxFraction > numberMaxPatternDigits)
    {
        GenDie("a pattern past the record's limits", pattern);
    }
    memset(record, 0, numberPatternSize);
    GenPut(record + patternPositivePrefix, TableAddString(strings, prefix.bytes, prefix.length), 2);
    GenPut(record + patternPositiveSuffix, TableAddString(strings, suffix.bytes, suffix.length), 2);
    GenPut(record + patternNegativePrefix,
           TableAddString(strings, negativePrefix.bytes, negativePrefix.length), 2);
    GenPut(record + patternNegativeSuffix,
           TableAddString(strings, negativeSuffix.bytes, negativeSuffix.length), 2);
    record[patternMinInteger] = (uint8_t)minInteger;
    record[patternMinFraction] = (uint8_t)minFraction;
    record[patternMaxFraction] = (uint8_t)maxFraction;
    record[patternPrimary] = (uint8_t)primary;
    record[patternSecondary] = (uint8_t)secondary;
    record[patternExponentDigits] = (uint8_t)exponentDigits;
    record[patternFlags] = (uint8_t)((exponentPlus ? patternFlagExponentPlus : 0) |
                                     (noNumber ? patternFlagNoNumber : 0));
}

static uint16_t PatternOf(Generator* g, const char* locale, const char* system, const char* kind)
{
    char chain[256];
    snprintf(chain, sizeof(chain),
             "//ldml/numbers/%sFormats[@numberSystem=\"%s\"]/%sFormatLength/%sFormat/pattern", kind,
             system, kind, kind);
    const char* pattern = Value(g, locale, chain);
    if (pattern == nullptr)
    {
        GenDie("a locale without a pattern", chain);
    }
    uint8_t record[numberPatternSize];
    CompilePattern(&g->patterns, pattern, false, record);
    return TableAddRecord(&g->patterns, record);
}

// Where a compiled pattern's positive affixes hold a currency
// placeholder: 0 nowhere, 1 in the prefix, 2 in the suffix, 3 in both.
static int CurrencySide(const Table* t, const uint8_t* record)
{
    int side = 0;
    for (int i = 0; i < 2; i++)
    {
        uint16_t index = (uint16_t)(record[2 * i] | record[2 * i + 1] << 8);
        for (size_t c = 0; c < t->lengths[index]; c++)
        {
            uint8_t b = (uint8_t)t->texts[index][c];
            side |= b >= numberAffixCurrency && b <= numberAffixCurrencyNarrow ? 1 << i : 0;
        }
    }
    return side;
}

// A pattern with a no-break space between its currency and the digits
// where they meet, as root's currencySpacing inserts one: the
// alphaNextToNumber pattern a locale's data does not give in place.
static uint16_t SpacedPattern(Generator* g, const uint8_t* pattern)
{
    uint8_t record[numberPatternSize];
    memcpy(record, pattern, sizeof(record));
    for (int i = 0; i < 4; i++)
    {
        bool prefix = i % 2 == 0;
        uint16_t index = (uint16_t)(record[2 * i] | record[2 * i + 1] << 8);
        const char* text = g->patterns.texts[index];
        size_t length = g->patterns.lengths[index];
        uint8_t edge = length == 0 ? 0 : (uint8_t)text[prefix ? length - 1 : 0];
        if (edge < numberAffixCurrency || edge > numberAffixCurrencyNarrow)
        {
            continue;
        }
        char spaced[maxAffix + 2];
        if (length + 2 > sizeof(spaced))
        {
            GenDie("a currency affix too long", "spacing");
        }
        memcpy(spaced + (prefix ? 0 : 2), text, length);
        memcpy(spaced + (prefix ? length : 0), "\xC2\xA0", 2);
        GenPut(record + 2 * i, TableAddString(&g->patterns, spaced, length + 2), 2);
    }
    return TableAddRecord(&g->patterns, record);
}

// The length of the space (U+0020, U+00A0, U+202F) starting text, with
// length bytes, or 0.
static size_t SpaceAt(const char* text, size_t length)
{
    if (length >= 1 && text[0] == ' ')
    {
        return 1;
    }
    if (length >= 2 && memcmp(text, "\xC2\xA0", 2) == 0)
    {
        return 2;
    }
    return length >= 3 && memcmp(text, "\xE2\x80\xAF", 3) == 0 ? 3 : 0;
}

// The length of the space ending text, or 0.
static size_t SpaceBefore(const char* text, size_t length)
{
    for (size_t n = 1; n <= 3 && n <= length; n++)
    {
        if (SpaceAt(text + length - n, n) == n)
        {
            return n;
        }
    }
    return 0;
}

// A pattern without its currency and the spaces beside it: the
// noCurrency pattern a locale's data does not give in place.
static uint16_t HiddenPattern(Generator* g, const uint8_t* pattern)
{
    uint8_t record[numberPatternSize];
    memcpy(record, pattern, sizeof(record));
    for (int i = 0; i < 4; i++)
    {
        uint16_t index = (uint16_t)(record[2 * i] | record[2 * i + 1] << 8);
        const char* text = g->patterns.texts[index];
        size_t length = g->patterns.lengths[index];
        char kept[maxAffix];
        size_t n = 0;
        for (size_t at = 0; at < length;)
        {
            uint8_t b = (uint8_t)text[at];
            if (b < numberAffixCurrency || b > numberAffixCurrencyNarrow)
            {
                kept[n++] = text[at++];
                continue;
            }
            for (size_t space = SpaceBefore(kept, n); space != 0; space = SpaceBefore(kept, n))
            {
                n -= space;
            }
            at++;
            for (size_t space = SpaceAt(text + at, length - at); space != 0;
                 space = SpaceAt(text + at, length - at))
            {
                at += space;
            }
        }
        GenPut(record + 2 * i, TableAddString(&g->patterns, kept, n), 2);
    }
    return TableAddRecord(&g->patterns, record);
}

// An affix without its spaces, in out.
static size_t Unspaced(const Table* t, const uint8_t* field, char* out)
{
    uint16_t index = (uint16_t)(field[0] | field[1] << 8);
    const char* text = t->texts[index];
    size_t length = t->lengths[index];
    size_t n = 0;
    for (size_t i = 0; i < length;)
    {
        size_t space = SpaceAt(text + i, length - i);
        if (space == 0)
        {
            out[n++] = text[i];
        }
        i += space == 0 ? 1 : space;
    }
    return n;
}

// Whether two compiled patterns differ in their spaces alone.
static bool SameButSpaces(const Table* t, const uint8_t* a, const uint8_t* b)
{
    if (memcmp(a + patternMinInteger, b + patternMinInteger,
               numberPatternSize - patternMinInteger) != 0)
    {
        return false;
    }
    for (int i = 0; i < 4; i++)
    {
        char x[maxAffix];
        char y[maxAffix];
        size_t n = Unspaced(t, a + 2 * i, x);
        if (Unspaced(t, b + 2 * i, y) != n || memcmp(x, y, n) != 0)
        {
            return false;
        }
    }
    return true;
}

// A long name's unit pattern ("{0} {1}"), or the ISO append pattern
// ("{0} ¤¤"), as the sets unit stores it, from latn's formats whatever the
// system (root gives other systems none; ICU reads latn's too): {0} as currencyTextNumber and
// the name's {1}, or the code's ¤¤, as currencyTextOther, once each.
static uint16_t CurrencyTextPattern(Generator* g, const char* locale, const char* chain,
                                    const char* other)
{
    const char* pattern = Value(g, locale, chain);
    if (pattern == nullptr)
    {
        GenDie("a locale without a currency unit or append pattern", chain);
    }
    char out[maxAffix];
    size_t n = 0;
    int numbers = 0;
    int others = 0;
    size_t otherLength = strlen(other);
    for (const char* c = pattern; *c != '\0';)
    {
        if (n + 1 >= sizeof(out) || (unsigned char)*c < 0x20)
        {
            GenDie("a currency unit pattern too long or with a control character", pattern);
        }
        if (strncmp(c, "{0}", 3) == 0)
        {
            out[n++] = currencyTextNumber;
            numbers++;
            c += 3;
        }
        else if (strncmp(c, other, otherLength) == 0)
        {
            out[n++] = currencyTextOther;
            others++;
            c += otherLength;
        }
        else
        {
            out[n++] = *c++;
        }
    }
    if (numbers != 1 || others != 1)
    {
        GenDie("a currency unit pattern without its two places once each", pattern);
    }
    return TableAddString(&g->currencySets, out, n);
}

// A locale's currency pattern set for a system, with its short compact
// currency tables: the standard and
// accounting patterns, each with its alphaNextToNumber variant and its
// noCurrency variant, which has no currency. A variant that differs from
// its pattern in more than spaces, or the noCurrency one in its digits,
// was written for another pattern (Aghem's own alphaNextToNumber one is
// provisional, so root's stands; Indian English inherits English's
// grouping), so the pattern spaced as root's currencySpacing says, or
// without its currency, stands instead.
static uint16_t CurrencySetOf(Generator* g, const char* locale, const char* system,
                              uint16_t compact, uint16_t compactAlpha)
{
    static const char* const s_types[2] = {"standard", "accounting"};
    static const char* const s_alts[3] = {"", "[@alt=\"alphaNextToNumber\"]",
                                          "[@alt=\"noCurrency\"]"};
    uint8_t set[currencySetSize];
    for (int type = 0; type < 2; type++)
    {
        uint8_t standard[numberPatternSize] = {0};
        for (int alt = 0; alt < 3; alt++)
        {
            char chain[320];
            snprintf(chain, sizeof(chain),
                     "//ldml/numbers/currencyFormats[@numberSystem=\"%s\"]/currencyFormatLength/"
                     "currencyFormat[@type=\"%s\"]/pattern%s",
                     system, s_types[type], s_alts[alt]);
            const char* pattern = Value(g, locale, chain);
            if (pattern == nullptr)
            {
                GenDie("a locale without a currency pattern", chain);
            }
            uint8_t record[numberPatternSize];
            CompilePattern(&g->patterns, pattern, false, record);
            int at = CurrencySide(&g->patterns, record);
            uint16_t index = TableAddRecord(&g->patterns, record);
            if (alt == 0)
            {
                memcpy(standard, record, sizeof(standard));
            }
            if ((alt == 0 && (at == 0 || at == 3)) || (alt == 2 && at != 0))
            {
                GenDie("a currency pattern with its currency out of place", chain);
            }
            if (alt == 1 && !SameButSpaces(&g->patterns, record, standard))
            {
                index = SpacedPattern(g, standard);
            }
            if (alt == 2 && memcmp(record + patternMinInteger, standard + patternMinInteger,
                                   numberPatternSize - patternMinInteger) != 0)
            {
                index = HiddenPattern(g, standard);
            }
            GenPut(set + 2 * (3 * type + alt), index, 2);
        }
    }
    GenPut(set + setCompact, compact, 2);
    GenPut(set + setCompactAlpha, compactAlpha, 2);
    static const char* const s_counts[6] = {"zero", "one", "two", "few", "many", "other"};
    for (int c = 0; c < 6; c++)
    {
        char chain[256];
        snprintf(
            chain, sizeof(chain),
            "//ldml/numbers/currencyFormats[@numberSystem=\"latn\"]/unitPattern[@count=\"%s\"]",
            s_counts[c]);
        GenPut(set + setUnitPatterns + 2 * c, CurrencyTextPattern(g, locale, chain, "{1}"), 2);
    }
    char chain[256];
    snprintf(chain, sizeof(chain),
             "//ldml/numbers/currencyFormats[@numberSystem=\"latn\"]/currencyPatternAppendISO");
    GenPut(set + setAppend, CurrencyTextPattern(g, locale, chain, "\xC2\xA4\xC2\xA4"), 2);
    return TableAddRecord(&g->currencySets, set);
}

// Compiles a currency's own pattern (the generator's context) into the
// patterns unit; its index.
static uint16_t CompileCurrencyPattern(void* context, const char* pattern)
{
    Generator* g = context;
    uint8_t record[numberPatternSize];
    CompilePattern(&g->patterns, pattern, false, record);
    int side = CurrencySide(&g->patterns, record);
    if (side == 0 || side == 3)
    {
        GenDie("a currency's pattern without one currency place", pattern);
    }
    return TableAddRecord(&g->patterns, record);
}

// The text between {0} and {1} in a locale's range pattern for a system,
// in a buffer of its own; every range pattern of CLDR 49 is the two ends
// around it.
static const char* RangeSeparator(Generator* g, const char* locale, const char* system)
{
    static char s_separator[64];
    char chain[256];
    snprintf(chain, sizeof(chain),
             "//ldml/numbers/miscPatterns[@numberSystem=\"%s\"]/pattern[@type=\"range\"]", system);
    const char* pattern = Value(g, locale, chain);
    size_t length = pattern == nullptr ? 0 : strlen(pattern);
    if (length < 6 || strncmp(pattern, "{0}", 3) != 0 || strcmp(pattern + length - 3, "{1}") != 0 ||
        length - 6 >= sizeof(s_separator))
    {
        GenDie("a range pattern that is not {0}, a separator, {1}", chain);
    }
    memcpy(s_separator, pattern + 3, length - 6);
    s_separator[length - 6] = '\0';
    return s_separator;
}

static uint16_t SymbolsOf(Generator* g, const char* locale, const char* system)
{
    uint8_t record[2 * numberSymbolCount];
    const char* values[numberSymbolCount];
    for (int s = 0; s < numberSymbolCount; s++)
    {
        char chain[256];
        snprintf(chain, sizeof(chain), "//ldml/numbers/symbols[@numberSystem=\"%s\"]/%s", system,
                 s_symbolNames[s]);
        values[s] =
            s == numberRangeSeparator ? RangeSeparator(g, locale, system) : Value(g, locale, chain);
        if (values[s] == nullptr)
        {
            // A currency separator CLDR does not give is the plain one.
            if (s == numberCurrencyDecimal || s == numberCurrencyGroup)
            {
                values[s] = values[s == numberCurrencyDecimal ? numberDecimal : numberGroup];
            }
            else
            {
                GenDie("a locale without a number symbol", chain);
            }
        }
        for (const unsigned char* c = (const unsigned char*)values[s]; *c != '\0';)
        {
            int n = GenCharLength(c);
            if (n == 0 || *c < 0x20)
            {
                GenDie("a number symbol with a control or malformed character", chain);
            }
            c += n;
        }
        GenPut(record + 2 * s, TableAddString(&g->symbols, values[s], strlen(values[s])), 2);
    }
    return TableAddRecord(&g->symbols, record);
}

static void MarkOwnFormats(const char* key, void* value, void* context)
{
    (void)value;
    static const char s_decimal[] = "//ldml/numbers/decimalFormats[@numberSystem=\"";
    static const char s_currency[] = "//ldml/numbers/currencyFormats[@numberSystem=\"";
    const char* name = nullptr;
    if (strncmp(key, s_decimal, sizeof(s_decimal) - 1) == 0)
    {
        name = key + sizeof(s_decimal) - 1;
    }
    else if (strncmp(key, s_currency, sizeof(s_currency) - 1) == 0)
    {
        name = key + sizeof(s_currency) - 1;
    }
    else
    {
        return;
    }
    uint64_t id = mlocPackSystem(name, (uint32_t)strcspn(name, "\""));
    const Generator* g = ((void**)context)[0];
    bool* own = ((void**)context)[1];
    for (int i = 0; i < g->systemCount; i++)
    {
        own[i] |= g->systems[i].id == id;
    }
}

// The systems a file gives decimal formats of its own, as a bool per
// system; none for a locale without a file.
static const bool* OwnFormats(Generator* g, const char* locale)
{
    bool* own = MapGet(&g->ownFormats, locale);
    if (own != nullptr)
    {
        return own;
    }
    own = GenAllocate(sizeof(bool) * maxSystems);
    const LdmlFile* file = CldrFile(&g->cldr, locale, componentMain);
    if (file != nullptr)
    {
        void* context[2] = {g, own};
        MapEach(&file->leaves, MarkOwnFormats, context);
    }
    MapPut(&g->ownFormats, locale, own);
    return own;
}

// Whether any file along a locale's chain, root's included, gives a
// system decimal formats of its own; when none does, root's alias makes
// them the locale's latn formats.
static bool HasOwnFormats(Generator* g, const char* locale, int system)
{
    for (const char* at = locale; at != nullptr; at = CldrParent(&g->cldr, at, componentMain))
    {
        if (OwnFormats(g, at)[system])
        {
            return true;
        }
    }
    return OwnFormats(g, "root")[system];
}

// A value CLDR's files give a chain along a locale's chain by plain
// inheritance, or null: no alias and no lateral fallback, which would
// give a long pattern's missing category the short pattern's, or the
// other category's.
static const char* PlainValue(Generator* g, const char* locale, const char* chain)
{
    static const char s_inherit[] = "\xE2\x86\x91\xE2\x86\x91\xE2\x86\x91";
    for (const char* at = locale; at != nullptr; at = CldrParent(&g->cldr, at, componentMain))
    {
        const LdmlFile* file = CldrFile(&g->cldr, at, componentMain);
        const Leaf* leaf = file == nullptr ? nullptr : MapGet(&file->leaves, chain);
        if (leaf != nullptr && leaf->draft <= g->cldr.threshold &&
            strcmp(leaf->text, s_inherit) != 0)
        {
            return strcmp(leaf->text, s_emptyMarker) == 0 ? nullptr : leaf->text;
        }
    }
    return nullptr;
}

// Whose compact patterns a table holds: the decimal formats', the
// currency formats', or the currency formats' alphaNextToNumber ones.
typedef enum CompactKind
{
    compactDecimal,
    compactCurrency,
    compactCurrencyAlpha,
} CompactKind;

// A compact pattern of a kind by plain inheritance, the system's else
// latn's, or null.
static const char* KindPattern(Generator* g, const char* locale, const char* system,
                               const char* length, const char* count, const char* type,
                               CompactKind kind)
{
    const char* pattern = nullptr;
    for (int pass = 0; pass < 2 && pattern == nullptr; pass++)
    {
        const char* name = pass == 0 ? system : "latn";
        char chain[384];
        if (kind == compactDecimal)
        {
            snprintf(chain, sizeof(chain),
                     "//ldml/numbers/decimalFormats[@numberSystem=\"%s\"]/decimalFormatLength"
                     "[@type=\"%s\"]/decimalFormat/pattern[@count=\"%s\"][@type=\"%s\"]",
                     name, length, count, type);
        }
        else
        {
            snprintf(chain, sizeof(chain),
                     "//ldml/numbers/currencyFormats[@numberSystem=\"%s\"]/currencyFormatLength"
                     "[@type=\"%s\"]/currencyFormat[@type=\"standard\"]/pattern%s[@count=\"%s\"]"
                     "[@type=\"%s\"]",
                     name, length,
                     kind == compactCurrencyAlpha ? "[@alt=\"alphaNextToNumber\"]" : "", count,
                     type);
        }
        pattern = PlainValue(g, locale, chain);
    }
    return pattern;
}

// Whether two pattern strings differ in their spaces alone.
static bool SameTextButSpaces(const char* a, const char* b)
{
    size_t i = 0;
    size_t j = 0;
    size_t m = strlen(a);
    size_t n = strlen(b);
    for (;;)
    {
        for (size_t space = SpaceAt(a + i, m - i); space != 0; space = SpaceAt(a + i, m - i))
        {
            i += space;
        }
        for (size_t space = SpaceAt(b + j, n - j); space != 0; space = SpaceAt(b + j, n - j))
        {
            j += space;
        }
        if (i == m || j == n)
        {
            return i == m && j == n;
        }
        if (a[i++] != b[j++])
        {
            return false;
        }
    }
}

// A compact currency pattern with a no-break space where its currency
// and its zeros meet, in a buffer of its own.
static const char* SpacedText(const char* pattern)
{
    static char s_spaced[maxAffix * 4];
    size_t n = 0;
    for (const char* c = pattern; *c != '\0'; c++)
    {
        bool sign = strncmp(c, "\xC2\xA4", 2) == 0;
        bool signThenZero = sign && c[2] == '0';
        bool zeroThenSign = *c == '0' && strncmp(c + 1, "\xC2\xA4", 2) == 0;
        if (n + 5 >= sizeof(s_spaced))
        {
            GenDie("a compact currency pattern too long", pattern);
        }
        s_spaced[n++] = *c;
        if (sign)
        {
            s_spaced[n++] = *++c;
        }
        if (signThenZero || zeroThenSign)
        {
            memcpy(s_spaced + n, "\xC2\xA0", 2);
            n += 2;
        }
    }
    s_spaced[n] = '\0';
    return s_spaced;
}

// Whether a compact pattern's currency stands right beside its zeros.
static bool CurrencyMeetsZeros(const char* pattern)
{
    const char* sign = strstr(pattern, "\xC2\xA4");
    while (sign != nullptr)
    {
        size_t run = 0;
        while (strncmp(sign + 2 * run, "\xC2\xA4", 2) == 0)
        {
            run++;
        }
        if ((sign > pattern && sign[-1] == '0') || sign[2 * run] == '0')
        {
            return true;
        }
        sign = strstr(sign + 2 * run, "\xC2\xA4");
    }
    return false;
}

// A compact pattern of a kind. UTS #35 switches to the alphaNextToNumber
// pattern only where the currency meets the zeros (Bengali's "0 লা¤"
// keeps its pattern); one that differs from its plain one in more than
// spaces was written for another pattern (as CurrencySetOf says), so the
// plain one spaced stands instead.
static const char* CompactPattern(Generator* g, const char* locale, const char* system,
                                  const char* length, const char* count, const char* type,
                                  CompactKind kind)
{
    const char* plain = KindPattern(g, locale, system, length, count, type,
                                    kind == compactDecimal ? compactDecimal : compactCurrency);
    if (kind != compactCurrencyAlpha || plain == nullptr || !CurrencyMeetsZeros(plain))
    {
        return plain;
    }
    const char* alpha = KindPattern(g, locale, system, length, count, type, kind);
    return alpha != nullptr && SameTextButSpaces(alpha, plain) ? alpha : SpacedText(plain);
}

// A compact row's slot: a pattern index, numberCompactAbsent, or for the
// pattern "0" numberCompactNone. A system without formats of its own
// takes latn's, as root's alias says.
static uint16_t CompactSlot(Generator* g, const char* locale, const char* system,
                            const char* length, const char* count, int power, CompactKind kind,
                            int* zerosOut)
{
    *zerosOut = 0;
    char type[32] = "1";
    for (int i = 0; i < power; i++)
    {
        strcat(type, "0");
    }
    const char* pattern = CompactPattern(g, locale, system, length, count, type, kind);
    if (pattern == nullptr)
    {
        return numberCompactAbsent;
    }
    if (strcmp(pattern, "0") == 0)
    {
        return numberCompactNone;
    }
    // A category's pattern may name the number in words alone (Italian's
    // one is "mille"); the other one's digits give the divisor.
    uint8_t full[numberPatternSize];
    CompilePattern(&g->compactPatterns, pattern, strcmp(count, "other") != 0, full);
    if (full[patternPrimary] != 0 || full[patternMaxFraction] != 0 ||
        full[patternExponentDigits] != 0 ||
        memcmp(full + patternNegativeSuffix, full + patternPositiveSuffix, 2) != 0 ||
        full[patternMinInteger] > numberCompactFirstPower + numberCompactRows)
    {
        GenDie("a compact pattern with more than zeros and affixes", pattern);
    }
    uint8_t record[numberCompactPatternSize] = {0};
    memcpy(record + compactPrefix, full + patternPositivePrefix, 2);
    memcpy(record + compactSuffix, full + patternPositiveSuffix, 2);
    memcpy(record + compactNegativePrefix, full + patternNegativePrefix, 2);
    record[compactFlags] = full[patternFlags];
    *zerosOut = full[patternMinInteger];
    return TableAddRecord(&g->compactPatterns, record);
}

// A locale's compact table of one length for a system; a power the long
// length lacks takes the short one's row, as UTS #35 says.
static uint16_t CompactOf(Generator* g, const char* locale, const char* system, bool longLength,
                          CompactKind kind)
{
    static const char* const s_counts[numberCompactSlots] = {"zero", "one",   "two", "few",
                                                             "many", "other", "1"};
    uint8_t table[numberCompactRows * (2 + 2 * numberCompactSlots)];
    size_t size = 0;
    for (int r = 0; r < numberCompactRows; r++)
    {
        int power = numberCompactFirstPower + r;
        const char* length = longLength ? "long" : "short";
        int zeros = 0;
        uint16_t other = CompactSlot(g, locale, system, length, "other", power, kind, &zeros);
        if (other == numberCompactAbsent && longLength)
        {
            length = "short";
            other = CompactSlot(g, locale, system, length, "other", power, kind, &zeros);
        }
        if (other == numberCompactAbsent || other == numberCompactNone)
        {
            table[size++] = 0;
            table[size++] = 0;
            continue;
        }
        if (zeros < 1 || zeros > power + 1)
        {
            GenDie("a compact pattern whose zeros give no divisor", locale);
        }
        uint16_t slots[numberCompactSlots];
        uint8_t mask = 0;
        for (int c = 0; c < numberCompactSlots; c++)
        {
            int slotZeros = 0;
            slots[c] = c == numberCompactOther ? other
                                               : CompactSlot(g, locale, system, length, s_counts[c],
                                                             power, kind, &slotZeros);
            // A category's "0" where the other one compacts is no pattern
            // of its own, and one equal to the other one need not be named
            // (CLDR's resolution gives a missing category the other one's).
            if (c != numberCompactOther && (slots[c] == numberCompactNone || slots[c] == other))
            {
                slots[c] = numberCompactAbsent;
            }
            if (slots[c] != numberCompactAbsent && slotZeros != 0 && slotZeros != zeros)
            {
                GenDie("a compact category whose zeros differ from the other one's", locale);
            }
            mask |= slots[c] != numberCompactAbsent ? (uint8_t)(1u << c) : 0;
        }
        table[size++] = (uint8_t)zeros;
        table[size++] = mask;
        for (int c = 0; c < numberCompactSlots; c++)
        {
            if (slots[c] != numberCompactAbsent)
            {
                GenPut(table + size, slots[c], 2);
                size += 2;
            }
        }
    }
    return AddBlob(g, table, size);
}

// Resolves a locale's numbers.
static void Resolve(Generator* g, const char* locale, Numbers* out)
{
    memset(out, 0, sizeof(*out));
    out->defaultSystem = SystemOf(g, locale, "//ldml/numbers/defaultNumberingSystem");
    out->native = SystemOf(g, locale, "//ldml/numbers/otherNumberingSystems/native");
    out->traditional = SystemOf(g, locale, "//ldml/numbers/otherNumberingSystems/traditional");
    out->finance = SystemOf(g, locale, "//ldml/numbers/otherNumberingSystems/finance");
    const char* grouping = Value(g, locale, "//ldml/numbers/minimumGroupingDigits");
    out->minGrouping = grouping == nullptr ? 0 : atoi(grouping);
    if (out->defaultSystem >= numberAlgorithmic || out->minGrouping < 1 || out->minGrouping > 4)
    {
        GenDie("a locale without a numeric default system or with odd grouping", locale);
    }
    if (out->native == numberNoSystem)
    {
        out->native = out->defaultSystem;
    }
    Entry latn = {.system = numberNoSystem};
    for (int i = 0; i < g->systemCount; i++)
    {
        const System* s = &g->systems[i];
        Entry e = {
            .system = (uint16_t)i,
            .symbols = SymbolsOf(g, locale, s->name),
            .decimal = PatternOf(g, locale, s->name, "decimal"),
            .percent = PatternOf(g, locale, s->name, "percent"),
        };
        if (s->id == numberLatn)
        {
            latn = e;
        }
        out->entries[out->entryCount++] = e;
    }
    // Compact tables, resolved only for latn and the systems with formats
    // of their own along the chain; the others are latn's.
    for (int i = 0; i < out->entryCount; i++)
    {
        Entry* e = &out->entries[i];
        if (e->system == latn.system || HasOwnFormats(g, locale, e->system))
        {
            const char* name = g->systems[e->system].name;
            e->compactShort = CompactOf(g, locale, name, false, compactDecimal);
            e->compactLong = CompactOf(g, locale, name, true, compactDecimal);
            e->currencyCompact = CompactOf(g, locale, name, false, compactCurrency);
            e->currencyAlpha = CompactOf(g, locale, name, false, compactCurrencyAlpha);
        }
        if (e->system == latn.system)
        {
            latn.compactShort = e->compactShort;
            latn.compactLong = e->compactLong;
            latn.currencyCompact = e->currencyCompact;
            latn.currencyAlpha = e->currencyAlpha;
        }
    }
    for (int i = 0; i < out->entryCount; i++)
    {
        Entry* e = &out->entries[i];
        if (e->system != latn.system && !HasOwnFormats(g, locale, e->system))
        {
            e->compactShort = latn.compactShort;
            e->compactLong = latn.compactLong;
            e->currencyCompact = latn.currencyCompact;
            e->currencyAlpha = latn.currencyAlpha;
        }
        e->currency = CurrencySetOf(g, locale, g->systems[e->system].name, e->currencyCompact,
                                    e->currencyAlpha);
        if (e->system == latn.system)
        {
            latn.currency = e->currency;
        }
    }
    if (latn.system == numberNoSystem)
    {
        GenDie("numberingSystems.xml has no latn", locale);
    }
    // Only the systems whose data differs from latn's, and latn.
    int kept = 0;
    for (int i = 0; i < out->entryCount; i++)
    {
        const Entry* e = &out->entries[i];
        if (g->systems[e->system].id == numberLatn || e->symbols != latn.symbols ||
            e->decimal != latn.decimal || e->percent != latn.percent ||
            e->compactShort != latn.compactShort || e->compactLong != latn.compactLong ||
            e->currency != latn.currency)
        {
            out->entries[kept++] = *e;
        }
    }
    memset(out->entries + kept, 0, sizeof(Entry) * (size_t)(out->entryCount - kept));
    out->entryCount = kept;
}

static void AddLocaleUnit(const Numbers* n, const char* key, DataFile* file)
{
    size_t size = numberLocaleHeaderSize + (size_t)n->entryCount * numberEntrySize;
    uint8_t* unit = GenAllocate(size);
    unit[localeDefaultSystem] = (uint8_t)n->defaultSystem;
    unit[localeNativeSystem] = (uint8_t)n->native;
    unit[localeTraditionalSystem] = (uint8_t)n->traditional;
    unit[localeFinanceSystem] = (uint8_t)n->finance;
    unit[localeMinGrouping] = (uint8_t)n->minGrouping;
    unit[localeEntryCount] = (uint8_t)n->entryCount;
    for (int i = 0; i < n->entryCount; i++)
    {
        uint8_t* entry = unit + numberLocaleHeaderSize + (size_t)i * numberEntrySize;
        entry[entrySystem] = (uint8_t)n->entries[i].system;
        GenPut(entry + entrySymbols, n->entries[i].symbols, 2);
        GenPut(entry + entryDecimal, n->entries[i].decimal, 2);
        GenPut(entry + entryPercent, n->entries[i].percent, 2);
        GenPut(entry + entryCompactShort, n->entries[i].compactShort, 2);
        GenPut(entry + entryCompactLong, n->entries[i].compactLong, 2);
        GenPut(entry + entryCurrency, n->entries[i].currency, 2);
    }
    DataFileAdd(file, kindNumbers, key, unit, size);
    free(unit);
}

static void FreeValue(const char* key, void* value, void* context)
{
    (void)key;
    (void)context;
    free(value);
}

static int CompareNames(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

// The ids of common/main's files, sorted; the caller frees each and the
// list.
static char** ListLocales(const char* commonDirectory, int* countOut)
{
    char** ids = GenAllocate(sizeof(char*) * maxLocales);
    int count = 0;
    char path[4096];
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s/main/*.xml", commonDirectory);
    struct _finddata_t found;
    intptr_t handle = _findfirst(path, &found);
    for (int more = handle != -1; more; more = _findnext(handle, &found) == 0)
    {
        const char* name = found.name;
#else
    snprintf(path, sizeof(path), "%s/main", commonDirectory);
    DIR* directory = opendir(path);
    if (directory == nullptr)
    {
        GenDie("cannot list", path);
    }
    for (struct dirent* item = readdir(directory); item != nullptr; item = readdir(directory))
    {
        const char* name = item->d_name;
#endif
        size_t length = strlen(name);
        if (length < 5 || strcmp(name + length - 4, ".xml") != 0)
        {
            continue;
        }
        if (count == maxLocales)
        {
            GenDie("too many locales in", path);
        }
        ids[count] = GenAllocate(length - 3);
        memcpy(ids[count], name, length - 4);
        count++;
    }
#ifdef _WIN32
    if (handle != -1)
    {
        _findclose(handle);
    }
#else
    closedir(directory);
#endif
    qsort(ids, (size_t)count, sizeof(char*), CompareNames);
    *countOut = count;
    return ids;
}

// The nearest locale along a locale's chain whose key differs from its
// own, or null for root.
static const char* NextKeyed(Generator* g, const char* locale, const char* key)
{
    for (const char* at = CldrParent(&g->cldr, locale, componentMain); at != nullptr;
         at = CldrParent(&g->cldr, at, componentMain))
    {
        char other[maxKey];
        KeyOf(&g->keys, at, other, sizeof(other));
        if (strcmp(other, key) != 0)
        {
            return at;
        }
    }
    return nullptr;
}

int GenerateNumberUnits(const char* commonDirectory, DataFile* file, NameFiles* names,
                        char* errorOut, size_t errorCapacity)
{
    static Generator s_g;
    memset(&s_g, 0, sizeof(s_g));
    Generator* g = &s_g;
    g->symbols = (Table){.recordSize = 2 * numberSymbolCount, .distinctRecords = true};
    g->patterns = (Table){.recordSize = numberPatternSize, .distinctRecords = true};
    g->compactPatterns = (Table){.recordSize = numberCompactPatternSize, .distinctRecords = true};
    g->currencySets = (Table){.recordSize = currencySetSize, .distinctRecords = true};
    if (!ReadSystems(g, commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
    if (!KeysOpen(&g->keys, commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
    if (!CldrOpen(&g->cldr, commonDirectory, errorOut, errorCapacity))
    {
        KeysClose(&g->keys);
        return 0;
    }
    int count = 0;
    char** ids = ListLocales(commonDirectory, &count);
    // Each locale's resolved numbers, by id; root's always filed.
    Map resolved = {0};
    Numbers* all = GenAllocate(sizeof(Numbers) * (size_t)count);
    for (int i = 0; i < count; i++)
    {
        Resolve(g, ids[i], &all[i]);
        MapPut(&resolved, ids[i], &all[i]);
    }
    for (int i = 0; i < count; i++)
    {
        char key[maxKey];
        KeyOf(&g->keys, ids[i], key, sizeof(key));
        const char* next = NextKeyed(g, ids[i], key);
        const char* parent = CldrParent(&g->cldr, ids[i], componentMain);
        char parentKey[maxKey] = "";
        if (parent != nullptr)
        {
            KeyOf(&g->keys, parent, parentKey, sizeof(parentKey));
        }
        if (parent != nullptr && strcmp(parentKey, key) == 0)
        {
            // A file under its parent's key holds the parent's data.
            const Numbers* other = MapGet(&resolved, parent);
            if (other == nullptr || memcmp(other, &all[i], sizeof(Numbers)) != 0)
            {
                GenDie("a locale's numbers differ from those of its parent's key", ids[i]);
            }
            continue;
        }
        const Numbers* base = next == nullptr ? nullptr : MapGet(&resolved, next);
        if (next != nullptr && base == nullptr)
        {
            GenDie("a parent locale without a file", next);
        }
        if (base == nullptr || memcmp(base, &all[i], sizeof(Numbers)) != 0)
        {
            AddLocaleUnit(&all[i], key, file);
        }
    }
    CurrencyInput currency = {
        .commonDirectory = commonDirectory,
        .cldr = &g->cldr,
        .keys = &g->keys,
        .ids = (const char* const*)ids,
        .count = count,
        .compile = CompileCurrencyPattern,
        .context = g,
        .names = &names->currencies,
    };
    UnitNamesInput units = {
        .commonDirectory = commonDirectory,
        .cldr = &g->cldr,
        .keys = &g->keys,
        .ids = (const char* const*)ids,
        .count = count,
    };
    NamesInput localeNamesInput = {
        .commonDirectory = commonDirectory,
        .cldr = &g->cldr,
        .keys = &g->keys,
        .ids = (const char* const*)ids,
        .count = count,
    };
    ListInput lists = {
        .cldr = &g->cldr,
        .keys = &g->keys,
        .ids = (const char* const*)ids,
        .count = count,
    };
    GenerateListUnits(&lists, file);
    GenerateContextUnits(&lists, file);
    DateInput dates = {
        .commonDirectory = commonDirectory,
        .cldr = &g->cldr,
        .keys = &g->keys,
        .ids = (const char* const*)ids,
        .count = count,
    };
    int ok = GenerateCurrencyUnits(&currency, file, errorOut, errorCapacity) &&
             GenerateDateUnits(&dates, &names->dates, errorOut, errorCapacity) &&
             GenerateUnitNames(&units, &names->units, errorOut, errorCapacity) &&
             GenerateNames(&localeNamesInput, &names->locales, &names->subdivisions, errorOut,
                           errorCapacity) &&
             GenerateZoneNames(&localeNamesInput, &names->zones, errorOut, errorCapacity);
    if (ok)
    {
        GenerateUnitPatterns(&units, &names->units);
    }
    GenerateParseSets(&g->cldr, file);
    AddSystemUnit(g, file);
    TableAddUnit(&g->currencySets, kindCurrencyPatterns, nullptr, file);
    TableAddUnit(&g->symbols, kindNumberSymbols, nullptr, file);
    TableAddUnit(&g->patterns, kindNumberPatterns, nullptr, file);
    TableAddUnit(&g->compactPatterns, kindNumberCompactPatterns, nullptr, file);
    AddCompactUnit(g, file);
    for (int i = 0; i < count; i++)
    {
        free(ids[i]);
    }
    free(ids);
    free(all);
    MapFree(&resolved);
    TableClear(&g->symbols);
    TableClear(&g->patterns);
    TableClear(&g->compactPatterns);
    TableClear(&g->currencySets);
    MapEach(&g->ownFormats, FreeValue, nullptr);
    MapFree(&g->ownFormats);
    CldrClose(&g->cldr);
    KeysClose(&g->keys);
    return ok;
}
