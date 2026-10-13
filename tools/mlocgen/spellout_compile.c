// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The rule compiler (spellout_compile.h). Each step follows the ICU4C
// 79 function named in its comment; positions are bytes of UTF-8 where
// ICU's are UTF-16 units, which only the descriptor's length reads.

#include "spellout_compile.h"

#include "table_gen.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Compiler
{
    RbnfGroup* group;
    char* error;
    size_t capacity;
    bool failed;
    // The rule set being read.
    int current;
} Compiler;

static void Fail(Compiler* c, const char* format, ...)
{
    if (c->failed)
    {
        return;
    }
    c->failed = true;
    va_list args;
    va_start(args, format);
    vsnprintf(c->error, c->capacity, format, args);
    va_end(args);
}

static char* CopyRange(const char* text, size_t length)
{
    char* copy = GenAllocate(length + 1);
    memcpy(copy, text, length);
    return copy;
}

// The width of a Pattern_White_Space character at text, or 0: U+0009 to
// U+000D, U+0020, U+0085, U+200E, U+200F, U+2028 and U+2029.
static size_t SpaceWidth(const char* text, size_t length)
{
    const unsigned char* s = (const unsigned char*)text;
    if (length >= 1 && ((s[0] >= 0x09 && s[0] <= 0x0D) || s[0] == 0x20))
    {
        return 1;
    }
    if (length >= 2 && s[0] == 0xC2 && s[1] == 0x85)
    {
        return 2;
    }
    if (length >= 3 && s[0] == 0xE2 && s[1] == 0x80 &&
        (s[2] == 0x8E || s[2] == 0x8F || s[2] == 0xA8 || s[2] == 0xA9))
    {
        return 3;
    }
    return 0;
}

static long IndexOf(const char* text, size_t length, char ch, size_t from)
{
    for (size_t i = from; i < length; i++)
    {
        if (text[i] == ch)
        {
            return (long)i;
        }
    }
    return -1;
}

static long IndexOfText(const char* text, size_t length, const char* key, size_t from)
{
    size_t n = strlen(key);
    for (size_t i = from; i + n <= length; i++)
    {
        if (memcmp(text + i, key, n) == 0)
        {
            return (long)i;
        }
    }
    return -1;
}

// RuleBasedNumberFormat::stripWhitespace: the whitespace and semicolons
// after each semicolon, and at the start, are removed.
static char* StripWhitespace(const char* text, size_t length, size_t* lengthOut)
{
    char* out = GenAllocate(length + 1);
    size_t n = 0;
    size_t at = 0;
    while (at < length)
    {
        for (;;)
        {
            size_t w = SpaceWidth(text + at, length - at);
            if (w == 0 && at < length && text[at] == ';')
            {
                w = 1;
            }
            if (w == 0)
            {
                break;
            }
            at += w;
        }
        long p = IndexOf(text, length, ';', at);
        size_t end = p < 0 ? length : (size_t)p + 1;
        memcpy(out + n, text + at, end - at);
        n += end - at;
        at = end;
    }
    *lengthOut = n;
    return out;
}

static uint64_t Power(uint32_t base, uint16_t exponent)
{
    if (base == 0)
    {
        return 0;
    }
    uint64_t result = 1;
    uint64_t power = base;
    for (;;)
    {
        if ((exponent & 1) == 1)
        {
            result *= power;
        }
        exponent >>= 1;
        if (exponent == 0)
        {
            break;
        }
        power *= power;
    }
    return result;
}

uint64_t RbnfDivisor(const RbnfRule* rule)
{
    return Power((uint32_t)rule->radix, (uint16_t)rule->exponent);
}

// NFRule::expectedExponent, with ICU's logarithms; the exact exponent is
// checked to agree.
static int16_t ExpectedExponent(int64_t base, int32_t radix)
{
    if (radix == 0 || base < 1)
    {
        return 0;
    }
    int16_t result = (int16_t)(log((double)base) / log((double)radix));
    int64_t next = (int64_t)Power((uint32_t)radix, (uint16_t)(result + 1));
    if (next <= base)
    {
        result += 1;
    }
    int16_t exact = 0;
    for (uint64_t p = 1; p <= (uint64_t)base / (uint64_t)radix; p *= (uint64_t)radix)
    {
        exact++;
    }
    if (exact != result)
    {
        GenDie("ICU computes another exponent for a rule base", "a base near a power");
    }
    return result;
}

// NFRule::setBaseValue.
static void SetBase(RbnfRule* rule, int64_t base)
{
    rule->base = base;
    rule->radix = 10;
    rule->exponent = base >= 1 ? ExpectedExponent(base, 10) : 0;
}

// Decodes the code point at text; 0 at the end.
static uint32_t CodePoint(const char* text, size_t length, size_t* width)
{
    if (length == 0)
    {
        *width = 0;
        return 0;
    }
    int n = GenCharLength((const unsigned char*)text);
    if (n == 0 || (size_t)n > length)
    {
        *width = 1;
        return 0xFFFD;
    }
    const unsigned char* s = (const unsigned char*)text;
    uint32_t cp = n == 1 ? s[0] : s[0] & (0x7Fu >> n);
    for (int i = 1; i < n; i++)
    {
        cp = cp << 6 | (s[i] & 0x3Fu);
    }
    *width = (size_t)n;
    return cp;
}

static size_t Utf16Length(const char* text, size_t length)
{
    size_t units = 0;
    for (size_t at = 0; at < length;)
    {
        size_t w = 0;
        uint32_t cp = CodePoint(text + at, length - at, &w);
        units += cp >= 0x10000 ? 2 : 1;
        at += w;
    }
    return units;
}

// The last code point of a text.
static uint32_t LastCodePoint(const char* text, size_t length)
{
    size_t at = length;
    while (at > 0 && ((unsigned char)text[at - 1] & 0xC0) == 0x80)
    {
        at--;
    }
    if (at > 0)
    {
        at--;
    }
    size_t w = 0;
    return CodePoint(text + at, length - at, &w);
}

// Parses digits, skipping spaces, commas and periods, to a stop
// character; false on anything else or past int64_t.
static bool ParseDigits(const char* d, size_t length, size_t* at, int64_t* value, char* stopped)
{
    int64_t val = 0;
    *stopped = 0;
    while (*at < length)
    {
        char ch = d[*at];
        size_t w = SpaceWidth(d + *at, length - *at);
        if (ch >= '0' && ch <= '9')
        {
            int64_t digit = ch - '0';
            if (val > (INT64_MAX - digit) / 10)
            {
                return false;
            }
            val = val * 10 + digit;
        }
        else if (ch == '/' || ch == '>')
        {
            *stopped = ch;
            break;
        }
        else if (w > 0)
        {
            *at += w - 1;
        }
        else if (ch != ',' && ch != '.')
        {
            return false;
        }
        ++*at;
    }
    *value = val;
    return true;
}

// The numeric descriptor of NFRule::parseRuleDescriptor: a base, an
// optional radix after "/", and ">" lowering the exponent.
static void NumericDescriptor(Compiler* c, RbnfRule* rule, const char* d, size_t length)
{
    size_t at = 0;
    int64_t value = 0;
    char stopped = 0;
    if (!ParseDigits(d, length, &at, &value, &stopped))
    {
        Fail(c, "a rule descriptor ICU refuses: %.*s", (int)length, d);
        return;
    }
    SetBase(rule, value);
    if (stopped == '/')
    {
        at++;
        if (!ParseDigits(d, length, &at, &value, &stopped) || value == 0 || value > INT32_MAX ||
            stopped == '/')
        {
            Fail(c, "a rule radix ICU refuses: %.*s", (int)length, d);
            return;
        }
        rule->radix = (int32_t)value;
        rule->exponent = ExpectedExponent(rule->base, rule->radix);
    }
    if (stopped == '>')
    {
        for (; at < length; at++)
        {
            if (d[at] != '>' || rule->exponent <= 0)
            {
                Fail(c, "a rule descriptor ICU refuses: %.*s", (int)length, d);
                return;
            }
            rule->exponent--;
        }
    }
}

// NFRule::parseRuleDescriptor: the descriptor before the first colon,
// then the text after the spaces that follow it, a leading apostrophe
// removed. Returns the text, which the caller owns.
static char* ParseDescriptor(Compiler* c, RbnfRule* rule, const char* desc, size_t length,
                             size_t* lengthOut)
{
    long colon = IndexOf(desc, length, ':', 0);
    size_t start = 0;
    if (colon >= 0)
    {
        const char* d = desc;
        size_t n = (size_t)colon;
        start = (size_t)colon + 1;
        for (size_t w = 0; (w = SpaceWidth(desc + start, length - start)) > 0;)
        {
            start += w;
        }
        uint32_t last = LastCodePoint(d, n);
        size_t units = Utf16Length(d, n);
        size_t w = 0;
        uint32_t first = CodePoint(d, n, &w);
        if (first >= '0' && first <= '9' && last != 'x')
        {
            NumericDescriptor(c, rule, d, n);
        }
        else if (n == 2 && memcmp(d, "-x", 2) == 0)
        {
            rule->kind = rbnfNegative;
        }
        else if (units == 3)
        {
            size_t mw = 0;
            uint32_t middle = CodePoint(d + w, n - w, &mw);
            if (first == '0' && last == 'x')
            {
                rule->kind = rbnfProper;
                rule->decimalPoint = middle;
            }
            else if (first == 'x' && last == 'x')
            {
                rule->kind = rbnfImproper;
                rule->decimalPoint = middle;
            }
            else if (first == 'x' && last == '0')
            {
                rule->kind = rbnfDefault;
                rule->decimalPoint = middle;
            }
            else if (memcmp(d, "NaN", 3) == 0)
            {
                rule->kind = rbnfNaN;
            }
            else if (memcmp(d, "Inf", 3) == 0)
            {
                rule->kind = rbnfInfinity;
            }
        }
    }
    if (start < length && desc[start] == '\'')
    {
        start++;
    }
    *lengthOut = length - start;
    return CopyRange(desc + start, length - start);
}

static int FindSet(const RbnfGroup* g, const char* name, size_t length)
{
    for (int i = 0; i < g->setCount; i++)
    {
        if (strlen(g->sets[i].name) == length && memcmp(g->sets[i].name, name, length) == 0)
        {
            return i;
        }
    }
    return -1;
}

static bool IsFractionKind(RbnfRuleKind kind)
{
    return kind == rbnfImproper || kind == rbnfProper || kind == rbnfDefault;
}

// NFSubstitution::makeSubstitution: the kind by the token's first
// character, the rule's kind and the set's.
static bool SubKind(Compiler* c, const RbnfSet* set, const RbnfRule* rule, const char* token,
                    size_t length, RbnfSubKind* kind)
{
    switch (token[0])
    {
    case '<':
        if (rule->kind == rbnfNegative)
        {
            Fail(c, "<< in a negative-number rule in %s", set->name);
            return false;
        }
        *kind = IsFractionKind(rule->kind) ? rbnfIntegral
                : set->fraction            ? rbnfNumerator
                                           : rbnfMultiplier;
        return true;
    case '>':
        if (rule->kind != rbnfNegative && !IsFractionKind(rule->kind) && set->fraction)
        {
            Fail(c, ">> in a fraction rule set: %s", set->name);
            return false;
        }
        *kind = rule->kind == rbnfNegative   ? rbnfAbsolute
                : IsFractionKind(rule->kind) ? rbnfFraction
                                             : rbnfModulus;
        return true;
    case '=':
        if (length == 2)
        {
            Fail(c, "== in %s", set->name);
            return false;
        }
        *kind = rbnfSame;
        return true;
    default:
        Fail(c, "a substitution ICU refuses in %s", set->name);
        return false;
    }
}

// NFSubstitution's constructor: what the token names between its ends,
// its own set when nothing (a numerator's being the default set).
static void SubTarget(Compiler* c, const RbnfSet* set, RbnfSub* sub, const char* token,
                      size_t length)
{
    if (length < 2 || token[0] != token[length - 1])
    {
        Fail(c, "a substitution ICU refuses in %s: %.*s", set->name, (int)length, token);
        return;
    }
    const char* inner = token + 1;
    size_t n = length - 2;
    int self = c->current;
    if (n == 0 || inner[0] == '>')
    {
        sub->set = sub->kind == rbnfNumerator ? c->group->defaultSet : self;
    }
    else if (inner[0] == '%')
    {
        sub->set = FindSet(c->group, inner, n);
        if (sub->set < 0)
        {
            Fail(c, "a substitution names no rule set: %.*s", (int)n, inner);
        }
    }
    else if (inner[0] == '#' || inner[0] == '0')
    {
        sub->set = -1;
        sub->pattern = CopyRange(inner, n);
    }
    else
    {
        Fail(c, "a substitution ICU refuses in %s: %.*s", set->name, (int)length, token);
    }
}

// The fraction substitution's mode (FractionalPartSubstitution): digit by
// digit for ">>", ">>>" or its own set, else the named set becomes a
// fraction rule set, which must not have been read yet.
static void FractionMode(Compiler* c, RbnfSub* sub, const char* token, size_t length)
{
    bool two = length == 2 && memcmp(token, ">>", 2) == 0;
    bool three = length == 3 && memcmp(token, ">>>", 3) == 0;
    sub->triple = three;
    if (two || three || sub->set == c->current)
    {
        sub->byDigits = true;
        return;
    }
    if (sub->set < 0)
    {
        Fail(c, "a fraction substitution with a pattern: %.*s", (int)length, token);
        return;
    }
    if (sub->set < c->current)
    {
        Fail(c, "a fraction rule set used after its rules were read: %s",
             c->group->sets[sub->set].name);
        return;
    }
    c->group->sets[sub->set].fraction = true;
}

static void MakeSub(Compiler* c, RbnfSet* set, RbnfRule* rule, const char* token, size_t length,
                    size_t pos, int predecessor)
{
    RbnfSub* sub = &rule->subs[rule->subCount];
    *sub = (RbnfSub){.pos = pos, .predecessor = -1};
    if (!SubKind(c, set, rule, token, length, &sub->kind))
    {
        return;
    }
    size_t n = length;
    if (sub->kind == rbnfNumerator && n >= 2 && memcmp(token + n - 2, "<<", 2) == 0)
    {
        // NumeratorSubstitution's fixdesc: "<...<<" writes leading zeros.
        sub->zeros = true;
        n--;
    }
    SubTarget(c, set, sub, token, n);
    if (c->failed)
    {
        return;
    }
    if (sub->kind == rbnfNumerator && rule->base <= 0)
    {
        Fail(c, "a fraction rule set's rule without a base in %s", set->name);
        return;
    }
    if (sub->kind == rbnfFraction)
    {
        FractionMode(c, sub, token, length);
    }
    if (sub->kind == rbnfModulus && length == 3 && memcmp(token, ">>>", 3) == 0)
    {
        sub->triple = true;
        sub->predecessor = predecessor;
    }
    rule->subCount++;
}

// NFRule::indexOfAnyRulePrefix, which searches for each prefix's first
// character ("*RULE_PREFIXES[i]"): the first '<', '>' or '='.
static long FirstSubstitution(const char* text, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        if (text[i] == '<' || text[i] == '>' || text[i] == '=')
        {
            return (long)i;
        }
    }
    return -1;
}

// NFRule::extractSubstitution: the first token, removed from the text.
static bool ExtractOne(Compiler* c, RbnfSet* set, RbnfRule* rule, int predecessor)
{
    long start = FirstSubstitution(rule->text, rule->length);
    if (start < 0)
    {
        return false;
    }
    long end = -1;
    if (IndexOfText(rule->text, rule->length, ">>>", 0) == start)
    {
        end = start + 2;
    }
    else
    {
        char ch = rule->text[start];
        end = IndexOf(rule->text, rule->length, ch, (size_t)start + 1);
        if (ch == '<' && end >= 0 && (size_t)end + 1 < rule->length && rule->text[end + 1] == ch)
        {
            end++;
        }
    }
    if (end < 0)
    {
        return false;
    }
    size_t length = (size_t)(end - start) + 1;
    MakeSub(c, set, rule, rule->text + start, length, (size_t)start, predecessor);
    memmove(rule->text + start, rule->text + end + 1, rule->length - (size_t)end - 1);
    rule->length -= length;
    rule->text[rule->length] = '\0';
    return true;
}

static int CategoryOf(const char* keyword, size_t length)
{
    static const char* const s_keywords[rbnfCategories] = {"zero", "one",  "two",
                                                           "few",  "many", "other"};
    for (int i = 0; i < rbnfCategories; i++)
    {
        if (strlen(s_keywords[i]) == length && memcmp(s_keywords[i], keyword, length) == 0)
        {
            return i;
        }
    }
    return -1;
}

// The plural pattern of a block ("one{...}other{...}"), as PluralFormat
// reads the simple ones CLDR writes: keywords, each with its text in
// braces, spaces between; other is required.
static void ParsePluralForms(Compiler* c, RbnfRule* rule, const char* p, size_t length)
{
    size_t at = 0;
    while (at < length)
    {
        size_t w = SpaceWidth(p + at, length - at);
        if (w > 0)
        {
            at += w;
            continue;
        }
        long open = IndexOf(p, length, '{', at);
        long close = open < 0 ? -1 : IndexOf(p, length, '}', (size_t)open);
        int category = open < 0 ? -1 : CategoryOf(p + at, (size_t)open - at);
        if (close < 0 || category < 0 || rule->forms[category] != nullptr ||
            IndexOf(p, (size_t)close, '{', (size_t)open + 1) >= 0)
        {
            Fail(c, "a plural block the library does not read: %.*s", (int)length, p);
            return;
        }
        rule->forms[category] = CopyRange(p + open + 1, (size_t)(close - open - 1));
        rule->order[rule->formCount++] = (uint8_t)category;
        at = (size_t)close + 1;
    }
    if (rule->forms[rbnfCategories - 1] == nullptr)
    {
        Fail(c, "a plural block without other: %.*s", (int)length, p);
    }
}

// The plural block of NFRule::extractSubstitutions: "$(type,pattern)$"
// in the text without the tokens.
static void ParsePlural(Compiler* c, RbnfRule* rule)
{
    long start = IndexOfText(rule->text, rule->length, "$(", 0);
    long end = start < 0 ? -1 : IndexOfText(rule->text, rule->length, ")$", (size_t)start);
    if (end < 0)
    {
        return;
    }
    long comma = IndexOf(rule->text, rule->length, ',', (size_t)start);
    if (comma < 0 || comma > end)
    {
        Fail(c, "a plural block without its type: %s", rule->text);
        return;
    }
    const char* type = rule->text + start + 2;
    if (strncmp(type, "cardinal", 8) == 0)
    {
        rule->ordinal = false;
    }
    else if (strncmp(type, "ordinal", 7) == 0)
    {
        rule->ordinal = true;
    }
    else
    {
        Fail(c, "a plural block of another type: %s", rule->text);
        return;
    }
    rule->plural = true;
    rule->pluralStart = (size_t)start;
    rule->pluralEnd = (size_t)end + 2;
    for (int i = 0; i < rule->subCount; i++)
    {
        if (rule->subs[i].pos > rule->pluralStart && rule->subs[i].pos < rule->pluralEnd)
        {
            Fail(c, "a substitution inside a plural block: %s", rule->text);
            return;
        }
    }
    ParsePluralForms(c, rule, rule->text + comma + 1, (size_t)(end - comma - 1));
}

static bool SameClass(const RbnfSub* a, const RbnfSub* b)
{
    return a->kind == b->kind;
}

// NFRule::extractSubstitutions: two substitutions at most, then the
// plural block. Two of a kind make ICU's formatter unparseable.
static void Extract(Compiler* c, RbnfSet* set, RbnfRule* rule, char* text, size_t length,
                    int predecessor)
{
    rule->text = text;
    rule->length = length;
    if (ExtractOne(c, set, rule, predecessor) && !c->failed)
    {
        ExtractOne(c, set, rule, predecessor);
    }
    if (rule->subCount == 2 && SameClass(&rule->subs[0], &rule->subs[1]))
    {
        c->group->unparseable = true;
    }
    if (!c->failed)
    {
        ParsePlural(c, rule);
    }
}

static void AddRule(RbnfSet* set, const RbnfRule* rule, int* capacity, int* specialCapacity)
{
    if (rule->kind == rbnfNormal)
    {
        if (set->ruleCount == *capacity)
        {
            *capacity = *capacity == 0 ? 16 : *capacity * 2;
            set->rules = realloc(set->rules, (size_t)*capacity * sizeof(RbnfRule));
            if (set->rules == nullptr)
            {
                GenDie("out of memory", "spellout rules");
            }
        }
        set->rules[set->ruleCount++] = *rule;
        return;
    }
    if (set->specialCount == *specialCapacity)
    {
        *specialCapacity = *specialCapacity == 0 ? 4 : *specialCapacity * 2;
        set->specials = realloc(set->specials, (size_t)*specialCapacity * sizeof(RbnfRule));
        if (set->specials == nullptr)
        {
            GenDie("out of memory", "spellout rules");
        }
    }
    set->specials[set->specialCount++] = *rule;
}

// A text made of ranges of another.
static char* Join(const char* a, size_t an, const char* b, size_t bn, const char* d, size_t dn,
                  size_t* lengthOut)
{
    char* out = GenAllocate(an + bn + dn + 1);
    memcpy(out, a, an);
    memcpy(out + an, b, bn);
    memcpy(out + an + bn, d, dn);
    *lengthOut = an + bn + dn;
    return out;
}

typedef struct Capacities
{
    int rules;
    int specials;
} Capacities;

// NFRule::makeRules: a description is one rule, or two when it has
// bracketed text and a base that is a multiple of its divisor (or is
// x.x or x.0): the first without the brackets' text (or with the text
// after "|"), the second with it.
static void MakeRules(Compiler* c, RbnfSet* set, const char* desc, size_t length, Capacities* caps)
{
    int predecessor = set->ruleCount - 1;
    RbnfRule rule1 = {.radix = 10};
    size_t n = 0;
    char* body = ParseDescriptor(c, &rule1, desc, length, &n);
    if (c->failed)
    {
        free(body);
        return;
    }
    long b1 = IndexOf(body, n, '[', 0);
    long b2 = b1 < 0 ? -1 : IndexOf(body, n, ']', 0);
    if (b2 < 0 || b1 > b2 || rule1.kind == rbnfProper || rule1.kind == rbnfNegative ||
        rule1.kind == rbnfInfinity || rule1.kind == rbnfNaN)
    {
        Extract(c, set, &rule1, body, n, predecessor);
        AddRule(set, &rule1, &caps->rules, &caps->specials);
        return;
    }
    long orElse = IndexOf(body, n, '|', 0);
    uint64_t mod = RbnfDivisor(&rule1);
    bool normal = rule1.kind == rbnfNormal;
    if (normal && rule1.base > 0 && rule1.radix != 0 && mod == 0)
    {
        Fail(c, "a rule whose divisor overflows in %s", set->name);
        free(body);
        return;
    }
    bool split =
        (normal && rule1.base > 0 && rule1.radix != 0 && (uint64_t)rule1.base % mod == 0) ||
        rule1.kind == rbnfImproper || rule1.kind == rbnfDefault;
    RbnfRule rule2 = {.radix = 10};
    size_t tail = n - (size_t)b2 - 1;
    if (split)
    {
        if (normal)
        {
            rule2.base = rule1.base;
            rule1.base += set->fraction ? 0 : 1;
        }
        else if (rule1.kind == rbnfImproper)
        {
            rule2.kind = rbnfProper;
        }
        else
        {
            rule2.kind = rbnfDefault;
            rule1.kind = rbnfImproper;
        }
        rule2.radix = rule1.radix;
        rule2.exponent = rule1.exponent;
        size_t n2 = 0;
        const char* middle = orElse >= 0 ? body + orElse + 1 : body;
        size_t middleLength = orElse >= 0 ? (size_t)(b2 - orElse - 1) : 0;
        char* text2 = Join(body, (size_t)b1, middle, middleLength, body + b2 + 1, tail, &n2);
        Extract(c, set, &rule2, text2, n2, predecessor);
    }
    size_t n1 = 0;
    size_t inner = orElse >= 0 ? (size_t)(orElse - b1 - 1) : (size_t)(b2 - b1 - 1);
    char* text1 = Join(body, (size_t)b1, body + b1 + 1, inner, body + b2 + 1, tail, &n1);
    free(body);
    Extract(c, set, &rule1, text1, n1, predecessor);
    if (split)
    {
        AddRule(set, &rule2, &caps->rules, &caps->specials);
    }
    AddRule(set, &rule1, &caps->rules, &caps->specials);
}

// NFRuleSet::parseRules: rules split at semicolons, then the bases of
// rules without one: the last base plus one, or the last base in a
// fraction rule set; bases must not decrease.
static void ParseRules(Compiler* c, RbnfSet* set, const char* desc, size_t length)
{
    Capacities caps = {0};
    for (size_t at = 0; at < length && !c->failed;)
    {
        long p = IndexOf(desc, length, ';', at);
        size_t end = p < 0 ? length : (size_t)p;
        MakeRules(c, set, desc + at, end - at, &caps);
        at = end + 1;
    }
    int64_t next = 0;
    for (int i = 0; i < set->ruleCount && !c->failed; i++)
    {
        RbnfRule* rule = &set->rules[i];
        if (rule->base == 0)
        {
            SetBase(rule, next);
        }
        else if (rule->base < next)
        {
            Fail(c, "rules not in order in %s", set->name);
        }
        else
        {
            next = rule->base;
        }
        next += set->fraction ? 0 : 1;
    }
}

// RuleBasedNumberFormat::initDefaultRuleSet.
static int DefaultSet(const RbnfGroup* g)
{
    static const char* const s_names[] = {"%spellout-numbering", "%digits-ordinal", "%duration"};
    for (int i = 0; i < g->setCount; i++)
    {
        for (size_t k = 0; k < sizeof(s_names) / sizeof(s_names[0]); k++)
        {
            if (strcmp(g->sets[i].name, s_names[k]) == 0)
            {
                return i;
            }
        }
    }
    int last = g->setCount - 1;
    if (!g->sets[last].isPublic)
    {
        for (int i = last - 1; i >= 0; i--)
        {
            if (g->sets[i].isPublic)
            {
                return i;
            }
        }
    }
    return last;
}

// NFRuleSet's constructor: the name before the colon and the spaces
// after it, "%default" when the description has none.
static size_t ReadName(Compiler* c, RbnfSet* set, const char* desc, size_t length)
{
    size_t start = 0;
    if (length > 0 && desc[0] == '%')
    {
        long colon = IndexOf(desc, length, ':', 0);
        if (colon < 2)
        {
            Fail(c, "%s", "a rule set name without its colon");
            return 0;
        }
        set->name = CopyRange(desc, (size_t)colon);
        start = (size_t)colon + 1;
        for (size_t w = 0; (w = SpaceWidth(desc + start, length - start)) > 0;)
        {
            start += w;
        }
    }
    else
    {
        set->name = CopyRange("%default", 8);
    }
    if (start >= length)
    {
        Fail(c, "an empty rule set: %s", set->name);
        return 0;
    }
    set->isPublic = strncmp(set->name, "%%", 2) != 0;
    set->parseable = true;
    size_t nameLength = strlen(set->name);
    if (nameLength > 8 && strcmp(set->name + nameLength - 8, "@noparse") == 0)
    {
        set->parseable = false;
        set->name[nameLength - 8] = '\0';
    }
    return start;
}

bool RbnfCompile(const char* text, size_t length, RbnfGroup* out, char* errorOut,
                 size_t errorCapacity)
{
    *out = (RbnfGroup){0};
    Compiler c = {.group = out, .error = errorOut, .capacity = errorCapacity};
    size_t n = 0;
    char* desc = StripWhitespace(text, length, &n);
    if (IndexOfText(desc, n, "%%lenient-parse:", 0) >= 0)
    {
        Fail(&c, "%s", "lenient-parse rules, which the library does not read");
    }
    // The rule sets end at each ";%" (RuleBasedNumberFormat::init).
    size_t starts[1024];
    size_t ends[1024];
    int count = 0;
    for (size_t at = 0; !c.failed;)
    {
        long p = IndexOfText(desc, n, ";%", at);
        if (count == 1024)
        {
            Fail(&c, "%s", "more than 1,024 rule sets in a group");
            break;
        }
        starts[count] = at;
        ends[count] = p < 0 ? n : (size_t)p + 1;
        count++;
        if (p < 0)
        {
            break;
        }
        at = (size_t)p + 1;
    }
    out->sets = GenAllocate((size_t)(count > 0 ? count : 1) * sizeof(RbnfSet));
    size_t bodies[1024];
    for (int i = 0; i < count && !c.failed; i++)
    {
        bodies[i] = ReadName(&c, &out->sets[i], desc + starts[i], ends[i] - starts[i]);
        out->setCount++;
    }
    if (!c.failed && count > 0)
    {
        out->defaultSet = DefaultSet(out);
    }
    for (int i = 0; i < count && !c.failed; i++)
    {
        c.current = i;
        ParseRules(&c, &out->sets[i], desc + starts[i] + bodies[i],
                   ends[i] - starts[i] - bodies[i]);
    }
    free(desc);
    if (c.failed)
    {
        RbnfFree(out);
        return false;
    }
    return true;
}

typedef struct Text
{
    char* bytes;
    size_t size;
    size_t capacity;
} Text;

static void Append(Text* t, const char* bytes, size_t length)
{
    if (t->size + length + 1 > t->capacity)
    {
        t->capacity = (t->size + length + 1) * 2;
        t->bytes = realloc(t->bytes, t->capacity);
        if (t->bytes == nullptr)
        {
            GenDie("out of memory", "spellout rules");
        }
    }
    memcpy(t->bytes + t->size, bytes, length);
    t->size += length;
    t->bytes[t->size] = '\0';
}

static void AppendText(Text* t, const char* text)
{
    Append(t, text, strlen(text));
}

static void AppendCodePoint(Text* t, uint32_t cp)
{
    char buffer[4];
    size_t n = 0;
    if (cp < 0x80)
    {
        buffer[n++] = (char)cp;
    }
    else if (cp < 0x800)
    {
        buffer[n++] = (char)(0xC0 | cp >> 6);
        buffer[n++] = (char)(0x80 | (cp & 0x3F));
    }
    else
    {
        buffer[n++] = (char)(0xE0 | cp >> 12);
        buffer[n++] = (char)(0x80 | (cp >> 6 & 0x3F));
        buffer[n++] = (char)(0x80 | (cp & 0x3F));
    }
    Append(t, buffer, n);
}

static char TokenChar(RbnfSubKind kind)
{
    switch (kind)
    {
    case rbnfSame:
        return '=';
    case rbnfMultiplier:
    case rbnfIntegral:
    case rbnfNumerator:
        return '<';
    default:
        return '>';
    }
}

// NFSubstitution::toString and ModulusSubstitution's.
static void AppendSub(Text* t, const RbnfGroup* g, const RbnfSub* sub)
{
    char token = TokenChar(sub->kind);
    if (sub->kind == rbnfModulus && sub->triple && sub->predecessor >= 0)
    {
        AppendText(t, ">>>");
        return;
    }
    Append(t, &token, 1);
    AppendText(t, sub->set >= 0 ? g->sets[sub->set].name : sub->pattern);
    Append(t, &token, 1);
}

// NFRule::_appendRuleText.
static void AppendRule(Text* t, const RbnfGroup* g, const RbnfRule* rule)
{
    uint32_t point = rule->decimalPoint == 0 ? '.' : rule->decimalPoint;
    char number[32];
    switch (rule->kind)
    {
    case rbnfNegative:
        AppendText(t, "-x");
        break;
    case rbnfImproper:
    case rbnfProper:
    case rbnfDefault:
        AppendText(t, rule->kind == rbnfProper ? "0" : "x");
        AppendCodePoint(t, point);
        AppendText(t, rule->kind == rbnfDefault ? "0" : "x");
        break;
    case rbnfInfinity:
        AppendText(t, "Inf");
        break;
    case rbnfNaN:
        AppendText(t, "NaN");
        break;
    default:
        snprintf(number, sizeof(number), "%lld", (long long)rule->base);
        AppendText(t, number);
        if (rule->radix != 10)
        {
            snprintf(number, sizeof(number), "/%d", rule->radix);
            AppendText(t, number);
        }
        for (int i = ExpectedExponent(rule->base, rule->radix) - rule->exponent; i > 0; i--)
        {
            AppendText(t, ">");
        }
        break;
    }
    AppendText(t, ": ");
    if (rule->length > 0 && rule->text[0] == ' ' && (rule->subCount == 0 || rule->subs[0].pos != 0))
    {
        AppendText(t, "'");
    }
    size_t at = 0;
    for (int i = 0; i < rule->subCount; i++)
    {
        const RbnfSub* sub = &rule->subs[i];
        Append(t, rule->text + at, sub->pos - at);
        AppendSub(t, g, sub);
        at = sub->pos;
    }
    Append(t, rule->text + at, rule->length - at);
    AppendText(t, ";\n");
}

// NFRuleSet::appendRules: the normal rules, then the special ones in
// ICU's order, every fraction rule of a kind together.
char* RbnfDump(const RbnfGroup* group)
{
    static const RbnfRuleKind s_order[] = {rbnfNegative, rbnfImproper, rbnfProper,
                                           rbnfDefault,  rbnfInfinity, rbnfNaN};
    Text t = {0};
    AppendText(&t, "");
    for (int s = 0; s < group->setCount; s++)
    {
        const RbnfSet* set = &group->sets[s];
        AppendText(&t, set->name);
        AppendText(&t, ":\n");
        for (int i = 0; i < set->ruleCount; i++)
        {
            AppendRule(&t, group, &set->rules[i]);
        }
        for (size_t k = 0; k < sizeof(s_order) / sizeof(s_order[0]); k++)
        {
            int last = -1;
            for (int i = 0; i < set->specialCount; i++)
            {
                if (set->specials[i].kind != s_order[k])
                {
                    continue;
                }
                if (IsFractionKind(s_order[k]))
                {
                    AppendRule(&t, group, &set->specials[i]);
                }
                last = i;
            }
            if (last >= 0 && !IsFractionKind(s_order[k]))
            {
                AppendRule(&t, group, &set->specials[last]);
            }
        }
    }
    return t.bytes;
}

static void FreeRule(RbnfRule* rule)
{
    free(rule->text);
    for (int i = 0; i < rule->subCount; i++)
    {
        free(rule->subs[i].pattern);
    }
    for (int i = 0; i < rbnfCategories; i++)
    {
        free(rule->forms[i]);
    }
}

void RbnfFree(RbnfGroup* group)
{
    for (int s = 0; s < group->setCount; s++)
    {
        RbnfSet* set = &group->sets[s];
        for (int i = 0; i < set->ruleCount; i++)
        {
            FreeRule(&set->rules[i]);
        }
        for (int i = 0; i < set->specialCount; i++)
        {
            FreeRule(&set->specials[i]);
        }
        free(set->rules);
        free(set->specials);
        free(set->name);
    }
    free(group->sets);
    *group = (RbnfGroup){0};
}
