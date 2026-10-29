// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out parser (spellout_parse.h). Each function follows the
// ICU4C 79 function named in its comment, strict (not lenient); ICU's
// double arithmetic runs here on the exact decimal, its integer upper
// bounds as ICU clamps them. A text is a byte range; every text a rule
// passes on is a prefix of what it was given, as ICU's substrings are.

#include "spellout_parse.h"

#include "invariant.h"
#include "number_format.h"
#include "number_parse.h"
#include "spellout_match.h"
#include "spellout_rule.h"

#include <string.h>

// Keeps a function off the recursion's frames: the parser nests 64 levels
// deep, and what a leaf needs should not be on the stack at each level.
#if defined(_MSC_VER) && !defined(__clang__)
#define MLOC_NOINLINE __declspec(noinline)
#else
#define MLOC_NOINLINE __attribute__((noinline))
#endif

enum
{
    // ICU's RECURSION_LIMIT: a rule set nested past it reads nothing.
    maxLevels = MLOC_MAX_SPELLOUT_LEVELS,
    // Rules a ">>>" calls directly nest too, without a rule set between to
    // count them; a crafted file could chain them without end. An error
    // past it, as when formatting.
    maxFrames = 192,
    // The rule matches a search may try, an error past it: ICU's search
    // is exponential, and a long text whose nesting meets the limit can
    // keep it busy for minutes. Of the 3,294,432 texts a survey of the
    // formatters of every locale and rule set wrote, the longest search
    // takes 73,227 steps here; a text written against the search (a long
    // number written four times over) still meets the bound.
    maxSteps = MLOC_MAX_SPELLOUT_STEPS,
    // A digit by digit fraction reads at most a decimal's digits.
    maxFractionDigits = MLOC_MAX_DECIMAL_DIGITS,
    // The rule set searches remembered (a power of two), in pairs of
    // slots: ICU's search reads the same part of the text by the same
    // rule set again and again; a search depends only on its inputs, so
    // its result is remembered, a costly one kept over a cheap one. The
    // longest of the survey's texts meet some 1,800 inputs; 1,024 slots
    // left four of its searches at the bound, 2,048 none.
    memoSize = 2048,
    memoWays = 2,
};

// util64_fromDouble clamps an upper bound to the largest integer a double
// holds exactly.
constexpr uint64_t maxUpper = 9007199254740991;
// DBL_MAX, ICU's bound for "none".
constexpr uint64_t noUpper = UINT64_MAX;

// A rule set search's inputs, which decide it: the set, the text's place
// (within the first 64 KiB of the text, past which no search is
// remembered), the upper bound and the special rules tried.
typedef struct MemoKey
{
    uint64_t upper;
    uint32_t set;
    uint32_t start;
    uint32_t length;
    uint32_t mask;
} MemoKey;

// The flags of a memo slot: whether it is used, whether its search met
// the depth limit, its value's sign and kind (two bits), and its cost,
// the steps its search took (three bits, a bucket per power of four).
enum
{
    memoUsed = 1,
    memoLimited = 2,
    memoNegative = 4,
    memoKindShift = 3,
    memoCostShift = 5,
    memoCostBuckets = 8,
};

// A rule set search remembered: its inputs and result.
typedef struct Memo
{
    uint64_t upper;
    // The value: its digits as an integer and the power of the last (a
    // value of more digits is not remembered).
    uint64_t digits;
    int32_t exponent;
    uint16_t start;
    uint16_t length;
    uint16_t end;
    uint16_t set;
    uint8_t mask;
    // The depth it was searched at.
    uint8_t depth;
    uint8_t flags;
} Memo;

typedef struct Parser
{
    const SpelloutFormat* f;
    SpelloutRules r;
    // The text's start, which every text read is within.
    const uint8_t* origin;
    uint64_t steps;
    mlocResult error;
    // Whether the search has met the depth limit, below the rule set
    // search under way.
    bool limited;
    Memo memo[memoSize];
} Parser;

static void Fail(Parser* p, mlocResult error)
{
    if (p->error == mloc_success)
    {
        p->error = error;
    }
}

static mlocDecimal Zero(void)
{
    return (mlocDecimal){.kind = mloc_decimalFinite};
}

static bool Finite(const mlocDecimal* d)
{
    return d->kind == mloc_decimalFinite;
}

static mlocDecimal Negated(const mlocDecimal* d)
{
    mlocDecimal n = *d;
    n.negative = d->kind == mloc_decimalNaN ? 0 : (uint8_t)(n.negative ^ 1);
    return n;
}

// a + b as a double adds them when one is not finite.
static mlocDecimal AddSpecial(const mlocDecimal* a, const mlocDecimal* b)
{
    if (a->kind == mloc_decimalNaN || b->kind == mloc_decimalNaN ||
        (!Finite(a) && !Finite(b) && a->negative != b->negative))
    {
        return (mlocDecimal){.kind = mloc_decimalNaN};
    }
    return Finite(a) ? *b : *a;
}

static bool Add(Parser* p, const mlocDecimal* a, const mlocDecimal* b, mlocDecimal* out)
{
    if (!Finite(a) || !Finite(b))
    {
        *out = AddSpecial(a, b);
        return true;
    }
    if (!mlocSpelloutAdd(a, b, out))
    {
        Fail(p, mloc_errorLimit);
        return false;
    }
    return true;
}

// x times a rule's divisor.
static bool MultiplyBy(Parser* p, const mlocDecimal* x, SpelloutDivisor divisor, mlocDecimal* out)
{
    bool done = !Finite(x)           ? (*out = *x, true)
                : divisor.power >= 0 ? mlocSpelloutShift(x, -divisor.power, out)
                                     : divisor.value < 1000000000000000000u &&
                                           mlocSpelloutMultiply(x, divisor.value, out);
    if (!done)
    {
        Fail(p, mloc_errorLimit);
    }
    return done;
}

// A positive integer as a divisor, with its power of ten when it is one.
static bool DivisorOf(const mlocDecimal* d, SpelloutDivisor* out)
{
    uint64_t value = 0;
    if (!Finite(d) || d->negative != 0 || !mlocSpelloutIsInteger(d) ||
        !mlocSpelloutMagnitude64(d, &value) || value == 0)
    {
        return false;
    }
    bool power = d->count == 1 && d->digits[0] == 1;
    *out = (SpelloutDivisor){.value = value, .power = power ? d->exponent : -1};
    return true;
}

// x over a positive integer, exact: the bases of fraction rule sets are
// powers of ten.
static bool Divide(Parser* p, const mlocDecimal* x, const mlocDecimal* by, mlocDecimal* out)
{
    SpelloutDivisor divisor;
    if (!Finite(x))
    {
        *out = *x;
        return true;
    }
    if (!DivisorOf(by, &divisor) || !mlocSpelloutDivideExact(x, divisor, out))
    {
        Fail(p, mloc_errorLimit);
        return false;
    }
    return true;
}

// The composeRuleValue of each substitution: the value read and the
// partial result before it.
MLOC_NOINLINE static bool Compose(Parser* p, const SpelloutRule* rule, SpelloutSubKind kind,
                                  const mlocDecimal* value, const mlocDecimal* old,
                                  mlocDecimal* out)
{
    switch (kind)
    {
    case spelloutMultiplier:
        return MultiplyBy(p, value, rule->div, out);
    case spelloutModulus:
    {
        // old - fmod(old, divisor) + new
        mlocDecimal remainder;
        mlocDecimal whole;
        if (!Finite(old))
        {
            *out = (mlocDecimal){.kind = mloc_decimalNaN};
            return true;
        }
        if (!mlocSpelloutModulo(old, rule->div, &remainder))
        {
            Fail(p, mloc_errorLimit);
            return false;
        }
        mlocDecimal negated = Negated(&remainder);
        return Add(p, old, &negated, &whole) && Add(p, &whole, value, out);
    }
    case spelloutIntegral:
    case spelloutFraction:
        return Add(p, value, old, out);
    case spelloutAbsolute:
        *out = Negated(value);
        return true;
    case spelloutNumerator:
        return Divide(p, value, old, out);
    default:
        *out = *value;
        return true;
    }
}

// Formattable::getLong of a parse result: truncated, clamped to int32_t.
static int64_t GetLong(const mlocDecimal* d)
{
    if (d->kind == mloc_decimalNaN)
    {
        return 0;
    }
    if (d->kind == mloc_decimalInfinity)
    {
        return d->negative != 0 ? INT32_MIN : INT32_MAX;
    }
    mlocDecimal whole = mlocSpelloutRound(d, mloc_roundTrunc);
    int64_t n = 0;
    if (!mlocSpelloutToInt64(&whole, &n) || n > INT32_MAX || n < INT32_MIN)
    {
        return d->negative != 0 ? INT32_MIN : INT32_MAX;
    }
    return n;
}

// A remembered value.
MLOC_NOINLINE static mlocDecimal Unpack(const Memo* memo)
{
    mlocDecimal d = {.kind = (uint8_t)(memo->flags >> memoKindShift & 3),
                     .negative = (memo->flags & memoNegative) != 0 ? 1 : 0};
    uint8_t digits[20];
    int count = 0;
    for (uint64_t n = memo->digits; n > 0; n /= 10)
    {
        digits[count++] = (uint8_t)(n % 10);
    }
    for (int i = 0; i < count; i++)
    {
        d.digits[i] = digits[count - 1 - i];
    }
    d.count = (uint8_t)count;
    d.exponent = count > 0 ? memo->exponent : 0;
    mlocSpelloutNormalize(&d);
    return d;
}

static void SetParse(Parser* p, uint32_t set, SpelloutText text, uint64_t upper, uint32_t mask,
                     int depth, size_t* endOut, mlocDecimal* valueOut);
static void RuleParse(Parser* p, uint32_t rule, SpelloutText text, bool fraction, uint64_t upper,
                      uint32_t mask, int depth, size_t* endOut, mlocDecimal* valueOut);

// DecimalFormat::parse of a substitution's pattern, strict, with the
// locale's symbols and the pattern's grouping.
MLOC_NOINLINE static void PatternParse(Parser* p, uint32_t pattern, SpelloutText text,
                                       size_t* endOut, mlocDecimal* valueOut)
{
    MLOC_ASSERT(pattern < p->r.patternCount);
    const uint8_t* compiled = p->r.patterns + (size_t)pattern * spelloutPatternSize;
    Formatter number;
    memset(&number, 0, sizeof(number));
    number.core = p->f->core;
    number.core.primary = compiled[3];
    number.core.secondary = compiled[4];
    number.core.minGrouping = 1;
    number.notation = mloc_notationStandard;
    number.style = mloc_numberDecimal;
    number.grouping = mloc_groupingAuto;
    number.signDisplay = mloc_signAuto;
    size_t end = 0;
    mlocResult r = mlocParseWith(&number, nullptr, (const char*)text.bytes, text.length,
                                 mloc_parseStrict, valueOut, nullptr, &end);
    *endOut = r == mloc_success ? end : 0;
    if (r == mloc_success)
    {
        mlocSpelloutNormalize(valueOut);
    }
    else if (r != mloc_errorSyntax)
    {
        Fail(p, r);
    }
}

// NFSubstitution::doParse: the substitution's rule set or pattern under
// its upper bound (calcUpperBound), the value composed with the partial
// result.
static bool SubBasic(Parser* p, const SpelloutRule* rule, const uint8_t* sub, SpelloutText text,
                     const mlocDecimal* base, uint64_t upper, uint32_t mask, int depth,
                     size_t* endOut, mlocDecimal* valueOut)
{
    SpelloutSubKind kind = (SpelloutSubKind)(sub[1] & spelloutSubKindMask);
    uint64_t bound = kind == spelloutSame                                    ? upper
                     : kind == spelloutMultiplier || kind == spelloutModulus ? rule->divisor
                     : kind == spelloutIntegral || kind == spelloutAbsolute  ? noUpper
                     : kind == spelloutFraction                              ? 0
                                                                             : rule->base;
    int64_t target = mlocSpelloutTarget(sub);
    mlocDecimal value = Zero();
    size_t end = 0;
    if (target >= 0)
    {
        SetParse(p, (uint32_t)target, text, bound, mask, depth, &end, &value);
    }
    else
    {
        PatternParse(p, (uint32_t)(-1 - target), text, &end, &value);
    }
    *endOut = end;
    if (end == 0 || p->error != mloc_success)
    {
        *valueOut = Zero();
        return false;
    }
    return Compose(p, rule, kind, &value, base, valueOut);
}

// FractionalPartSubstitution::doParse digit by digit: each digit a rule
// below ten reads, spaces after it skipped. A rule reading other than a
// digit from 0 to 9 ends the fraction (ICU appends it as a digit).
MLOC_NOINLINE static bool SubDigits(Parser* p, const uint8_t* sub, SpelloutText text,
                                    const mlocDecimal* base, uint32_t mask, int depth,
                                    size_t* endOut, mlocDecimal* valueOut)
{
    uint8_t digits[maxFractionDigits];
    int count = 0;
    size_t at = 0;
    for (;;)
    {
        size_t end = 0;
        mlocDecimal value = Zero();
        if (at == text.length)
        {
            break;
        }
        SetParse(p, (uint32_t)mlocSpelloutTarget(sub),
                 (SpelloutText){text.bytes + at, text.length - at}, 10, mask, depth, &end, &value);
        int64_t digit = GetLong(&value);
        if (end == 0 || digit < 0 || digit > 9 || p->error != mloc_success)
        {
            break;
        }
        if (count == maxFractionDigits)
        {
            Fail(p, mloc_errorLimit);
            return false;
        }
        digits[count++] = (uint8_t)digit;
        at += end;
        while (at < text.length && text.bytes[at] == ' ')
        {
            at++;
        }
    }
    mlocDecimal fraction = Zero();
    for (int i = count; i > 0 && digits[i - 1] == 0; i--)
    {
        count--;
    }
    if (count > 0)
    {
        memcpy(fraction.digits, digits, (size_t)count);
        fraction.count = (uint8_t)count;
        fraction.exponent = -count;
        // Leading zeros are not significant digits.
        int lead = 0;
        while (fraction.digits[lead] == 0)
        {
            lead++;
        }
        memmove(fraction.digits, fraction.digits + lead, (size_t)(count - lead));
        memset(fraction.digits + count - lead, 0, (size_t)lead);
        fraction.count = (uint8_t)(count - lead);
        mlocSpelloutNormalize(&fraction);
    }
    *endOut = at;
    return Add(p, &fraction, base, valueOut);
}

// NumeratorSubstitution::doParse: with "<<", the zeros first (a rule
// below one), then the numerator over the power of ten past it and the
// zeros. The position ICU reports leaves the zeros out, which it then
// fails to match.
MLOC_NOINLINE static bool SubNumerator(Parser* p, const SpelloutRule* rule, const uint8_t* sub,
                                       SpelloutText text, const mlocDecimal* base, uint64_t upper,
                                       uint32_t mask, int depth, size_t* endOut,
                                       mlocDecimal* valueOut)
{
    if ((sub[1] & spelloutSubZeros) == 0 || mlocSpelloutTarget(sub) < 0)
    {
        (void)SubBasic(p, rule, sub, text, base, upper, mask, depth, endOut, valueOut);
        return true;
    }
    int zeros = 0;
    size_t at = 0;
    while (at < text.length)
    {
        size_t end = 0;
        mlocDecimal value;
        SetParse(p, (uint32_t)mlocSpelloutTarget(sub),
                 (SpelloutText){text.bytes + at, text.length - at}, 1, mask, depth, &end, &value);
        if (end == 0)
        {
            break;
        }
        zeros++;
        at += end;
        while (at < text.length && text.bytes[at] == ' ')
        {
            at++;
        }
    }
    mlocDecimal one = mlocSpelloutFromUnsigned(1);
    mlocDecimal value = Zero();
    (void)SubBasic(p, rule, sub, (SpelloutText){text.bytes + at, text.length - at}, &one, upper,
                   mask, depth, endOut, &value);
    int64_t n = GetLong(&value);
    int power = 0;
    for (int64_t d = 1; d <= n; d *= 10)
    {
        power++;
    }
    power += zeros;
    if (power > 18)
    {
        *valueOut = Zero();
        return false;
    }
    mlocDecimal numerator = mlocSpelloutFromInt64(n);
    if (!mlocSpelloutShift(&numerator, power, valueOut))
    {
        Fail(p, mloc_errorLimit);
        return false;
    }
    return true;
}

// NFSubstitution::doParse and its subclasses': whether it read, the end
// and the value composed.
static bool SubParse(Parser* p, const SpelloutRule* rule, const uint8_t* sub, SpelloutText text,
                     const mlocDecimal* base, uint64_t upper, uint32_t mask, int depth,
                     size_t* endOut, mlocDecimal* valueOut)
{
    SpelloutSubKind kind = (SpelloutSubKind)(sub[1] & spelloutSubKindMask);
    uint32_t before = mlocLoad16(sub + 4);
    if (kind == spelloutModulus && (sub[1] & spelloutSubTriple) != 0 && before != spelloutNone)
    {
        // ModulusSubstitution::doParse of ">>>": the rule before, its
        // value composed when it read.
        mlocDecimal value = Zero();
        RuleParse(p, before, text, false, upper, mask, depth + 1, endOut, &value);
        *valueOut = value;
        if (*endOut != 0)
        {
            (void)Compose(p, rule, kind, &value, base, valueOut);
        }
        return true;
    }
    if (kind == spelloutFraction && (sub[1] & spelloutSubByDigits) != 0 &&
        mlocSpelloutTarget(sub) >= 0)
    {
        return SubDigits(p, sub, text, base, mask, depth, endOut, valueOut);
    }
    if (kind == spelloutNumerator)
    {
        return SubNumerator(p, rule, sub, text, base, upper, mask, depth, endOut, valueOut);
    }
    return SubBasic(p, rule, sub, text, base, upper, mask, depth, endOut, valueOut);
}

// NFRule::matchToDelimiter: with a delimiter, each place it is found in
// turn, the text before it read by the substitution, which must read all
// of it; the last such match before one that fails is kept. Without one,
// the substitution reads what it can, or no substitution gives the base.
static mlocDecimal MatchToDelimiter(Parser* p, const SpelloutRule* rule, const SpelloutParts* parts,
                                    SpelloutText text, size_t start, const mlocDecimal* base,
                                    SpelloutText key, bool keyHoldsBlock, const uint8_t* sub,
                                    uint64_t upper, uint32_t mask, int depth, size_t* endOut)
{
    *endOut = 0;
    if (key.length > 0 || keyHoldsBlock)
    {
        mlocDecimal best = Zero();
        bool found = false;
        size_t from = start;
        for (;;)
        {
            size_t keyLength = 0;
            int64_t at = mlocFindSpelloutText(&p->r, parts, text, key, from, &keyLength);
            if (at < 0 || p->error != mloc_success)
            {
                break;
            }
            if (at > 0 && sub != nullptr)
            {
                size_t end = 0;
                mlocDecimal value;
                bool read = SubParse(p, rule, sub, (SpelloutText){text.bytes, (size_t)at}, base,
                                     upper, mask, depth, &end, &value);
                if (read && end == (size_t)at)
                {
                    *endOut = (size_t)at + keyLength;
                    best = value;
                    found = true;
                }
                else if (found)
                {
                    return best;
                }
            }
            from = (size_t)at + keyLength;
        }
        if (found)
        {
            return best;
        }
        *endOut = 0;
        return Zero();
    }
    if (sub == nullptr)
    {
        return *base;
    }
    size_t end = 0;
    mlocDecimal value;
    bool read = SubParse(p, rule, sub, text, base, upper, mask, depth, &end, &value);
    if (read && end != 0)
    {
        *endOut = end;
        return value;
    }
    return Zero();
}

// One over a rule's value: a rule of a fraction rule set without
// substitutions reads one over its base.
MLOC_NOINLINE static bool Inverse(Parser* p, mlocDecimal* value)
{
    mlocDecimal one = mlocSpelloutFromUnsigned(1);
    return Divide(p, &one, value, value);
}

// NFRule::doParse: the prefix, then the first substitution up to the
// text between the two, then the second up to the text after it. A rule
// of a fraction rule set without substitutions reads one over its base.
static void RuleParse(Parser* p, uint32_t index, SpelloutText text, bool fraction, uint64_t upper,
                      uint32_t mask, int depth, size_t* endOut, mlocDecimal* valueOut)
{
    *endOut = 0;
    *valueOut = Zero();
    if (++p->steps > maxSteps)
    {
        Fail(p, mloc_errorLimit);
        return;
    }
    if (depth >= maxFrames)
    {
        // Only a crafted file chains rules that deep without a rule set.
        Fail(p, mloc_errorFormat);
        return;
    }
    SpelloutRule rule = mlocReadSpelloutRule(&p->r, index);
    SpelloutParts parts = mlocReadSpelloutParts(&p->r, &rule);
    // stripPrefix: a prefix holding the plural block is its pattern text,
    // which spelled-out text never holds.
    SpelloutText prefix;
    bool prefixBlock = mlocSpelloutDelimiter(&p->r, &parts, 0, parts.prefixEnd, &prefix);
    size_t prefixLength = 0;
    if (!prefixBlock && prefix.length > 0 && prefix.length <= text.length &&
        memcmp(text.bytes, prefix.bytes, prefix.length) == 0)
    {
        prefixLength = prefix.length;
    }
    if (prefixLength == 0 && (prefixBlock || prefix.length > 0))
    {
        return;
    }
    if (rule.kind == spelloutInfinity || rule.kind == spelloutNaN)
    {
        *endOut = prefixLength;
        *valueOut = (mlocDecimal){.kind = rule.kind == spelloutNaN ? mloc_decimalNaN
                                                                   : mloc_decimalInfinity};
        return;
    }
    SpelloutText work = {text.bytes + prefixLength, text.length - prefixLength};
    mlocDecimal base = rule.kind == spelloutNormal ? mlocSpelloutFromUnsigned(rule.base) : Zero();
    SpelloutText middle;
    SpelloutText suffix = {nullptr, 0};
    bool middleBlock =
        parts.sub[0] != nullptr &&
        mlocSpelloutDelimiter(&p->r, &parts, parts.subAt[0] + 1, parts.middleEnd, &middle);
    if (parts.sub[0] == nullptr)
    {
        middle = (SpelloutText){nullptr, 0};
    }
    bool suffixBlock =
        parts.sub[1] != nullptr &&
        mlocSpelloutDelimiter(&p->r, &parts, parts.subAt[1] + 1, parts.pieceCount, &suffix);
    size_t first = 0;
    mlocDecimal partial = MatchToDelimiter(p, &rule, &parts, work, 0, &base, middle, middleBlock,
                                           parts.sub[0], upper, mask, depth, &first);
    if (p->error != mloc_success || (first == 0 && parts.sub[0] != nullptr))
    {
        return;
    }
    size_t second = 0;
    mlocDecimal result = MatchToDelimiter(
        p, &rule, &parts, (SpelloutText){work.bytes + first, work.length - first}, 0, &partial,
        suffix, suffixBlock, parts.sub[1], upper, mask, depth, &second);
    if (p->error != mloc_success || (second == 0 && parts.sub[1] != nullptr) ||
        prefixLength + first + second == 0)
    {
        return;
    }
    if (fraction && parts.sub[0] == nullptr && !Inverse(p, &result))
    {
        return;
    }
    *endOut = prefixLength + first + second;
    *valueOut = result;
}

// The pair of memo slots of a rule set search's inputs.
static Memo* MemoOf(Parser* p, const MemoKey* key)
{
    uint64_t hash = ((uint64_t)key->start * 0x9E3779B1u) ^ ((uint64_t)key->length * 0x85EBCA77u) ^
                    (key->upper * 0xC2B2AE3Du) ^ ((uint64_t)key->set << 7) ^
                    ((uint64_t)key->mask << 3);
    return &p->memo[(hash ^ hash >> 29) & (memoSize - memoWays)];
}

// Whether a slot holds the search of these inputs, standing at this
// depth. A span of text searched once is not searched again deeper, where
// ICU would search it again and could stop at its recursion limit:
// searched again at each depth, a long text whose search meets the limit
// is exponential. A search the limit cut short is searched again
// shallower, where more of it may be read.
static bool Holds(const Memo* memo, const MemoKey* key, int depth)
{
    return (memo->flags & memoUsed) != 0 && memo->start == key->start &&
           memo->length == key->length && memo->upper == key->upper && memo->set == key->set &&
           memo->mask == key->mask && ((memo->flags & memoLimited) == 0 || depth >= memo->depth);
}

// Whether a pair of slots holds the search of these inputs, standing at
// this depth; its result then, the limit it met counted as met.
static bool Recall(Parser* p, const Memo* pair, const MemoKey* key, int depth, size_t* endOut,
                   mlocDecimal* valueOut)
{
    for (uint32_t way = 0; way < memoWays; way++)
    {
        const Memo* memo = &pair[way];
        if (Holds(memo, key, depth))
        {
            p->limited = p->limited || (memo->flags & memoLimited) != 0;
            *endOut = memo->end;
            *valueOut = Unpack(memo);
            return true;
        }
    }
    return false;
}

// The cost bucket of a search of so many steps.
static uint32_t CostBucket(uint64_t steps)
{
    uint32_t bucket = 0;
    for (; steps >= 4 && bucket < memoCostBuckets - 1; steps >>= 2)
    {
        bucket++;
    }
    return bucket;
}

// Remembers a search's result, when its digits fit a slot: in a free slot
// of the pair, else in place of the one whose search cost less, the
// second of equal ones.
static void Remember(const Parser* p, Memo* pair, const MemoKey* key, int depth, size_t end,
                     const mlocDecimal* value, uint64_t steps)
{
    if (p->error != mloc_success || value->count > 19)
    {
        return;
    }
    Memo* memo = (pair[0].flags & memoUsed) == 0                                   ? &pair[0]
                 : (pair[1].flags & memoUsed) == 0                                 ? &pair[1]
                 : pair[0].flags >> memoCostShift < pair[1].flags >> memoCostShift ? &pair[0]
                                                                                   : &pair[1];
    uint32_t flags = memoUsed | (p->limited ? memoLimited : 0u) |
                     (value->negative != 0 ? memoNegative : 0u) |
                     (uint32_t)value->kind << memoKindShift | CostBucket(steps) << memoCostShift;
    *memo = (Memo){.upper = key->upper,
                   .exponent = value->exponent,
                   .start = (uint16_t)key->start,
                   .length = (uint16_t)key->length,
                   .end = (uint16_t)end,
                   .set = (uint16_t)key->set,
                   .mask = (uint8_t)key->mask,
                   .depth = (uint8_t)depth,
                   .flags = (uint8_t)flags};
    for (uint32_t i = 0; i < value->count; i++)
    {
        memo->digits = memo->digits * 10 + value->digits[i];
    }
}

// The rules of NFRuleSet::parse: the special rules not yet tried on this
// path (each then marked tried for the rest), then the normal rules from
// the last, below the upper bound unless the set is a fraction rule set;
// the longest match wins, the first of equal ones.
static size_t TryRules(Parser* p, uint32_t index, SpelloutText text, uint64_t upper, uint32_t mask,
                       int depth, mlocDecimal* valueOut)
{
    SpelloutSet set = mlocReadSpelloutSet(&p->r, index);
    size_t best = 0;
    for (uint32_t i = 0; i < 6 && p->error == mloc_success; i++)
    {
        int64_t rule = mlocFindSpelloutSpecial(
            &p->r, &set, (SpelloutRuleKind)(spelloutNegative + i), p->f->decimalPoint);
        if (rule == spelloutNoRule || (mask >> i & 1) != 0)
        {
            continue;
        }
        mask |= 1u << i;
        size_t end = 0;
        mlocDecimal value;
        RuleParse(p, (uint32_t)rule, text, false, upper, mask, depth + 1, &end, &value);
        if (end > best)
        {
            best = end;
            *valueOut = value;
        }
    }
    uint64_t bound = upper < maxUpper ? upper : maxUpper;
    for (uint32_t i = set.normal; i-- > 0 && best < text.length && p->error == mloc_success;)
    {
        if (!set.fraction &&
            mlocLoad64(p->r.rules + (size_t)(set.first + i) * spelloutRuleSize) >= bound)
        {
            continue;
        }
        size_t end = 0;
        mlocDecimal value;
        RuleParse(p, set.first + i, text, set.fraction, upper, mask, depth + 1, &end, &value);
        if (end > best)
        {
            best = end;
            *valueOut = value;
        }
    }
    return best;
}

// Whether a rule of a set may start reading the text, by the set's lead
// record: it reaches a pattern under the upper bound, or a special rule
// not yet tried, or a text starting with the text's first character. A
// search refused reads nothing at any depth, as ICU's would.
static bool MayStart(const Parser* p, uint32_t index, SpelloutText text, uint64_t upper,
                     uint32_t mask)
{
    const uint8_t* lead = p->r.leads + (size_t)index * spelloutLeadSize;
    uint64_t need = mlocLoad64(lead);
    if ((need != UINT64_MAX && upper >= need) || (lead[8] & ~mask) != 0)
    {
        return true;
    }
    uint32_t bit = mlocSpelloutLeadHash(text.bytes, text.length);
    return (lead[spelloutLeadBits + (bit >> 3)] >> (bit & 7) & 1) != 0;
}

// NFRuleSet::parse, its result remembered for its inputs (the text's
// place, the upper bound, the special rules tried), which decide it at
// its depth and deeper.
static void SetParse(Parser* p, uint32_t index, SpelloutText text, uint64_t upper, uint32_t mask,
                     int depth, size_t* endOut, mlocDecimal* valueOut)
{
    *endOut = 0;
    *valueOut = Zero();
    if (depth >= maxLevels)
    {
        p->limited = true;
        return;
    }
    if (text.length == 0 || !MayStart(p, index, text, upper, mask))
    {
        return;
    }
    size_t start = (size_t)(text.bytes - p->origin);
    MemoKey key = {upper, index, (uint32_t)start, (uint32_t)text.length, mask};
    Memo* pair = start + text.length <= UINT16_MAX ? MemoOf(p, &key) : nullptr;
    if (pair != nullptr && Recall(p, pair, &key, depth, endOut, valueOut))
    {
        return;
    }
    bool limited = p->limited;
    uint64_t steps = p->steps;
    p->limited = false;
    *endOut = TryRules(p, index, text, upper, mask, depth, valueOut);
    if (pair != nullptr)
    {
        Remember(p, pair, &key, depth, *endOut, valueOut, p->steps - steps);
    }
    p->limited = limited || p->limited;
}

mlocResult mlocReadSpellout(const SpelloutFormat* format, const uint8_t* text, size_t length,
                            mlocDecimal* valueOut, size_t* endOut)
{
    static_assert(sizeof(Memo) == 32 && mloc_decimalNaN < 4, "a memo slot stays 32 bytes");
    Parser p;
    p.f = format;
    p.r = mlocReadSpelloutRules((DataSpan){format->group, format->groupSize});
    p.origin = text;
    p.steps = 0;
    p.error = mloc_success;
    p.limited = false;
    for (uint32_t i = 0; i < memoSize; i++)
    {
        p.memo[i].flags = 0;
    }
    *endOut = 0;
    if ((p.r.flags & spelloutGroupUnparseable) != 0)
    {
        return mloc_errorSyntax;
    }
    size_t best = 0;
    mlocDecimal value = Zero();
    for (uint32_t s = 0; s < p.r.setCount && best < length && p.error == mloc_success; s++)
    {
        const uint8_t* set = p.r.sets + (size_t)s * spelloutSetSize;
        if ((set[3] & spelloutSetPublic) == 0 || (set[3] & spelloutSetParseable) == 0)
        {
            continue;
        }
        size_t end = 0;
        mlocDecimal read;
        SetParse(&p, s, (SpelloutText){text, length}, noUpper, 0, 0, &end, &read);
        if (end > best)
        {
            best = end;
            value = read;
        }
    }
    if (p.error != mloc_success)
    {
        return p.error;
    }
    if (best == 0)
    {
        return mloc_errorSyntax;
    }
    mlocSpelloutNormalize(&value);
    if (value.kind == mloc_decimalFinite && value.count == 0)
    {
        value.negative = 0;
    }
    *valueOut = value;
    *endOut = best;
    return mloc_success;
}
