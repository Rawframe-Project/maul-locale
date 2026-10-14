// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The lead records (spellout_lead.h), worked out to a fixed point over
// the sets the rules reach through the substitutions they start with;
// the parser's departures from ICU's search (src/spellout_parse.c) are
// mirrored here: the bounds a substitution reads with and the special
// rules a search tries.

#include "spellout_lead.h"

#include "spellout_tables.h"
#include "table_gen.h"

#include <stdlib.h>
#include <string.h>

// The largest integer a double holds exactly, to which the parser clamps
// an upper bound, and the need of a set that reaches no pattern.
static const uint64_t maxUpper = 9007199254740991;
static const uint64_t never = UINT64_MAX;

typedef struct Lead
{
    uint64_t need;
    uint8_t specials;
    uint8_t bits[32];
} Lead;

bool SpelloutKept(const RbnfSet* set, int index)
{
    RbnfRuleKind kind = set->specials[index].kind;
    if (kind == rbnfImproper || kind == rbnfProper || kind == rbnfDefault)
    {
        return true;
    }
    for (int i = index + 1; i < set->specialCount; i++)
    {
        if (set->specials[i].kind == kind)
        {
            return false;
        }
    }
    return true;
}

// Whether a set read with an upper bound reaches a pattern.
static bool Reaches(const Lead* leads, int set, uint64_t upper)
{
    return leads[set].need != never && upper >= leads[set].need;
}

// The least upper bound with which a normal rule starting with a
// substitution reaches a pattern, the bound read with deciding both
// whether the rule is tried (below it, in a set not of fractions) and,
// through "==", the bound its set is read with. A normal rule's
// substitutions are "<<" and ">>", read below its divisor, "==", and
// ">>>" and a fraction rule set's numerator "<<".
static uint64_t RuleNeed(const Lead* leads, const RbnfSet* set, const RbnfRule* rule)
{
    uint64_t gate = set->fraction                     ? 0
                    : (uint64_t)rule->base < maxUpper ? (uint64_t)rule->base + 1
                                                      : never;
    const RbnfSub* sub = &rule->subs[0];
    if (gate == never || sub->set < 0 || sub->triple ||
        (sub->kind != rbnfMultiplier && sub->kind != rbnfModulus && sub->kind != rbnfSame))
    {
        // A pattern; ">>>", whose rule before the parser reads whatever
        // the bound; a numerator, whose set every bound tries: taken as
        // reaching one.
        return gate;
    }
    uint64_t need = leads[sub->set].need;
    if (sub->kind == rbnfSame)
    {
        return need == never ? never : gate > need ? gate : need;
    }
    return Reaches(leads, sub->set, RbnfDivisor(rule)) ? gate : never;
}

// Adds a rule to its set's lead: the text it starts with, or what the
// substitution it starts with reaches. A rule whose text before its
// first substitution holds the plural block reads nothing.
static void AddRule(Lead* leads, Lead* lead, const RbnfSet* set, const RbnfRule* rule)
{
    size_t prefix = rule->subCount > 0 ? rule->subs[0].pos : rule->length;
    if (rule->plural && rule->pluralStart < prefix)
    {
        return;
    }
    if (prefix > 0)
    {
        uint32_t bit = mlocSpelloutLeadHash((const uint8_t*)rule->text, prefix);
        lead->bits[bit >> 3] |= (uint8_t)(1u << (bit & 7));
        return;
    }
    if (rule->subCount == 0)
    {
        return;
    }
    int target = rule->subs[0].set;
    if (target >= 0)
    {
        for (int k = 0; k < 32; k++)
        {
            lead->bits[k] |= leads[target].bits[k];
        }
        lead->specials |= leads[target].specials;
    }
    if (rule->kind != rbnfNormal)
    {
        lead->specials |= (uint8_t)(1u << (rule->kind - rbnfNegative));
        return;
    }
    uint64_t need = RuleNeed(leads, set, rule);
    lead->need = need < lead->need ? need : lead->need;
}

void SpelloutLeads(const RbnfGroup* group, uint8_t* out)
{
    Lead* leads = GenAllocate((size_t)group->setCount * sizeof(Lead));
    for (int s = 0; s < group->setCount; s++)
    {
        leads[s] = (Lead){.need = never};
    }
    for (bool changed = true; changed;)
    {
        changed = false;
        for (int s = 0; s < group->setCount; s++)
        {
            const RbnfSet* set = &group->sets[s];
            Lead lead = leads[s];
            for (int i = 0; i < set->ruleCount; i++)
            {
                AddRule(leads, &lead, set, &set->rules[i]);
            }
            for (int i = 0; i < set->specialCount; i++)
            {
                if (SpelloutKept(set, i))
                {
                    AddRule(leads, &lead, set, &set->specials[i]);
                }
            }
            changed |= lead.need != leads[s].need || lead.specials != leads[s].specials ||
                       memcmp(lead.bits, leads[s].bits, sizeof(lead.bits)) != 0;
            leads[s] = lead;
        }
    }
    for (int s = 0; s < group->setCount; s++)
    {
        uint8_t* record = out + (size_t)s * spelloutLeadSize;
        GenPut(record, leads[s].need, 8);
        record[8] = leads[s].specials;
        record[9] = 0;
        memcpy(record + spelloutLeadBits, leads[s].bits, 32);
    }
    free(leads);
}
