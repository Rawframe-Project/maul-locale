// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Compiles one group of CLDR's rule-based number format rules (an
// rbnfRules block in ICU's syntax) as ICU4C 79's RuleBasedNumberFormat
// reads it (rbnf.cpp, nfrs.cpp, nfrule.cpp, nfsubs.cpp): the rule sets in
// order, each rule's base, radix and exponent, its text without the
// substitution tokens, its substitutions and its plural block, a
// bracketed rule expanded into its two rules. Rules ICU refuses, and the
// few it takes that the library does not (a fraction rule set used
// before it is known as one, a substitution inside a plural block), are
// refused with a message.

#ifndef MLOCGEN_SPELLOUT_COMPILE_H
#define MLOCGEN_SPELLOUT_COMPILE_H

#include <stddef.h>
#include <stdint.h>

typedef enum RbnfRuleKind
{
    rbnfNormal = 0,
    rbnfNegative = 1,
    rbnfImproper = 2,
    rbnfProper = 3,
    rbnfDefault = 4,
    rbnfInfinity = 5,
    rbnfNaN = 6,
} RbnfRuleKind;

typedef enum RbnfSubKind
{
    // "<<" of a normal rule: the quotient by the divisor.
    rbnfMultiplier = 0,
    // ">>" of a normal rule: the remainder.
    rbnfModulus = 1,
    // "=...=": the same value.
    rbnfSame = 2,
    // "<<" of a fraction rule: the integer part.
    rbnfIntegral = 3,
    // ">>" of a fraction rule: the fraction.
    rbnfFraction = 4,
    // ">>" of the negative rule: the absolute value.
    rbnfAbsolute = 5,
    // "<<" of a fraction rule set's rule: the numerator.
    rbnfNumerator = 6,
} RbnfSubKind;

typedef struct RbnfSub
{
    RbnfSubKind kind;
    // ">>>": a remainder by the rule before, a fraction digit by digit
    // without spaces.
    bool triple;
    // A numerator written "<...<<": its leading zeros are written.
    bool zeros;
    // A fraction written digit by digit (">>", ">>>" or its own set).
    bool byDigits;
    // The rule set it formats with, or -1 with a pattern.
    int set;
    // The decimal pattern ("#,##0"), or null.
    char* pattern;
    // Its byte offset in the rule's text.
    size_t pos;
    // A remainder by the rule before: that rule's index among its set's
    // rules, or -1.
    int predecessor;
} RbnfSub;

enum
{
    // The plural categories of a plural block, in mlocPluralCategory's
    // order: zero, one, two, few, many, other.
    rbnfCategories = 6,
};

typedef struct RbnfRule
{
    RbnfRuleKind kind;
    int64_t base;
    int32_t radix;
    int16_t exponent;
    // A fraction rule's decimal point as written ('.', ','), 0 for a
    // rule split from one, which matches none.
    uint32_t decimalPoint;
    // The text without the substitution tokens, the plural block kept.
    char* text;
    size_t length;
    RbnfSub subs[2];
    int subCount;
    // The plural block: its type, where it starts ("$(") and ends (past
    // ")$") in the text, and its text per category (null when absent).
    bool plural;
    bool ordinal;
    size_t pluralStart;
    size_t pluralEnd;
    char* forms[rbnfCategories];
    // The categories in the order the block writes them.
    uint8_t order[rbnfCategories];
    int formCount;
} RbnfRule;

typedef struct RbnfSet
{
    // With its "%" or "%%".
    char* name;
    bool isPublic;
    bool parseable;
    bool fraction;
    // The normal rules, bases ascending.
    RbnfRule* rules;
    int ruleCount;
    // The special rules in the order they came; ICU keeps the last
    // negative, infinity and NaN rule and every fraction rule.
    RbnfRule* specials;
    int specialCount;
} RbnfSet;

typedef struct RbnfGroup
{
    RbnfSet* sets;
    int setCount;
    // ICU's default rule set.
    int defaultSet;
    // Whether a rule has two substitutions of a kind, which makes ICU's
    // formatter refuse to parse.
    bool unparseable;
} RbnfGroup;

// Compiles a group's rules; false with a message on rules refused.
bool RbnfCompile(const char* text, size_t length, RbnfGroup* out, char* errorOut,
                 size_t errorCapacity);

// The group written as ICU's getRules writes it, which the caller frees.
char* RbnfDump(const RbnfGroup* group);

// The rule's divisor, its radix to its exponent, wrapping as ICU's
// util64_pow wraps.
uint64_t RbnfDivisor(const RbnfRule* rule);

void RbnfFree(RbnfGroup* group);

#endif
