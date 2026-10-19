// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_rules.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Parser
{
    // The rule string after the escapes are decoded, and each code
    // point's byte in the original.
    uint32_t* c;
    size_t* at;
    size_t n;
    size_t i;
    size_t length;
    CollationRules* out;
    char* error;
    size_t errorSize;
    bool failed;
    // A setting has been read: the list keeps settings in one record, so
    // an import after one could not be applied in ICU's order.
    bool settingSeen;
} Parser;

// Stops the program, as the generator does on running out of memory; its
// own so that the fuzz target needs no other generator part.
[[noreturn]] static void OutOfMemory(void)
{
    fputs("collation rules: out of memory\n", stderr);
    exit(2);
}

static void* Allocate(size_t size)
{
    void* p = calloc(1, size);
    if (p == nullptr)
    {
        OutOfMemory();
    }
    return p;
}

static void* Grow(void* items, size_t* capacity, size_t needed, size_t size)
{
    if (needed <= *capacity)
    {
        return items;
    }
    size_t grown = *capacity < 16 ? 16 : *capacity * 2;
    grown = grown < needed ? needed : grown;
    void* bigger = realloc(items, grown * size);
    if (bigger == nullptr)
    {
        OutOfMemory();
    }
    *capacity = grown;
    return bigger;
}

static bool FailAt(Parser* p, size_t byte, const char* reason)
{
    if (!p->failed)
    {
        snprintf(p->error, p->errorSize, "byte %zu: %s", byte, reason);
        p->failed = true;
    }
    return false;
}

static bool Fail(Parser* p, size_t index, const char* reason)
{
    return FailAt(p, index < p->n ? p->at[index] : p->length, reason);
}

// Pattern_White_Space.
static bool IsWhite(uint32_t c)
{
    return (c >= 0x09 && c <= 0x0D) || c == 0x20 || c == 0x85 || c == 0x200E || c == 0x200F ||
           c == 0x2028 || c == 0x2029;
}

// ASCII punctuation and symbols, ICU's isSyntaxChar.
static bool IsSyntax(uint32_t c)
{
    return c >= 0x21 && c <= 0x7E &&
           (c <= 0x2F || (c >= 0x3A && c <= 0x40) || (c >= 0x5B && c <= 0x60) || c >= 0x7B);
}

static size_t SkipWhite(const Parser* p, size_t i)
{
    while (i < p->n && IsWhite(p->c[i]))
    {
        i++;
    }
    return i;
}

static int Hex(uint32_t c)
{
    return c >= '0' && c <= '9'   ? (int)(c - '0')
           : c >= 'a' && c <= 'f' ? (int)(c - 'a' + 10)
           : c >= 'A' && c <= 'F' ? (int)(c - 'A' + 10)
                                  : -1;
}

// Reads digits hex digits from raw[k], or false.
static bool HexDigits(const uint32_t* raw, size_t n, size_t k, int digits, uint32_t* value)
{
    uint32_t v = 0;
    for (int d = 0; d < digits; d++)
    {
        int h = k + (size_t)d < n ? Hex(raw[k + (size_t)d]) : -1;
        if (h < 0)
        {
            return false;
        }
        v = v * 16 + (uint32_t)h;
    }
    *value = v;
    return true;
}

// One escape at raw[k], a backslash: its code point and length, or 0
// when it is none of \u, \U, \x and \\ (the backslash then stays).
static size_t Escape(const uint32_t* raw, size_t n, size_t k, uint32_t* value, bool* bad)
{
    uint32_t next = k + 1 < n ? raw[k + 1] : 0;
    *bad = false;
    if (next == '\\')
    {
        *value = '\\';
        return 2;
    }
    if (next == 'u' || next == 'U')
    {
        int digits = next == 'u' ? 4 : 8;
        *bad = !HexDigits(raw, n, k + 2, digits, value) || *value > 0x10FFFF;
        return 2 + (size_t)digits;
    }
    if (next != 'x')
    {
        return 0;
    }
    if (k + 2 < n && raw[k + 2] == '{')
    {
        size_t end = k + 3;
        uint32_t v = 0;
        while (end < n && Hex(raw[end]) >= 0 && end - (k + 3) < 6)
        {
            v = v * 16 + (uint32_t)Hex(raw[end++]);
        }
        *value = v;
        *bad = end == k + 3 || end >= n || raw[end] != '}' || v > 0x10FFFF;
        return end + 1 - k;
    }
    size_t end = k + 2;
    uint32_t v = 0;
    while (end < n && Hex(raw[end]) >= 0 && end - (k + 2) < 2)
    {
        v = v * 16 + (uint32_t)Hex(raw[end++]);
    }
    *value = v;
    *bad = end == k + 2;
    return end - k;
}

// The rule string as code points with the escapes decoded.
static bool Decode(Parser* p, const char* text, size_t length)
{
    uint32_t* raw = Allocate((length + 1) * sizeof(uint32_t));
    size_t* rawAt = Allocate((length + 1) * sizeof(size_t));
    size_t n = 0;
    for (size_t at = 0; at < length;)
    {
        uint32_t c = 0;
        size_t size = 0;
        if (muniDecodeUtf8(text + at, length - at, &c, &size) != muni_success)
        {
            free(raw);
            free(rawAt);
            return FailAt(p, at, "ill-formed UTF-8");
        }
        rawAt[n] = at;
        raw[n++] = c;
        at += size;
    }
    p->c = Allocate((n + 1) * sizeof(uint32_t));
    p->at = Allocate((n + 1) * sizeof(size_t));
    for (size_t k = 0; k < n;)
    {
        uint32_t value = raw[k];
        size_t used = 1;
        bool bad = false;
        if (raw[k] == '\\')
        {
            size_t escape = Escape(raw, n, k, &value, &bad);
            used = escape == 0 ? 1 : escape;
            value = escape == 0 ? '\\' : value;
        }
        if (bad)
        {
            size_t byte = rawAt[k];
            free(raw);
            free(rawAt);
            return FailAt(p, byte, "a malformed escape");
        }
        p->at[p->n] = rawAt[k];
        p->c[p->n++] = value;
        k += used;
    }
    free(raw);
    free(rawAt);
    return true;
}

static uint32_t Push(Parser* p, uint32_t c)
{
    CollationRules* r = p->out;
    r->points = Grow(r->points, &r->pointCapacity, r->pointCount + 1, sizeof(uint32_t));
    r->points[r->pointCount] = c;
    return (uint32_t)r->pointCount++;
}

// A string up to unquoted white space or syntax, ICU's parseString: its
// span of the pool, and the index after it.
static size_t String(Parser* p, size_t i, uint32_t* start, uint32_t* count)
{
    size_t begin = i;
    *start = (uint32_t)p->out->pointCount;
    while (i < p->n)
    {
        uint32_t c = p->c[i++];
        if (c == '\'')
        {
            if (i < p->n && p->c[i] == '\'')
            {
                Push(p, '\'');
                i++;
                continue;
            }
            for (;;)
            {
                if (i == p->n)
                {
                    Fail(p, i, "quoted text without its closing apostrophe");
                    return i;
                }
                c = p->c[i++];
                if (c == '\'' && !(i < p->n && p->c[i] == '\''))
                {
                    break;
                }
                i += c == '\'';
                Push(p, c);
            }
        }
        else if (c == '\\')
        {
            if (i == p->n)
            {
                Fail(p, i, "a backslash at the end of the rules");
                return i;
            }
            Push(p, p->c[i++]);
        }
        else if (IsSyntax(c) || IsWhite(c))
        {
            i--;
            break;
        }
        else
        {
            Push(p, c);
        }
    }
    *count = (uint32_t)(p->out->pointCount - *start);
    for (uint32_t k = 0; k < *count; k++)
    {
        uint32_t c = p->out->points[*start + k];
        if ((c >= 0xD800 && c <= 0xDFFF) || (c >= 0xFFFD && c <= 0xFFFF))
        {
            Fail(p, begin, "a string with a surrogate or U+FFFD to U+FFFF");
        }
    }
    return i;
}

static size_t TailoringString(Parser* p, size_t i, uint32_t* start, uint32_t* count)
{
    size_t from = SkipWhite(p, i);
    i = String(p, from, start, count);
    if (!p->failed && *count == 0)
    {
        Fail(p, from, "a missing string");
    }
    return SkipWhite(p, i);
}

static CollationRule* Add(Parser* p, uint8_t kind, uint8_t strength)
{
    CollationRules* r = p->out;
    r->rules = Grow(r->rules, &r->capacity, r->count + 1, sizeof(CollationRule));
    CollationRule* rule = &r->rules[r->count++];
    *rule = (CollationRule){.kind = kind, .strength = strength};
    return rule;
}

// Words up to syntax other than - and _, spaces folded to one, as ICU's
// readWords: ASCII into raw (another code point as 0x01, which no word
// matches); the index of the syntax, or 0 at the end.
static size_t Words(const Parser* p, size_t i, char* raw, size_t size)
{
    size_t n = 0;
    raw[0] = '\0';
    i = SkipWhite(p, i);
    for (;;)
    {
        if (i >= p->n)
        {
            return 0;
        }
        uint32_t c = p->c[i];
        if (IsSyntax(c) && c != '-' && c != '_')
        {
            n -= n > 0 && raw[n - 1] == ' ';
            raw[n] = '\0';
            return i;
        }
        char put = IsWhite(c) ? ' ' : c < 0x80 ? (char)c : (char)1;
        if (n + 1 < size)
        {
            raw[n++] = put;
        }
        i = IsWhite(c) ? SkipWhite(p, i + 1) : i + 1;
    }
}

static const char* const s_positions[] = {
    "first tertiary ignorable",
    "last tertiary ignorable",
    "first secondary ignorable",
    "last secondary ignorable",
    "first primary ignorable",
    "last primary ignorable",
    "first variable",
    "last variable",
    "first regular",
    "last regular",
    "first implicit",
    "last implicit",
    "first trailing",
    "last trailing",
};

static size_t Position(Parser* p, size_t i, uint8_t* position)
{
    char raw[64];
    size_t j = Words(p, i + 1, raw, sizeof(raw));
    if (j > i && p->c[j] == ']' && raw[0] != '\0')
    {
        for (size_t k = 0; k < sizeof(s_positions) / sizeof(s_positions[0]); k++)
        {
            if (strcmp(raw, s_positions[k]) == 0)
            {
                *position = (uint8_t)(k + 1);
                return j + 1;
            }
        }
        if (strcmp(raw, "top") == 0)
        {
            *position = rulePositionLastRegular;
            return j + 1;
        }
        if (strcmp(raw, "variable top") == 0)
        {
            *position = rulePositionLastVariable;
            return j + 1;
        }
    }
    Fail(p, i, "not a special reset position");
    return i;
}

// The reset at p->i: its strength, ruleIdentical without [before n].
static uint8_t Reset(Parser* p)
{
    size_t i = SkipWhite(p, p->i + 1);
    uint8_t strength = ruleIdentical;
    static const uint32_t before[] = {'[', 'b', 'e', 'f', 'o', 'r', 'e'};
    bool isBefore = i + 7 < p->n;
    for (size_t k = 0; isBefore && k < 7; k++)
    {
        isBefore = p->c[i + k] == before[k];
    }
    if (isBefore && IsWhite(p->c[i + 7]))
    {
        size_t j = SkipWhite(p, i + 8);
        if (j + 1 < p->n && p->c[j] >= '1' && p->c[j] <= '3' && p->c[j + 1] == ']')
        {
            strength = (uint8_t)(p->c[j] - '0');
            i = SkipWhite(p, j + 2);
        }
    }
    if (i >= p->n)
    {
        Fail(p, i, "a reset without a position");
        return strength;
    }
    CollationRule* rule = Add(p, ruleReset, strength);
    if (p->c[i] == '[')
    {
        i = Position(p, i, &rule->position);
    }
    else
    {
        i = TailoringString(p, i, &rule->text, &rule->textLength);
    }
    p->i = i;
    return strength;
}

// The relation operator at p->i: its strength, 0 for none; *starred and
// *size set.
static uint8_t Operator(Parser* p, bool* starred, size_t* size)
{
    p->i = SkipWhite(p, p->i);
    *starred = false;
    *size = 1;
    if (p->i >= p->n)
    {
        return 0;
    }
    size_t i = p->i;
    uint32_t c = p->c[i++];
    uint8_t strength = 0;
    if (c == '<')
    {
        strength = rulePrimary;
        while (strength < ruleQuaternary && i < p->n && p->c[i] == '<')
        {
            strength++;
            i++;
        }
    }
    else if (c == ';' || c == ',')
    {
        return c == ';' ? ruleSecondary : ruleTertiary;
    }
    else if (c == '=')
    {
        strength = ruleIdentical;
    }
    else
    {
        return 0;
    }
    if (i < p->n && p->c[i] == '*')
    {
        *starred = true;
        i++;
    }
    *size = i - p->i;
    return strength;
}

// Whether a code point is unchanged by NFD and starts and ends a
// segment: no decomposition and combining class 0.
static bool Inert(uint32_t c)
{
    uint32_t a = 0;
    uint32_t b = 0;
    return !muniDecomposePair(c, &a, &b) && muniGetCombiningClass(c) == 0;
}

// Whether text may break before the code point in NFC.
static bool NfcBoundary(uint32_t c)
{
    char bytes[4];
    size_t size = 0;
    muniQuickCheck answer = muni_quickCheckNo;
    return muniEncodeUtf8(c, bytes, &size) == muni_success && muniGetCombiningClass(c) == 0 &&
           muniCheckNormalization(bytes, size, muni_nfc, &answer).status == muni_success &&
           answer == muni_quickCheckYes;
}

static void Relation(Parser* p, uint8_t strength, size_t i)
{
    uint32_t start = 0;
    uint32_t count = 0;
    uint32_t prefix = 0;
    uint32_t prefixCount = 0;
    uint32_t extension = 0;
    uint32_t extensionCount = 0;
    size_t from = i;
    i = TailoringString(p, i, &start, &count);
    if (!p->failed && i < p->n && p->c[i] == '|')
    {
        prefix = start;
        prefixCount = count;
        i = TailoringString(p, i + 1, &start, &count);
    }
    if (!p->failed && i < p->n && p->c[i] == '/')
    {
        i = TailoringString(p, i + 1, &extension, &extensionCount);
    }
    if (!p->failed && prefixCount > 0 &&
        (!NfcBoundary(p->out->points[prefix]) || !NfcBoundary(p->out->points[start])))
    {
        Fail(p, from, "a prefix and its string must each start at an NFC boundary");
    }
    if (p->failed)
    {
        return;
    }
    CollationRule* rule = Add(p, ruleRelation, strength);
    rule->prefix = prefix;
    rule->prefixLength = prefixCount;
    rule->text = start;
    rule->textLength = count;
    rule->extension = extension;
    rule->extensionLength = extensionCount;
    p->i = i;
}

static bool One(Parser* p, uint8_t strength, uint32_t c, size_t at)
{
    if (!Inert(c))
    {
        return Fail(p, at, "a starred string not all NFD-inert");
    }
    CollationRule* rule = Add(p, ruleRelation, strength);
    rule->text = Push(p, c);
    rule->textLength = 1;
    return true;
}

// a<*bcd-gh: one relation a code point, ranges expanded.
static void Starred(Parser* p, uint8_t strength, size_t i)
{
    uint32_t start = 0;
    uint32_t count = 0;
    size_t from = SkipWhite(p, i);
    i = String(p, from, &start, &count);
    if (!p->failed && count == 0)
    {
        Fail(p, from, "a missing starred string");
    }
    int64_t previous = -1;
    while (!p->failed)
    {
        for (uint32_t k = 0; k < count && !p->failed; k++)
        {
            previous = p->out->points[start + k];
            One(p, strength, (uint32_t)previous, from);
        }
        if (p->failed || i >= p->n || p->c[i] != '-')
        {
            break;
        }
        if (previous < 0)
        {
            Fail(p, i, "a starred range without its start");
            return;
        }
        from = i + 1;
        i = String(p, from, &start, &count);
        if (!p->failed && count == 0)
        {
            Fail(p, from, "a starred range without its end");
            return;
        }
        uint32_t last = p->failed ? 0 : p->out->points[start];
        if (!p->failed && last < previous)
        {
            Fail(p, from, "a starred range whose start is past its end");
            return;
        }
        for (uint32_t c = (uint32_t)previous + 1; !p->failed && c <= last; c++)
        {
            if ((c >= 0xD800 && c <= 0xDFFF) || (c >= 0xFFFD && c <= 0xFFFF))
            {
                Fail(p, from, "a starred range over a surrogate or U+FFFD to U+FFFF");
                return;
            }
            One(p, strength, c, from);
        }
        previous = -1;
        start++;
        count--;
    }
    p->i = SkipWhite(p, i);
}

static size_t Comment(const Parser* p, size_t i)
{
    while (i < p->n)
    {
        uint32_t c = p->c[i++];
        if (c == 0x0A || c == 0x0C || c == 0x0D || c == 0x85 || c == 0x2028 || c == 0x2029)
        {
            break;
        }
    }
    return i;
}

static void Chain(Parser* p)
{
    uint8_t reset = Reset(p);
    bool first = true;
    while (!p->failed)
    {
        bool starred = false;
        size_t size = 0;
        uint8_t strength = Operator(p, &starred, &size);
        if (strength == 0)
        {
            if (p->i < p->n && p->c[p->i] == '#')
            {
                p->i = Comment(p, p->i + 1);
                continue;
            }
            if (first)
            {
                Fail(p, p->i, "a reset not followed by a relation");
            }
            return;
        }
        if (reset < ruleIdentical && first && strength != reset)
        {
            Fail(p, p->i, "a [before n] reset whose first relation has another strength");
            return;
        }
        if (reset < ruleIdentical && !first && strength < reset)
        {
            Fail(p, p->i, "a [before n] reset followed by a stronger relation");
            return;
        }
        if (starred)
        {
            Starred(p, strength, p->i + size);
        }
        else
        {
            Relation(p, strength, p->i + size);
        }
        first = false;
    }
}

static bool IsSetSyntax(uint32_t c)
{
    return c == '[' || c == '^' || c == ':' || c == '{' || c == '}' || c == '&' || c == '$';
}

// A set of CLDR's form at p->c[i], '[': code points and ranges; the
// index after its ']' and the option's closing ']'.
static size_t Set(Parser* p, size_t i, uint32_t** ranges, size_t* rangeCount)
{
    size_t capacity = *rangeCount * 2;
    size_t j = i + 1;
    for (;;)
    {
        j = SkipWhite(p, j);
        if (j >= p->n)
        {
            Fail(p, j, "a set without its closing bracket");
            return j;
        }
        uint32_t c = p->c[j];
        if (c == ']')
        {
            break;
        }
        if (IsSetSyntax(c))
        {
            Fail(p, j, "a set form other than code points and ranges");
            return j;
        }
        if (c == '\\' && j + 1 < p->n)
        {
            c = p->c[++j];
        }
        uint32_t last = c;
        size_t k = SkipWhite(p, j + 1);
        size_t m = k + 1 < p->n && p->c[k] == '-' ? SkipWhite(p, k + 1) : p->n;
        if (m < p->n && p->c[m] != ']')
        {
            k = m;
            last = p->c[k] == '\\' && k + 1 < p->n ? p->c[++k] : p->c[k];
            if (last < c)
            {
                Fail(p, k, "a set range whose start is past its end");
                return k;
            }
            k++;
        }
        *ranges = Grow(*ranges, &capacity, *rangeCount * 2 + 2, sizeof(uint32_t));
        (*ranges)[*rangeCount * 2] = c;
        (*ranges)[*rangeCount * 2 + 1] = last;
        (*rangeCount)++;
        j = k;
    }
    j = SkipWhite(p, j + 1);
    if (j >= p->n || p->c[j] != ']')
    {
        Fail(p, j, "a set option without its closing bracket");
        return j;
    }
    return j + 1;
}

static uint8_t OnOff(const char* value)
{
    return strcmp(value, "on") == 0 ? 1 : strcmp(value, "off") == 0 ? 0 : ruleUnset;
}

// The value of a setting from its words, or ruleUnset.
static uint8_t Value(const char* name, const char* value, uint8_t** field, CollationRules* r)
{
    static const char* const alternates[] = {"non-ignorable", "shifted"};
    static const char* const variables[] = {"space", "punct", "symbol", "currency"};
    static const char* const cases[] = {"off", "lower", "upper"};
    const char* const* names = nullptr;
    size_t count = 0;
    *field = nullptr;
    if (strcmp(name, "strength") == 0)
    {
        *field = &r->strength;
        return value[0] >= '1' && value[0] <= '4' && value[1] == '\0' ? (uint8_t)(value[0] - '0')
               : strcmp(value, "I") == 0                              ? ruleIdentical
                                                                      : ruleUnset;
    }
    if (strcmp(name, "alternate") == 0)
    {
        *field = &r->alternate;
        names = alternates;
        count = 2;
    }
    else if (strcmp(name, "maxVariable") == 0)
    {
        *field = &r->maxVariable;
        names = variables;
        count = 4;
    }
    else if (strcmp(name, "caseFirst") == 0)
    {
        *field = &r->caseFirst;
        names = cases;
        count = 3;
    }
    for (size_t k = 0; k < count; k++)
    {
        if (strcmp(value, names[k]) == 0)
        {
            return (uint8_t)k;
        }
    }
    if (count > 0)
    {
        return ruleUnset;
    }
    *field = strcmp(name, "caseLevel") == 0         ? &r->caseLevel
             : strcmp(name, "normalization") == 0   ? &r->normalization
             : strcmp(name, "numericOrdering") == 0 ? &r->numeric
                                                    : nullptr;
    return OnOff(value);
}

// Whether n bytes equal an ASCII word whatever their case.
static bool SameWord(const char* text, size_t n, const char* word)
{
    if (strlen(word) != n)
    {
        return false;
    }
    for (size_t k = 0; k < n; k++)
    {
        if ((text[k] | 0x20) != word[k])
        {
            return false;
        }
    }
    return true;
}

static bool Reorder(Parser* p, const char* words, size_t at)
{
    static const char* const groups[] = {"space", "punct", "symbol", "currency", "digit"};
    CollationRules* r = p->out;
    size_t capacity = r->reorderCount;
    r->reorderGiven = true;
    for (const char* w = words; *w != '\0';)
    {
        w += *w == ' ';
        const char* end = strchr(w, ' ');
        size_t n = end == nullptr ? strlen(w) : (size_t)(end - w);
        uint32_t code = 0;
        for (size_t k = 0; k < 5 && code == 0; k++)
        {
            code = SameWord(w, n, groups[k]) ? ruleReorderSpace + (uint32_t)k : 0;
        }
        if (code == 0 && SameWord(w, n, "others"))
        {
            code = MUNI_SCRIPT('Z', 'z', 'z', 'z');
        }
        bool letters = n == 4;
        for (size_t k = 0; letters && k < 4; k++)
        {
            char c = (char)(w[k] | 0x20);
            letters = c >= 'a' && c <= 'z';
        }
        if (code == 0 && letters)
        {
            code = MUNI_SCRIPT(w[0] & ~0x20, w[1] | 0x20, w[2] | 0x20, w[3] | 0x20);
        }
        if (code == 0)
        {
            return Fail(p, at, "an unknown script or reorder code");
        }
        r->reorder = Grow(r->reorder, &capacity, r->reorderCount + 1, sizeof(uint32_t));
        r->reorder[r->reorderCount++] = code;
        w += n;
    }
    return true;
}

static bool Import(Parser* p, const char* tag, size_t at)
{
    CollationRules* r = p->out;
    size_t n = strlen(tag);
    bool ok = n > 0;
    for (size_t k = 0; ok && k < n; k++)
    {
        char c = tag[k];
        ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
             c == '-' || c == '_';
    }
    if (!ok)
    {
        return Fail(p, at, "an import without a language tag");
    }
    if (p->settingSeen)
    {
        return Fail(p, at, "an import after a setting, whose order the list cannot keep");
    }
    size_t capacity = r->importsLength;
    r->imports = Grow(r->imports, &capacity, r->importsLength + n + 1, 1);
    CollationRule* rule = Add(p, ruleImport, 0);
    rule->text = (uint32_t)r->importsLength;
    rule->textLength = (uint32_t)n;
    memcpy(r->imports + r->importsLength, tag, n + 1);
    r->importsLength += n + 1;
    r->importCount++;
    return true;
}

static void Setting(Parser* p)
{
    char raw[1024];
    size_t i = p->i + 1;
    size_t j = Words(p, i, raw, sizeof(raw));
    if (j <= i || raw[0] == '\0')
    {
        Fail(p, p->i, "a setting expected at '['");
        return;
    }
    CollationRules* r = p->out;
    if (p->c[j] == '[')
    {
        bool suppress = strcmp(raw, "suppressContractions") == 0;
        if (!suppress && strcmp(raw, "optimize") != 0)
        {
            Fail(p, p->i, "not a setting");
            return;
        }
        p->i = suppress ? Set(p, j, &r->suppress, &r->suppressCount)
                        : Set(p, j, &r->optimize, &r->optimizeCount);
        return;
    }
    size_t at = p->i;
    if (p->c[j] != ']')
    {
        Fail(p, at, "not a setting");
        return;
    }
    p->i = j + 1;
    bool isImport = strncmp(raw, "import ", 7) == 0;
    p->settingSeen = p->settingSeen || !isImport;
    if (strncmp(raw, "reorder", 7) == 0 && (raw[7] == '\0' || raw[7] == ' '))
    {
        r->reorderCount = 0;
        Reorder(p, raw + 7, at);
        return;
    }
    if (strcmp(raw, "backwards 2") == 0)
    {
        r->backwards = 1;
        return;
    }
    char* space = strrchr(raw, ' ');
    const char* value = space == nullptr ? "" : space + 1;
    if (space != nullptr)
    {
        *space = '\0';
    }
    if (strcmp(raw, "import") == 0)
    {
        Import(p, value, at);
        return;
    }
    if (strcmp(raw, "hiraganaQ") == 0)
    {
        // [hiraganaQ off] is accepted and changes nothing, as in ICU.
        uint8_t v = OnOff(value);
        if (v != 0)
        {
            Fail(p, at, v == 1 ? "[hiraganaQ on] is not supported" : "not a setting");
        }
        return;
    }
    uint8_t* field = nullptr;
    uint8_t v = Value(raw, value, &field, r);
    if (field == nullptr || v == ruleUnset)
    {
        Fail(p, at, "not a setting");
        return;
    }
    *field = v;
}

int CollationRulesParse(const char* text, size_t length, CollationRules* out, char* error,
                        size_t errorSize)
{
    *out = (CollationRules){
        .strength = ruleUnset,
        .alternate = ruleUnset,
        .maxVariable = ruleUnset,
        .caseFirst = ruleUnset,
        .caseLevel = ruleUnset,
        .normalization = ruleUnset,
        .numeric = ruleUnset,
        .backwards = ruleUnset,
    };
    Parser p = {.out = out, .error = error, .errorSize = errorSize, .length = length};
    if (Decode(&p, text, length))
    {
        while (!p.failed && p.i < p.n)
        {
            uint32_t c = p.c[p.i];
            if (IsWhite(c) || c == '!')
            {
                // '!' once turned on Thai and Lao reversal; ICU accepts and
                // ignores it, as the root's contractions do the work.
                p.i++;
            }
            else if (c == '&')
            {
                Chain(&p);
            }
            else if (c == '[')
            {
                Setting(&p);
            }
            else if (c == '#')
            {
                p.i = Comment(&p, p.i + 1);
            }
            else if (c == '@')
            {
                p.settingSeen = true;
                out->backwards = 1;
                p.i++;
            }
            else
            {
                Fail(&p, p.i, "a reset, a setting or a comment expected");
            }
        }
    }
    free(p.c);
    free(p.at);
    if (p.failed)
    {
        CollationRulesFree(out);
        return 0;
    }
    return 1;
}

void CollationRulesFree(CollationRules* rules)
{
    free(rules->rules);
    free(rules->points);
    free(rules->reorder);
    free(rules->imports);
    free(rules->suppress);
    free(rules->optimize);
    *rules = (CollationRules){0};
}

typedef struct Text
{
    char* bytes;
    size_t length;
    size_t capacity;
} Text;

static void Put(Text* t, const char* bytes, size_t n)
{
    t->bytes = Grow(t->bytes, &t->capacity, t->length + n + 1, 1);
    memcpy(t->bytes + t->length, bytes, n);
    t->length += n;
    t->bytes[t->length] = '\0';
}

static void PutText(Text* t, const char* text)
{
    Put(t, text, strlen(text));
}

// A code point as the parser reads it back, in a string or a set: raw
// UTF-8, syntax and white space after a backslash, a backslash as four,
// which the escape pass makes two and the parser one, and a surrogate
// (a set may hold one) as \uhhhh, which UTF-8 cannot carry.
static void PutPoint(Text* t, uint32_t c)
{
    char bytes[16];
    size_t size = 0;
    if (c == '\\')
    {
        PutText(t, "\\\\\\\\");
        return;
    }
    if (IsSyntax(c) || IsWhite(c))
    {
        PutText(t, "\\");
    }
    if (muniEncodeUtf8(c, bytes, &size) == muni_success)
    {
        Put(t, bytes, size);
        return;
    }
    snprintf(bytes, sizeof(bytes), "\\u%04X", (unsigned)c);
    PutText(t, bytes);
}

static void PutString(Text* t, const CollationRules* r, uint32_t start, uint32_t count)
{
    for (uint32_t k = 0; k < count; k++)
    {
        PutPoint(t, r->points[start + k]);
    }
}

static void PutRanges(Text* t, const char* name, const uint32_t* ranges, size_t count)
{
    if (count == 0)
    {
        return;
    }
    PutText(t, "[");
    PutText(t, name);
    PutText(t, " [");
    for (size_t k = 0; k < count; k++)
    {
        PutPoint(t, ranges[2 * k]);
        if (ranges[2 * k + 1] != ranges[2 * k])
        {
            PutText(t, "-");
            PutPoint(t, ranges[2 * k + 1]);
        }
        PutText(t, " ");
    }
    PutText(t, "]]\n");
}

static void PutSettings(Text* t, const CollationRules* r)
{
    static const char* const strengths[] = {"", "1", "2", "3", "4", "I"};
    static const char* const alternates[] = {"non-ignorable", "shifted"};
    static const char* const variables[] = {"space", "punct", "symbol", "currency"};
    static const char* const cases[] = {"off", "lower", "upper"};
    static const char* const onOff[] = {"off", "on"};
    char line[96];
    const struct
    {
        uint8_t value;
        const char* name;
        const char* const* values;
    } settings[] = {
        {r->strength, "strength", strengths},       {r->alternate, "alternate", alternates},
        {r->maxVariable, "maxVariable", variables}, {r->caseFirst, "caseFirst", cases},
        {r->caseLevel, "caseLevel", onOff},         {r->normalization, "normalization", onOff},
        {r->numeric, "numericOrdering", onOff},
    };
    for (size_t k = 0; k < sizeof(settings) / sizeof(settings[0]); k++)
    {
        if (settings[k].value != ruleUnset)
        {
            snprintf(line, sizeof(line), "[%s %s]\n", settings[k].name,
                     settings[k].values[settings[k].value]);
            PutText(t, line);
        }
    }
    if (r->backwards == 1)
    {
        PutText(t, "[backwards 2]\n");
    }
    if (r->reorderGiven)
    {
        static const char* const groups[] = {"space", "punct", "symbol", "currency", "digit"};
        PutText(t, "[reorder");
        for (size_t k = 0; k < r->reorderCount; k++)
        {
            uint32_t code = r->reorder[k];
            char word[5] = {(char)(code >> 24), (char)(code >> 16), (char)(code >> 8), (char)code,
                            '\0'};
            PutText(t, " ");
            if (code == MUNI_SCRIPT('Z', 'z', 'z', 'z'))
            {
                PutText(t, "others");
            }
            else if (code >= ruleReorderSpace && code <= ruleReorderDigit)
            {
                PutText(t, groups[code - ruleReorderSpace]);
            }
            else
            {
                PutText(t, word);
            }
        }
        PutText(t, "]\n");
    }
    PutRanges(t, "suppressContractions", r->suppress, r->suppressCount);
    PutRanges(t, "optimize", r->optimize, r->optimizeCount);
}

char* CollationRulesWrite(const CollationRules* r, size_t* lengthOut)
{
    static const char* const operators[] = {"", "<", "<<", "<<<", "<<<<", "="};
    Text t = {0};
    PutText(&t, "");
    // The rules first, imports in their place; then the settings, which
    // an import may not follow.
    for (size_t k = 0; k < r->count; k++)
    {
        const CollationRule* rule = &r->rules[k];
        if (rule->kind == ruleImport)
        {
            PutText(&t, "[import ");
            Put(&t, r->imports + rule->text, rule->textLength);
            PutText(&t, "]\n");
            continue;
        }
        if (rule->kind == ruleReset)
        {
            char before[16];
            snprintf(before, sizeof(before), "[before %u]", (unsigned)rule->strength);
            PutText(&t, "\n&");
            PutText(&t, rule->strength < ruleIdentical ? before : "");
            if (rule->position != 0)
            {
                PutText(&t, "[");
                PutText(&t, s_positions[rule->position - 1]);
                PutText(&t, "]");
            }
            PutString(&t, r, rule->text, rule->textLength);
            continue;
        }
        PutText(&t, operators[rule->strength]);
        if (rule->prefixLength > 0)
        {
            PutString(&t, r, rule->prefix, rule->prefixLength);
            PutText(&t, "|");
        }
        PutString(&t, r, rule->text, rule->textLength);
        if (rule->extensionLength > 0)
        {
            PutText(&t, "/");
            PutString(&t, r, rule->extension, rule->extensionLength);
        }
    }
    PutText(&t, "\n");
    PutSettings(&t, r);
    *lengthOut = t.length;
    return t.bytes;
}

static bool SameSpan(const CollationRules* a, uint32_t as, uint32_t al, const CollationRules* b,
                     uint32_t bs, uint32_t bl)
{
    return al == bl && (al == 0 || memcmp(a->points + as, b->points + bs, al * 4) == 0);
}

bool CollationRulesSame(const CollationRules* a, const CollationRules* b)
{
    bool same = a->count == b->count && a->strength == b->strength &&
                a->alternate == b->alternate && a->maxVariable == b->maxVariable &&
                a->caseFirst == b->caseFirst && a->caseLevel == b->caseLevel &&
                a->normalization == b->normalization && a->numeric == b->numeric &&
                a->backwards == b->backwards && a->reorderGiven == b->reorderGiven &&
                a->reorderCount == b->reorderCount && a->suppressCount == b->suppressCount &&
                a->optimizeCount == b->optimizeCount && a->importCount == b->importCount;
    same =
        same && (a->reorderCount == 0 || memcmp(a->reorder, b->reorder, a->reorderCount * 4) == 0);
    same = same &&
           (a->suppressCount == 0 || memcmp(a->suppress, b->suppress, a->suppressCount * 8) == 0);
    same = same &&
           (a->optimizeCount == 0 || memcmp(a->optimize, b->optimize, a->optimizeCount * 8) == 0);
    for (size_t k = 0; same && k < a->count; k++)
    {
        const CollationRule* x = &a->rules[k];
        const CollationRule* y = &b->rules[k];
        same = x->kind == y->kind && x->strength == y->strength && x->position == y->position;
        if (same && x->kind == ruleImport)
        {
            same = strcmp(a->imports + x->text, b->imports + y->text) == 0;
            continue;
        }
        same = same && SameSpan(a, x->prefix, x->prefixLength, b, y->prefix, y->prefixLength) &&
               SameSpan(a, x->text, x->textLength, b, y->text, y->textLength) &&
               SameSpan(a, x->extension, x->extensionLength, b, y->extension, y->extensionLength);
    }
    return same;
}
