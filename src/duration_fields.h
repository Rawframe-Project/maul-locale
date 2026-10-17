// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A duration's fields as exact decimals (record mloc-0044): each double's
// value, and a field with the fractions after it added, in 128 bits.

#ifndef MLOC_DURATION_FIELDS_H
#define MLOC_DURATION_FIELDS_H

#include "maul-locale/duration.h"

// A duration's fields read once: each magnitude in 128 bits (high and
// low halves), and whether the duration is negative (a field below 0).
typedef struct DurationFields
{
    uint64_t high[mloc_durationFieldCount];
    uint64_t low[mloc_durationFieldCount];
    bool negative;
} DurationFields;

// Reads a duration's fields; false for one mlocMakeDuration refuses.
bool mlocReadDurationFields(const mlocDurationValues* duration, DurationFields* fields);

// The value of a field plus each of the next fractions fields over 1000
// times the one before (ECMA-402's ComputeFractionalDigits), negative
// (-0 for 0) when the duration is.
void mlocDurationValue(const DurationFields* fields, uint32_t field, uint32_t fractions,
                       mlocDecimal* out);

#endif
