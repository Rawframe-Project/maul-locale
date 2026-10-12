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
#include "keys.h"
#include "kinds.h"
#include "map.h"
#include "number_tables.h"
#include "xml.h"

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
    maxSystems = 96,
    maxLocales = 2048,
    maxTable = 8192,
    maxKey = 64,
    maxAffix = 128,
};

static const char* const s_emptyMarker = "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85";

// The CLDR element of each symbol, in NumberSymbol order.
static const char* const s_symbolNames[numberSymbolCount] = {
    "decimal",      "group",    "minusSign",         "plusSign",
    "percentSign",  "perMille", "exponential",       "superscriptingExponent",
    "infinity",     "nan",      "approximatelySign", "currencyDecimal",
    "currencyGroup"};

typedef struct System
{
    uint64_t id;
    char name[9];
    uint8_t digits[10][numberDigitSize];
} System;

// A string table and the records that index it, as the symbol and
// pattern units store them.
typedef struct Table
{
    Map strings;
    char* texts[maxTable];
    size_t lengths[maxTable];
    int stringCount;
    Map records;
    uint8_t* recordBytes;
    int recordCount;
    int recordSize;
} Table;

// A numbering system's data in a locale.
typedef struct Entry
{
    // The system's index.
    uint16_t system;
    uint16_t symbols;
    uint16_t decimal;
    uint16_t percent;
    uint16_t scientific;
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
} Generator;

[[noreturn]] static void Die(const char* message, const char* detail)
{
    fprintf(stderr, "mlocgen: %s: %s\n", message, detail);
    exit(1);
}

static void* Allocate(size_t size)
{
    void* p = calloc(1, size);
    if (p == nullptr)
    {
        Die("out of memory", "numbers");
    }
    return p;
}

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// The UTF-8 length of the character at text, or 0 when it is malformed.
static int CharLength(const unsigned char* text)
{
    unsigned c = text[0];
    int n = c < 0x80             ? 1
            : (c & 0xE0) == 0xC0 ? 2
            : (c & 0xF0) == 0xE0 ? 3
            : (c & 0xF8) == 0xF0 ? 4
                                 : 0;
    for (int i = 1; i < n; i++)
    {
        if ((text[i] & 0xC0) != 0x80)
        {
            return 0;
        }
    }
    return n;
}

// The index of a string in a table, added when new.
static uint16_t AddString(Table* t, const char* text, size_t length)
{
    char key[maxAffix * 4 + 1];
    if (length >= sizeof(key))
    {
        Die("a number string too long", text);
    }
    memcpy(key, text, length);
    key[length] = '\0';
    void* found = MapGet(&t->strings, key);
    if (found != nullptr)
    {
        return (uint16_t)((uintptr_t)found - 1);
    }
    if (t->stringCount == maxTable)
    {
        Die("too many number strings", key);
    }
    t->texts[t->stringCount] = Allocate(length + 1);
    memcpy(t->texts[t->stringCount], text, length);
    t->lengths[t->stringCount] = length;
    MapPut(&t->strings, key, (void*)(uintptr_t)(t->stringCount + 1));
    return (uint16_t)t->stringCount++;
}

// The index of a record, added when new.
static uint16_t AddRecord(Table* t, const uint8_t* record)
{
    char key[2 * 64 + 1];
    for (int i = 0; i < t->recordSize; i++)
    {
        snprintf(key + 2 * i, 3, "%02X", record[i]);
    }
    void* found = MapGet(&t->records, key);
    if (found != nullptr)
    {
        return (uint16_t)((uintptr_t)found - 1);
    }
    if (t->recordCount == maxTable)
    {
        Die("too many number records", key);
    }
    if (t->recordBytes == nullptr)
    {
        t->recordBytes = Allocate((size_t)maxTable * (size_t)t->recordSize);
    }
    memcpy(t->recordBytes + (size_t)t->recordCount * (size_t)t->recordSize, record,
           (size_t)t->recordSize);
    MapPut(&t->records, key, (void*)(uintptr_t)(t->recordCount + 1));
    return (uint16_t)t->recordCount++;
}

static void AddTableUnit(const Table* t, uint32_t kind, DataFile* file)
{
    size_t text = 0;
    for (int i = 0; i < t->stringCount; i++)
    {
        text += t->lengths[i];
    }
    size_t offsets = (size_t)(t->stringCount + 1) * 2;
    size_t records = (size_t)t->recordCount * (size_t)t->recordSize;
    size_t size = numberTableHeaderSize + offsets + records + text;
    if (text > 0xFFFF || t->recordCount > 0xFFFF)
    {
        Die("a number table too large", "text or records");
    }
    uint8_t* unit = Allocate(size);
    Put(unit, (uint64_t)t->stringCount, 2);
    Put(unit + 2, (uint64_t)t->recordCount, 2);
    Put(unit + 4, text, 4);
    uint8_t* textAt = unit + numberTableHeaderSize + offsets + records;
    size_t at = 0;
    for (int i = 0; i < t->stringCount; i++)
    {
        Put(unit + numberTableHeaderSize + 2 * (size_t)i, at, 2);
        memcpy(textAt + at, t->texts[i], t->lengths[i]);
        at += t->lengths[i];
    }
    Put(unit + numberTableHeaderSize + 2 * (size_t)t->stringCount, at, 2);
    if (records > 0)
    {
        memcpy(unit + numberTableHeaderSize + offsets, t->recordBytes, records);
    }
    DataFileAdd(file, kind, nullptr, unit, size);
    free(unit);
}

static void FreeTable(Table* t)
{
    for (int i = 0; i < t->stringCount; i++)
    {
        free(t->texts[i]);
    }
    free(t->recordBytes);
    MapFree(&t->strings);
    MapFree(&t->records);
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
            Die("a numeric system the table cannot hold", id == nullptr ? "(no id)" : id);
        }
        System* s = &g->systems[g->systemCount++];
        s->id = mlocPackSystem(id, (uint32_t)strlen(id));
        snprintf(s->name, sizeof(s->name), "%s", id);
        const unsigned char* at = (const unsigned char*)digits;
        int length = 0;
        for (int d = 0; d < 10; d++)
        {
            int n = CharLength(at);
            if (n == 0 || (d > 0 && n != length))
            {
                Die("digits of unequal or malformed UTF-8", id);
            }
            length = n;
            memcpy(s->digits[d], at, (size_t)n);
            at += n;
        }
        if (s->id == 0 || *at != '\0')
        {
            Die("a numeric system not of ten digits", id);
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
    uint8_t* unit = Allocate(size);
    Put(unit, (uint64_t)g->systemCount, 4);
    for (int i = 0; i < g->systemCount; i++)
    {
        uint8_t* entry = unit + numberSystemsHeaderSize + (size_t)i * numberSystemSize;
        Put(entry, g->systems[i].id, 8);
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
        Die("not a numbering system id", name);
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
        Die("a pattern affix too long", pattern);
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
                Die("a currency sign run the library has no meaning for", pattern);
            }
            code = s_codes[run];
            advance = 2 * (size_t)run;
        }
        else if ((unsigned char)*c < 0x20)
        {
            Die("a control character in a pattern", pattern);
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
        Die("an unended quote in a pattern", pattern);
    }
}

// Compiles a pattern into the bytes of a pattern record.
static void CompilePattern(Generator* g, const char* pattern, uint8_t* record)
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
                Die("an exponent without digits", pattern);
            }
            break;
        }
        if (c == '.')
        {
            if (fraction)
            {
                Die("two decimal points in a pattern", pattern);
            }
            fraction = true;
        }
        else if (c == ',')
        {
            if (fraction)
            {
                Die("a group separator in a fraction", pattern);
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
        Die("a pattern with significant digits or an increment", pattern);
    }
    if (integer == 0 && !fraction)
    {
        Die("a pattern without a number part", pattern);
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
        Die("text after a pattern", pattern);
    }
    int primary = sinceComma > 0 ? sinceComma : 0;
    int secondary = previousGroup > 0 ? previousGroup : primary;
    if (primary > numberMaxGroup || secondary > numberMaxGroup || exponentDigits > numberMaxGroup ||
        minInteger > numberMaxPatternDigits || maxFraction > numberMaxPatternDigits)
    {
        Die("a pattern past the record's limits", pattern);
    }
    memset(record, 0, numberPatternSize);
    Put(record + patternPositivePrefix, AddString(&g->patterns, prefix.bytes, prefix.length), 2);
    Put(record + patternPositiveSuffix, AddString(&g->patterns, suffix.bytes, suffix.length), 2);
    Put(record + patternNegativePrefix,
        AddString(&g->patterns, negativePrefix.bytes, negativePrefix.length), 2);
    Put(record + patternNegativeSuffix,
        AddString(&g->patterns, negativeSuffix.bytes, negativeSuffix.length), 2);
    record[patternMinInteger] = (uint8_t)minInteger;
    record[patternMinFraction] = (uint8_t)minFraction;
    record[patternMaxFraction] = (uint8_t)maxFraction;
    record[patternPrimary] = (uint8_t)primary;
    record[patternSecondary] = (uint8_t)secondary;
    record[patternExponentDigits] = (uint8_t)exponentDigits;
    record[patternFlags] = exponentPlus ? patternFlagExponentPlus : 0;
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
        Die("a locale without a pattern", chain);
    }
    uint8_t record[numberPatternSize];
    CompilePattern(g, pattern, record);
    return AddRecord(&g->patterns, record);
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
        values[s] = Value(g, locale, chain);
        if (values[s] == nullptr)
        {
            // A currency separator CLDR does not give is the plain one.
            if (s == numberCurrencyDecimal || s == numberCurrencyGroup)
            {
                values[s] = values[s == numberCurrencyDecimal ? numberDecimal : numberGroup];
            }
            else
            {
                Die("a locale without a number symbol", chain);
            }
        }
        for (const unsigned char* c = (const unsigned char*)values[s]; *c != '\0';)
        {
            int n = CharLength(c);
            if (n == 0 || *c < 0x20)
            {
                Die("a number symbol with a control or malformed character", chain);
            }
            c += n;
        }
        Put(record + 2 * s, AddString(&g->symbols, values[s], strlen(values[s])), 2);
    }
    return AddRecord(&g->symbols, record);
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
        Die("a locale without a numeric default system or with odd grouping", locale);
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
            .scientific = PatternOf(g, locale, s->name, "scientific"),
        };
        if (s->id == numberLatn)
        {
            latn = e;
        }
        out->entries[out->entryCount++] = e;
    }
    if (latn.system == numberNoSystem)
    {
        Die("numberingSystems.xml has no latn", locale);
    }
    // Only the systems whose data differs from latn's, and latn.
    int kept = 0;
    for (int i = 0; i < out->entryCount; i++)
    {
        const Entry* e = &out->entries[i];
        if (g->systems[e->system].id == numberLatn || e->symbols != latn.symbols ||
            e->decimal != latn.decimal || e->percent != latn.percent ||
            e->scientific != latn.scientific)
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
    uint8_t* unit = Allocate(size);
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
        Put(entry + entrySymbols, n->entries[i].symbols, 2);
        Put(entry + entryDecimal, n->entries[i].decimal, 2);
        Put(entry + entryPercent, n->entries[i].percent, 2);
        Put(entry + entryScientific, n->entries[i].scientific, 2);
    }
    DataFileAdd(file, kindNumbers, key, unit, size);
    free(unit);
}

static int CompareNames(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

// The ids of common/main's files, sorted; the caller frees each and the
// list.
static char** ListLocales(const char* commonDirectory, int* countOut)
{
    char** ids = Allocate(sizeof(char*) * maxLocales);
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
        Die("cannot list", path);
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
            Die("too many locales in", path);
        }
        ids[count] = Allocate(length - 3);
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

int GenerateNumberUnits(const char* commonDirectory, DataFile* file, char* errorOut,
                        size_t errorCapacity)
{
    static Generator s_g;
    memset(&s_g, 0, sizeof(s_g));
    Generator* g = &s_g;
    g->symbols.recordSize = 2 * numberSymbolCount;
    g->patterns.recordSize = numberPatternSize;
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
    Numbers* all = Allocate(sizeof(Numbers) * (size_t)count);
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
                Die("a locale's numbers differ from those of its parent's key", ids[i]);
            }
            continue;
        }
        const Numbers* base = next == nullptr ? nullptr : MapGet(&resolved, next);
        if (next != nullptr && base == nullptr)
        {
            Die("a parent locale without a file", next);
        }
        if (base == nullptr || memcmp(base, &all[i], sizeof(Numbers)) != 0)
        {
            AddLocaleUnit(&all[i], key, file);
        }
    }
    AddSystemUnit(g, file);
    AddTableUnit(&g->symbols, kindNumberSymbols, file);
    AddTableUnit(&g->patterns, kindNumberPatterns, file);
    for (int i = 0; i < count; i++)
    {
        free(ids[i]);
    }
    free(ids);
    free(all);
    MapFree(&resolved);
    FreeTable(&g->symbols);
    FreeTable(&g->patterns);
    CldrClose(&g->cldr);
    KeysClose(&g->keys);
    return 1;
}
