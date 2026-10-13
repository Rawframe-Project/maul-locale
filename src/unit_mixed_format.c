// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Mixed units formatted (unit_mixed_format.h). The amount, in the
// largest part, is split exactly (unit_mixed.h): whole larger parts, the
// rest in the smallest, rounded by the formatter's precision, a carry
// moving up. The larger parts are written as the locale's default number
// format writes them, each in its plural category; the smallest with the
// formatter's options; the first part given carries the sign, by the
// formatter's sign display. The unit list of the width joins them, the
// contextual changes reading the smallest part as ICU gives it to its
// list formatter: its pattern, "{0}" unfilled.

#include "unit_mixed_format.h"

#include "currency_tables.h"
#include "decimal.h"
#include "kinds.h"
#include "list_join.h"
#include "number_precision.h"
#include "plural.h"
#include "plural_rules.h"
#include "unit_mixed.h"

#include <string.h>

enum
{
    // The bytes of an item's start the contextual changes read.
    itemLead = 16,
};

bool mlocIsMixedUnit(const char* unit, size_t length)
{
    for (size_t at = 0; at + 5 <= length; at++)
    {
        if (memcmp(unit + at, "-and-", 5) == 0)
        {
            return true;
        }
    }
    return false;
}

static ListStyle StyleOf(mlocUnitWidth width)
{
    return width == mloc_unitWidthLong     ? listUnit
           : width == mloc_unitWidthNarrow ? listUnitNarrow
                                           : listUnitShort;
}

// Loads each part's patterns, the parts split at "-and-".
static mlocResult LoadParts(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                            const char* unit, size_t length, uint32_t kind, size_t* used)
{
    uint32_t part = 0;
    size_t start = 0;
    for (size_t at = 0; at <= length; at++)
    {
        bool end = at == length || (at + 5 <= length && memcmp(unit + at, "-and-", 5) == 0);
        if (!end)
        {
            continue;
        }
        if (part == u->mixed.count)
        {
            return mloc_errorSyntax;
        }
        UnitForms forms;
        mlocResult r = mlocLoadUnitForms(data, locale, unit + start, at - start, kind, &forms);
        r = r == mloc_success ? mlocAddUnitPatterns(u, part, used, &forms) : r;
        if (r != mloc_success)
        {
            return r;
        }
        part++;
        start = at + 5;
        at += 4;
    }
    return part == u->mixed.count ? mloc_success : mloc_errorSyntax;
}

mlocResult mlocInitMixedUnit(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                             const char* unit, size_t length, const mlocUnitOptions* options,
                             size_t* used)
{
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
    mlocResult r = mlocParseUnitId(&u->table, unit, length, &u->mixed);
    if (r != mloc_success)
    {
        return r;
    }
    if (!u->mixed.mixed)
    {
        return mloc_errorSyntax;
    }
    uint32_t smallest = 0;
    r = mlocCheckMixed(&u->table, &u->mixed, &smallest);
    r = r == mloc_success
            ? LoadParts(u, data, locale, unit, length, mlocUnitWidthKind(options->width), used)
            : r;
    if (r != mloc_success)
    {
        return r;
    }
    if (!mlocFindListPatterns(data, locale, StyleOf(options->width), &u->list))
    {
        return mloc_errorNotFound;
    }
    u->parts = u->mixed.count;
    u->smallest = (uint8_t)smallest;
    return mloc_success;
}

bool mlocValidMixedUnit(const UnitFormat* u)
{
    bool texts = true;
    for (uint32_t p = 0; p < listPatternCount; p++)
    {
        texts &= u->list.text[p] != nullptr;
    }
    return texts && u->list.rule <= listRuleHebrew && u->mixed.mixed == 1 &&
           u->mixed.count == u->parts && u->smallest < u->parts && u->table.simples != nullptr &&
           u->table.text != nullptr;
}

// Rounds the smallest part: by the formatter's precision, else an
// inexact value to 38 digits.
static mlocResult RoundLast(void* context, mlocDecimal* amount, bool exact)
{
    const Formatter* f = context;
    if (amount->kind != mloc_decimalFinite)
    {
        return mloc_success;
    }
    return f->precision.kind == mloc_precisionNone ? mlocRoundConverted(amount, exact, nullptr)
                                                   : mlocApplyPrecision(&f->precision, amount);
}

// What writing the parts needs.
typedef struct Mixed
{
    const UnitFormat* unit;
    mlocUnitAmounts amounts;
    // The formatters of the larger parts and of the smallest, each with
    // the sign display of the first part where it is first.
    Formatter larger[2];
    Formatter smallest[2];
    uint32_t last;
    mlocResult result;
} Mixed;

static void WritePart(void* context, uint32_t index, NumberWriter* writer)
{
    Mixed* m = context;
    Outer outer;
    mlocUnitOuter(m->unit, index, &outer);
    const Formatter* f = index == m->last ? &m->smallest[index == 0] : &m->larger[index == 0];
    mlocResult r = mlocWriteFormatted(f, &outer, &m->amounts.amounts[index], writer);
    m->result = m->result == mloc_success ? r : m->result;
}

// The start of the smallest part's pattern for its amount's category, as
// ICU's list formatter sees it: "{0}" where the number goes.
static void SmallestLead(const Mixed* m, char* lead, ListItem* item)
{
    const Formatter* f = &m->smallest[0];
    mlocDecimal d = m->amounts.amounts[m->last];
    uint32_t category = mloc_pluralOther;
    if (d.kind == mloc_decimalFinite && mlocApplyPrecision(&f->precision, &d) == mloc_success)
    {
        mlocPluralOperands operands;
        mlocFillPluralOperands(&d, &operands);
        category = mlocSelectPluralRule(f->pluralRule, &operands);
    }
    const uint8_t* text = m->unit->pool + m->unit->start[m->last][category];
    size_t length = m->unit->length[m->last][category];
    size_t n = 0;
    for (size_t at = 0; at < length && n + 3 <= itemLead; at++)
    {
        if (text[at] == currencyTextNumber)
        {
            lead[n++] = '{';
            lead[n++] = '0';
            lead[n++] = '}';
        }
        else
        {
            lead[n++] = (char)text[at];
        }
    }
    *item = (ListItem){lead, n};
}

// The start of a larger part as it is written.
static void LargerLead(Mixed* m, uint32_t index, char* lead, ListItem* item)
{
    NumberWriter w = {.buffer = lead, .capacity = itemLead};
    WritePart(m, index, &w);
    *item = (ListItem){lead, w.length < itemLead ? w.length : itemLead};
}

static void SetUp(Mixed* m, const UnitFormat* u)
{
    m->unit = u;
    m->last = u->smallest;
    m->result = mloc_success;
    m->smallest[1] = u->formatter;
    m->smallest[0] = u->formatter;
    m->smallest[0].signDisplay = mloc_signAuto;
    Formatter plain = u->formatter;
    plain.precision = (mlocPrecision){0};
    plain.grouping = mloc_groupingAuto;
    plain.minInteger = 1;
    m->larger[0] = plain;
    m->larger[0].signDisplay = mloc_signAuto;
    m->larger[1] = plain;
}

mlocResult mlocFormatMixedUnit(const UnitFormat* u, const mlocDecimal* decimal, mlocOutput* output)
{
    if (decimal == nullptr || output == nullptr ||
        (output->buffer == nullptr && output->capacity != 0) || !mlocValidDecimal(decimal))
    {
        return mloc_errorInvalid;
    }
    static Mixed s_zero;
    Mixed m = s_zero;
    SetUp(&m, u);
    bool nan = decimal->kind == mloc_decimalNaN;
    UnitValue value = {0};
    if (!nan && !mlocUnitValueOf(decimal, &value))
    {
        return mloc_errorCapacity;
    }
    mlocResult r = mlocSplitMixed(&u->table, &u->mixed, nullptr, &value, nan, RoundLast,
                                  (void*)&u->formatter, &m.amounts);
    if (r != mloc_success)
    {
        return r;
    }
    char leads[unitMaxParts][itemLead];
    ListItem items[unitMaxParts];
    for (uint32_t i = 0; i < u->parts; i++)
    {
        if (i == m.last)
        {
            SmallestLead(&m, leads[i], &items[i]);
        }
        else
        {
            LargerLead(&m, i, leads[i], &items[i]);
        }
    }
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    mlocWriteList(&u->list, items, u->parts, WritePart, &m, &w);
    if (m.result != mloc_success)
    {
        return m.result;
    }
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        mlocWriteList(&u->list, items, u->parts, WritePart, &m, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}
