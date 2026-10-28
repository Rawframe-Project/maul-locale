// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Parsing unit identifiers (unit_id.h). The identifier is split at its
// hyphens; a simple unit is the longest run of tokens the table names
// (no two simple units overlap, UTS #35 says), else a prefix on the
// first token and such a run after it.

#include "unit_id.h"

#include "bytes.h"
#include "currency_tables.h"
#include "unit_tables.h"

#include <string.h>

enum
{
    maxTokens = unitMaxIdLength / 2 + 1,
    // The longest constant, in characters (UTS #35: under 9).
    maxConstantLength = 8,
};

typedef struct Token
{
    uint16_t start;
    uint16_t length;
} Token;

typedef struct Parser
{
    const UnitTable* table;
    const char* text;
    Token tokens[maxTokens];
    uint32_t count;
    UnitId id;
} Parser;

static bool Is(const Parser* p, uint32_t i, const char* word)
{
    size_t n = strlen(word);
    return p->tokens[i].length == n && memcmp(p->text + p->tokens[i].start, word, n) == 0;
}

// Splits the text into tokens of lowercase letters and digits.
static bool Tokenize(Parser* p, size_t length)
{
    p->count = 0;
    size_t start = 0;
    for (size_t i = 0; i <= length; i++)
    {
        if (i < length && p->text[i] != '-')
        {
            char c = p->text[i];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
            {
                return false;
            }
            continue;
        }
        if (i == start)
        {
            return false;
        }
        p->tokens[p->count++] = (Token){(uint16_t)start, (uint16_t)(i - start)};
        start = i + 1;
    }
    return true;
}

// The bytes from token i's start to token j - 1's end.
static size_t SpanLength(const Parser* p, uint32_t i, uint32_t j)
{
    return (size_t)p->tokens[j - 1].start + p->tokens[j - 1].length - p->tokens[i].start;
}

// The power a dimensionality token gives, 2 to 15, or 0.
static int Power(const Parser* p, uint32_t i)
{
    if (Is(p, i, "square"))
    {
        return 2;
    }
    if (Is(p, i, "cubic"))
    {
        return 3;
    }
    const char* s = p->text + p->tokens[i].start;
    uint32_t n = p->tokens[i].length;
    if (n < 4 || n > 5 || memcmp(s, "pow", 3) != 0 || s[3] < '1' || s[3] > '9')
    {
        return 0;
    }
    int power =
        n == 4 ? s[3] - '0' : (s[4] >= '0' && s[4] <= '9' ? 10 * (s[3] - '0') + s[4] - '0' : 0);
    return power >= 2 && power <= 15 ? power : 0;
}

static bool Times(uint64_t* value, uint64_t factor)
{
    if (factor != 0 && *value > UINT64_MAX / factor)
    {
        return false;
    }
    *value *= factor;
    return true;
}

// Reads a unit constant ("100", "1e6"): mloc_success, mloc_errorSyntax
// when the token is not one, mloc_errorLimit when it passes 64 bits.
static mlocResult Constant(const Parser* p, uint32_t i, uint64_t* valueOut)
{
    const char* s = p->text + p->tokens[i].start;
    uint32_t n = p->tokens[i].length;
    uint64_t mantissa = 0;
    uint32_t k = 0;
    for (; k < n && s[k] >= '0' && s[k] <= '9'; k++)
    {
        mantissa = mantissa * 10 + (uint64_t)(s[k] - '0');
    }
    uint32_t exponent = 0;
    bool hasExponent = k < n && s[k] == 'e' && k + 1 < n && s[k + 1] >= '1' && s[k + 1] <= '9';
    if (hasExponent)
    {
        for (k++; k < n && s[k] >= '0' && s[k] <= '9'; k++)
        {
            exponent = exponent * 10 + (uint32_t)(s[k] - '0');
        }
    }
    if (k != n || n > maxConstantLength || s[0] == '0' || (mantissa < 2 && exponent == 0))
    {
        return mloc_errorSyntax;
    }
    for (uint32_t e = 0; e < exponent; e++)
    {
        if (!Times(&mantissa, 10))
        {
            return mloc_errorLimit;
        }
    }
    *valueOut = mantissa;
    return mloc_success;
}

// The table's simple unit named by a text, directly or by an alias that
// names one; -1 for none.
static int32_t FindSimple(const UnitTable* t, const char* text, size_t length)
{
    int32_t found = mlocFindSimpleUnit(t, text, length);
    const char* replacement = nullptr;
    size_t n = found < 0 ? mlocFindUnitAlias(t, text, length, &replacement) : 0;
    return n > 0 ? mlocFindSimpleUnit(t, replacement, n) : found;
}

// Matches a simple unit at token i before token end: the tokens it
// takes, 0 for none.
static uint32_t MatchSimple(const Parser* p, uint32_t i, uint32_t end, UnitPart* part)
{
    const char* start = p->text + p->tokens[i].start;
    for (uint32_t j = end; j > i; j--)
    {
        int32_t s = FindSimple(p->table, start, SpanLength(p, i, j));
        if (s >= 0)
        {
            *part = (UnitPart){.simple = (uint16_t)s, .prefix = -1};
            return j - i;
        }
    }
    int32_t prefix = mlocFindUnitPrefix(p->table, start, p->tokens[i].length);
    if (prefix < 0)
    {
        return 0;
    }
    size_t skip = mlocUnitPrefix(p->table, (uint32_t)prefix)[4];
    for (uint32_t j = end; j > i; j--)
    {
        int32_t s = FindSimple(p->table, start + skip, SpanLength(p, i, j) - skip);
        if (s >= 0)
        {
            *part = (UnitPart){.simple = (uint16_t)s, .prefix = (int8_t)prefix};
            return j - i;
        }
    }
    return 0;
}

// The first token at or after i that is "per" or "and", or the count.
static uint32_t RunEnd(const Parser* p, uint32_t i)
{
    while (i < p->count && !Is(p, i, "per") && !Is(p, i, "and"))
    {
        i++;
    }
    return i;
}

static mlocResult AddPart(Parser* p, UnitPart part)
{
    if (p->id.count == unitMaxFactors)
    {
        return mloc_errorLimit;
    }
    p->id.parts[p->id.count++] = part;
    return mloc_success;
}

// Reads a single unit at *at (a power, then a simple or currency unit),
// advancing past it.
static mlocResult SingleUnit(Parser* p, uint32_t* at, bool denominator, bool allowCurrency)
{
    uint32_t i = *at;
    int power = Power(p, i);
    if (power != 0 && ++i == p->count)
    {
        return mloc_errorSyntax;
    }
    power = power == 0 ? 1 : power;
    UnitPart part = {0};
    if (Is(p, i, "xxx") || Is(p, i, "x"))
    {
        return mloc_errorNotFound;
    }
    if (Is(p, i, "curr") && allowCurrency)
    {
        uint16_t code = i + 1 < p->count && p->tokens[i + 1].length == 3
                            ? mlocPackCurrency(p->text + p->tokens[i + 1].start, 3)
                            : 0;
        if (code == 0)
        {
            return mloc_errorSyntax;
        }
        part = (UnitPart){.simple = code, .prefix = -1, .currency = 1};
        i += 2;
    }
    else
    {
        char c = p->text[p->tokens[i].start];
        uint32_t taken = c >= 'a' && c <= 'z' ? MatchSimple(p, i, RunEnd(p, i), &part) : 0;
        if (taken == 0)
        {
            return c >= 'a' && c <= 'z' && p->tokens[i].length >= 3 ? mloc_errorNotFound
                                                                    : mloc_errorSyntax;
        }
        i += taken;
    }
    part.power = (int8_t)(denominator ? -power : power);
    *at = i;
    return AddPart(p, part);
}

static mlocResult ParseCore(Parser* p)
{
    bool denominator = false;
    uint32_t i = 0;
    while (i < p->count)
    {
        if (Is(p, i, "per"))
        {
            i++;
            if (i == p->count || Is(p, i, "per"))
            {
                return mloc_errorSyntax;
            }
            denominator = true;
            continue;
        }
        char c = p->text[p->tokens[i].start];
        mlocResult r;
        if (c >= '1' && c <= '9')
        {
            uint64_t value = 0;
            r = Constant(p, i, &value);
            uint64_t* into = denominator ? &p->id.denominatorConstant : &p->id.numeratorConstant;
            if (r == mloc_success && !Times(into, value))
            {
                r = mloc_errorLimit;
            }
            i++;
        }
        else
        {
            r = SingleUnit(p, &i, denominator, true);
        }
        if (r != mloc_success)
        {
            return r;
        }
    }
    return p->id.count == 0 && p->id.numeratorConstant == 1 && p->id.denominatorConstant == 1
               ? mloc_errorSyntax
               : mloc_success;
}

static mlocResult ParseMixed(Parser* p)
{
    p->id.mixed = 1;
    uint32_t i = 0;
    while (true)
    {
        if (p->id.count == unitMaxParts)
        {
            return mloc_errorLimit;
        }
        uint32_t end = RunEnd(p, i);
        if (end == i || (end < p->count && !Is(p, end, "and")))
        {
            return mloc_errorSyntax;
        }
        mlocResult r = SingleUnit(p, &i, false, false);
        if (r != mloc_success)
        {
            return r;
        }
        if (i != end)
        {
            return mloc_errorSyntax;
        }
        if (end == p->count)
        {
            return mloc_success;
        }
        i = end + 1;
        if (i == p->count)
        {
            return mloc_errorSyntax;
        }
    }
}

mlocResult mlocParseUnitId(const UnitTable* t, const char* text, size_t length, UnitId* out)
{
    if (length == 0 || length > unitMaxIdLength)
    {
        return length == 0 ? mloc_errorSyntax : mloc_errorLimit;
    }
    Parser p = {.table = t, .text = text};
    const char* replacement = nullptr;
    size_t n = mlocFindUnitAlias(t, text, length, &replacement);
    if (n > 0)
    {
        p.text = replacement;
        length = n;
    }
    if (!Tokenize(&p, length))
    {
        return mloc_errorSyntax;
    }
    p.id = (UnitId){.numeratorConstant = 1, .denominatorConstant = 1};
    bool mixed = false;
    for (uint32_t i = 0; i < p.count; i++)
    {
        mixed |= Is(&p, i, "and");
    }
    mlocResult r = mixed ? ParseMixed(&p) : ParseCore(&p);
    if (r == mloc_success)
    {
        *out = p.id;
    }
    return r;
}
