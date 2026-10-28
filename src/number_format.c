// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The formatting the number formatters share (number_format.h).

#include "number_format.h"

#include "currency_tables.h"
#include "decimal.h"
#include "invariant.h"
#include "number_compact.h"
#include "number_precision.h"
#include "plural.h"
#include "plural_rules.h"

#include <string.h>

enum
{
    maxMinInteger = 21,
};

static bool IsCompact(uint32_t notation)
{
    return notation == mloc_notationCompactShort || notation == mloc_notationCompactLong;
}

bool mlocValidFormatOptions(const mlocNumberOptions* o)
{
    // Compact notation writes the decimal style alone.
    return (uint32_t)o->notation <= mloc_notationCompactLong &&
           (uint32_t)o->style <= mloc_numberPerMille &&
           (!IsCompact(o->notation) || o->style == mloc_numberDecimal) &&
           (uint32_t)o->grouping <= mloc_groupingOff &&
           (uint32_t)o->signDisplay <= mloc_signNegative && o->minIntegerDigits <= maxMinInteger &&
           mlocValidPrecision(&o->precision);
}

void mlocSetFormatOptions(Formatter* f, const mlocNumberOptions* o, uint8_t stamp)
{
    f->precision = o->precision;
    f->notation = (uint8_t)o->notation;
    f->style = (uint8_t)o->style;
    f->grouping = (uint8_t)o->grouping;
    f->signDisplay = (uint8_t)o->signDisplay;
    f->minInteger = o->minIntegerDigits == 0 ? 1 : o->minIntegerDigits;
    f->stamp = stamp;
}

bool mlocReadFormatter(const void* opaque, size_t size, uint8_t stamp, Formatter* f)
{
    if (opaque == nullptr || size < sizeof(*f))
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    const NumberCore* c = &f->core;
    return f->stamp == stamp && c->digitLength >= 1 && c->digitLength <= numberDigitSize &&
           f->minInteger >= 1 && f->minInteger <= maxMinInteger &&
           f->grouping <= mloc_groupingOff && f->signDisplay <= mloc_signNegative &&
           f->notation <= mloc_notationCompactLong && f->style <= mloc_numberPerMille &&
           f->unscaled <= 1 && (c->primary == 0) == (c->secondary == 0) &&
           IsCompact(f->notation) == (f->compactTable != nullptr) &&
           (f->compactTable == nullptr ||
            (f->compactPatterns != nullptr && f->pluralRule < mlocPluralRuleSetCount)) &&
           mlocValidPrecision(&f->precision);
}

// Rounds a finite decimal in compact notation and points the core at the
// compact pattern chosen.
static mlocResult Compact(const Formatter* f, mlocDecimal* d, NumberLayout* layout,
                          NumberCore* core, int32_t* exponentOut)
{
    // Compact notation groups only with two digits before the first
    // separator, unless the options say otherwise.
    layout->grouping = layout->grouping == mloc_groupingAuto ? mloc_groupingMin2 : layout->grouping;
    CompactChoice choice;
    CompactData compact = {.table = f->compactTable, .patterns = f->compactPatterns};
    mlocResult r = mlocApplyCompact(compact, &f->precision, f->pluralRule, d, &choice);
    *exponentOut = choice.exponent;
    if (r == mloc_success && choice.pattern != compactNoPattern)
    {
        layout->noNumber = mlocUseCompactPattern(core, f->compactPatterns, choice.pattern);
        layout->compact = true;
    }
    return r;
}

// Scales a finite decimal for its style and rounds it, in scientific
// notation its mantissa.
static mlocResult Round(const Formatter* f, mlocDecimal* d, NumberLayout* layout)
{
    mlocResult r = f->style == mloc_numberDecimal || f->unscaled != 0
                       ? mloc_success
                       : mlocScaleDecimal(d, f->style == mloc_numberPercent ? 2 : 3);
    if (r == mloc_success && layout->scientific)
    {
        // A mantissa of one to three digits is never grouped.
        layout->grouping = mloc_groupingOff;
        return mlocApplyScientific(&f->precision, f->notation == mloc_notationEngineering, d,
                                   &layout->exponent);
    }
    return r == mloc_success ? mlocApplyPrecision(&f->precision, d) : r;
}

// A decimal made ready to write: rounded, with its layout, the core its
// affixes come from and its sign.
typedef struct Prepared
{
    mlocDecimal decimal;
    NumberLayout layout;
    // The formatter's core, or in compact notation the copy its pattern
    // is chosen in (a Prepared copied keeps pointing at its source's).
    const NumberCore* core;
    NumberCore compact;
    NumberSign sign;
    // The compact exponent, the plural operand c.
    int32_t exponent;
} Prepared;

static mlocResult Prepare(const Formatter* f, const mlocDecimal* decimal, Prepared* p)
{
    p->decimal = *decimal;
    p->exponent = 0;
    p->layout = (NumberLayout){
        .grouping = (mlocGrouping)f->grouping,
        .minInteger = f->minInteger,
        .perMille = f->style == mloc_numberPerMille,
        .scientific =
            f->notation == mloc_notationScientific || f->notation == mloc_notationEngineering,
    };
    p->core = &f->core;
    if (p->decimal.kind == mloc_decimalFinite)
    {
        if (f->compactTable != nullptr)
        {
            p->compact = f->core;
            p->core = &p->compact;
        }
        mlocResult r = f->compactTable != nullptr
                           ? Compact(f, &p->decimal, &p->layout, &p->compact, &p->exponent)
                           : Round(f, &p->decimal, &p->layout);
        if (r != mloc_success)
        {
            return r;
        }
    }
    p->sign = mlocNumberSign(&p->decimal, (mlocSignDisplay)f->signDisplay);
    return mloc_success;
}

// The plural category of a prepared number, other for infinity and NaN.
// A compact number's operands are those of its whole value with its
// exponent (UTS #35: "1.2c6" has i 1200000 and c 6), so "1M" takes
// "days" as ICU writes it, though its pattern took the one of "1".
static uint32_t Category(const Formatter* f, const Prepared* p)
{
    if (p->decimal.kind != mloc_decimalFinite)
    {
        return mloc_pluralOther;
    }
    mlocDecimal whole = p->decimal;
    if (whole.count > 0 && p->exponent > 0)
    {
        whole.exponent += p->exponent;
        whole.fractionDigits =
            whole.fractionDigits > p->exponent ? whole.fractionDigits - p->exponent : 0;
    }
    mlocPluralOperands operands;
    mlocFillPluralOperands(&whole, &operands);
    operands.c = p->exponent > 0 ? (uint32_t)p->exponent : 0;
    return mlocSelectPluralRule(f->pluralRule, &operands);
}

// Where a category's outer pattern writes the number; its length when it
// writes none. Patterns are a few bytes, which a loop scans for less than
// a call.
static size_t NumberAt(const Outer* o, uint32_t category)
{
    MLOC_ASSERT(category < sizeof(o->patternStart) / sizeof(o->patternStart[0]));
    const uint8_t* pattern = o->patternText + o->patternStart[category];
    size_t length = o->patternLength[category];
    size_t at = 0;
    while (at < length && pattern[at] != currencyTextNumber)
    {
        at++;
    }
    return at;
}

// Writes the part of an outer pattern before its number (at split), or
// after it.
static void WriteOuterSide(const Outer* o, uint32_t category, size_t split, bool before,
                           NumberWriter* w)
{
    const uint8_t* pattern = o->patternText + o->patternStart[category];
    size_t length = o->patternLength[category];
    size_t at = before ? 0 : (split < length ? split + 1 : length);
    size_t end = before ? split : length;
    size_t literal = at;
    for (; at < end; at++)
    {
        if (pattern[at] != currencyTextOther && pattern[at] != currencyTextNumber)
        {
            continue;
        }
        mlocWriteTrimmed(w, o->patternKind, pattern + literal, at - literal);
        if (pattern[at] == currencyTextOther)
        {
            const uint8_t* text = o->text[category];
            mlocWritePart(w, o->textKind, text != nullptr ? text : (const uint8_t*)o->code,
                          text != nullptr ? o->textLength[category] : sizeof(o->code));
        }
        literal = at + 1;
    }
    mlocWriteTrimmed(w, o->patternKind, pattern + literal, end - literal);
}

// Writes a prepared number, within an outer pattern's of its category
// when there is one.
static void WriteIn(const Outer* o, const Prepared* p, uint32_t category, NumberWriter* w)
{
    size_t split = o == nullptr ? 0 : NumberAt(o, category);
    if (o != nullptr)
    {
        WriteOuterSide(o, category, split, true, w);
    }
    if (o == nullptr || split < o->patternLength[category])
    {
        mlocWriteNumber(p->core, &p->decimal, p->sign, &p->layout, w);
    }
    if (o != nullptr)
    {
        WriteOuterSide(o, category, split, false, w);
    }
}

// Writes a prepared number, within an outer pattern when there is one.
static void WriteOne(const Formatter* f, const Outer* o, const Prepared* p, NumberWriter* w)
{
    WriteIn(o, p, o == nullptr ? 0 : Category(f, p), w);
}

mlocResult mlocFormattedCategory(const Formatter* f, const mlocDecimal* decimal,
                                 uint32_t* categoryOut)
{
    Prepared p;
    mlocResult r = Prepare(f, decimal, &p);
    *categoryOut = r == mloc_success ? Category(f, &p) : mloc_pluralOther;
    return r;
}

mlocResult mlocWriteFormatted(const Formatter* f, const Outer* outer, const mlocDecimal* decimal,
                              NumberWriter* writer)
{
    Prepared p;
    mlocResult r = Prepare(f, decimal, &p);
    if (r == mloc_success)
    {
        WriteOne(f, outer, &p, writer);
    }
    return r;
}

static bool CheckArguments(const mlocDecimal* decimal, const mlocOutput* output)
{
    return decimal != nullptr && output != nullptr &&
           (output->buffer != nullptr || output->capacity == 0) && mlocValidDecimal(decimal);
}

// Ends a text pass: the length written, and whether it fit.
static mlocResult Finish(const NumberWriter* w, mlocOutput* output)
{
    output->length = w->length;
    return output->buffer == nullptr || w->length > output->capacity ? mloc_errorCapacity
                                                                     : mloc_success;
}

// Writes a prepared number into an output, within the outer pattern's of
// the category: the text, then the parts.
static mlocResult WritePrepared(const Outer* outer, const Prepared* p, uint32_t category,
                                mlocOutput* output)
{
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    WriteIn(outer, p, category, &w);
    mlocResult r = Finish(&w, output);
    if (r == mloc_success && output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        WriteIn(outer, p, category, &parts);
        mlocEndParts(&parts);
    }
    return r;
}

mlocResult mlocFormatWith(const Formatter* f, const Outer* outer, const mlocDecimal* decimal,
                          mlocOutput* output)
{
    Prepared p;
    if (!CheckArguments(decimal, output))
    {
        return mloc_errorInvalid;
    }
    mlocResult r = Prepare(f, decimal, &p);
    if (r != mloc_success)
    {
        return r;
    }
    return WritePrepared(outer, &p, outer == nullptr ? 0 : Category(f, &p), output);
}

bool mlocShownDecimal(const Formatter* f, const mlocDecimal* decimal, mlocDecimal* out)
{
    Prepared p;
    if (decimal->kind != mloc_decimalFinite || f->compactTable != nullptr ||
        f->notation == mloc_notationScientific || f->notation == mloc_notationEngineering ||
        Prepare(f, decimal, &p) != mloc_success)
    {
        return false;
    }
    *out = p.decimal;
    out->negative = 0;
    return true;
}

mlocResult mlocFormatChosen(const Formatter* f, OuterChooser choose, void* context,
                            const mlocDecimal* decimal, mlocOutput* output)
{
    Prepared p;
    mlocResult r = Prepare(f, decimal, &p);
    uint32_t category = r == mloc_success ? Category(f, &p) : 0;
    const Outer* outer = r == mloc_success ? choose(context, category, &r) : nullptr;
    if (r != mloc_success)
    {
        return r;
    }
    return WritePrepared(outer, &p, category, output);
}

// Whether two ends share their affixes: one sign, one pattern. The rest
// of their cores is the formatter's.
static bool SameAffixes(const Prepared* a, const Prepared* b)
{
    return a->sign == b->sign && a->core->affixText == b->core->affixText &&
           memcmp(a->core->affixStart, b->core->affixStart, sizeof(a->core->affixStart)) == 0 &&
           memcmp(a->core->affixLength, b->core->affixLength, sizeof(a->core->affixLength)) == 0;
}

// Whether two prepared ends write alike.
static bool Alike(const Prepared* a, const Prepared* b)
{
    return mlocSameDecimal(&a->decimal, &b->decimal) && SameAffixes(a, b) &&
           a->layout.exponent == b->layout.exponent && a->layout.noNumber == b->layout.noNumber;
}

// Whether text starts (or ends) with Pattern_White_Space, which ICU
// takes as spacing already in a range pattern: U+0009 to U+000D, U+0020,
// U+0085, U+200E, U+200F, U+2028 and U+2029.
static bool IsSpace(const uint8_t* text, size_t length, bool atEnd)
{
    static const char* const s_spaces[] = {
        "\x09",     "\x0A",         "\x0B",         "\x0C",         "\x0D",        " ",
        "\xC2\x85", "\xE2\x80\x8E", "\xE2\x80\x8F", "\xE2\x80\xA8", "\xE2\x80\xA9"};
    for (size_t i = 0; i < sizeof(s_spaces) / sizeof(s_spaces[0]); i++)
    {
        size_t n = strlen(s_spaces[i]);
        if (n <= length && memcmp(atEnd ? text + length - n : text, s_spaces[i], n) == 0)
        {
            return true;
        }
    }
    return false;
}

void mlocWriteRangeSeparator(const NumberCore* core, bool spaced, NumberWriter* w)
{
    w->source = 0;
    const uint8_t* separator = core->symbolText + core->symbolStart[numberRangeSeparator];
    size_t length = core->symbolLength[numberRangeSeparator];
    if (spaced && !IsSpace(separator, length, false))
    {
        mlocWriteLiteral(w, " ", 1);
    }
    mlocWriteLiteral(w, (const char*)separator, length);
    if (spaced && !IsSpace(separator, length, true))
    {
        mlocWriteLiteral(w, " ", 1);
    }
    w->source = mloc_numberRangeEnd;
}

// Writes a range as ICU's NumberRangeFormatter does with its default
// collapse and identity fallback.
static void WriteRange(const NumberCore* core, Prepared* a, Prepared* b, NumberWriter* w)
{
    if (Alike(a, b))
    {
        a->layout.approximately = true;
        w->source = 0;
        mlocWriteNumber(a->core, &a->decimal, a->sign, &a->layout, w);
        return;
    }
    // An affix both share is written once when it is more than one
    // character; any affix written twice, or an exponent, spaces the
    // separator.
    size_t points = mlocAffixCodePoints(a->core, a->sign, &a->layout);
    bool collapse = SameAffixes(a, b) && a->layout.noNumber == b->layout.noNumber && points > 1 &&
                    !a->layout.scientific;
    bool spaced = !collapse && (points > 0 || a->layout.scientific);
    a->layout.skipSuffix = collapse;
    a->layout.sharedPrefix = collapse;
    b->layout.skipPrefix = collapse;
    b->layout.sharedSuffix = collapse;
    w->source = mloc_numberRangeStart;
    mlocWriteNumber(a->core, &a->decimal, a->sign, &a->layout, w);
    mlocWriteRangeSeparator(core, spaced, w);
    mlocWriteNumber(b->core, &b->decimal, b->sign, &b->layout, w);
}

// Writes a range within an outer pattern, once around both ends, as ICU
// collapses a long name or a unit: in the category CLDR's plural ranges
// give the ends' ("1–5 долараў"), other where they give none, the
// start's when the ends write alike.
static void WriteOuterRange(const Formatter* f, const Outer* o, Prepared* a, Prepared* b,
                            NumberWriter* w)
{
    mlocPluralCategory category = (mlocPluralCategory)Category(f, a);
    if (!Alike(a, b))
    {
        category = mlocResolvePluralRange(&o->rules, category, (mlocPluralCategory)Category(f, b));
    }
    w->source = 0;
    size_t split = NumberAt(o, category);
    WriteOuterSide(o, category, split, true, w);
    if (split < o->patternLength[category])
    {
        WriteRange(&f->core, a, b, w);
        w->source = 0;
        WriteOuterSide(o, category, split, false, w);
    }
}

static void WriteEither(const Formatter* f, const Outer* outer, Prepared* a, Prepared* b,
                        NumberWriter* w)
{
    if (outer != nullptr)
    {
        WriteOuterRange(f, outer, a, b, w);
    }
    else
    {
        WriteRange(&f->core, a, b, w);
    }
}

mlocResult mlocFormatRangeWith(const Formatter* f, const Outer* outer, const mlocDecimal* start,
                               const mlocDecimal* end, mlocOutput* output)
{
    Prepared a;
    Prepared b;
    if (!CheckArguments(start, output) || end == nullptr || !mlocValidDecimal(end) ||
        start->kind == mloc_decimalNaN || end->kind == mloc_decimalNaN)
    {
        return mloc_errorInvalid;
    }
    mlocResult r = Prepare(f, start, &a);
    r = r == mloc_success ? Prepare(f, end, &b) : r;
    if (r != mloc_success)
    {
        return r;
    }
    Prepared textA = a;
    Prepared textB = b;
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    WriteEither(f, outer, &textA, &textB, &w);
    r = Finish(&w, output);
    if (r == mloc_success && output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        WriteEither(f, outer, &a, &b, &parts);
        mlocEndParts(&parts);
    }
    return r;
}
