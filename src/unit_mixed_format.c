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
    return width == mloc_unitWidthLong                                       ? listUnit
           : width == mloc_unitWidthNarrow || width == mloc_unitWidthNumeric ? listUnitNarrow
                                                                             : listUnitShort;
}

// Loads each part's patterns into the slots, the parts split at "-and-".
static mlocResult LoadParts(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                            const char* unit, size_t length, uint32_t kind, uint32_t* slots,
                            size_t* used, const UnitOutput* o)
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
        if (part == o->id.count)
        {
            return mloc_errorSyntax;
        }
        if (*slots == unitSlots)
        {
            return mloc_errorCapacity;
        }
        UnitForms forms;
        mlocResult r = mlocLoadUnitForms(data, locale, unit + start, at - start, kind,
                                         u->grammaticalCase, &forms);
        r = r == mloc_success ? mlocAddUnitPatterns(u, *slots, used, &forms) : r;
        if (r != mloc_success)
        {
            return r;
        }
        ++*slots;
        part++;
        start = at + 5;
        at += 4;
    }
    return part == o->id.count ? mloc_success : mloc_errorSyntax;
}

// The numeric width's pattern of a mixed unit of hours, minutes and
// seconds in that order, each plain: hours and minutes, minutes and
// seconds, or all three (hours and seconds take it too, minutes 0, as
// ICU's MeasureFormat).
static void SetNumeric(const UnitTable* t, UnitOutput* o)
{
    static const char* const s_fields[3] = {"hour", "minute", "second"};
    int32_t simples[3];
    for (uint32_t f = 0; f < 3; f++)
    {
        simples[f] = mlocFindSimpleUnit(t, s_fields[f], strlen(s_fields[f]));
        o->field[f] = -1;
    }
    int32_t last = -1;
    for (uint32_t i = 0; i < o->id.count; i++)
    {
        const UnitPart* p = &o->id.parts[i];
        int32_t f = 0;
        while (f < 3 && (p->simple != simples[f] || p->prefix >= 0 || p->power != 1))
        {
            f++;
        }
        if (f == 3 || f <= last)
        {
            return;
        }
        o->field[f] = (int8_t)i;
        last = f;
    }
    bool hours = o->field[0] >= 0;
    o->numeric = (uint8_t)(1 + (!hours             ? unitPatternMs
                                : o->field[2] >= 0 ? unitPatternHms
                                                   : unitPatternHm));
}

mlocResult mlocLoadMixedOutput(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                               const char* unit, size_t length, mlocUnitWidth width,
                               uint32_t* slots, size_t* used, UnitOutput* out)
{
    uint32_t smallest = 0;
    out->slot = (uint8_t)*slots;
    mlocResult r = mlocCheckMixed(&u->table, &out->id, &smallest);
    r = r == mloc_success
            ? LoadParts(u, data, locale, unit, length, mlocUnitWidthKind(width), slots, used, out)
            : r;
    if (r != mloc_success)
    {
        return r;
    }
    if (u->list.text[0] == nullptr && !mlocFindListPatterns(data, locale, StyleOf(width), &u->list))
    {
        return mloc_errorNotFound;
    }
    out->parts = out->id.count;
    out->smallest = (uint8_t)smallest;
    if (width == mloc_unitWidthNumeric)
    {
        SetNumeric(&u->table, out);
    }
    return mloc_success;
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
    UnitOutput* o = &u->output[0];
    mlocResult r = mlocParseUnitId(&u->table, unit, length, &o->id);
    if (r != mloc_success)
    {
        return r;
    }
    if (!o->id.mixed)
    {
        return mloc_errorSyntax;
    }
    uint32_t slots = 0;
    return mlocLoadMixedOutput(u, data, locale, unit, length, options->width, &slots, used, o);
}

bool mlocValidMixedUnit(const UnitFormat* u)
{
    bool texts = true;
    for (uint32_t p = 0; p < listPatternCount; p++)
    {
        texts &= u->list.text[p] != nullptr;
    }
    return texts && u->list.rule <= listRuleHebrew && u->table.simples != nullptr &&
           u->table.text != nullptr;
}

mlocResult mlocRoundSmallest(void* context, mlocDecimal* amount, bool exact)
{
    const mlocPrecision* precision = context;
    if (amount->kind != mloc_decimalFinite)
    {
        return mloc_success;
    }
    return precision->kind == mloc_precisionNone ? mlocRoundConverted(amount, exact, nullptr)
                                                 : mlocApplyPrecision(precision, amount);
}

// What writing the parts needs.
typedef struct Mixed
{
    const UnitFormat* unit;
    const UnitOutput* output;
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
    mlocUnitOuter(m->unit, m->output->slot + index, &outer);
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
    uint32_t slot = m->output->slot + m->last;
    const uint8_t* text = m->unit->patterns + m->unit->start[slot][category];
    size_t length = m->unit->length[slot][category];
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

static void SetUp(Mixed* m, const UnitFormat* u, const UnitOutput* o)
{
    m->unit = u;
    m->output = o;
    m->last = o->smallest;
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

// Writes a field of the numeric width: its part's amount, 0 for a field
// the unit lacks, the smallest with the formatter's options; at least two
// digits for a doubled letter.
static mlocResult WriteField(const UnitFormat* u, const UnitOutput* o,
                             const mlocUnitAmounts* amounts, int32_t part, bool doubled,
                             NumberWriter* w)
{
    Formatter f = u->formatter;
    if (part != o->smallest)
    {
        f.precision = (mlocPrecision){0};
        f.grouping = mloc_groupingAuto;
        f.minInteger = 1;
    }
    f.signDisplay = mloc_signAuto;
    f.minInteger = doubled && f.minInteger < 2 ? 2 : f.minInteger;
    mlocDecimal zero = {0};
    return mlocWriteFormatted(&f, nullptr, part >= 0 ? &amounts->amounts[part] : &zero, w);
}

// Writes the amounts by a duration pattern, as ICU's formatNumeric: "h"
// (or "H"), "m" and "s" stand for the fields, doubled for two digits;
// text in apostrophes is literal, "''" an apostrophe.
static mlocResult WriteNumeric(const UnitFormat* u, const UnitOutput* o,
                               const mlocUnitAmounts* amounts, NumberWriter* w)
{
    const char* p = u->duration[o->numeric - 1];
    size_t length = u->durationLength[o->numeric - 1];
    bool quoted = false;
    mlocResult r = mloc_success;
    for (size_t at = 0; at < length; at++)
    {
        char c = p[at];
        bool doubled = at + 1 < length && p[at + 1] == c;
        int32_t f = c == 'h' || c == 'H' ? 0 : c == 'm' ? 1 : c == 's' ? 2 : -1;
        if (c == '\'' && doubled)
        {
            mlocWriteLiteral(w, "'", 1);
            at++;
        }
        else if (c == '\'')
        {
            quoted = !quoted;
        }
        else if (f >= 0 && !quoted)
        {
            mlocResult s = WriteField(u, o, amounts, o->field[f], doubled, w);
            r = r == mloc_success ? s : r;
            at += doubled ? 1 : 0;
        }
        else
        {
            mlocWriteLiteral(w, p + at, 1);
        }
    }
    return r;
}

// Whether amounts take the numeric width: none negative or not finite.
static bool NumericAmounts(const UnitOutput* o, const mlocUnitAmounts* amounts)
{
    bool plain = o->numeric != 0;
    for (uint32_t i = 0; i < o->parts; i++)
    {
        plain &= amounts->amounts[i].kind == mloc_decimalFinite && !amounts->amounts[i].negative;
    }
    return plain;
}

mlocResult mlocWriteMixedTo(const UnitFormat* u, const UnitOutput* o,
                            const mlocUnitAmounts* amounts, NumberWriter* writer)
{
    if (NumericAmounts(o, amounts))
    {
        return WriteNumeric(u, o, amounts, writer);
    }
    static Mixed s_zero;
    Mixed m = s_zero;
    SetUp(&m, u, o);
    m.amounts = *amounts;
    char leads[unitMaxParts][itemLead];
    ListItem items[unitMaxParts];
    for (uint32_t i = 0; i < o->parts; i++)
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
    mlocWriteList(&u->list, items, o->parts, WritePart, &m, writer);
    return m.result;
}

mlocResult mlocWriteMixed(const UnitFormat* u, const UnitOutput* o, const mlocUnitAmounts* amounts,
                          mlocOutput* output)
{
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    mlocResult r = mlocWriteMixedTo(u, o, amounts, &w);
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
        (void)mlocWriteMixedTo(u, o, amounts, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}

mlocResult mlocFormatMixedUnit(const UnitFormat* u, const mlocDecimal* decimal, mlocOutput* output)
{
    bool nan = decimal->kind == mloc_decimalNaN;
    UnitValue value = {0};
    if (!nan && !mlocUnitValueOf(decimal, &value))
    {
        return mloc_errorCapacity;
    }
    mlocUnitAmounts amounts;
    mlocResult r = mlocSplitMixed(&u->table, &u->output[0].id, nullptr, &value, nan,
                                  mlocRoundSmallest, (void*)&u->formatter.precision, &amounts);
    return r == mloc_success ? mlocWriteMixed(u, &u->output[0], &amounts, output) : r;
}
