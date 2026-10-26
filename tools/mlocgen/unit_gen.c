// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit table (unit_gen.h). An expression is numbers and earlier
// constants joined by * and /, where * binds tighter (UTS #35 Part 6:
// a * b / c * d is (a * b) / (c * d)); a number is an exact decimal.

#include "unit_gen.h"

#include "kinds.h"
#include "locale_tables.h"
#include "table_gen.h"
#include "unit_tables.h"
#include "wide.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxConstants = 64,
    maxSimples = 512,
    maxPrefixes = 64,
    maxAliases = 64,
    maxBases = 128,
    maxToken = 64,
    maxText = 16384,
    maxNumbers = 16384,
    maxSets = 64,
    maxRuns = 128,
    maxEntries = 2048,
};

// The least speed of each Beaufort force in tenths of a meter per
// second, then one past force 17: UTS #35 names the conversion but gives
// no table, so this is the one ICU4C uses (CLDR-17421).
static const uint16_t s_beaufort[] = {0,   3,   16,  34,  55,  80,  108, 139, 172, 208,
                                      245, 285, 327, 369, 414, 461, 511, 558, 614};

typedef struct Fraction
{
    Wide n;
    Wide d;
} Fraction;

typedef struct Constant
{
    const char* name;
    Fraction value;
} Constant;

typedef struct Simple
{
    const char* id;
    int base;
    int flags;
    int order;
    int systems;
    Fraction factor;
    Fraction offset;
} Simple;

typedef struct Gen
{
    Constant constants[maxConstants];
    int constantCount;
    Simple simples[maxSimples];
    int simpleCount;
    const char* bases[maxBases];
    uint8_t numerators[maxBases][unitMaxDimensions];
    uint8_t denominators[maxBases][unitMaxDimensions];
    int baseCount;
    char atoms[unitMaxDimensions][maxToken];
    int atomCount;
    const char* quantities[maxBases];
    const char* quantityNames[maxBases];
    uint8_t quantityNumerators[maxBases][unitMaxDimensions];
    uint8_t quantityDenominators[maxBases][unitMaxDimensions];
    int quantityCount;
    const XmlNode* prefixes[maxPrefixes];
    int prefixCount;
    const XmlNode* aliases[maxAliases];
    int aliasCount;
} Gen;

static void Reduce(Fraction* f)
{
    Wide a = f->n;
    Wide b = f->d;
    while (!mlocWideIsZero(&b))
    {
        Wide r;
        mlocWideDivide(&a, &b, nullptr, &r);
        a = b;
        b = r;
    }
    if (!mlocWideIsZero(&a))
    {
        // An output of mlocWideDivide may not be one of its inputs.
        Wide n;
        Wide d;
        mlocWideDivide(&f->n, &a, &n, nullptr);
        mlocWideDivide(&f->d, &a, &d, nullptr);
        f->n = n;
        f->d = d;
    }
}

static void Multiply(Fraction* f, const Fraction* by, bool divide, const char* where)
{
    const Wide* n = divide ? &by->d : &by->n;
    const Wide* d = divide ? &by->n : &by->d;
    if (mlocWideIsZero(n) && divide)
    {
        GenDie("a unit expression divides by zero", where);
    }
    if (!mlocWideMultiply(&f->n, &f->n, n) || !mlocWideMultiply(&f->d, &f->d, d))
    {
        GenDie("a unit expression passes the exact integers", where);
    }
    Reduce(f);
}

// An exact decimal: digits, a point, an exponent ("6.02214076E+23").
static Fraction ParseNumber(const char* text, size_t length, const char* where)
{
    Fraction f = {.n = mlocWideOf(0), .d = mlocWideOf(1)};
    size_t i = 0;
    int scale = 0;
    bool point = false;
    bool digits = false;
    for (; i < length && ((text[i] >= '0' && text[i] <= '9') || (text[i] == '.' && !point)); i++)
    {
        if (text[i] == '.')
        {
            point = true;
            continue;
        }
        digits = true;
        scale -= point;
        (void)mlocWideMultiplyAdd(&f.n, 10, (uint32_t)(text[i] - '0'));
    }
    if (i < length && (text[i] == 'E' || text[i] == 'e'))
    {
        bool negative = ++i < length && text[i] == '-';
        i += i < length && (text[i] == '-' || text[i] == '+');
        int exponent = 0;
        for (; i < length && text[i] >= '0' && text[i] <= '9' && exponent < 1000; i++)
        {
            exponent = exponent * 10 + text[i] - '0';
        }
        scale += negative ? -exponent : exponent;
    }
    if (!digits || i != length)
    {
        GenDie("a unit number that is not a decimal", where);
    }
    if (!mlocWideScale10(scale >= 0 ? &f.n : &f.d, (uint32_t)(scale >= 0 ? scale : -scale)))
    {
        GenDie("a unit number passes the exact integers", where);
    }
    Reduce(&f);
    return f;
}

static Fraction Term(const Gen* g, const char* text, size_t length, const char* where)
{
    while (length > 0 && text[0] == ' ')
    {
        text++;
        length--;
    }
    while (length > 0 && text[length - 1] == ' ')
    {
        length--;
    }
    for (int i = 0; i < g->constantCount; i++)
    {
        if (strlen(g->constants[i].name) == length &&
            memcmp(g->constants[i].name, text, length) == 0)
        {
            return g->constants[i].value;
        }
    }
    return ParseNumber(text, length, where);
}

// A product of terms joined by *.
static Fraction Product(const Gen* g, const char* text, size_t length, const char* where)
{
    Fraction f = {.n = mlocWideOf(1), .d = mlocWideOf(1)};
    size_t start = 0;
    for (size_t i = 0; i <= length; i++)
    {
        if (i == length || text[i] == '*')
        {
            Fraction t = Term(g, text + start, i - start, where);
            Multiply(&f, &t, false, where);
            start = i + 1;
        }
    }
    return f;
}

static Fraction Evaluate(const Gen* g, const char* text)
{
    const char* slash = strchr(text, '/');
    size_t first = slash == nullptr ? strlen(text) : (size_t)(slash - text);
    Fraction f = Product(g, text, first, text);
    while (slash != nullptr)
    {
        const char* start = slash + 1;
        slash = strchr(start, '/');
        size_t n = slash == nullptr ? strlen(start) : (size_t)(slash - start);
        Fraction d = Product(g, start, n, text);
        Multiply(&f, &d, true, text);
    }
    return f;
}

static int AtomIndex(Gen* g, const char* atom, size_t length)
{
    for (int i = 0; i < g->atomCount; i++)
    {
        if (strlen(g->atoms[i]) == length && memcmp(g->atoms[i], atom, length) == 0)
        {
            return i;
        }
    }
    if (g->atomCount == unitMaxDimensions || length >= maxToken)
    {
        GenDie("too many unit dimensions", atom);
    }
    memcpy(g->atoms[g->atomCount], atom, length);
    g->atoms[g->atomCount][length] = '\0';
    return g->atomCount++;
}

// A power token's value, or 0.
static int PowerOf(const char* token, size_t length)
{
    if (length == 6 && memcmp(token, "square", 6) == 0)
    {
        return 2;
    }
    if (length == 5 && memcmp(token, "cubic", 5) == 0)
    {
        return 3;
    }
    return length > 3 && memcmp(token, "pow", 3) == 0 ? atoi(token + 3) : 0;
}

// Reads a base unit identifier's exponents: atoms with powers, those
// after "per" in the denominator, nothing cancelled.
static void ParseDimensions(Gen* g, const char* id, uint8_t* numerator, uint8_t* denominator)
{
    bool per = false;
    int power = 1;
    for (const char* t = id; *t != '\0';)
    {
        size_t n = strcspn(t, "-");
        int p = PowerOf(t, n);
        if (n == 3 && memcmp(t, "per", 3) == 0)
        {
            per = true;
        }
        else if (p != 0)
        {
            power = p;
        }
        else
        {
            uint8_t* side = per ? denominator : numerator;
            int atom = AtomIndex(g, t, n);
            if (side[atom] + power > 255)
            {
                GenDie("a base unit power past a byte", id);
            }
            side[atom] = (uint8_t)(side[atom] + power);
            power = 1;
        }
        t += n + (t[n] == '-');
    }
}

// The index of a base unit, added with its exponents when new.
static int BaseIndex(Gen* g, const char* id)
{
    for (int i = 0; i < g->baseCount; i++)
    {
        if (strcmp(g->bases[i], id) == 0)
        {
            return i;
        }
    }
    if (g->baseCount == maxBases)
    {
        GenDie("too many base units", id);
    }
    int index = g->baseCount++;
    g->bases[index] = id;
    ParseDimensions(g, id, g->numerators[index], g->denominators[index]);
    return index;
}

// The systems attribute's bits.
static int Systems(const char* text, const char* where)
{
    static const char* const s_names[] = {
        "si",       "si_acceptable", "metric",       "metric_adjacent", "ussystem",
        "uksystem", "jpsystem",      "astronomical", "person_age",      "prefixable"};
    enum
    {
        nameCount = sizeof(s_names) / sizeof(s_names[0]),
    };
    int bits = 0;
    for (const char* t = text == nullptr ? "" : text; *t != '\0';)
    {
        size_t n = strcspn(t, " ");
        size_t k = 0;
        while (k < nameCount && !(strlen(s_names[k]) == n && memcmp(s_names[k], t, n) == 0))
        {
            k++;
        }
        if (n > 0 && k == nameCount)
        {
            GenDie("a unit system the library does not know", where);
        }
        bits |= n > 0 ? 1 << k : 0;
        t += n + (t[n] == ' ');
    }
    return bits;
}

static void ReadConstants(Gen* g, const XmlNode* list)
{
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        const XmlNode* c = list->children[i];
        const char* name = XmlGetAttribute(c, "constant");
        const char* value = XmlGetAttribute(c, "value");
        if (strcmp(c->name, "unitConstant") != 0)
        {
            continue;
        }
        if (name == nullptr || value == nullptr || g->constantCount == maxConstants)
        {
            GenDie("a unit constant without a name or value, or too many", "units.xml");
        }
        g->constants[g->constantCount] = (Constant){name, Evaluate(g, value)};
        g->constantCount++;
    }
}

static int QuantityOrder(const Gen* g, const char* base)
{
    for (int i = 0; i < g->quantityCount; i++)
    {
        if (strcmp(g->quantities[i], base) == 0)
        {
            return i;
        }
    }
    GenDie("a base unit without a quantity", base);
}

static void ReadConversions(Gen* g, const XmlNode* list)
{
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        const XmlNode* c = list->children[i];
        const char* source = XmlGetAttribute(c, "source");
        const char* base = XmlGetAttribute(c, "baseUnit");
        if (strcmp(c->name, "convertUnit") != 0)
        {
            continue;
        }
        if (source == nullptr || base == nullptr || g->simpleCount == maxSimples)
        {
            GenDie("a conversion without a source or base, or too many", "units.xml");
        }
        const char* factor = XmlGetAttribute(c, "factor");
        const char* offset = XmlGetAttribute(c, "offset");
        const char* special = XmlGetAttribute(c, "special");
        Simple s = {
            .id = source,
            .base = BaseIndex(g, base),
            .factor = Evaluate(g, factor == nullptr ? "1" : factor),
            .offset = Evaluate(g, offset == nullptr ? "0" : offset),
            .order = QuantityOrder(g, base),
            .systems = Systems(XmlGetAttribute(c, "systems"), source),
        };
        s.flags = offset != nullptr ? unitOffset : 0;
        if (special != nullptr)
        {
            if (strcmp(special, "beaufort") != 0 || factor != nullptr || offset != nullptr)
            {
                GenDie("a special conversion the library does not know", special);
            }
            s.flags = unitBeaufort;
        }
        g->simples[g->simpleCount++] = s;
    }
}

static void Collect(Gen* g, const XmlNode* node)
{
    for (int i = 0; i < node->childCount; i++)
    {
        const XmlNode* c = node->children[i];
        if (strcmp(c->name, "unitPrefix") == 0 && g->prefixCount < maxPrefixes)
        {
            g->prefixes[g->prefixCount++] = c;
        }
        else if (strcmp(c->name, "unitAlias") == 0 && g->aliasCount < maxAliases)
        {
            g->aliases[g->aliasCount++] = c;
        }
        else if (strcmp(c->name, "unitQuantity") == 0 && g->quantityCount < maxBases)
        {
            const char* base = XmlGetAttribute(c, "baseUnit");
            const char* name = XmlGetAttribute(c, "quantity");
            if (base == nullptr || name == nullptr)
            {
                GenDie("a unit quantity without a base unit or name", "units.xml");
            }
            g->quantities[g->quantityCount] = base;
            g->quantityNames[g->quantityCount++] = name;
        }
        Collect(g, c);
    }
}

static int CompareSimples(const void* a, const void* b)
{
    return strcmp(((const Simple*)a)->id, ((const Simple*)b)->id);
}

static int CompareNodes(const void* a, const void* b, const char* attribute)
{
    return strcmp(XmlGetAttribute(*(const XmlNode* const*)a, attribute),
                  XmlGetAttribute(*(const XmlNode* const*)b, attribute));
}

static int ComparePrefixes(const void* a, const void* b)
{
    return CompareNodes(a, b, "type");
}

static int CompareAliases(const void* a, const void* b)
{
    return CompareNodes(a, b, "type");
}

// The unit's growing text and numbers.
typedef struct Out
{
    char text[maxText];
    uint32_t textSize;
    uint8_t numbers[maxNumbers];
    uint32_t numbersSize;
} Out;

// Adds a text, giving its offset; its length must fit a byte.
static uint32_t AddText(Out* o, const char* text, const char* where)
{
    size_t n = text == nullptr ? 0 : strlen(text);
    if (n == 0 || n > 255 || o->textSize + n > maxText)
    {
        GenDie("a unit identifier that is empty, too long or past the text", where);
    }
    memcpy(o->text + o->textSize, text, n);
    o->textSize += (uint32_t)n;
    return o->textSize - (uint32_t)n;
}

static void AddNumber(Out* o, const Wide* w, const char* where)
{
    uint8_t bytes[4 * wideLimbs];
    uint32_t length = 0;
    for (uint32_t i = 0; i < 4 * w->count; i++)
    {
        bytes[i] = (uint8_t)(w->limb[i / 4] >> (8 * (i % 4)));
        length = bytes[i] != 0 ? i + 1 : length;
    }
    if (length > unitMaxNumberBytes || o->numbersSize + 1 + length > maxNumbers)
    {
        GenDie("a unit factor too large for the table", where);
    }
    o->numbers[o->numbersSize++] = (uint8_t)length;
    memcpy(o->numbers + o->numbersSize, bytes, length);
    o->numbersSize += length;
}

static void WriteSimples(const Gen* g, Out* o, uint8_t* at)
{
    for (int i = 0; i < g->simpleCount; i++, at += unitSimpleSize)
    {
        const Simple* s = &g->simples[i];
        GenPut(at + simpleText, AddText(o, s->id, s->id), 4);
        at[simpleLength] = (uint8_t)strlen(s->id);
        at[simpleBase] = (uint8_t)s->base;
        at[simpleFlags] = (uint8_t)s->flags;
        at[simpleOrder] = (uint8_t)s->order;
        GenPut(at + simpleFactor, o->numbersSize, 4);
        GenPut(at + simpleSystems, (uint64_t)s->systems, 2);
        AddNumber(o, &s->factor.n, s->id);
        AddNumber(o, &s->factor.d, s->id);
        if ((s->flags & unitOffset) != 0)
        {
            AddNumber(o, &s->offset.n, s->id);
            AddNumber(o, &s->offset.d, s->id);
        }
    }
}

static void WritePrefixes(const Gen* g, Out* o, uint8_t* at)
{
    for (int i = 0; i < g->prefixCount; i++, at += unitPrefixSize)
    {
        const char* type = XmlGetAttribute(g->prefixes[i], "type");
        const char* power10 = XmlGetAttribute(g->prefixes[i], "power10");
        const char* power2 = XmlGetAttribute(g->prefixes[i], "power2");
        int p10 = power10 == nullptr ? 0 : atoi(power10);
        int p2 = power2 == nullptr ? 0 : atoi(power2);
        if ((power10 == nullptr) == (power2 == nullptr) || p10 < -30 || p10 > 30 || p2 % 10 != 0 ||
            p2 < 0 || p2 > 100)
        {
            GenDie("a unit prefix the table cannot hold", type == nullptr ? "(none)" : type);
        }
        GenPut(at, AddText(o, type, "a unit prefix"), 4);
        at[4] = (uint8_t)strlen(type);
        at[5] = (uint8_t)(int8_t)p10;
        at[6] = (uint8_t)(p2 / 10);
    }
}

static void WriteAliases(const Gen* g, Out* o, uint8_t* at)
{
    for (int i = 0; i < g->aliasCount; i++, at += unitAliasSize)
    {
        const char* type = XmlGetAttribute(g->aliases[i], "type");
        const char* replacement = XmlGetAttribute(g->aliases[i], "replacement");
        GenPut(at, AddText(o, type, "a unit alias"), 4);
        GenPut(at + 4, AddText(o, replacement, type), 4);
        at[8] = (uint8_t)strlen(type);
        at[9] = (uint8_t)strlen(replacement);
    }
}

static void WriteBases(const Gen* g, Out* o, uint8_t* at)
{
    for (int i = 0; i < g->baseCount; i++, at += unitBaseSize)
    {
        GenPut(at, AddText(o, g->bases[i], g->bases[i]), 4);
        at[4] = (uint8_t)strlen(g->bases[i]);
        memcpy(at + baseNumerator, g->numerators[i], unitMaxDimensions);
        memcpy(at + baseDenominator, g->denominators[i], unitMaxDimensions);
    }
}

// The dimensions: each atom's identifier and its place in the order.
static void WriteDimensions(const Gen* g, Out* o, uint8_t* at)
{
    for (int i = 0; i < g->atomCount; i++, at += unitDimensionSize)
    {
        GenPut(at, AddText(o, g->atoms[i], g->atoms[i]), 4);
        at[4] = (uint8_t)strlen(g->atoms[i]);
        at[5] = (uint8_t)QuantityOrder(g, g->atoms[i]);
    }
}

static void WriteQuantities(const Gen* g, Out* o, uint8_t* at)
{
    for (int i = 0; i < g->quantityCount; i++, at += unitQuantitySize)
    {
        GenPut(at, AddText(o, g->quantityNames[i], g->quantityNames[i]), 4);
        GenPut(at + 4, AddText(o, g->quantities[i], g->quantities[i]), 4);
        at[8] = (uint8_t)strlen(g->quantityNames[i]);
        at[9] = (uint8_t)strlen(g->quantities[i]);
        memcpy(at + quantityNumerator, g->quantityNumerators[i], unitMaxDimensions);
        memcpy(at + quantityDenominator, g->quantityDenominators[i], unitMaxDimensions);
    }
}

static void Write(Gen* g, DataFile* file)
{
    static Out s_out;
    s_out = (Out){0};
    uint32_t beaufortCount = sizeof(s_beaufort) / sizeof(s_beaufort[0]);
    size_t fixed = unitsHeaderSize + (size_t)g->simpleCount * unitSimpleSize +
                   (size_t)g->prefixCount * unitPrefixSize + (size_t)g->aliasCount * unitAliasSize +
                   (size_t)g->baseCount * unitBaseSize + (size_t)g->atomCount * unitDimensionSize +
                   (size_t)g->quantityCount * unitQuantitySize + (size_t)beaufortCount * 2;
    uint8_t* records = GenAllocate(fixed);
    uint8_t* at = records + unitsHeaderSize;
    WriteSimples(g, &s_out, at);
    at += (size_t)g->simpleCount * unitSimpleSize;
    WritePrefixes(g, &s_out, at);
    at += (size_t)g->prefixCount * unitPrefixSize;
    WriteAliases(g, &s_out, at);
    at += (size_t)g->aliasCount * unitAliasSize;
    WriteBases(g, &s_out, at);
    at += (size_t)g->baseCount * unitBaseSize;
    WriteDimensions(g, &s_out, at);
    at += (size_t)g->atomCount * unitDimensionSize;
    WriteQuantities(g, &s_out, at);
    at += (size_t)g->quantityCount * unitQuantitySize;
    for (uint32_t i = 0; i < beaufortCount; i++)
    {
        GenPut(at + 2 * i, s_beaufort[i], 2);
    }
    GenPut(records, (uint64_t)g->simpleCount, 2);
    GenPut(records + 2, (uint64_t)g->prefixCount, 2);
    GenPut(records + 4, (uint64_t)g->aliasCount, 2);
    GenPut(records + 6, (uint64_t)g->baseCount, 2);
    GenPut(records + 8, beaufortCount, 2);
    GenPut(records + 10, (uint64_t)g->atomCount, 2);
    GenPut(records + 12, (uint64_t)g->quantityCount, 2);
    GenPut(records + 16, s_out.textSize, 4);
    GenPut(records + 20, s_out.numbersSize, 4);
    size_t size = fixed + s_out.textSize + s_out.numbersSize;
    uint8_t* unit = GenAllocate(size);
    memcpy(unit, records, fixed);
    memcpy(unit + fixed, s_out.text, s_out.textSize);
    memcpy(unit + fixed + s_out.textSize, s_out.numbers, s_out.numbersSize);
    DataFileAdd(file, kindUnits, nullptr, unit, size);
    free(records);
    free(unit);
}

// The preference data's parts while they are gathered.
typedef struct Entry
{
    char unit[maxToken];
    uint32_t threshold;
    int places;
    int increment;
} Entry;

typedef struct Run
{
    uint32_t region;
    int first;
    int count;
} Run;

typedef struct Set
{
    const char* category;
    char usage[maxToken];
    Run runs[maxRuns];
    int runCount;
} Set;

typedef struct Prefs
{
    Set sets[maxSets];
    int setCount;
    Entry entries[maxEntries];
    int entryCount;
} Prefs;

// A threshold: digits with an optional point ("0.9", "300.0").
static void ReadThreshold(const char* text, Entry* e)
{
    e->threshold = 0;
    e->places = 0;
    bool point = false;
    for (const char* c = text; *c != '\0'; c++)
    {
        if (*c == '.' && !point)
        {
            point = true;
        }
        else if (*c >= '0' && *c <= '9' && e->threshold < 100000000u && e->places < unitMaxPlaces)
        {
            e->threshold = e->threshold * 10 + (uint32_t)(*c - '0');
            e->places += point;
        }
        else
        {
            GenDie("a unit preference threshold the table cannot hold", text);
        }
    }
}

static int Increment(const char* skeleton)
{
    static const char s_prefix[] = "precision-increment/";
    if (skeleton == nullptr)
    {
        return 0;
    }
    int n = strncmp(skeleton, s_prefix, sizeof(s_prefix) - 1) == 0
                ? atoi(skeleton + sizeof(s_prefix) - 1)
                : 0;
    if (n != 1 && n != 2 && n != 5 && n != 10 && n != 20 && n != 25 && n != 50 && n != 100)
    {
        GenDie("a unit preference skeleton the library does not read", skeleton);
    }
    return n;
}

// A preference of a set before its runs are laid out: a region and an
// entry, in CLDR's order.
typedef struct Pending
{
    uint32_t region;
    Entry entry;
} Pending;

static Run* RunFor(Set* set, uint32_t region)
{
    for (int i = 0; i < set->runCount; i++)
    {
        if (set->runs[i].region == region)
        {
            return &set->runs[i];
        }
    }
    if (set->runCount == maxRuns)
    {
        GenDie("too many regions in a unit preference set", set->category);
    }
    set->runs[set->runCount] = (Run){.region = region};
    return &set->runs[set->runCount++];
}

// Adds a preference for each region it names to the pending list.
static int AddPreference(Set* set, const XmlNode* node, Pending* pending, int count)
{
    const char* regions = XmlGetAttribute(node, "regions");
    const char* geq = XmlGetAttribute(node, "geq");
    Entry e = {.increment = Increment(XmlGetAttribute(node, "skeleton"))};
    size_t start = strspn(node->text, " \t\r\n");
    size_t n = strcspn(node->text + start, " \t\r\n");
    if (regions == nullptr || n == 0 || n >= maxToken)
    {
        GenDie("a unit preference without regions or a unit", set->category);
    }
    memcpy(e.unit, node->text + start, n);
    if (geq != nullptr)
    {
        ReadThreshold(geq, &e);
    }
    else
    {
        // Marked to be set when the run is complete: 1, or 0 for its last.
        e.places = -1;
    }
    for (const char* t = regions; *t != '\0';)
    {
        size_t k = strcspn(t, " ");
        uint32_t region = mlocPackRegion(t, (uint32_t)k);
        if (region == 0 || (k == 3 && memcmp(t, "001", 3) != 0))
        {
            GenDie("a unit preference region the library does not fall back from", regions);
        }
        if (count == maxEntries)
        {
            GenDie("too many unit preferences", regions);
        }
        (void)RunFor(set, region);
        pending[count++] = (Pending){region, e};
        t += k + (t[k] == ' ');
    }
    return count;
}

// Lays out a set's runs: each region's entries together, in CLDR's order,
// a run's last threshold 0 and the others 1 where CLDR gives none.
static void LayOut(Prefs* p, Set* set, const Pending* pending, int count)
{
    for (int r = 0; r < set->runCount; r++)
    {
        Run* run = &set->runs[r];
        run->first = p->entryCount;
        for (int k = 0; k < count; k++)
        {
            if (pending[k].region == run->region)
            {
                if (p->entryCount == maxEntries)
                {
                    GenDie("too many unit preferences", set->category);
                }
                p->entries[p->entryCount++] = pending[k].entry;
                run->count++;
            }
        }
        for (int k = run->first; k < run->first + run->count; k++)
        {
            if (p->entries[k].places < 0)
            {
                p->entries[k].places = 0;
                p->entries[k].threshold = k + 1 < run->first + run->count;
            }
        }
        if (p->entries[run->first + run->count - 1].threshold != 0)
        {
            GenDie("a unit preference run whose last threshold is not 0", set->category);
        }
    }
}

static int CompareRuns(const void* a, const void* b)
{
    uint32_t x = ((const Run*)a)->region;
    uint32_t y = ((const Run*)b)->region;
    return x < y ? -1 : x > y;
}

static int CompareSets(const void* a, const void* b)
{
    const Set* x = a;
    const Set* y = b;
    int c = strcmp(x->category, y->category);
    return c != 0 ? c : strcmp(x->usage, y->usage);
}

static void ReadPreferences(Prefs* p, const XmlNode* data)
{
    for (int i = 0; data != nullptr && i < data->childCount; i++)
    {
        const XmlNode* list = data->children[i];
        const char* category = XmlGetAttribute(list, "category");
        const char* usage = XmlGetAttribute(list, "usage");
        if (strcmp(list->name, "unitPreferences") != 0)
        {
            continue;
        }
        if (category == nullptr || usage == nullptr || strchr(usage, ' ') != nullptr ||
            strlen(usage) >= maxToken || p->setCount == maxSets)
        {
            GenDie("a unit preference set the table cannot hold", category);
        }
        Set* set = &p->sets[p->setCount++];
        set->category = category;
        memcpy(set->usage, usage, strlen(usage));
        static Pending s_pending[maxEntries];
        int count = 0;
        for (int k = 0; k < list->childCount; k++)
        {
            if (strcmp(list->children[k]->name, "unitPreference") == 0)
            {
                count = AddPreference(set, list->children[k], s_pending, count);
            }
        }
        LayOut(p, set, s_pending, count);
        bool world = false;
        for (int r = 0; r < set->runCount; r++)
        {
            world |= set->runs[r].region == mlocPackRegion("001", 3);
        }
        if (!world)
        {
            GenDie("a unit preference set without 001", category);
        }
        GenSort(set->runs, (size_t)set->runCount, sizeof(Run), CompareRuns);
    }
    GenSort(p->sets, (size_t)p->setCount, sizeof(Set), CompareSets);
}

static void WritePreferences(const Prefs* p, DataFile* file)
{
    static Out s_out;
    s_out = (Out){0};
    int runCount = 0;
    for (int i = 0; i < p->setCount; i++)
    {
        runCount += p->sets[i].runCount;
    }
    size_t fixed = unitPrefsHeaderSize + (size_t)p->setCount * unitSetSize +
                   (size_t)runCount * unitRunSize + (size_t)p->entryCount * unitEntrySize;
    uint8_t* unit = GenAllocate(fixed + maxText);
    uint8_t* set = unit + unitPrefsHeaderSize;
    uint8_t* run = set + (size_t)p->setCount * unitSetSize;
    uint8_t* entry = run + (size_t)runCount * unitRunSize;
    int runIndex = 0;
    int entryIndex = 0;
    for (int i = 0; i < p->setCount; i++, set += unitSetSize)
    {
        const Set* s = &p->sets[i];
        GenPut(set, AddText(&s_out, s->category, s->category), 4);
        GenPut(set + 4, AddText(&s_out, s->usage, s->category), 4);
        set[8] = (uint8_t)strlen(s->category);
        set[9] = (uint8_t)strlen(s->usage);
        GenPut(set + 10, (uint64_t)runIndex, 2);
        GenPut(set + 12, (uint64_t)s->runCount, 2);
        for (int r = 0; r < s->runCount; r++, runIndex++, run += unitRunSize)
        {
            GenPut(run, s->runs[r].region, 2);
            GenPut(run + 2, (uint64_t)entryIndex, 2);
            for (int k = 0; k < s->runs[r].count; k++, entryIndex++, entry += unitEntrySize)
            {
                const Entry* e = &p->entries[s->runs[r].first + k];
                GenPut(entry, AddText(&s_out, e->unit, s->category), 4);
                entry[4] = (uint8_t)strlen(e->unit);
                entry[5] = (uint8_t)e->places;
                GenPut(entry + 6, (uint64_t)e->increment, 2);
                GenPut(entry + 8, e->threshold, 4);
            }
        }
    }
    GenPut(unit, (uint64_t)p->setCount, 2);
    GenPut(unit + 2, (uint64_t)runCount, 2);
    GenPut(unit + 4, (uint64_t)entryIndex, 2);
    GenPut(unit + 8, s_out.textSize, 4);
    memcpy(unit + fixed, s_out.text, s_out.textSize);
    DataFileAdd(file, kindUnitPreferences, nullptr, unit, fixed + s_out.textSize);
    free(unit);
}

int GenerateUnitTable(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/units.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    static Gen s_gen;
    s_gen = (Gen){0};
    Collect(&s_gen, root);
    ReadConstants(&s_gen, XmlFindChild(root, "unitConstants"));
    ReadConversions(&s_gen, XmlFindChild(root, "convertUnits"));
    for (int i = 0; i < s_gen.quantityCount; i++)
    {
        ParseDimensions(&s_gen, s_gen.quantities[i], s_gen.quantityNumerators[i],
                        s_gen.quantityDenominators[i]);
    }
    if (s_gen.prefixCount == maxPrefixes || s_gen.aliasCount == maxAliases ||
        s_gen.simpleCount == 0)
    {
        GenDie("units.xml has no conversions, or more prefixes or aliases than kept", path);
    }
    GenSort(s_gen.simples, (size_t)s_gen.simpleCount, sizeof(Simple), CompareSimples);
    GenSort(s_gen.prefixes, (size_t)s_gen.prefixCount, sizeof(XmlNode*), ComparePrefixes);
    GenSort(s_gen.aliases, (size_t)s_gen.aliasCount, sizeof(XmlNode*), CompareAliases);
    for (int i = 1; i < s_gen.simpleCount; i++)
    {
        if (strcmp(s_gen.simples[i - 1].id, s_gen.simples[i].id) == 0)
        {
            GenDie("a unit converted twice", s_gen.simples[i].id);
        }
    }
    Write(&s_gen, file);
    static Prefs s_prefs;
    s_prefs = (Prefs){0};
    ReadPreferences(&s_prefs, XmlFindChild(root, "unitPreferenceData"));
    WritePreferences(&s_prefs, file);
    XmlFree(root);
    return 1;
}
