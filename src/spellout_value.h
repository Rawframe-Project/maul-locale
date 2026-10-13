// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out engine's exact arithmetic (record mloc-0034): what ICU
// computes on an int64_t or a double, on a decimal of its value alone
// (the fraction digits shown being those the digits need, zero never
// negative). A result past 40 significant digits or 10^999999 is a
// failure, never rounded.

#ifndef MLOC_SPELLOUT_VALUE_H
#define MLOC_SPELLOUT_VALUE_H

#include "maul-locale/decimal.h"

#include <stdint.h>

// A rule's divisor, and its power of ten or -1.
typedef struct SpelloutDivisor
{
    uint64_t value;
    int power;
} SpelloutDivisor;

// Sets the fraction digits shown to those the digits need; zero
// positive.
void mlocSpelloutNormalize(mlocDecimal* d);

bool mlocSpelloutIsInteger(const mlocDecimal* d);
bool mlocSpelloutIsZero(const mlocDecimal* d);

// An integer decimal's magnitude in 64 bits; false past them.
bool mlocSpelloutMagnitude64(const mlocDecimal* d, uint64_t* out);

// An integer decimal as int64_t, INT64_MIN aside; false otherwise.
bool mlocSpelloutToInt64(const mlocDecimal* d, int64_t* out);

mlocDecimal mlocSpelloutFromInt64(int64_t n);
mlocDecimal mlocSpelloutFromUnsigned(uint64_t n);

// A finite decimal rounded to an integer in a mode.
mlocDecimal mlocSpelloutRound(const mlocDecimal* x, mlocRoundingMode mode);

mlocDecimal mlocSpelloutAbsolute(const mlocDecimal* x);

// x divided by 10^power (multiplied for a negative power), exact; false
// past 10^±999999.
bool mlocSpelloutShift(const mlocDecimal* x, int power, mlocDecimal* out);

// x - floor(x), ICU's number - uprv_floor(number).
bool mlocSpelloutFraction(const mlocDecimal* x, mlocDecimal* out);

// x times a factor below 10^18.
bool mlocSpelloutMultiply(const mlocDecimal* x, uint64_t factor, mlocDecimal* out);

// floor(x / divisor), a divisor not a power of ten needing an integer
// part within int64_t.
bool mlocSpelloutDivideFloor(const mlocDecimal* x, SpelloutDivisor divisor, mlocDecimal* out);

// x / divisor truncated, with x's sign.
bool mlocSpelloutDivideTrunc(const mlocDecimal* x, SpelloutDivisor divisor, mlocDecimal* out);

// x / divisor exactly: a divisor of twos and fives alone, or a power of
// ten.
bool mlocSpelloutDivideExact(const mlocDecimal* x, SpelloutDivisor divisor, mlocDecimal* out);

// fmod(x, divisor), with x's sign.
bool mlocSpelloutModulo(const mlocDecimal* x, SpelloutDivisor divisor, mlocDecimal* out);

// An integer decimal's magnitude mod a divisor, for any exponent.
uint64_t mlocSpelloutMagnitudeMod(const mlocDecimal* d, uint64_t divisor);

// The order of two finite decimals: -1, 0 or 1.
int mlocSpelloutCompare(const mlocDecimal* a, const mlocDecimal* b);

#endif
