// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A group's rules and sets read from its unit (spellout_tables.h), shared
// by the engines that format and parse.

#ifndef MLOC_SPELLOUT_RULE_H
#define MLOC_SPELLOUT_RULE_H

#include "bytes.h"
#include "invariant.h"
#include "spellout_data.h"
#include "spellout_value.h"

enum
{
    spelloutNoRule = -1,
};

// A rule's fields.
typedef struct SpelloutRule
{
    const uint8_t* bytes;
    uint64_t base;
    uint64_t divisor;
    SpelloutRuleKind kind;
    bool rollback;
    // The divisor's power of ten, or -1.
    int power;
    SpelloutDivisor div;
    uint32_t firstPiece;
    uint32_t pieces;
    uint32_t decimalPoint;
} SpelloutRule;

static inline SpelloutRule mlocReadSpelloutRule(const SpelloutRules* r, uint32_t index)
{
    MLOC_ASSERT(index < r->ruleCount);
    const uint8_t* p = r->rules + (size_t)index * spelloutRuleSize;
    bool power = (p[13] & spelloutPowerOfTen) != 0;
    SpelloutRule rule = {
        .bytes = p,
        .base = mlocLoad64(p),
        .divisor = mlocSpelloutDivisor(p),
        .kind = (SpelloutRuleKind)p[12],
        .rollback = (p[13] & spelloutRollback) != 0,
        .power = power ? p[15] : -1,
        .firstPiece = mlocLoad16(p + 8),
        .pieces = p[14],
        .decimalPoint = mlocLoad16(p + 10),
    };
    rule.div = (SpelloutDivisor){.value = rule.divisor, .power = rule.power};
    return rule;
}

// A set's rules: its normal ones from first, then its special ones.
typedef struct SpelloutSet
{
    uint32_t first;
    uint32_t normal;
    uint32_t special;
    bool fraction;
} SpelloutSet;

static inline SpelloutSet mlocReadSpelloutSet(const SpelloutRules* r, uint32_t index)
{
    MLOC_ASSERT(index < r->setCount);
    const uint8_t* p = r->sets + (size_t)index * spelloutSetSize;
    return (SpelloutSet){
        .first = mlocLoad16(p + 4),
        .normal = mlocLoad16(p + 6),
        .special = mlocLoad16(p + 8),
        .fraction = (p[3] & spelloutSetFraction) != 0,
    };
}

// A special rule of a kind, or spelloutNoRule: ICU keeps one negative,
// infinity and NaN rule; of the fraction rules, the first unless a later
// one's decimal point is the locale's (NFRuleSet::setBestFractionRule).
static inline int64_t mlocFindSpelloutSpecial(const SpelloutRules* r, const SpelloutSet* set,
                                              SpelloutRuleKind kind, uint32_t decimalPoint)
{
    int64_t best = spelloutNoRule;
    for (uint32_t i = 0; i < set->special; i++)
    {
        const uint8_t* p = r->rules + (size_t)(set->first + set->normal + i) * spelloutRuleSize;
        if ((SpelloutRuleKind)p[12] == kind &&
            (best == spelloutNoRule || mlocLoad16(p + 10) == decimalPoint))
        {
            best = set->first + set->normal + i;
        }
    }
    return best;
}

// A substitution's target: a rule set, or a pattern (-1 - its index).
static inline int64_t mlocSpelloutTarget(const uint8_t* piece)
{
    uint32_t index = mlocLoad16(piece + 2);
    return (piece[1] & spelloutSubPattern) != 0 ? -1 - (int64_t)index : (int64_t)index;
}

#endif
