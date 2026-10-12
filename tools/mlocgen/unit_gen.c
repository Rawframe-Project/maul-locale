// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit table (unit_gen.h). An expression is numbers and earlier
// constants joined by * and /, where * binds tighter (UTS #35 Part 6:
// a * b / c * d is (a * b) / (c * d)); a number is an exact decimal.

#include "unit_gen.h"

#include "kinds.h"
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
    int8_t dimensions[maxBases][unitMaxDimensions];
    int baseCount;
    char atoms[unitMaxDimensions][maxToken];
    int atomCount;
    const char* quantities[maxBases];
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
        mlocWideDivide(&f->n, &a, &f->n, nullptr);
        mlocWideDivide(&f->d, &a, &f->d, nullptr);
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

// The index of a base unit, its dimensions read from its identifier when
// new: atoms with powers, "per" negating those after it.
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
    int sign = 1;
    int power = 1;
    for (const char* t = id; *t != '\0';)
    {
        size_t n = strcspn(t, "-");
        int p = PowerOf(t, n);
        if (n == 3 && memcmp(t, "per", 3) == 0)
        {
            sign = -1;
        }
        else if (p != 0)
        {
            power = p;
        }
        else
        {
            int atom = AtomIndex(g, t, n);
            g->dimensions[index][atom] = (int8_t)(g->dimensions[index][atom] + sign * power);
            power = 1;
        }
        t += n + (t[n] == '-');
    }
    return index;
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
            g->quantities[g->quantityCount++] = XmlGetAttribute(c, "baseUnit");
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
        memcpy(at + 8, g->dimensions[i], unitMaxDimensions);
    }
}

static void Write(Gen* g, DataFile* file)
{
    static Out s_out;
    s_out = (Out){0};
    uint32_t beaufortCount = sizeof(s_beaufort) / sizeof(s_beaufort[0]);
    size_t fixed = unitsHeaderSize + (size_t)g->simpleCount * unitSimpleSize +
                   (size_t)g->prefixCount * unitPrefixSize + (size_t)g->aliasCount * unitAliasSize +
                   (size_t)g->baseCount * unitBaseSize + (size_t)beaufortCount * 2;
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
    GenPut(records + 12, s_out.textSize, 4);
    GenPut(records + 16, s_out.numbersSize, 4);
    size_t size = fixed + s_out.textSize + s_out.numbersSize;
    uint8_t* unit = GenAllocate(size);
    memcpy(unit, records, fixed);
    memcpy(unit + fixed, s_out.text, s_out.textSize);
    memcpy(unit + fixed + s_out.textSize, s_out.numbers, s_out.numbersSize);
    DataFileAdd(file, kindUnits, nullptr, unit, size);
    free(records);
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
    if (s_gen.prefixCount == maxPrefixes || s_gen.aliasCount == maxAliases ||
        s_gen.simpleCount == 0)
    {
        GenDie("units.xml has no conversions, or more prefixes or aliases than kept", path);
    }
    qsort(s_gen.simples, (size_t)s_gen.simpleCount, sizeof(Simple), CompareSimples);
    qsort(s_gen.prefixes, (size_t)s_gen.prefixCount, sizeof(XmlNode*), ComparePrefixes);
    qsort(s_gen.aliases, (size_t)s_gen.aliasCount, sizeof(XmlNode*), CompareAliases);
    for (int i = 1; i < s_gen.simpleCount; i++)
    {
        if (strcmp(s_gen.simples[i - 1].id, s_gen.simples[i].id) == 0)
        {
            GenDie("a unit converted twice", s_gen.simples[i].id);
        }
    }
    Write(&s_gen, file);
    XmlFree(root);
    return 1;
}
