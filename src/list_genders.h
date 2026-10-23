// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The gender of a list of persons by locale, from CLDR's
// supplemental/genderList.xml, which src/generated/list_genders.c defines.

#ifndef MLOC_LIST_GENDERS_H
#define MLOC_LIST_GENDERS_H

#include <stdint.h>

// genderList.xml's rules.
enum
{
    // Always other.
    listGenderNeutral = 0,
    // All male: male; all female: female; else other.
    listGenderMixedNeutral = 1,
    // All female: female; else male.
    listGenderMaleTaints = 2,
};

// A locale ID ("el", "zh_Hant") and its rule.
typedef struct ListGenderRule
{
    char locale[12];
    uint8_t rule;
} ListGenderRule;

// Sorted by their IDs' bytes.
extern const ListGenderRule mlocListGenderRules[];
extern const uint32_t mlocListGenderRuleCount;

#endif
