// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The alias table's validator (locale_aliases.h).

#include "locale_aliases.h"

static bool Optional(uint32_t packed, bool (*valid)(uint32_t))
{
    return packed == 0 || valid(packed);
}

static bool ValidVariants(const Aliases* aliases, const uint32_t* variants, uint32_t count)
{
    // The count comes from 4 bits of the file; the rule holds two.
    if (count > aliasMaxVariants)
    {
        return false;
    }
    for (uint32_t v = 0; v < count; v++)
    {
        if (variants[v] >= aliases->variantCount || (v > 0 && variants[v] <= variants[v - 1]))
        {
            return false;
        }
    }
    return true;
}

// A variant name as the parser writes one: 5 to 8 lowercase letters and
// digits, or a digit and 3 of them.
static bool ValidVariantName(const uint8_t* name, uint32_t length)
{
    for (uint32_t k = 0; k < length; k++)
    {
        if (!((name[k] >= 'a' && name[k] <= 'z') || (name[k] >= '0' && name[k] <= '9')))
        {
            return false;
        }
    }
    return length >= 5 || (length == 4 && name[0] >= '0' && name[0] <= '9');
}

static bool ValidRule(const Aliases* aliases, const Rule* rule)
{
    bool fields = Optional(rule->typeLanguage, mlocValidPackedLanguage) &&
                  Optional(rule->replacementLanguage, mlocValidPackedLanguage) &&
                  Optional(rule->typeScript, mlocValidPackedScript) &&
                  Optional(rule->replacementScript, mlocValidPackedScript) &&
                  Optional(rule->typeRegion, mlocValidPackedRegion) &&
                  Optional(rule->replacementRegion, mlocValidPackedRegion);
    bool empty = rule->typeLanguage == 0 && rule->typeScript == 0 && rule->typeRegion == 0 &&
                 rule->typeVariantCount == 0;
    return fields && !empty && ValidVariants(aliases, rule->typeVariants, rule->typeVariantCount) &&
           ValidVariants(aliases, rule->replacementVariants, rule->replacementVariantCount) &&
           rule->regionCount != 1 &&
           (uint64_t)rule->regionStart + rule->regionCount <= aliases->regionCount;
}

// Whether two legacy entries, whose texts are in bounds, are in order.
static bool LegacyBefore(const Aliases* aliases, const uint8_t* a, const uint8_t* b)
{
    uint32_t aAt = mlocLoad16(a);
    uint32_t bAt = mlocLoad16(b);
    uint32_t aLength = mlocLoad16(a + 2);
    uint32_t bLength = mlocLoad16(b + 2);
    return aAt + aLength <= aliases->textSize && bAt + bLength <= aliases->textSize &&
           CompareText(aliases->text + aAt, aLength, aliases->text + bAt, bLength) < 0;
}

static bool ValidTables(const Aliases* aliases)
{
    for (uint32_t i = 0; i < aliases->variantCount; i++)
    {
        const uint8_t* name = aliases->variants + (size_t)i * 8;
        uint32_t length = 0;
        while (length < 8 && name[length] != 0)
        {
            length++;
        }
        bool sorted = i == 0 || memcmp(name - 8, name, 8) < 0;
        for (uint32_t k = length; k < 8; k++)
        {
            sorted = sorted && name[k] == 0;
        }
        if (!ValidVariantName(name, length) || !sorted)
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < aliases->regionCount; i++)
    {
        if (!mlocValidPackedRegion(mlocLoad16(aliases->regions + (size_t)i * 2)))
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < aliases->legacyCount; i++)
    {
        const uint8_t* entry = aliases->legacy + (size_t)i * aliasLegacySize;
        if (i > 0 && !LegacyBefore(aliases, entry - aliasLegacySize, entry))
        {
            return false;
        }
        uint32_t typeLength = mlocLoad16(entry + 2);
        uint32_t replacementLength = mlocLoad16(entry + 6);
        if ((uint32_t)mlocLoad16(entry) + typeLength > aliases->textSize ||
            (uint32_t)mlocLoad16(entry + 4) + replacementLength > aliases->textSize ||
            typeLength == 0 || replacementLength == 0 || typeLength >= MAX_TEXT ||
            replacementLength >= MAX_TEXT)
        {
            return false;
        }
    }
    return true;
}

// Whether the slots name each key's first index once and nothing else,
// with a 0 to end every probe.
static bool ValidSlots(const Aliases* aliases)
{
    uint32_t used = 0;
    for (uint32_t at = 0; at < aliases->slotCount; at++)
    {
        uint32_t slot = mlocLoad16(aliases->slots + (size_t)at * 2);
        if (slot == 0)
        {
            continue;
        }
        if (slot > aliases->ruleCount ||
            (slot > 1 && KeyAt(aliases, slot - 2) == KeyAt(aliases, slot - 1)))
        {
            return false;
        }
        used++;
    }
    if (used == aliases->slotCount)
    {
        return false;
    }
    uint32_t starts = 0;
    for (uint32_t i = 0; i < aliases->ruleCount; i++)
    {
        if (i > 0 && KeyAt(aliases, i - 1) == KeyAt(aliases, i))
        {
            continue;
        }
        if (KeyStart(aliases, KeyAt(aliases, i)) != i)
        {
            return false;
        }
        starts++;
    }
    return starts == used;
}

bool mlocValidAliases(const DataView* view, DataSpan payload)
{
    (void)view;
    Aliases aliases;
    if (!ReadAliases(payload, &aliases) || mlocLoad32(payload.bytes + 28) != 0 ||
        !ValidTables(&aliases))
    {
        return false;
    }
    uint64_t previous = 0;
    for (uint32_t i = 0; i < aliases.ruleCount; i++)
    {
        Rule rule = ReadRule(&aliases, i);
        uint32_t key = KeyAt(&aliases, i);
        uint32_t keyRule = mlocLoad16(aliases.keyRules + (size_t)i * 2);
        uint64_t sortKey = (uint64_t)key << 16 | keyRule;
        if (!ValidRule(&aliases, &rule) || keyRule >= aliases.ruleCount ||
            (i > 0 && sortKey <= previous))
        {
            return false;
        }
        Rule filed = ReadRule(&aliases, keyRule);
        if (RuleKey(&filed) != key)
        {
            return false;
        }
        previous = sortKey;
    }
    return ValidSlots(&aliases);
}
