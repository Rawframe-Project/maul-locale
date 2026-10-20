// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's collation rule strings (UTS #35 part 5 section 3), read as
// ICU4C's CollationRuleParser reads them, into a list the tailoring
// builder takes: resets, relations and settings. As ICU's data build
// does, \uhhhh, \Uhhhhhhhh, \x{h...} and \\ are decoded first; then a
// backslash escapes one code point outside apostrophes. Every refusal
// names the byte of the rule string where it happened.

#ifndef MLOCGEN_COLLATION_RULES_H
#define MLOCGEN_COLLATION_RULES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    ruleReset = 1,
    ruleRelation = 2,
    // [import tag]: text and textLength the tag's bytes in imports.
    ruleImport = 3,
    // Strengths: a relation's, and a reset's [before n] (1 to 3); a plain
    // reset has ruleIdentical.
    rulePrimary = 1,
    ruleSecondary = 2,
    ruleTertiary = 3,
    ruleQuaternary = 4,
    ruleIdentical = 5,
    // A setting not given.
    ruleUnset = 0xFF,
    // Reorder codes past the scripts: the five groups, as ICU's
    // UCOL_REORDER_CODE_FIRST and on.
    ruleReorderSpace = 0x1000,
    ruleReorderDigit = 0x1004,
};

// The special reset positions, ICU's order from 1 (0 for a string).
enum
{
    rulePositionFirstTertiaryIgnorable = 1,
    rulePositionLastTertiaryIgnorable,
    rulePositionFirstSecondaryIgnorable,
    rulePositionLastSecondaryIgnorable,
    rulePositionFirstPrimaryIgnorable,
    rulePositionLastPrimaryIgnorable,
    rulePositionFirstVariable,
    rulePositionLastVariable,
    rulePositionFirstRegular,
    rulePositionLastRegular,
    rulePositionFirstImplicit,
    rulePositionLastImplicit,
    rulePositionFirstTrailing,
    rulePositionLastTrailing,
};

// A reset or a relation; its strings are code point spans of the pool.
typedef struct CollationRule
{
    uint8_t kind;
    uint8_t strength;
    uint8_t position;
    uint32_t prefix;
    uint32_t prefixLength;
    uint32_t text;
    uint32_t textLength;
    uint32_t extension;
    uint32_t extensionLength;
} CollationRule;

typedef struct CollationRules
{
    CollationRule* rules;
    size_t count;
    size_t capacity;
    uint32_t* points;
    size_t pointCount;
    size_t pointCapacity;
    // Settings: ruleUnset, or the value given. strength 1 to 5;
    // alternate 0 non-ignorable, 1 shifted; maxVariable 0 space to 3
    // currency; caseFirst 0 off, 1 lower, 2 upper; the others 0 off, 1 on.
    uint8_t strength;
    uint8_t alternate;
    uint8_t maxVariable;
    uint8_t caseFirst;
    uint8_t caseLevel;
    uint8_t normalization;
    uint8_t numeric;
    uint8_t backwards;
    // [reorder ...]: Maul Unicode's packed script codes and the groups;
    // reorderGiven for an empty [reorder] too.
    bool reorderGiven;
    uint32_t* reorder;
    size_t reorderCount;
    // The tags of the imports, which are rules in their place, NUL-ended
    // one after another.
    char* imports;
    size_t importsLength;
    size_t importCount;
    // [suppressContractions [...]] and [optimize [...]]: code point
    // ranges, first and last. suppressAt gives each suppressed range the
    // count of rules before its setting, where the builder applies it
    // (ICU's parser hands the set to the builder in its place); the
    // written rules put the sets last.
    uint32_t* suppress;
    size_t suppressCount;
    size_t* suppressAt;
    uint32_t* optimize;
    size_t optimizeCount;
} CollationRules;

// A rule list's settings as a builder gathers them over its imports:
// each field ruleUnset, or the value the rules give last; reorder points
// into the rules that gave it.
typedef struct CollationRuleSettings
{
    uint8_t strength;
    uint8_t alternate;
    uint8_t maxVariable;
    uint8_t caseFirst;
    uint8_t caseLevel;
    uint8_t numeric;
    uint8_t backwards;
    bool reorderGiven;
    const uint32_t* reorder;
    size_t reorderCount;
    // Set by the builder over the unihan root: Han in radical-stroke order.
    bool hanRadical;
} CollationRuleSettings;

// Parses a rule string (UTF-8); 0 with "byte N: reason" on a refusal.
int CollationRulesParse(const char* text, size_t length, CollationRules* out, char* error,
                        size_t errorSize);

void CollationRulesFree(CollationRules* rules);

// Whether two lists hold the same rules and settings.
bool CollationRulesSame(const CollationRules* a, const CollationRules* b);

// The rules written back as a canonical rule string, NUL-ended, which
// parses into the same list; the caller frees it.
char* CollationRulesWrite(const CollationRules* rules, size_t* lengthOut);

#endif
