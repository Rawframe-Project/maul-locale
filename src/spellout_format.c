// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out engine (spellout_format.h). Each function follows the
// ICU4C 79 function named in its comment. ICU runs a value that fits
// int64_t on its integer path and anything else as a double; here the
// double path runs on the exact decimal, with the same rules for which
// rule applies and what each substitution passes on.

#include "spellout_format.h"

#include "bytes.h"
#include "decimal.h"
#include "number_precision.h"
#include "plural.h"
#include "spellout_rule.h"
#include "spellout_value.h"

#include <string.h>

enum
{
    // ICU's RECURSION_LIMIT: rule sets nested past it are an error.
    maxLevels = 64,
    // The places a fraction is written digit by digit to.
    fractionPlaces = 20,
    // Rules a ">>>" or infinity calls directly also nest; a crafted file
    // could chain them without end.
    maxFrames = 192,
};

typedef struct Engine
{
    const SpelloutFormat* f;
    SpelloutRules r;
    NumberWriter* w;
    int levels;
    int frames;
    mlocResult error;
    // The text's first code point when a rule's text or a symbol begins
    // it, else 0.
    uint32_t first;
} Engine;

typedef SpelloutRule Rule;
typedef SpelloutSet Set;

static Rule ReadRule(const Engine* e, uint32_t index)
{
    return mlocReadSpelloutRule(&e->r, index);
}

static Set ReadSet(const Engine* e, uint32_t index)
{
    return mlocReadSpelloutSet(&e->r, index);
}

static bool Fail(Engine* e, mlocResult error)
{
    if (e->error == mloc_success)
    {
        e->error = error;
    }
    return false;
}

uint32_t mlocSpelloutFirstCodePoint(const uint8_t* text, size_t length)
{
    if (length == 0)
    {
        return 0;
    }
    uint8_t c = text[0];
    uint32_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    if (n > length)
    {
        return 0;
    }
    uint32_t cp = n == 1 ? c : c & (0x7Fu >> n);
    for (uint32_t i = 1; i < n; i++)
    {
        cp = cp << 6 | (text[i] & 0x3Fu);
    }
    return cp;
}

static void Write(Engine* e, const uint8_t* text, size_t length)
{
    if (e->w->length == 0 && length > 0)
    {
        e->first = mlocSpelloutFirstCodePoint(text, length);
    }
    mlocWriteLiteral(e->w, (const char*)text, length);
}

// What a rule search compares: a magnitude of 64 bits or a larger
// integer.
typedef struct Magnitude
{
    bool big;
    uint64_t small;
    const mlocDecimal* value;
} Magnitude;

static bool Divisible(const Magnitude* m, const Rule* rule)
{
    if (!m->big)
    {
        return m->small % rule->divisor == 0;
    }
    if (rule->power >= 0)
    {
        return m->value->count == 0 || m->value->exponent >= rule->power;
    }
    return mlocSpelloutMagnitudeMod(m->value, rule->divisor) == 0;
}

// NFRuleSet::findNormalRule: the last rule whose base is at most the
// magnitude (an equal base taken at once), the one before it for ICU's
// rollback; a set without normal rules takes its default rule.
static int64_t FindNormal(const Engine* e, const Set* set, const Magnitude* m)
{
    if (set->normal == 0)
    {
        for (uint32_t i = 0; i < set->special; i++)
        {
            if (ReadRule(e, set->first + i).kind == spelloutDefault)
            {
                return set->first + i;
            }
        }
        return spelloutNoRule;
    }
    uint32_t lo = 0;
    uint32_t hi = set->normal;
    while (lo < hi)
    {
        uint32_t mid = (lo + hi) / 2;
        uint64_t base = mlocLoad64(e->r.rules + (size_t)(set->first + mid) * spelloutRuleSize);
        if (!m->big && base == m->small)
        {
            return set->first + mid;
        }
        if (!m->big && base > m->small)
        {
            hi = mid;
        }
        else
        {
            lo = mid + 1;
        }
    }
    if (hi == 0)
    {
        return spelloutNoRule;
    }
    Rule rule = ReadRule(e, set->first + hi - 1);
    if (rule.rollback && Divisible(m, &rule))
    {
        return hi == 1 ? spelloutNoRule : (int64_t)(set->first + hi - 2);
    }
    return set->first + hi - 1;
}

static int64_t FindSpecial(const Engine* e, const Set* set, SpelloutRuleKind kind)
{
    return mlocFindSpelloutSpecial(&e->r, set, kind, e->f->decimalPoint);
}

// NFRuleSet::findFractionRuleSetRule, its bases powers of ten: the
// numerator over the largest base, rounded, makes the first base whose
// fraction it is exact the winner; of two equal bases, the second unless
// the numerator is one (base times the value below 0.5 or at least 2).
static int64_t FindFraction(const Engine* e, const Set* set, const mlocDecimal* x)
{
    if (set->normal == 0)
    {
        return spelloutNoRule;
    }
    Rule last = ReadRule(e, set->first + set->normal - 1);
    int largest = last.power >= 0 ? last.power : 0;
    // A value too large to scale is a whole number of every base.
    mlocDecimal magnitude = mlocSpelloutAbsolute(x);
    mlocDecimal scaled;
    int64_t zeros = largest;
    if (mlocSpelloutShift(&magnitude, -largest, &scaled))
    {
        mlocDecimal numerator = mlocSpelloutRound(&scaled, mloc_roundHalfCeil);
        zeros = numerator.count == 0 ? largest : numerator.exponent;
    }
    uint32_t winner = set->normal - 1;
    for (uint32_t i = 0; i < set->normal; i++)
    {
        Rule rule = ReadRule(e, set->first + i);
        if (rule.power >= 0 && zeros + rule.power >= largest)
        {
            winner = i;
            break;
        }
    }
    if (winner + 1 < set->normal)
    {
        Rule rule = ReadRule(e, set->first + winner);
        Rule next = ReadRule(e, set->first + winner + 1);
        if (next.base == rule.base && rule.power >= 0)
        {
            mlocDecimal n;
            mlocDecimal half = {.digits = {5}, .count = 1, .exponent = -1, .fractionDigits = 1};
            mlocDecimal two = mlocSpelloutFromUnsigned(2);
            bool large = !mlocSpelloutShift(&magnitude, -rule.power, &n);
            winner +=
                large || mlocSpelloutCompare(&n, &half) < 0 || mlocSpelloutCompare(&n, &two) >= 0
                    ? 1
                    : 0;
        }
    }
    return set->first + winner;
}

static bool Enter(Engine* e, bool level)
{
    if (e->error != mloc_success)
    {
        return false;
    }
    if ((level && e->levels >= maxLevels) || e->frames >= maxFrames)
    {
        return Fail(e, mloc_errorCapacity);
    }
    e->levels += level ? 1 : 0;
    e->frames++;
    return true;
}

static void Leave(Engine* e, bool level)
{
    e->levels -= level ? 1 : 0;
    e->frames--;
}

static void SetInt(Engine* e, uint32_t set, int64_t n);
static void SetDec(Engine* e, uint32_t set, const mlocDecimal* x);
static void PassOn(Engine* e, int64_t target, const mlocDecimal* t);
static void RuleInt(Engine* e, uint32_t rule, int64_t n);
static void RuleDec(Engine* e, uint32_t rule, const mlocDecimal* x);

// The text of a symbol of the locale (NaN, infinity), which ICU's default
// rules write.
static void WriteSymbol(Engine* e, NumberSymbol symbol)
{
    const NumberCore* c = &e->f->core;
    Write(e, c->symbolText + c->symbolStart[symbol], c->symbolLength[symbol]);
}

// A decimal pattern's text of a value, as DecimalFormat writes it: half
// even to the pattern's fraction digits, its zeros, grouping and point,
// the locale's symbols.
static void WritePattern(Engine* e, uint32_t index, const mlocDecimal* x)
{
    const uint8_t* p = e->r.patterns + (size_t)index * spelloutPatternSize;
    mlocDecimal d = *x;
    if (d.kind == mloc_decimalFinite)
    {
        mlocSpelloutNormalize(&d);
        mlocPrecision precision = {
            .kind = mloc_precisionFraction,
            .mode = mloc_roundHalfEven,
            .minFractionDigits = p[1],
            .maxFractionDigits = p[2],
        };
        if (mlocApplyPrecision(&precision, &d) != mloc_success)
        {
            Fail(e, mloc_errorCapacity);
            return;
        }
    }
    NumberCore core = e->f->core;
    core.primary = p[3];
    core.secondary = p[4];
    core.minGrouping = 1;
    NumberLayout layout = {
        .grouping = p[3] > 0 ? mloc_groupingAlways : mloc_groupingOff,
        .minInteger = p[0],
        .point = (p[5] & spelloutPatternPoint) != 0,
    };
    mlocWriteNumber(&core, &d, mlocNumberSign(&d, mloc_signAuto), &layout, e->w);
}

static void WritePlural(Engine* e, const uint8_t* piece, const mlocDecimal* operand)
{
    const uint8_t* plural = e->r.plurals + (size_t)mlocLoad16(piece + 2) * spelloutPluralSize;
    mlocDecimal magnitude = mlocSpelloutAbsolute(operand);
    mlocPluralOperands operands;
    mlocFillPluralOperands(&magnitude, &operands);
    mlocPluralCategory category =
        mlocSelectPluralRule(piece[1] != 0 ? e->f->ordinal : e->f->cardinal, &operands);
    if (mlocLoad16(plural + 4 * category + 2) == spelloutNone)
    {
        category = mloc_pluralOther;
    }
    Write(e, e->r.text + mlocLoad16(plural + 4 * category), mlocLoad16(plural + 4 * category + 2));
}

static bool HasModulus(const Engine* e, const Rule* rule)
{
    for (uint32_t i = 0; i < rule->pieces; i++)
    {
        const uint8_t* piece = e->r.pieces + (size_t)(rule->firstPiece + i) * spelloutPieceSize;
        if (piece[0] == spelloutSubstitution && (piece[1] & spelloutSubKindMask) == spelloutModulus)
        {
            return true;
        }
    }
    return false;
}

// NFSubstitution::doSubstitution(int64_t) and its subclasses'.
static void SubInt(Engine* e, const Rule* rule, const uint8_t* piece, int64_t n)
{
    if (rule->divisor > (uint64_t)INT64_MAX)
    {
        Fail(e, mloc_errorCapacity);
        return;
    }
    int64_t d = (int64_t)rule->divisor;
    int64_t t = n;
    int64_t target = mlocSpelloutTarget(piece);
    switch ((SpelloutSubKind)(piece[1] & spelloutSubKindMask))
    {
    case spelloutMultiplier:
        t = n / d;
        break;
    case spelloutModulus:
        t = n % d;
        if ((piece[1] & spelloutSubTriple) != 0 && mlocLoad16(piece + 4) != spelloutNone)
        {
            RuleInt(e, mlocLoad16(piece + 4), t);
            return;
        }
        break;
    case spelloutFraction:
        t = 0;
        break;
    case spelloutAbsolute:
        t = n < 0 ? -n : n;
        break;
    case spelloutNumerator:
        // NumeratorSubstitution writes nothing for an integer.
        return;
    default:
        break;
    }
    if (target < 0)
    {
        mlocDecimal x = mlocSpelloutFromInt64(t);
        WritePattern(e, (uint32_t)(-1 - target), &x);
    }
    else
    {
        SetInt(e, (uint32_t)target, t);
    }
}

// A transformed value passed on (NFSubstitution::doSubstitution(double)):
// infinity to the target set's infinity rule, an integer on the integer
// path when it fits, anything else on this one, or to the pattern.
static void PassOn(Engine* e, int64_t target, const mlocDecimal* t)
{
    if (target < 0)
    {
        WritePattern(e, (uint32_t)(-1 - target), t);
        return;
    }
    if (t->kind == mloc_decimalInfinity)
    {
        Set set = ReadSet(e, (uint32_t)target);
        int64_t rule = FindSpecial(e, &set, spelloutInfinity);
        if (rule == spelloutNoRule)
        {
            WriteSymbol(e, numberInfinity);
        }
        else
        {
            RuleDec(e, (uint32_t)rule, t);
        }
        return;
    }
    int64_t n = 0;
    if (mlocSpelloutToInt64(t, &n))
    {
        SetInt(e, (uint32_t)target, n);
    }
    else
    {
        SetDec(e, (uint32_t)target, t);
    }
}

// FractionalPartSubstitution digit by digit: each digit after the point
// of the value's magnitude rounded half even to 20 places, as ICU rounds
// it (it reads the digits of the number itself, not of number -
// floor(number)), by the set, from the first, spaced unless ">>>"; none
// writes zero. The rounding also bounds the digits a value can ask for.
static void FractionDigits(Engine* e, const uint8_t* piece, const mlocDecimal* x)
{
    mlocDecimal magnitude = mlocSpelloutAbsolute(x);
    (void)mlocRoundDecimal(&magnitude, -fractionPlaces, 1, mloc_roundHalfEven);
    mlocSpelloutNormalize(&magnitude);
    mlocDecimal fractionValue;
    (void)mlocSpelloutFraction(&magnitude, &fractionValue);
    const mlocDecimal* fraction = &fractionValue;
    int64_t target = mlocSpelloutTarget(piece);
    if (target < 0)
    {
        return;
    }
    if (mlocSpelloutIsZero(fraction))
    {
        SetInt(e, (uint32_t)target, 0);
        return;
    }
    bool spaced = (piece[1] & spelloutSubTriple) == 0;
    for (int64_t power = -1; power >= fraction->exponent && e->error == mloc_success; power--)
    {
        if (power < -1 && spaced)
        {
            Write(e, (const uint8_t*)" ", 1);
        }
        SetInt(e, (uint32_t)target, mlocDigitAt(fraction, power));
    }
}

// NumeratorSubstitution: the numerator, rounded half up, after a zero
// for each place it is short of the base when it is written "<<".
static void Numerator(Engine* e, const Rule* rule, const uint8_t* piece, const mlocDecimal* x)
{
    mlocDecimal scaled;
    if (!mlocSpelloutShift(x, rule->power >= 0 ? -rule->power : 0, &scaled))
    {
        Fail(e, mloc_errorCapacity);
        return;
    }
    mlocDecimal t = mlocSpelloutRound(&scaled, mloc_roundHalfCeil);
    int64_t target = mlocSpelloutTarget(piece);
    uint64_t numerator = 0;
    if ((piece[1] & spelloutSubZeros) != 0 && target >= 0 &&
        mlocSpelloutMagnitude64(&t, &numerator) && numerator != 0)
    {
        for (uint64_t nf = numerator;
             nf < rule->base / 10 + (rule->base % 10 != 0) && e->error == mloc_success; nf *= 10)
        {
            SetInt(e, (uint32_t)target, 0);
            Write(e, (const uint8_t*)" ", 1);
        }
    }
    PassOn(e, target, &t);
}

// NFSubstitution::doSubstitution(double) and its subclasses', exact.
static void SubDec(Engine* e, const Rule* rule, const uint8_t* piece, const mlocDecimal* x)
{
    int64_t target = mlocSpelloutTarget(piece);
    mlocDecimal t = *x;
    bool done = true;
    switch ((SpelloutSubKind)(piece[1] & spelloutSubKindMask))
    {
    case spelloutMultiplier:
        done = target >= 0 || HasModulus(e, rule) ? mlocSpelloutDivideFloor(x, rule->div, &t)
                                                  : mlocSpelloutDivideExact(x, rule->div, &t);
        break;
    case spelloutModulus:
        done = mlocSpelloutModulo(x, rule->div, &t);
        if (done && (piece[1] & spelloutSubTriple) != 0 && mlocLoad16(piece + 4) != spelloutNone)
        {
            RuleDec(e, mlocLoad16(piece + 4), &t);
            return;
        }
        break;
    case spelloutIntegral:
        t = mlocSpelloutRound(x, mloc_roundFloor);
        break;
    case spelloutFraction:
        if ((piece[1] & spelloutSubByDigits) != 0)
        {
            FractionDigits(e, piece, x);
            return;
        }
        done = mlocSpelloutFraction(x, &t);
        break;
    case spelloutAbsolute:
        t = mlocSpelloutAbsolute(x);
        break;
    case spelloutNumerator:
        Numerator(e, rule, piece, x);
        return;
    default:
        break;
    }
    if (!done)
    {
        Fail(e, mloc_errorCapacity);
        return;
    }
    PassOn(e, target, &t);
}

// A plural block's operand: the value divided by the rule's divisor, a
// fraction's numerator ICU's round(number * divisor); false when it does
// not fit.
static bool PluralOperand(const Rule* rule, int64_t n, const mlocDecimal* x, mlocDecimal* out)
{
    if (x == nullptr)
    {
        if (rule->divisor > (uint64_t)INT64_MAX)
        {
            return false;
        }
        *out = mlocSpelloutFromInt64(n / (int64_t)rule->divisor);
        return true;
    }
    if (x->negative == 0 && mlocSpelloutCompare(x, &(mlocDecimal){.digits = {1}, .count = 1}) < 0)
    {
        mlocDecimal scaled;
        if (!mlocSpelloutShift(x, rule->power >= 0 ? -rule->power : 0, &scaled))
        {
            return false;
        }
        *out = mlocSpelloutRound(&scaled, mloc_roundHalfCeil);
        return true;
    }
    return mlocSpelloutDivideTrunc(x, rule->div, out);
}

// The pieces of a rule, written in order; plural blocks by an operand.
static void Pieces(Engine* e, const Rule* rule, int64_t n, const mlocDecimal* x)
{
    for (uint32_t i = 0; i < rule->pieces && e->error == mloc_success; i++)
    {
        const uint8_t* piece = e->r.pieces + (size_t)(rule->firstPiece + i) * spelloutPieceSize;
        switch ((SpelloutPieceType)piece[0])
        {
        case spelloutText:
            Write(e, e->r.text + mlocLoad16(piece + 4), mlocLoad16(piece + 2));
            break;
        case spelloutPlural:
        {
            mlocDecimal operand;
            if (!PluralOperand(rule, n, x, &operand))
            {
                Fail(e, mloc_errorCapacity);
                return;
            }
            WritePlural(e, piece, &operand);
            break;
        }
        case spelloutSubstitution:
            if (x == nullptr)
            {
                SubInt(e, rule, piece, n);
            }
            else
            {
                SubDec(e, rule, piece, x);
            }
            break;
        default:
            break;
        }
    }
}

// NFRule::doFormat(int64_t).
static void RuleInt(Engine* e, uint32_t rule, int64_t n)
{
    if (!Enter(e, false))
    {
        return;
    }
    Rule r = ReadRule(e, rule);
    Pieces(e, &r, n, nullptr);
    Leave(e, false);
}

// NFRule::doFormat(double), exact.
static void RuleDec(Engine* e, uint32_t rule, const mlocDecimal* x)
{
    if (!Enter(e, false))
    {
        return;
    }
    Rule r = ReadRule(e, rule);
    Pieces(e, &r, 0, x);
    Leave(e, false);
}

// NFRuleSet::format(int64_t): the negative rule for a negative value, else
// the rule of its magnitude, which formats the value as it is.
static void SetInt(Engine* e, uint32_t set, int64_t n)
{
    if (!Enter(e, true))
    {
        return;
    }
    Set s = ReadSet(e, set);
    int64_t rule = spelloutNoRule;
    if (s.fraction)
    {
        mlocDecimal x = mlocSpelloutFromInt64(n);
        rule = FindFraction(e, &s, &x);
    }
    else
    {
        rule = n < 0 ? FindSpecial(e, &s, spelloutNegative) : spelloutNoRule;
        if (rule == spelloutNoRule)
        {
            Magnitude m = {.small = n < 0 ? 0 - (uint64_t)n : (uint64_t)n};
            rule = FindNormal(e, &s, &m);
        }
    }
    if (rule != spelloutNoRule)
    {
        RuleInt(e, (uint32_t)rule, n);
    }
    Leave(e, true);
}

// NFRuleSet::findDoubleRule: NaN, the negative rule, infinity, a fraction
// rule for a value not an integer (the proper one below 1), the default
// rule, else the rule of the magnitude rounded half up.
static int64_t FindDouble(const Engine* e, const Set* s, const mlocDecimal* x, bool* symbol)
{
    *symbol = false;
    if (x->kind == mloc_decimalNaN)
    {
        int64_t rule = FindSpecial(e, s, spelloutNaN);
        *symbol = rule == spelloutNoRule;
        return rule;
    }
    if (x->negative != 0 && !mlocSpelloutIsZero(x))
    {
        int64_t rule = FindSpecial(e, s, spelloutNegative);
        if (rule != spelloutNoRule)
        {
            return rule;
        }
    }
    if (x->kind == mloc_decimalInfinity)
    {
        int64_t rule = FindSpecial(e, s, spelloutInfinity);
        *symbol = rule == spelloutNoRule;
        return rule;
    }
    mlocDecimal magnitude = mlocSpelloutAbsolute(x);
    if (!mlocSpelloutIsInteger(&magnitude))
    {
        mlocDecimal one = mlocSpelloutFromUnsigned(1);
        int64_t rule = mlocSpelloutCompare(&magnitude, &one) < 0 ? FindSpecial(e, s, spelloutProper)
                                                                 : spelloutNoRule;
        rule = rule == spelloutNoRule ? FindSpecial(e, s, spelloutImproper) : rule;
        if (rule != spelloutNoRule)
        {
            return rule;
        }
    }
    int64_t rule = FindSpecial(e, s, spelloutDefault);
    if (rule != spelloutNoRule)
    {
        return rule;
    }
    mlocDecimal rounded = mlocSpelloutRound(&magnitude, mloc_roundHalfCeil);
    Magnitude m = {.value = &rounded};
    m.big = !mlocSpelloutMagnitude64(&rounded, &m.small);
    return FindNormal(e, s, &m);
}

// NFRuleSet::format(double), exact.
static void SetDec(Engine* e, uint32_t set, const mlocDecimal* x)
{
    if (!Enter(e, true))
    {
        return;
    }
    Set s = ReadSet(e, set);
    bool symbol = false;
    int64_t rule = s.fraction ? FindFraction(e, &s, x) : FindDouble(e, &s, x, &symbol);
    if (symbol)
    {
        WriteSymbol(e, x->kind == mloc_decimalNaN ? numberNaN : numberInfinity);
    }
    else if (rule != spelloutNoRule)
    {
        RuleDec(e, (uint32_t)rule, x);
    }
    Leave(e, true);
}

mlocResult mlocWriteSpellout(const SpelloutFormat* format, const mlocDecimal* decimal,
                             NumberWriter* writer, uint32_t* firstOut)
{
    Engine e = {
        .f = format,
        .r = mlocReadSpelloutRules((DataSpan){format->group, format->groupSize}),
        .w = writer,
    };
    mlocDecimal x = *decimal;
    mlocSpelloutNormalize(&x);
    int64_t n = 0;
    if (mlocSpelloutToInt64(&x, &n))
    {
        SetInt(&e, format->set, n);
    }
    else
    {
        SetDec(&e, format->set, &x);
    }
    *firstOut = e.first;
    return e.error;
}
