// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A duration written (maul-locale/duration.h): ECMA-402's
// PartitionDurationFormatPattern, FormatNumericUnits and ListFormatParts.
// Each field is the exact value of its double; a fraction's sum is exact
// (duration_fields.h).

#include "duration_fields.h"
#include "duration_format.h"
#include "list_join.h"
#include "number_core.h"
#include "number_output.h"

#include <stddef.h>
#include <string.h>

enum
{
    // The bytes of an item's start the contextual changes read.
    itemLead = 16,
    // The bytes of an item's text kept for the text pass, which writes
    // each item once; a longer item is written again where it goes.
    itemText = 64,
};

// An item of the list: a unit-style field, or the numeric fields.
typedef struct Item
{
    // The field; the first numeric one for the numeric item.
    uint8_t field;
    bool numeric;
    // A unit-style field's value, sign and all.
    mlocDecimal value;
} Item;

typedef struct Writing
{
    const DurationFormat* format;
    // The duration's fields, read once.
    DurationFields fields;
    Item items[mloc_durationFieldCount];
    uint32_t count;
    // The numeric fields shown, their values, and whether hours or
    // minutes carry the sign; the seconds with the fractions.
    bool shown[3];
    mlocDecimal clock[3];
} Writing;

// Whether a field's next smaller one is a fraction (NextUnitFractional).
static bool NextFraction(const DurationFormat* f, uint32_t field)
{
    return field >= mloc_durationSeconds && field < mloc_durationNanoseconds &&
           f->style[field + 1] == durationFieldFraction;
}

// A field's value with the fractions after it (ComputeFractionalDigits
// added).
static void ValueWithFractions(const Writing* g, uint32_t field, mlocDecimal* out)
{
    uint32_t fractions = 0;
    while (NextFraction(g->format, field + fractions))
    {
        fractions++;
    }
    mlocDurationValue(&g->fields, field, fractions, out);
}

static bool IsZero(const mlocDecimal* d)
{
    return d->count == 0;
}

// FormatNumericUnits' choice of the fields shown, from the first numeric
// one; false when none is.
static bool PlanClock(Writing* g, uint32_t first)
{
    const DurationFormat* f = g->format;
    for (uint32_t i = 0; i < 3; i++)
    {
        uint32_t field = mloc_durationHours + i;
        if (field == mloc_durationSeconds)
        {
            ValueWithFractions(g, field, &g->clock[i]);
        }
        else
        {
            mlocDurationValue(&g->fields, field, 0, &g->clock[i]);
        }
    }
    const bool always[3] = {f->display[mloc_durationHours] == mloc_durationDisplayAlways,
                            f->display[mloc_durationMinutes] == mloc_durationDisplayAlways,
                            f->display[mloc_durationSeconds] == mloc_durationDisplayAlways};
    g->shown[0] = first == mloc_durationHours && (!IsZero(&g->clock[0]) || always[0]);
    g->shown[2] = !IsZero(&g->clock[2]) || always[2];
    g->shown[1] = first <= mloc_durationMinutes &&
                  ((g->shown[0] && g->shown[2]) || !IsZero(&g->clock[1]) || always[1]);
    return g->shown[0] || g->shown[1] || g->shown[2];
}

// PartitionDurationFormatPattern's items, the unit-style fields up to
// the first numeric one or the one the fractions join.
static void Plan(Writing* g)
{
    const DurationFormat* f = g->format;
    for (uint32_t i = 0; i < mloc_durationFieldCount; i++)
    {
        uint32_t style = f->style[i];
        if (style == mloc_durationFieldNumeric || style == mloc_durationFieldTwoDigit)
        {
            if (PlanClock(g, i))
            {
                g->items[g->count++] = (Item){.field = (uint8_t)i, .numeric = true};
            }
            return;
        }
        Item item = {.field = (uint8_t)i};
        bool fraction = NextFraction(f, i);
        if (fraction)
        {
            ValueWithFractions(g, i, &item.value);
        }
        else
        {
            mlocDurationValue(&g->fields, i, 0, &item.value);
        }
        if (f->display[i] == mloc_durationDisplayAlways || !IsZero(&item.value))
        {
            g->items[g->count++] = item;
        }
        if (fraction)
        {
            return;
        }
    }
}

// The formatter of a field: grouping and no fraction for whole fields,
// the fraction digits (truncated) where the fractions join it.
static Formatter FormatterOf(const Writing* g, uint32_t field, bool clock, bool signShown)
{
    const DurationFormat* f = g->format;
    Formatter n = f->formatter;
    n.signDisplay = signShown ? mloc_signAuto : mloc_signNever;
    n.grouping = clock ? mloc_groupingOff : mloc_groupingAuto;
    n.minInteger = f->style[field] == mloc_durationFieldTwoDigit ? 2 : 1;
    bool fraction = clock ? field == mloc_durationSeconds : NextFraction(f, field);
    if (fraction)
    {
        uint8_t most = f->fixedFraction ? f->fractionalDigits : 9;
        n.precision = (mlocPrecision){.kind = mloc_precisionFraction,
                                      .mode = mloc_roundTrunc,
                                      .minFractionDigits = f->fixedFraction ? most : 0,
                                      .maxFractionDigits = most};
    }
    return n;
}

// Writes a field's number. A field is below 2^84 and a fraction of at
// most 9 digits is truncated, so no rounding passes a decimal's limits.
static void WriteNumber(const Formatter* n, const Outer* outer, const mlocDecimal* value,
                        NumberWriter* w)
{
    (void)mlocWriteFormatted(n, outer, value, w);
}

// Writes the numeric fields shown, the sign on the first, with the
// separators between them.
static void WriteClock(const Writing* g, bool signShown, NumberWriter* w)
{
    const DurationFormat* f = g->format;
    bool sign = signShown;
    for (uint32_t i = 0; i < 3; i++)
    {
        if (!g->shown[i])
        {
            continue;
        }
        if (i > 0 && g->shown[i - 1])
        {
            w->source = 0;
            mlocWriteLiteral(w, f->separator[i - 1], f->separatorLength[i - 1]);
        }
        uint32_t field = mloc_durationHours + i;
        Formatter n = FormatterOf(g, field, true, sign);
        w->source = (field + 1) * mloc_durationPartField;
        WriteNumber(&n, nullptr, &g->clock[i], w);
        sign = false;
    }
    w->source = 0;
}

static void WriteItem(void* context, uint32_t index, NumberWriter* w)
{
    const Writing* g = context;
    const Item* item = &g->items[index];
    if (item->numeric)
    {
        WriteClock(g, index == 0, w);
        return;
    }
    const DurationFormat* f = g->format;
    Outer outer;
    mlocSlotOuter(f->patterns, f->slots[item->field].start, f->slots[item->field].length, &f->rules,
                  &outer);
    Formatter n = FormatterOf(g, item->field, false, index == 0);
    w->source = (item->field + 1U) * mloc_durationPartField;
    WriteNumber(&n, &outer, &item->value, w);
    w->source = 0;
}

// Writes the items joined by the list, each item's start read first for
// the contextual changes. The text pass keeps each item's text and joins
// the texts, so that each is formatted once; the parts pass, or an item
// longer than its room, writes them again where they go.
static void WriteItems(const Writing* g, NumberWriter* w)
{
    if (g->count == 0)
    {
        return;
    }
    char texts[mloc_durationFieldCount][itemText];
    ListItem items[mloc_durationFieldCount];
    bool kept = w->parts == nullptr;
    for (uint32_t i = 0; i < g->count; i++)
    {
        NumberWriter text = {.buffer = texts[i], .capacity = kept ? itemText : itemLead};
        WriteItem((void*)g, i, &text);
        kept = kept && text.length <= itemText;
        items[i] = (ListItem){texts[i], text.length < itemText ? text.length : itemText};
    }
    if (!kept)
    {
        for (uint32_t i = 0; i < g->count; i++)
        {
            items[i].length = items[i].length < itemLead ? items[i].length : itemLead;
        }
    }
    mlocWriteList(&g->format->list, items, g->count, kept ? nullptr : WriteItem, (void*)g, w);
}

// The duration formatter of an opaque value, its pool read in place, or
// false when the library did not make it.
static bool ReadDuration(const mlocDurationFormatter* opaque, DurationFormat* f)
{
    if (opaque == nullptr ||
        !mlocReadFormatter(opaque, sizeof(*opaque), durationStamp, &f->formatter))
    {
        return false;
    }
    size_t head = sizeof(Formatter);
    memcpy((uint8_t*)f + head, (const uint8_t*)opaque + head,
           offsetof(DurationFormat, pool) - head);
    f->patterns = (const uint8_t*)opaque + offsetof(DurationFormat, pool);
    return mlocValidDurationFormat(f);
}

static mlocResult WriteDurationText(const void* context, NumberWriter* w)
{
    WriteItems(context, w);
    return mloc_success;
}

mlocResult mlocDurationFormatter_Format(const mlocDurationFormatter* formatter,
                                        const mlocDurationValues* duration, mlocOutput* textOut)
{
    DurationFormat f;
    static const Writing s_zero;
    Writing g = s_zero;
    if (!ReadDuration(formatter, &f) || duration == nullptr || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocReadDurationFields(duration, &g.fields))
    {
        return mloc_errorInvalid;
    }
    g.format = &f;
    Plan(&g);
    return mlocWriteOutput(WriteDurationText, &g, textOut);
}
