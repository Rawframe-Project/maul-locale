// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Compact notation (number_compact.h).

#include "number_compact.h"

#include "bytes.h"
#include "decimal.h"
#include "number_precision.h"
#include "number_tables.h"
#include "plural.h"

// The row of a power in a table: rows run from 10^3 to 10^14, the last
// serving every larger power.
static const uint8_t* Row(const uint8_t* table, int64_t power)
{
    int64_t last = numberCompactFirstPower + numberCompactRows - 1;
    int64_t rows = (power < last ? power : last) - numberCompactFirstPower;
    const uint8_t* row = table;
    for (int64_t r = 0; r < rows; r++)
    {
        uint32_t slots = 0;
        for (uint32_t m = row[1]; m != 0; m &= m - 1)
        {
            slots++;
        }
        row += 2 + 2 * slots;
    }
    return row;
}

// The pattern of a slot, or the other one's when the slot is absent.
static uint32_t Slot(const uint8_t* row, uint32_t slot)
{
    uint32_t mask = row[1];
    uint32_t chosen = (mask >> slot & 1u) != 0 ? slot : numberCompactOther;
    uint32_t before = 0;
    for (uint32_t m = mask & ((1u << chosen) - 1); m != 0; m &= m - 1)
    {
        before++;
    }
    return mlocLoad16(row + 2 + 2 * before);
}

bool mlocValidCompactUse(CompactData data)
{
    NumberTable patterns =
        mlocReadNumberTable((DataSpan){data.patterns, 0}, numberCompactPatternSize);
    const uint8_t* row = data.table;
    for (int r = 0; r < numberCompactRows; r++)
    {
        uint32_t slots = 0;
        for (uint32_t m = row[1]; m != 0; m &= m - 1)
        {
            slots++;
        }
        for (uint32_t s = 0; s < slots; s++)
        {
            if (mlocLoad16(row + 2 + 2 * s) >= patterns.recordCount)
            {
                return false;
            }
        }
        if (row[0] != 0)
        {
            uint32_t other = Slot(row, numberCompactOther);
            const uint8_t* p = patterns.records + (size_t)other * numberCompactPatternSize;
            if ((p[compactFlags] & patternFlagNoNumber) != 0)
            {
                return false;
            }
        }
        row += 2 + 2 * slots;
    }
    return true;
}

// Whether a magnitude's value is compacted, and the power it is then
// divided by.
static bool Divisor(const uint8_t* table, int64_t magnitude, int64_t* divisorOut)
{
    *divisorOut = 0;
    if (magnitude < numberCompactFirstPower)
    {
        return false;
    }
    const uint8_t* row = Row(table, magnitude);
    int64_t last = numberCompactFirstPower + numberCompactRows - 1;
    int64_t power = magnitude < last ? magnitude : last;
    *divisorOut = row[0] == 0 ? 0 : power - row[0] + 1;
    return row[0] != 0;
}

static bool IsExactlyOne(const mlocDecimal* d)
{
    return d->count == 1 && d->digits[0] == 1 && d->exponent == 0 && d->fractionDigits == 0;
}

mlocResult mlocApplyCompact(CompactData data, const mlocPrecision* precision, uint8_t pluralRule,
                            mlocDecimal* decimal, CompactChoice* choiceOut)
{
    int64_t magnitude = decimal->count == 0 ? 0 : mlocTopPower(decimal);
    int64_t divisor = 0;
    bool compacted = Divisor(data.table, magnitude, &divisor);
    mlocDecimal m = mlocShiftDecimal(decimal, divisor);
    mlocResult r = mlocApplyCompactPrecision(precision, &m);
    if (r == mloc_success && m.count > 0 && mlocTopPower(&m) + divisor > magnitude)
    {
        // Rounding carried the value to the next power: its row decides.
        magnitude++;
        compacted = Divisor(data.table, magnitude, &divisor);
        m = mlocShiftDecimal(decimal, divisor);
        r = mlocApplyCompactPrecision(precision, &m);
    }
    if (r != mloc_success)
    {
        return r;
    }
    CompactChoice choice = {.exponent = (int32_t)divisor, .pattern = compactNoPattern};
    if (compacted)
    {
        mlocPluralOperands operands;
        mlocFillPluralOperands(&m, &operands);
        operands.c = (uint32_t)divisor;
        const uint8_t* row = Row(data.table, magnitude);
        // The exact value 1 takes its own pattern where the locale has one
        // (French's "mille"), else its category's.
        uint32_t slot = IsExactlyOne(&m) && (row[1] >> numberCompactExactOne & 1u) != 0
                            ? numberCompactExactOne
                            : (uint32_t)mlocSelectPluralRule(pluralRule, &operands);
        choice.pattern = Slot(row, slot);
    }
    *decimal = m;
    *choiceOut = choice;
    return mloc_success;
}
