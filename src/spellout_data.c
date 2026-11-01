// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out part's units (spellout_data.h).

#include "spellout_data.h"

#include "bytes.h"
#include "number_data.h"

#include <string.h>

static bool InText(const SpelloutRules* r, uint32_t offset, uint32_t length)
{
    return offset <= r->textSize && length <= r->textSize - offset;
}

// A set's rules, at least one: its normal rules of the normal kind,
// bases ascending (equal ones allowed in a fraction rule set), then its
// special ones.
static bool ValidSetRules(const SpelloutRules* r, const uint8_t* set)
{
    uint32_t first = mlocLoad16(set + 4);
    uint32_t normal = mlocLoad16(set + 6);
    uint32_t special = mlocLoad16(set + 8);
    if (first > r->ruleCount || normal + special == 0 || normal + special > r->ruleCount - first)
    {
        return false;
    }
    bool fraction = (set[3] & spelloutSetFraction) != 0;
    for (uint32_t i = 0; i < normal + special; i++)
    {
        const uint8_t* rule = r->rules + (size_t)(first + i) * spelloutRuleSize;
        bool isNormal = rule[12] == spelloutNormal;
        if (isNormal != (i < normal))
        {
            return false;
        }
        if (i > 0 && i < normal)
        {
            uint64_t base = mlocLoad64(rule);
            uint64_t before = mlocLoad64(rule - spelloutRuleSize);
            if (fraction ? base < before : base <= before)
            {
                return false;
            }
        }
    }
    return true;
}

// Each set's rules, and its lead record naming special rule kinds alone.
static bool ValidSets(const SpelloutRules* r)
{
    for (uint32_t s = 0; s < r->setCount; s++)
    {
        const uint8_t* set = r->sets + (size_t)s * spelloutSetSize;
        const uint8_t* lead = r->leads + (size_t)s * spelloutLeadSize;
        if (set[2] == 0 || !InText(r, mlocLoad16(set), set[2]) ||
            (set[3] & ~(spelloutSetPublic | spelloutSetFraction | spelloutSetParseable)) != 0 ||
            !ValidSetRules(r, set) || lead[8] >> (spelloutKindCount - spelloutNegative) != 0 ||
            lead[9] != 0)
        {
            return false;
        }
    }
    return true;
}

// A rule's divisor: ten, or a normal rule's radix, to its exponent; 0
// when it passes 64 bits.
uint64_t mlocSpelloutDivisor(const uint8_t* rule)
{
    if ((rule[13] & spelloutPowerOfTen) != 0)
    {
        return mlocSpelloutTen(rule[15]);
    }
    uint64_t radix = mlocLoad16(rule + 10);
    uint64_t divisor = 1;
    for (uint32_t k = 0; k < rule[15]; k++)
    {
        if (divisor > UINT64_MAX / radix)
        {
            return 0;
        }
        divisor *= radix;
    }
    return divisor;
}

static bool ValidRules(const SpelloutRules* r)
{
    for (uint32_t i = 0; i < r->ruleCount; i++)
    {
        const uint8_t* rule = r->rules + (size_t)i * spelloutRuleSize;
        uint32_t firstPiece = mlocLoad16(rule + 8);
        bool normal = rule[12] == spelloutNormal;
        bool power = (rule[13] & spelloutPowerOfTen) != 0;
        // A special rule divides by one; a normal one's radix is at least
        // two when it is not ten.
        bool divides = normal ? power || mlocLoad16(rule + 10) >= 2 : power && rule[15] == 0;
        if (rule[12] >= spelloutKindCount || !divides || mlocSpelloutDivisor(rule) == 0 ||
            (rule[13] & ~(spelloutRollback | spelloutPowerOfTen)) != 0 ||
            firstPiece > r->pieceCount || rule[14] > r->pieceCount - firstPiece)
        {
            return false;
        }
    }
    return true;
}

static bool ValidSubstitution(const SpelloutRules* r, const uint8_t* piece)
{
    uint8_t detail = piece[1];
    uint32_t index = mlocLoad16(piece + 2);
    uint32_t rule = mlocLoad16(piece + 4);
    if ((detail & spelloutSubKindMask) >= spelloutSubKindCount || (detail & 0x80) != 0)
    {
        return false;
    }
    uint32_t limit = (detail & spelloutSubPattern) != 0 ? r->patternCount : r->setCount;
    return index < limit && (rule == spelloutNone || rule < r->ruleCount);
}

static bool ValidPieces(const SpelloutRules* r)
{
    for (uint32_t i = 0; i < r->pieceCount; i++)
    {
        const uint8_t* piece = r->pieces + (size_t)i * spelloutPieceSize;
        uint32_t index = mlocLoad16(piece + 2);
        bool valid = false;
        switch (piece[0])
        {
        case spelloutText:
            valid = piece[1] == 0 && index > 0 && InText(r, mlocLoad16(piece + 4), index);
            break;
        case spelloutSubstitution:
            valid = ValidSubstitution(r, piece);
            break;
        case spelloutPlural:
            valid = piece[1] <= 1 && index < r->pluralCount && mlocLoad16(piece + 4) == 0;
            break;
        default:
            break;
        }
        if (!valid)
        {
            return false;
        }
    }
    return true;
}

// Each plural block's forms in the text, other's present, and its order
// naming each form once and nothing else.
static bool ValidPlurals(const SpelloutRules* r)
{
    for (uint32_t i = 0; i < r->pluralCount; i++)
    {
        const uint8_t* plural = r->plurals + (size_t)i * spelloutPluralSize;
        uint32_t present = 0;
        for (uint32_t c = 0; c < 6; c++)
        {
            uint32_t length = mlocLoad16(plural + 4 * c + 2);
            bool none = length == spelloutNone;
            if ((none && c == 5) || (!none && !InText(r, mlocLoad16(plural + 4 * c), length)))
            {
                return false;
            }
            present |= none ? 0 : 1u << c;
        }
        uint32_t order = mlocLoad32(plural + 24);
        uint32_t named = 0;
        uint32_t k = 0;
        for (; k < 8 && (order >> (4 * k) & 0xF) != 0xF; k++)
        {
            uint32_t c = order >> (4 * k) & 0xF;
            if (c >= 6 || (named & 1u << c) != 0)
            {
                return false;
            }
            named |= 1u << c;
        }
        if (named != present || (k < 8 && order >> (4 * k) != 0xFFFFFFFFu >> (4 * k)))
        {
            return false;
        }
    }
    return true;
}

// The substitution kinds a rule of a set can hold, as ICU's
// NFSubstitution::makeSubstitution makes them: "<<" an integral part in
// a fraction rule, a numerator in a fraction rule set, else a multiplier
// (none in a negative rule); ">>" the absolute value in a negative rule,
// a fractional part in a fraction rule, else a modulus (none in a
// fraction rule set); "=" the same value in any.
static uint32_t KindsOf(uint8_t rule, bool fractionSet)
{
    uint32_t same = 1u << spelloutSame;
    if (rule == spelloutNegative)
    {
        return same | 1u << spelloutAbsolute;
    }
    if (rule == spelloutImproper || rule == spelloutProper || rule == spelloutDefault)
    {
        return same | 1u << spelloutIntegral | 1u << spelloutFraction;
    }
    return fractionSet ? same | 1u << spelloutNumerator
                       : same | 1u << spelloutMultiplier | 1u << spelloutModulus;
}

// Each set's rules hold only substitutions their kind and the set's can
// make: a fractional part of a value without digits (a negative rule's
// infinity) would have no places to take.
static bool ValidKinds(const SpelloutRules* r)
{
    for (uint32_t s = 0; s < r->setCount; s++)
    {
        const uint8_t* set = r->sets + (size_t)s * spelloutSetSize;
        uint32_t first = mlocLoad16(set + 4);
        uint32_t count = mlocLoad16(set + 6) + (uint32_t)mlocLoad16(set + 8);
        for (uint32_t i = first; i < first + count; i++)
        {
            const uint8_t* rule = r->rules + (size_t)i * spelloutRuleSize;
            uint32_t kinds = KindsOf(rule[12], (set[3] & spelloutSetFraction) != 0);
            const uint8_t* piece = r->pieces + (size_t)mlocLoad16(rule + 8) * spelloutPieceSize;
            for (uint32_t k = 0; k < rule[14]; k++, piece += spelloutPieceSize)
            {
                if (piece[0] == spelloutSubstitution &&
                    (kinds & 1u << (piece[1] & spelloutSubKindMask)) == 0)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool ValidPatterns(const SpelloutRules* r)
{
    for (uint32_t i = 0; i < r->patternCount; i++)
    {
        const uint8_t* p = r->patterns + (size_t)i * spelloutPatternSize;
        if (p[0] < 1 || p[0] > 9 || p[1] > p[2] || p[2] > 20 || (p[3] == 0) != (p[4] == 0) ||
            p[5] > spelloutPatternPoint)
        {
            return false;
        }
    }
    return true;
}

bool mlocValidSpelloutRules(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < spelloutHeaderSize)
    {
        return false;
    }
    SpelloutRules r = mlocReadSpelloutRules(payload);
    uint64_t size =
        spelloutHeaderSize + (uint64_t)r.setCount * (spelloutSetSize + spelloutLeadSize) +
        (uint64_t)r.ruleCount * spelloutRuleSize + (uint64_t)r.pieceCount * spelloutPieceSize +
        (uint64_t)r.pluralCount * spelloutPluralSize +
        (uint64_t)r.patternCount * spelloutPatternSize + r.textSize;
    return size == payload.size && r.setCount > 0 && r.defaultSet < r.setCount &&
           (mlocLoad16(payload.bytes + 14) & ~spelloutGroupUnparseable) == 0 &&
           mlocValidDataText(r.text, r.textSize, 0) && ValidSets(&r) && ValidRules(&r) &&
           ValidPieces(&r) && ValidKinds(&r) && ValidPlurals(&r) && ValidPatterns(&r);
}

// Whether one id sorts before another, byte by byte.
static bool Before(const uint8_t* a, uint32_t aLength, const uint8_t* b, uint32_t bLength)
{
    uint32_t n = aLength < bLength ? aLength : bLength;
    int order = memcmp(a, b, n);
    return order < 0 || (order == 0 && aLength < bLength);
}

bool mlocValidSpelloutSystems(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < spelloutSystemHeaderSize)
    {
        return false;
    }
    uint32_t count = mlocLoad16(payload.bytes);
    uint32_t textSize = mlocLoad16(payload.bytes + 2);
    const uint8_t* records = payload.bytes + spelloutSystemHeaderSize;
    const uint8_t* text = records + (size_t)count * spelloutSystemSize;
    if (spelloutSystemHeaderSize + (uint64_t)count * spelloutSystemSize + textSize !=
            payload.size ||
        !mlocValidDataText(text, textSize, 0))
    {
        return false;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* s = records + (size_t)i * spelloutSystemSize;
        for (uint32_t k = 0; k < 3; k++)
        {
            uint32_t offset = mlocLoad16(s + 2 * k);
            if (offset > textSize || s[6 + k] > textSize - offset)
            {
                return false;
            }
        }
        if (s[6] == 0 || s[8] == 0 || s[9] >= spelloutGroupCount)
        {
            return false;
        }
        const uint8_t* before = s - spelloutSystemSize;
        if (i > 0 && !Before(text + mlocLoad16(before), before[6], text + mlocLoad16(s), s[6]))
        {
            return false;
        }
    }
    return true;
}

bool mlocValidSpelloutTitlecase(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < spelloutTitlecaseHeaderSize)
    {
        return false;
    }
    uint32_t count = mlocLoad16(payload.bytes);
    uint32_t textSize = mlocLoad16(payload.bytes + 2);
    const uint8_t* records = payload.bytes + spelloutTitlecaseHeaderSize;
    const uint8_t* text = records + (size_t)count * spelloutTitlecaseSize;
    if (spelloutTitlecaseHeaderSize + (uint64_t)count * spelloutTitlecaseSize + textSize !=
        payload.size)
    {
        return false;
    }
    uint32_t last = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* t = records + (size_t)i * spelloutTitlecaseSize;
        uint32_t codePoint = mlocLoad32(t);
        uint32_t offset = mlocLoad16(t + 4);
        if ((i > 0 && codePoint <= last) || codePoint > 0x10FFFF || t[6] == 0 || t[7] != 0 ||
            offset > textSize || t[6] > textSize - offset ||
            !mlocValidDataText(text + offset, t[6], 0))
        {
            return false;
        }
        last = codePoint;
    }
    return true;
}

bool mlocSpelloutTitlecase(DataSpan unit, uint32_t codePoint, const uint8_t** textOut,
                           uint32_t* lengthOut)
{
    uint32_t count = mlocLoad16(unit.bytes);
    const uint8_t* records = unit.bytes + spelloutTitlecaseHeaderSize;
    uint32_t low = 0;
    uint32_t high = count;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* t = records + (size_t)middle * spelloutTitlecaseSize;
        uint32_t at = mlocLoad32(t);
        if (at == codePoint)
        {
            *textOut = records + (size_t)count * spelloutTitlecaseSize + mlocLoad16(t + 4);
            *lengthOut = t[6];
            return true;
        }
        if (at < codePoint)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return false;
}

bool mlocFindSpelloutSystem(DataSpan unit, const char* id, size_t length, SpelloutSystem* out)
{
    uint32_t count = mlocLoad16(unit.bytes);
    const uint8_t* records = unit.bytes + spelloutSystemHeaderSize;
    const uint8_t* text = records + (size_t)count * spelloutSystemSize;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* s = records + (size_t)i * spelloutSystemSize;
        if (s[6] == length && memcmp(text + mlocLoad16(s), id, length) == 0)
        {
            out->locale = (const char*)text + mlocLoad16(s + 2);
            out->localeLength = s[7];
            out->name = (const char*)text + mlocLoad16(s + 4);
            out->nameLength = s[8];
            out->group = s[9];
            return true;
        }
    }
    return false;
}
