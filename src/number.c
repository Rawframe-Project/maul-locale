// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Number formatting (record mloc-0021): the formatter is the core and its
// options; formatting rounds the decimal, then writes it.

#include "maul-locale/number.h"

#include "decimal.h"
#include "number_compact.h"
#include "number_core.h"
#include "number_precision.h"
#include "plural.h"
#include "plural_rules.h"

#include <string.h>

enum
{
    // Marks a value the library made.
    formatterStamp = 0x4E,
    maxMinInteger = 21,
};

// The opaque value's contents.
typedef struct Formatter
{
    NumberCore core;
    // Compact notation's table and patterns; null otherwise.
    const uint8_t* compactTable;
    const uint8_t* compactPatterns;
    mlocPrecision precision;
    uint8_t notation;
    uint8_t style;
    uint8_t grouping;
    uint8_t signDisplay;
    uint8_t minInteger;
    // The locale's cardinal rule set, for compact patterns.
    uint8_t pluralRule;
    uint8_t stamp;
} Formatter;

static_assert(sizeof(Formatter) <= sizeof(mlocNumberFormatter),
              "a formatter fits its opaque value");

static bool IsCompact(uint32_t notation)
{
    return notation == mloc_notationCompactShort || notation == mloc_notationCompactLong;
}

static bool ValidOptions(const mlocNumberOptions* o)
{
    // Compact notation writes the decimal style alone.
    return (uint32_t)o->notation <= mloc_notationCompactLong &&
           (uint32_t)o->style <= mloc_numberPerMille &&
           (!IsCompact(o->notation) || o->style == mloc_numberDecimal) &&
           (uint32_t)o->grouping <= mloc_groupingOff &&
           (uint32_t)o->signDisplay <= mloc_signNegative && o->minIntegerDigits <= maxMinInteger &&
           mlocValidPrecision(&o->precision);
}

mlocResult mlocInitNumberFormatter(mlocNumberFormatter* formatterOut, const mlocDataSet* data,
                                   const mlocLocale* locale, const mlocNumberOptions* options)
{
    static const mlocNumberOptions s_defaults = {0};
    const mlocNumberOptions* o = options == nullptr ? &s_defaults : options;
    if (formatterOut == nullptr || data == nullptr || locale == nullptr || !ValidOptions(o))
    {
        return mloc_errorInvalid;
    }
    Formatter f;
    memset(&f, 0, sizeof(f));
    mlocResult result =
        IsCompact(o->notation)
            ? mlocInitCompactCore(&f.core, data, locale, o->notation == mloc_notationCompactLong,
                                  &f.compactTable, &f.compactPatterns)
            : mlocInitNumberCore(&f.core, data, locale, o->style);
    if (result >= 0 && IsCompact(o->notation))
    {
        mlocResult rule = mlocFindPluralRule(data, locale, mloc_pluralCardinal, &f.pluralRule);
        result = rule < 0 ? rule : result;
    }
    if (result < 0)
    {
        return result;
    }
    f.precision = o->precision;
    f.notation = (uint8_t)o->notation;
    f.style = (uint8_t)o->style;
    f.grouping = (uint8_t)o->grouping;
    f.signDisplay = (uint8_t)o->signDisplay;
    f.minInteger = o->minIntegerDigits == 0 ? 1 : o->minIntegerDigits;
    f.stamp = formatterStamp;
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

// The formatter of an opaque value, or false when the library did not
// make it.
static bool Read(const mlocNumberFormatter* opaque, Formatter* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    return f->stamp == formatterStamp && f->core.symbolText != nullptr &&
           f->core.affixText != nullptr && f->core.digitLength >= 1 &&
           f->core.digitLength <= numberDigitSize && f->minInteger >= 1 &&
           f->minInteger <= maxMinInteger && f->grouping <= mloc_groupingOff &&
           f->signDisplay <= mloc_signNegative && f->notation <= mloc_notationCompactLong &&
           f->style <= mloc_numberPerMille && (f->core.primary == 0) == (f->core.secondary == 0) &&
           IsCompact(f->notation) == (f->compactTable != nullptr) &&
           (f->compactTable == nullptr ||
            (f->compactPatterns != nullptr && f->pluralRule < mlocPluralRuleSetCount)) &&
           mlocValidPrecision(&f->precision);
}

// Rounds a finite decimal in compact notation and points the core at the
// compact pattern chosen.
static mlocResult Compact(const Formatter* f, mlocDecimal* d, NumberLayout* layout,
                          NumberCore* core)
{
    // Compact notation groups only with two digits before the first
    // separator, unless the options say otherwise.
    layout->grouping = layout->grouping == mloc_groupingAuto ? mloc_groupingMin2 : layout->grouping;
    CompactChoice choice;
    CompactData compact = {.table = f->compactTable, .patterns = f->compactPatterns};
    mlocResult r = mlocApplyCompact(compact, &f->precision, f->pluralRule, d, &choice);
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
    mlocResult r = f->style == mloc_numberDecimal
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
    NumberCore core;
    NumberSign sign;
} Prepared;

static mlocResult Prepare(const Formatter* f, const mlocDecimal* decimal, Prepared* p)
{
    p->decimal = *decimal;
    p->layout = (NumberLayout){
        .grouping = (mlocGrouping)f->grouping,
        .minInteger = f->minInteger,
        .perMille = f->style == mloc_numberPerMille,
        .scientific =
            f->notation == mloc_notationScientific || f->notation == mloc_notationEngineering,
    };
    p->core = f->core;
    if (p->decimal.kind == mloc_decimalFinite)
    {
        mlocResult r = f->compactTable != nullptr ? Compact(f, &p->decimal, &p->layout, &p->core)
                                                  : Round(f, &p->decimal, &p->layout);
        if (r != mloc_success)
        {
            return r;
        }
    }
    p->sign = mlocNumberSign(&p->decimal, (mlocSignDisplay)f->signDisplay);
    return mloc_success;
}

static bool CheckArguments(const mlocNumberFormatter* formatter, Formatter* f,
                           const mlocDecimal* decimal, const mlocOutput* output)
{
    return Read(formatter, f) && decimal != nullptr && output != nullptr &&
           (output->buffer != nullptr || output->capacity == 0) && mlocValidDecimal(decimal);
}

// Ends a text pass: the length written, and whether it fit.
static mlocResult Finish(const NumberWriter* w, mlocOutput* output)
{
    output->length = w->length;
    return output->buffer == nullptr || w->length > output->capacity ? mloc_errorCapacity
                                                                     : mloc_success;
}

mlocResult mlocFormatNumber(const mlocNumberFormatter* formatter, const mlocDecimal* decimal,
                            mlocOutput* output)
{
    Formatter f;
    Prepared p;
    if (!CheckArguments(formatter, &f, decimal, output))
    {
        return mloc_errorInvalid;
    }
    mlocResult r = Prepare(&f, decimal, &p);
    if (r != mloc_success)
    {
        return r;
    }
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    mlocWriteNumber(&p.core, &p.decimal, p.sign, &p.layout, &w);
    r = Finish(&w, output);
    if (r == mloc_success && output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        mlocWriteNumber(&p.core, &p.decimal, p.sign, &p.layout, &parts);
        mlocEndParts(&parts);
    }
    return r;
}

// Whether two ends share their affixes: one sign, one pattern. The rest
// of their cores is the formatter's.
static bool SameAffixes(const Prepared* a, const Prepared* b)
{
    return a->sign == b->sign && a->core.affixText == b->core.affixText &&
           memcmp(a->core.affixStart, b->core.affixStart, sizeof(a->core.affixStart)) == 0 &&
           memcmp(a->core.affixLength, b->core.affixLength, sizeof(a->core.affixLength)) == 0;
}

// Whether two prepared ends write alike.
static bool Alike(const Prepared* a, const Prepared* b)
{
    return memcmp(&a->decimal, &b->decimal, sizeof(a->decimal)) == 0 && SameAffixes(a, b) &&
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

// Writes a range as ICU's NumberRangeFormatter does with its default
// collapse and identity fallback.
static void WriteRange(const NumberCore* core, Prepared* a, Prepared* b, NumberWriter* w)
{
    if (Alike(a, b))
    {
        a->layout.approximately = true;
        w->source = 0;
        mlocWriteNumber(&a->core, &a->decimal, a->sign, &a->layout, w);
        return;
    }
    // An affix both share is written once when it is more than one
    // character; any affix written twice, or an exponent, spaces the
    // separator.
    size_t points = mlocAffixCodePoints(&a->core, a->sign, &a->layout);
    bool collapse = SameAffixes(a, b) && a->layout.noNumber == b->layout.noNumber && points > 1 &&
                    !a->layout.scientific;
    bool spaced = !collapse && (points > 0 || a->layout.scientific);
    a->layout.skipSuffix = collapse;
    a->layout.sharedPrefix = collapse;
    b->layout.skipPrefix = collapse;
    b->layout.sharedSuffix = collapse;
    w->source = mloc_numberRangeStart;
    mlocWriteNumber(&a->core, &a->decimal, a->sign, &a->layout, w);
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
    mlocWriteNumber(&b->core, &b->decimal, b->sign, &b->layout, w);
}

mlocResult mlocFormatNumberRange(const mlocNumberFormatter* formatter, const mlocDecimal* start,
                                 const mlocDecimal* end, mlocOutput* output)
{
    Formatter f;
    Prepared a;
    Prepared b;
    if (!CheckArguments(formatter, &f, start, output) || end == nullptr || !mlocValidDecimal(end) ||
        start->kind == mloc_decimalNaN || end->kind == mloc_decimalNaN)
    {
        return mloc_errorInvalid;
    }
    mlocResult r = Prepare(&f, start, &a);
    r = r == mloc_success ? Prepare(&f, end, &b) : r;
    if (r != mloc_success)
    {
        return r;
    }
    Prepared textA = a;
    Prepared textB = b;
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    WriteRange(&f.core, &textA, &textB, &w);
    r = Finish(&w, output);
    if (r == mloc_success && output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        WriteRange(&f.core, &a, &b, &parts);
        mlocEndParts(&parts);
    }
    return r;
}
