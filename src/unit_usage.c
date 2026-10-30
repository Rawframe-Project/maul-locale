// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Units formatted for a usage (unit_usage.h). The formatter keeps the
// input unit and every output its route may choose (CLDR's unit
// preferences), each with its patterns; a value chooses the first output
// whose threshold it reaches (NaN the first), is converted exactly,
// split across a mixed output's parts, and rounded by the formatter's
// precision, else the preference's increment, else to an integer of at
// least two significant digits, as ICU's UnitsRouter.

#include "unit_usage.h"

#include "kinds.h"
#include "number_output.h"
#include "unit_mixed.h"
#include "unit_mixed_format.h"
#include "unit_prefs.h"

#include <string.h>

// Fills one output from a route entry.
static mlocResult LoadOutput(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                             const UnitRouteEntry* entry, mlocUnitWidth width, uint32_t* slots,
                             size_t* used, UnitOutput* out)
{
    *out = (UnitOutput){
        .threshold = entry->threshold,
        .increment = entry->increment,
        .places = entry->places,
        .slot = (uint8_t)*slots,
        .parts = 1,
    };
    if (mlocParseUnitId(&u->table, entry->id, entry->length, &out->id) != mloc_success)
    {
        return mloc_errorFormat;
    }
    if (out->id.mixed)
    {
        return mlocLoadMixedOutput(u, data, locale, entry->id, entry->length, width, slots, used,
                                   out);
    }
    if (*slots == unitSlots)
    {
        return mloc_errorLimit;
    }
    UnitForms forms;
    mlocResult r = mlocLoadUnitForms(data, locale, entry->id, entry->length,
                                     mlocUnitWidthKind(width), u->grammaticalCase, &forms);
    r = r == mloc_success ? mlocAddUnitPatterns(u, *slots, used, &forms) : r;
    ++*slots;
    return r;
}

mlocResult mlocInitUsage(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                         const char* unit, size_t length, const mlocUnitOptions* options,
                         size_t* used)
{
    if (options->usageLength > unitMaxUsage ||
        !mlocValidUsage(options->usage, options->usageLength))
    {
        return mloc_errorSyntax;
    }
    if (options->number.notation != mloc_notationStandard)
    {
        return mloc_errorInvalid;
    }
    DataSpan table;
    if (!mlocFindDataSetUnit(data, kindUnits, nullptr, 0, &table))
    {
        return mloc_errorNotFound;
    }
    u->table = mlocReadUnitTable(table);
    mlocResult r = mlocParseUnitId(&u->table, unit, length, &u->input);
    UnitScale scale;
    if (r == mloc_success &&
        (u->input.mixed || mlocScaleUnit(&u->table, &u->input, &scale) != mloc_success))
    {
        r = mloc_errorInvalid;
    }
    static UnitRoute s_zero;
    UnitRoute route = s_zero;
    r = r == mloc_success ? mlocRouteUnit(data, locale, &u->table, &u->input, &scale,
                                          options->usage, options->usageLength, &route)
                          : r;
    uint32_t slots = 0;
    for (uint32_t i = 0; r == mloc_success && i < route.count; i++)
    {
        r = LoadOutput(u, data, locale, &route.entries[i], options->width, &slots, used,
                       &u->output[i]);
    }
    if (r == mloc_success)
    {
        u->outputs = (uint8_t)route.count;
        u->usage = true;
    }
    return r;
}

bool mlocValidUsageOutputs(const UnitFormat* u)
{
    return u->table.simples != nullptr && u->table.text != nullptr && !u->input.mixed &&
           u->input.count >= 1 && u->input.count <= unitMaxFactors;
}

// The output a value chooses: the first whose threshold it reaches, the
// last otherwise; NaN the first.
static mlocResult Choose(const UnitFormat* u, const UnitScale* input, const UnitValue* value,
                         bool nan, const UnitOutput** outputOut)
{
    for (uint32_t i = 0; i < u->outputs; i++)
    {
        const UnitOutput* o = &u->output[i];
        bool reaches = false;
        mlocResult r = mlocReachesThreshold(&u->table, input, value, &o->id, o->threshold,
                                            o->places, &reaches);
        if (r != mloc_success)
        {
            return r;
        }
        if (nan || reaches || i + 1 == u->outputs)
        {
            *outputOut = o;
            return mloc_success;
        }
    }
    return mloc_errorInvalid;
}

// A value in a single output unit, rounded.
static mlocResult AmountIn(const UnitFormat* u, const UnitScale* input, const UnitValue* value,
                           bool nan, const UnitOutput* o, const mlocPrecision* precision,
                           mlocDecimal* out)
{
    if (nan)
    {
        *out = (mlocDecimal){.kind = mloc_decimalNaN};
        return mloc_success;
    }
    UnitScale scale;
    UnitValue v;
    bool exact = true;
    if (mlocScaleUnit(&u->table, &o->id, &scale) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    if (!mlocUnitValueIn(&u->table, input, value, &scale, &v))
    {
        return mloc_errorLimit;
    }
    mlocResult r = mlocUnitValueDecimal(&v, out, &exact);
    return r == mloc_success ? mlocRoundConverted(out, exact, precision) : r;
}

// An amount routed: its output, and its amounts there.
typedef struct Routed
{
    const UnitOutput* output;
    mlocUnitAmounts amounts;
} Routed;

// Routes an amount of the input unit: the output its value chooses, and
// the value converted, split and rounded there.
static mlocResult Route(const UnitFormat* u, const UnitScale* input, const mlocDecimal* decimal,
                        Routed* out)
{
    bool nan = decimal->kind == mloc_decimalNaN;
    mlocDecimal chosen = *decimal;
    chosen.kind = nan ? mloc_decimalInfinity : decimal->kind;
    UnitValue value;
    if (!mlocUnitValueOf(&chosen, &value))
    {
        return mloc_errorLimit;
    }
    mlocResult r = Choose(u, input, &value, nan, &out->output);
    if (r != mloc_success)
    {
        return r;
    }
    const UnitOutput* o = out->output;
    mlocPrecision own;
    const mlocPrecision* precision =
        mlocRoutePrecision(&u->formatter.precision, o->increment, &own);
    if (o->parts > 1)
    {
        return mlocSplitMixed(&u->table, &o->id, input, &value, nan, mlocRoundSmallest,
                              (void*)precision, &out->amounts);
    }
    out->amounts.count = 1;
    return AmountIn(u, input, &value, nan, o, precision, &out->amounts.amounts[0]);
}

// Writes a routed amount.
static mlocResult WriteRouted(const UnitFormat* u, const Routed* r, NumberWriter* w)
{
    if (r->output->parts > 1)
    {
        return mlocWriteMixedTo(u, r->output, &r->amounts, w);
    }
    Outer outer;
    mlocUnitOuter(u, r->output->slot, &outer);
    return mlocWriteFormatted(&u->formatter, &outer, &r->amounts.amounts[0], w);
}

mlocResult mlocFormatUsage(const UnitFormat* u, const mlocDecimal* decimal, mlocOutput* output)
{
    UnitScale input;
    Routed routed;
    if (mlocScaleUnit(&u->table, &u->input, &input) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    mlocResult r = Route(u, &input, decimal, &routed);
    if (r != mloc_success)
    {
        return r;
    }
    if (routed.output->parts > 1)
    {
        return mlocWriteMixed(u, routed.output, &routed.amounts, output);
    }
    Outer outer;
    mlocUnitOuter(u, routed.output->slot, &outer);
    return mlocFormatWith(&u->formatter, &outer, &routed.amounts.amounts[0], output);
}

// Writes two routed ends, each whole, joined by the spaced range
// separator.
static mlocResult WriteEnds(const UnitFormat* u, const Routed* a, const Routed* b, NumberWriter* w)
{
    w->source = mloc_numberRangeStart;
    mlocResult r = WriteRouted(u, a, w);
    mlocWriteRangeSeparator(&u->formatter.core, true, w);
    mlocResult s = WriteRouted(u, b, w);
    w->source = 0;
    return r != mloc_success ? r : s;
}

// Two routed ends as one writing.
typedef struct Ends
{
    const UnitFormat* unit;
    const Routed* start;
    const Routed* end;
} Ends;

static mlocResult WriteEndsWith(const void* context, NumberWriter* w)
{
    const Ends* e = context;
    return WriteEnds(e->unit, e->start, e->end, w);
}

mlocResult mlocFormatUsageRange(const UnitFormat* u, const mlocDecimal* start,
                                const mlocDecimal* end, mlocOutput* output)
{
    UnitScale input;
    Routed a;
    Routed b;
    if (mlocScaleUnit(&u->table, &u->input, &input) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    mlocResult r = Route(u, &input, start, &a);
    r = r == mloc_success ? Route(u, &input, end, &b) : r;
    if (r != mloc_success)
    {
        return r;
    }
    if (a.output == b.output && a.output->parts == 1)
    {
        Outer outer;
        mlocUnitOuter(u, a.output->slot, &outer);
        return mlocFormatRangeWith(&u->formatter, &outer, &a.amounts.amounts[0],
                                   &b.amounts.amounts[0], output);
    }
    // The end fails, if at all, after the start and the separator are
    // written.
    Ends ends = {u, &a, &b};
    return mlocWriteOutput(WriteEndsWith, &ends, output);
}
