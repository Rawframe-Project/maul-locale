// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Units of measurement (record mloc-0023): the public conversion over the
// unit table, the identifier parser and the exact converter.

#include "maul-locale/unit.h"

#include "data_view.h"
#include "decimal.h"
#include "kinds.h"
#include "number_precision.h"
#include "unit_convert.h"
#include "unit_data.h"
#include "unit_id.h"

enum
{
    // The significant digits an inexact conversion keeps by default: two
    // fewer than it computes, so that rounding to odd makes them exact.
    defaultDigits = 38,
};

static mlocResult Scale(const UnitTable* t, const char* id, size_t length, UnitScale* out)
{
    UnitId parsed;
    mlocResult r = mlocParseUnitId(t, id, length, &parsed);
    return r == mloc_success ? mlocScaleUnit(t, &parsed, out) : r;
}

mlocResult mlocConvertUnit(const mlocDataSet* data, const char* from, size_t fromLength,
                           const char* to, size_t toLength, const mlocPrecision* precision,
                           const mlocDecimal* value, mlocDecimal* valueOut)
{
    static const mlocPrecision s_none = {0};
    precision = precision == nullptr ? &s_none : precision;
    if (data == nullptr || from == nullptr || to == nullptr || value == nullptr ||
        valueOut == nullptr || !mlocValidDecimal(value) || !mlocValidPrecision(precision))
    {
        return mloc_errorInvalid;
    }
    DataSpan unit;
    if (!mlocFindDataSetUnit(data, kindUnits, nullptr, 0, &unit))
    {
        return mloc_errorNotFound;
    }
    UnitTable t = mlocReadUnitTable(unit);
    UnitScale source;
    UnitScale target;
    mlocResult r = Scale(&t, from, fromLength, &source);
    if (r == mloc_success)
    {
        r = Scale(&t, to, toLength, &target);
    }
    mlocDecimal converted;
    bool exact = true;
    if (r == mloc_success)
    {
        r = mlocConvertScaled(&t, &source, &target, value, &converted, &exact);
    }
    if (r == mloc_success && converted.kind == mloc_decimalFinite)
    {
        if (precision->kind != mloc_precisionNone)
        {
            r = mlocApplyPrecision(precision, &converted);
        }
        else if (!exact)
        {
            int64_t position = mlocTopPower(&converted) - defaultDigits + 1;
            r = mlocRoundDecimal(
                &converted,
                (int32_t)(position < -mlocDecimalMaxExponent ? -mlocDecimalMaxExponent : position),
                1, mloc_roundHalfEven);
            // Shown as the digits it holds, without the zeros rounding left.
            converted.fractionDigits = converted.exponent < 0 ? -converted.exponent : 0;
        }
    }
    if (r == mloc_success)
    {
        *valueOut = converted;
    }
    return r;
}
