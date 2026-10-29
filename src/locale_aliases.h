// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The alias table (locale_tables.h): its parts, its rules unpacked and
// the lookups canonicalization and the validator share, and the
// validator.

#ifndef MLOC_LOCALE_ALIASES_H
#define MLOC_LOCALE_ALIASES_H

#include "bytes.h"
#include "data_view.h"
#include "locale_tables.h"

#include "maul-locale/locale.h"

#include <string.h>

#define MAX_TEXT (MLOC_MAX_LOCALE_TEXT + 1)

typedef struct Aliases
{
    const uint8_t* rules;
    const uint8_t* keys;
    const uint8_t* keyRules;
    const uint8_t* slots;
    const uint8_t* variants;
    const uint8_t* regions;
    const uint8_t* legacy;
    const uint8_t* text;
    uint32_t ruleCount;
    uint32_t slotCount;
    uint32_t variantCount;
    uint32_t regionCount;
    uint32_t legacyCount;
    uint32_t textSize;
} Aliases;

// The table's parts, or false when its counts do not give its size.
static inline bool ReadAliases(DataSpan span, Aliases* out)
{
    if (span.size < aliasHeaderSize)
    {
        return false;
    }
    uint64_t rules = mlocLoad32(span.bytes);
    uint64_t keys = mlocLoad32(span.bytes + 4);
    uint64_t variants = mlocLoad32(span.bytes + 8);
    uint64_t regions = mlocLoad32(span.bytes + 12);
    uint64_t legacy = mlocLoad32(span.bytes + 16);
    uint64_t text = mlocLoad32(span.bytes + 20);
    uint64_t slots = mlocLoad32(span.bytes + 24);
    uint64_t keysAt = aliasHeaderSize + rules * aliasRuleSize;
    uint64_t slotsAt = keysAt + rules * 6;
    uint64_t variantsAt = (slotsAt + slots * 2 + 7) / 8 * 8;
    uint64_t regionsAt = variantsAt + variants * 8;
    uint64_t legacyAt = regionsAt + regions * 2;
    uint64_t textAt = legacyAt + legacy * aliasLegacySize;
    if (keys != rules || rules > 0xFFFF || variants > 0xFFFF || slots == 0 || slots > 65536 ||
        (slots & (slots - 1)) != 0 || textAt + text != span.size)
    {
        return false;
    }
    *out = (Aliases){
        .rules = span.bytes + aliasHeaderSize,
        .keys = span.bytes + keysAt,
        .keyRules = span.bytes + keysAt + rules * 4,
        .slots = span.bytes + slotsAt,
        .variants = span.bytes + variantsAt,
        .regions = span.bytes + regionsAt,
        .legacy = span.bytes + legacyAt,
        .text = span.bytes + textAt,
        .ruleCount = (uint32_t)rules,
        .slotCount = (uint32_t)slots,
        .variantCount = (uint32_t)variants,
        .regionCount = (uint32_t)regions,
        .legacyCount = (uint32_t)legacy,
        .textSize = (uint32_t)text,
    };
    return true;
}

// One rule, unpacked.
typedef struct Rule
{
    uint32_t typeScript;
    uint32_t replacementScript;
    uint32_t typeLanguage;
    uint32_t typeRegion;
    uint32_t typeVariants[aliasMaxVariants];
    uint32_t replacementLanguage;
    uint32_t replacementRegion;
    uint32_t replacementVariants[aliasMaxVariants];
    uint32_t regionStart;
    uint32_t regionCount;
    uint32_t typeVariantCount;
    uint32_t replacementVariantCount;
} Rule;

static inline Rule ReadRule(const Aliases* aliases, uint32_t index)
{
    const uint8_t* r = aliases->rules + (size_t)index * aliasRuleSize;
    return (Rule){
        .typeScript = mlocLoad32(r),
        .replacementScript = mlocLoad32(r + 4),
        .typeLanguage = mlocLoad16(r + 8),
        .typeRegion = mlocLoad16(r + 10),
        .typeVariants = {mlocLoad16(r + 12), mlocLoad16(r + 14)},
        .replacementLanguage = mlocLoad16(r + 16),
        .replacementRegion = mlocLoad16(r + 18),
        .replacementVariants = {mlocLoad16(r + 20), mlocLoad16(r + 22)},
        .regionStart = mlocLoad16(r + 24),
        .regionCount = r[26],
        .typeVariantCount = r[27] & 15u,
        .replacementVariantCount = r[27] >> 4,
    };
}

// The key a rule is filed under: its type's first field.
static inline uint32_t RuleKey(const Rule* rule)
{
    if (rule->typeLanguage != 0)
    {
        return rule->typeLanguage;
    }
    if (rule->typeScript != 0)
    {
        return 1u << 30 | rule->typeScript;
    }
    if (rule->typeRegion != 0)
    {
        return 2u << 30 | rule->typeRegion;
    }
    return 3u << 30 | rule->typeVariants[0];
}

// Orders texts by their bytes, a prefix first, as the generator sorts
// the legacy tags.
static inline int CompareText(const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    // A loop rather than memcmp: legacy tags are a dozen bytes, fewer
    // than a call to the library takes to start.
    size_t n = aLength < bLength ? aLength : bLength;
    for (size_t i = 0; i < n; i++)
    {
        if (a[i] != b[i])
        {
            return a[i] < b[i] ? -1 : 1;
        }
    }
    return aLength < bLength ? -1 : aLength > bLength;
}

static inline uint32_t KeyAt(const Aliases* aliases, uint32_t index)
{
    return mlocLoad32(aliases->keys + (size_t)index * 4);
}

// The first index of a key, or ruleCount: the slots' probe, which a 0
// ends (validation finds one).
static inline uint32_t KeyStart(const Aliases* aliases, uint32_t key)
{
    uint32_t mask = aliases->slotCount - 1;
    for (uint32_t at = mlocAliasSlot(key, aliases->slotCount);; at = (at + 1) & mask)
    {
        uint32_t slot = mlocLoad16(aliases->slots + (size_t)at * 2);
        if (slot == 0)
        {
            return aliases->ruleCount;
        }
        if (KeyAt(aliases, slot - 1) == key)
        {
            return slot - 1;
        }
    }
}
bool mlocValidAliases(const DataView* view, DataSpan payload);

#endif
