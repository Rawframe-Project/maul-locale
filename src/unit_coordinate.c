// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Coordinates (unit_coordinate.h). CLDR gives each width four patterns,
// east, north, south and west ("{0}N"); a latitude takes north for a
// value not negative (NaN included) and south otherwise, a longitude east
// and west. The amount is written as the formatter writes its unit, a
// mixed one across its parts, without its sign.

#include "unit_coordinate.h"

#include "kinds.h"
#include "unit_mixed.h"
#include "unit_mixed_format.h"
#include "unit_names.h"

#include <string.h>

// Whether a parsed unit is an angle: its dimensions are a degree's.
static bool IsAngle(const UnitTable* t, const UnitId* id)
{
    UnitId degree;
    UnitScale angle;
    UnitScale scale;
    bool reciprocal = false;
    UnitId first = id->mixed ? mlocUnitPartOf(id, 0) : *id;
    return mlocParseUnitId(t, "degree", 6, &degree) == mloc_success &&
           mlocScaleUnit(t, &degree, &angle) == mloc_success &&
           mlocScaleUnit(t, &first, &scale) == mloc_success &&
           mlocSameDimensions(&angle, &scale, &reciprocal) && !reciprocal;
}

mlocResult mlocInitCoordinate(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                              const char* unit, size_t length, const mlocUnitOptions* options)
{
    if (options->coordinate == mloc_coordinateNone)
    {
        return mloc_success;
    }
    DataSpan table;
    if (u->usage || !mlocFindDataSetUnit(data, kindUnits, nullptr, 0, &table))
    {
        return u->usage ? mloc_errorInvalid : mloc_errorNotFound;
    }
    UnitTable t = mlocReadUnitTable(table);
    UnitId id;
    if (mlocParseUnitId(&t, unit, length, &id) != mloc_success || !IsAngle(&t, &id))
    {
        return mloc_errorInvalid;
    }
    UnitPatternTexts p;
    if (!mlocFindUnitPatterns(data, locale, &p))
    {
        return mloc_errorNotFound;
    }
    uint32_t width = options->width == mloc_unitWidthLong    ? 0
                     : options->width == mloc_unitWidthShort ? 1
                                                             : 2;
    for (uint32_t d = 0; d < 4; d++)
    {
        u->direction[d] = p.text[unitPatternCoordinates + 4 * width + d];
        u->directionLength[d] = p.length[unitPatternCoordinates + 4 * width + d];
    }
    u->coordinate = (uint8_t)options->coordinate;
    return mloc_success;
}

bool mlocValidCoordinate(const UnitFormat* u)
{
    bool texts = true;
    for (uint32_t d = 0; d < 4; d++)
    {
        texts &= u->direction[d] != nullptr;
    }
    return u->coordinate == mloc_coordinateNone ||
           (u->coordinate <= mloc_coordinateLongitude && !u->usage && texts);
}

// The amount within the pattern, written once in each pass.
typedef struct Coordinate
{
    UnitFormat unit;
    mlocDecimal amount;
    mlocUnitAmounts amounts;
    const char* pattern;
    size_t length;
    size_t zero;
} Coordinate;

static mlocResult WriteCoordinate(const Coordinate* c, NumberWriter* w)
{
    mlocWriteLiteral(w, c->pattern, c->zero);
    mlocResult r;
    const UnitOutput* o = &c->unit.output[0];
    if (o->parts > 1)
    {
        r = mlocWriteMixedTo(&c->unit, o, &c->amounts, w);
    }
    else
    {
        Outer outer;
        mlocUnitOuter(&c->unit, o->slot, &outer);
        r = mlocWriteFormatted(&c->unit.formatter, &outer, &c->amount, w);
    }
    mlocWriteLiteral(w, c->pattern + c->zero + 3, c->length - c->zero - 3);
    return r;
}

mlocResult mlocFormatCoordinate(const UnitFormat* u, const mlocDecimal* decimal, mlocOutput* output)
{
    static Coordinate s_zero;
    Coordinate c = s_zero;
    c.unit = *u;
    c.unit.formatter.signDisplay = mloc_signAuto;
    bool negative = decimal->negative && decimal->kind != mloc_decimalNaN;
    uint32_t d = u->coordinate == mloc_coordinateLatitude
                     ? (negative ? unitDirectionSouth : unitDirectionNorth)
                     : (negative ? unitDirectionWest : unitDirectionEast);
    c.pattern = u->direction[d];
    c.length = u->directionLength[d];
    while (c.zero + 3 <= c.length && memcmp(c.pattern + c.zero, "{0}", 3) != 0)
    {
        c.zero++;
    }
    c.amount = *decimal;
    c.amount.negative = 0;
    if (u->output[0].parts > 1)
    {
        bool nan = c.amount.kind == mloc_decimalNaN;
        UnitValue value = {0};
        if (!nan && !mlocUnitValueOf(&c.amount, &value))
        {
            return mloc_errorLimit;
        }
        mlocResult r =
            mlocSplitMixed(&u->table, &u->output[0].id, nullptr, &value, nan, mlocRoundSmallest,
                           (void*)&u->formatter.precision, &c.amounts);
        if (r != mloc_success)
        {
            return r;
        }
    }
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    mlocResult r = WriteCoordinate(&c, &w);
    if (r != mloc_success)
    {
        return r;
    }
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        (void)WriteCoordinate(&c, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}
