// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Currency formatting: a number formatter's core with the locale's
// currency pattern, the currency's symbol, separators and names gathered
// field by field along the locale's chain, its digits, and for a long name
// or the ISO append a pattern around the number.

#include "maul-locale/currency.h"

#include "bytes.h"
#include "currency_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "locale_keyword.h"
#include "number_core.h"
#include "number_data.h"
#include "number_format.h"
#include "number_parse.h"
#include "parse_currency.h"
#include "plural.h"
#include "plural_rules.h"

#include <string.h>

enum
{
    // Marks a currency formatter the library made.
    currencyStamp = 0x43,
};

// The opaque value's contents: the formatter, the pattern around its
// number when it has one, and the currency units of its locale's chain,
// which parsing reads.
typedef struct Currency
{
    Formatter formatter;
    Outer outer;
    bool hasOuter;
    // The outer pattern is the ISO append's.
    bool appended;
    CurrencyChain chain;
} Currency;

static_assert(sizeof(Currency) <= sizeof(mlocCurrencyFormatter),
              "a currency formatter fits its opaque value");
static_assert((int)currencyFieldCount == (int)currencyNameFieldCount,
              "the symbol and name units have as many fields");

// A currency's fields of one kind of unit, each from the first key along
// the chain that gives it.
typedef struct Fields
{
    uint32_t kind;
    uint16_t code;
    uint32_t found;
    const uint8_t* text[currencyFieldCount];
    uint16_t length[currencyFieldCount];
    uint8_t spacing[currencyFieldCount];
    // The own pattern's index, in the patterns unit of its source.
    uint16_t pattern;
    DataSpan patterns;
    bool broken;
} Fields;

static bool Gather(void* context, DataSpan unit, const DataView* view, int depth)
{
    (void)depth;
    Fields* f = context;
    bool symbols = f->kind == kindCurrencySymbols;
    SparseTable t = mlocReadSparseTable(unit);
    const uint8_t* row = mlocFindSparseRow(&t, f->code);
    for (uint32_t field = 0; row != nullptr && field < currencyFieldCount; field++)
    {
        uint16_t value = 0;
        if ((f->found & 1u << field) != 0 || !mlocSparseField(row, field, &value))
        {
            continue;
        }
        f->found |= 1u << field;
        if (symbols && field == currencyOwnPattern)
        {
            // The index names a pattern of the unit's own source, which a
            // walk over kept units (view null) does not read.
            f->pattern = value;
            f->broken |= view != nullptr &&
                         !mlocFindDataUnit(view, kindNumberPatterns, dataNoLocale, &f->patterns);
            continue;
        }
        uint16_t start = mlocLoad16(t.offsets + (size_t)value * 2);
        f->text[field] = t.text + start;
        f->length[field] = (uint16_t)(mlocLoad16(t.offsets + (size_t)value * 2 + 2) - start);
        f->spacing[field] = symbols ? t.text[t.textSize + value] : 0;
    }
    return f->found == (1u << currencyFieldCount) - 1;
}

// A currency's fields of a kind along a locale's chain; false when the
// chain cannot be walked or a field names what its source lacks.
static bool GatherFields(const mlocDataSet* data, const mlocLocale* locale, uint32_t kind,
                         uint16_t code, Fields* out)
{
    *out = (Fields){.kind = kind, .code = code};
    return mlocVisitLocaleUnits(data, mloc_componentMain, kind, locale, Gather, out) &&
           !out->broken;
}

// The field a display shows, falling back to the symbol; currencyFieldCount
// for the code, which a currency without the field or a symbol shows.
static uint32_t ShownField(const Fields* f, mlocCurrencyDisplay display)
{
    uint32_t field = display == mloc_currencyNarrowSymbol    ? currencyNarrow
                     : display == mloc_currencyFormalSymbol  ? currencyFormal
                     : display == mloc_currencyVariantSymbol ? currencyVariant
                                                             : currencySymbol;
    if (display == mloc_currencyCode || display == mloc_currencyName ||
        display == mloc_currencyHidden)
    {
        return currencyFieldCount;
    }
    field = (f->found & 1u << field) != 0 ? field : currencySymbol;
    return (f->found & 1u << field) != 0 ? field : currencyFieldCount;
}

static bool IsSymbolDisplay(mlocCurrencyDisplay display)
{
    return display == mloc_currencySymbol || display == mloc_currencyNarrowSymbol ||
           display == mloc_currencyFormalSymbol || display == mloc_currencyVariantSymbol;
}

static bool ValidOptions(const mlocCurrencyOptions* o)
{
    const mlocNumberOptions* n = &o->number;
    return mlocValidFormatOptions(n) && n->style == mloc_numberDecimal &&
           (uint32_t)o->display <= mloc_currencyName &&
           (uint32_t)o->usage <= mloc_currencyUsageCash && o->appendCode <= 1 &&
           (uint32_t)o->sign <= mloc_currencySignAccounting &&
           (o->appendCode == 0 || IsSymbolDisplay(o->display));
}

// A currency's fraction; false for data without the fractions.
static bool FindFraction(const mlocDataSet* data, uint16_t code, CurrencyFraction* out)
{
    DataSpan fractions;
    if (!mlocFindDataSetUnit(data, kindCurrencyFractions, nullptr, 0, &fractions))
    {
        return false;
    }
    *out = mlocFindCurrencyFraction(fractions, code);
    return true;
}

// The zero precision's: the currency's digits and rounding increment.
static bool CurrencyPrecision(const mlocDataSet* data, uint16_t code, mlocCurrencyUsage usage,
                              mlocPrecision* precision)
{
    CurrencyFraction f;
    if (!FindFraction(data, code, &f))
    {
        return false;
    }
    bool cash = usage == mloc_currencyUsageCash;
    uint8_t digits = cash ? f.cashDigits : f.digits;
    uint8_t rounding = cash ? f.cashRounding : f.rounding;
    precision->kind = mloc_precisionFraction;
    precision->minFractionDigits = digits;
    precision->maxFractionDigits = digits;
    precision->increment = rounding > 1 ? rounding : 0;
    return true;
}

// Whether the options, or else the locale (-u-cf-account), ask for
// accounting.
static bool Accounting(const mlocLocale* locale, mlocCurrencySign sign)
{
    if (sign != mloc_currencySignLocale)
    {
        return sign == mloc_currencySignAccounting;
    }
    const char* value = nullptr;
    size_t length = mlocFindKeyword(locale, "cf", &value);
    return length == 7 && memcmp(value, "account", 7) == 0;
}

// Sets the currency a core shows and the separators it gives of its own.
static bool ShowCurrency(NumberCore* core, const Fields* f, mlocCurrencyDisplay display,
                         bool ownPattern)
{
    mlocUnpackCurrency(f->code, core->currencyCode);
    uint32_t field = ShownField(f, display);
    if (field != currencyFieldCount)
    {
        if (f->length[field] > UINT8_MAX)
        {
            return false;
        }
        core->currencyText = f->text[field];
        core->currencyLength = (uint8_t)f->length[field];
    }
    if ((f->found & 1u << currencyOwnDecimal) != 0)
    {
        core->decimalText = f->text[currencyOwnDecimal];
        core->symbolLength[numberDecimal] = f->length[currencyOwnDecimal];
    }
    if ((f->found & 1u << currencyOwnGroup) != 0)
    {
        core->groupText = f->text[currencyOwnGroup];
        core->symbolLength[numberGroup] = f->length[currencyOwnGroup];
    }
    return !ownPattern || mlocUseNumberPattern(core, f->patterns, f->pattern);
}

// The name a category takes: its own, else other's, else the name without
// a count; currencyNameFieldCount for the code (UTS #35).
static uint32_t NameField(const Fields* names, uint32_t category)
{
    uint32_t own = currencyNameZero + category;
    if ((names->found & 1u << own) != 0)
    {
        return own;
    }
    if ((names->found & 1u << currencyNameOther) != 0)
    {
        return currencyNameOther;
    }
    return (names->found & 1u << currencyName) != 0 ? currencyName : currencyNameFieldCount;
}

// The pattern around a long name's number: the unit pattern of each
// category with its name.
static void LongNameOuter(Outer* o, const CurrencyTexts* texts, const Fields* names)
{
    o->patternText = texts->text;
    for (uint32_t c = 0; c < 6; c++)
    {
        o->patternStart[c] = texts->start[c];
        o->patternLength[c] = texts->length[c];
        uint32_t field = NameField(names, c);
        o->text[c] = field == currencyNameFieldCount ? nullptr : names->text[field];
        o->textLength[c] = field == currencyNameFieldCount ? 0 : names->length[field];
    }
}

// The pattern around the number for the ISO append: the same, with the
// code, for every category.
static void AppendOuter(Outer* o, const CurrencyTexts* texts)
{
    o->patternText = texts->text;
    for (uint32_t c = 0; c < 6; c++)
    {
        o->patternStart[c] = texts->start[6];
        o->patternLength[c] = texts->length[6];
        o->text[c] = nullptr;
        o->textLength[c] = 0;
    }
}

// Keeps a unit of a walk along the chain, with its step.
typedef struct Keep
{
    CurrencyChain* chain;
    bool names;
} Keep;

static bool KeepUnit(void* context, DataSpan unit, const DataView* view, int depth)
{
    (void)view;
    Keep* k = context;
    CurrencyChain* c = k->chain;
    uint8_t* count = k->names ? &c->nameCount : &c->symbolCount;
    if (*count == currencyChainUnits || depth > UINT8_MAX)
    {
        c->complete = false;
        return false;
    }
    (k->names ? c->names : c->symbols)[*count] = unit;
    (k->names ? c->nameSteps : c->symbolSteps)[*count] = (uint8_t)depth;
    (*count)++;
    return false;
}

// One walk of a kind along a locale's chain: each unit kept for parsing
// and, when fields are asked for, the currency's gathered (one walk
// rather than a walk for each).
typedef struct Walk
{
    Fields* fields;
    bool done;
    Keep keep;
} Walk;

static bool WalkUnit(void* context, DataSpan unit, const DataView* view, int depth)
{
    Walk* w = context;
    if (w->fields != nullptr && !w->done)
    {
        w->done = Gather(w->fields, unit, view, depth);
    }
    (void)KeepUnit(&w->keep, unit, view, depth);
    return false;
}

// The currency units of a locale's chain, which parsing reads (the names
// when the set has the names data file), and a currency's symbol fields,
// and its name fields when names is set; false when the chain cannot be
// walked or a field names what its source lacks.
static bool WalkChain(const mlocDataSet* data, const mlocLocale* locale, uint16_t code,
                      Fields* symbols, Fields* names, CurrencyChain* out)
{
    *out = (CurrencyChain){.complete = true};
    out->turkic = strcmp(locale->language, "tr") == 0 || strcmp(locale->language, "az") == 0;
    *symbols = (Fields){.kind = kindCurrencySymbols, .code = code};
    Walk walk = {.fields = symbols, .keep = {out, false}};
    if (!mlocVisitLocaleUnits(data, mloc_componentMain, kindCurrencySymbols, locale, WalkUnit,
                              &walk) ||
        symbols->broken)
    {
        return false;
    }
    if (!mlocDataSetHasKind(data, kindCurrencyNames))
    {
        return true;
    }
    if (names != nullptr)
    {
        *names = (Fields){.kind = kindCurrencyNames, .code = code};
    }
    walk = (Walk){.fields = names, .keep = {out, true}};
    return mlocVisitLocaleUnits(data, mloc_componentMain, kindCurrencyNames, locale, WalkUnit,
                                &walk) &&
           (names == nullptr || !names->broken);
}

// Makes the core, and the compact tables and plural rule that compact
// notation and long names need.
static mlocResult InitParts(Currency* c, const mlocDataSet* data, const mlocLocale* locale,
                            const CurrencyStyle* style, bool compact, CurrencyTexts* texts)
{
    Formatter* f = &c->formatter;
    mlocResult result =
        mlocInitCurrencyCore(&f->core, data, locale, style, compact ? &f->compactTable : nullptr,
                             compact ? &f->compactPatterns : nullptr, texts);
    if (result >= 0 && (compact || style->name))
    {
        mlocResult rule = mlocFindPluralRule(data, locale, mloc_pluralCardinal, &f->pluralRule);
        result = rule < 0 ? rule : result;
    }
    return result;
}

mlocResult mlocInitCurrencyFormatter(mlocCurrencyFormatter* formatterOut, const mlocDataSet* data,
                                     const mlocLocale* locale, const char* code, size_t codeLength,
                                     const mlocCurrencyOptions* options)
{
    static const mlocCurrencyOptions s_defaults = {0};
    const mlocCurrencyOptions* o = options == nullptr ? &s_defaults : options;
    uint16_t packed = code == nullptr || codeLength != 3 ? 0 : mlocPackCurrency(code, 3);
    if (formatterOut == nullptr || data == nullptr || locale == nullptr || packed == 0 ||
        !ValidOptions(o))
    {
        return mloc_errorInvalid;
    }
    bool name = o->display == mloc_currencyName;
    Fields fields;
    Fields names;
    Currency c;
    memset(&c, 0, sizeof(c));
    // A long name needs the names data file in the set.
    if ((name && !mlocDataSetHasKind(data, kindCurrencyNames)) ||
        !WalkChain(data, locale, packed, &fields, name ? &names : nullptr, &c.chain))
    {
        return mloc_errorNotFound;
    }
    bool compact = o->number.notation == mloc_notationCompactShort ||
                   o->number.notation == mloc_notationCompactLong;
    // A code shows as three letters.
    uint32_t shown = ShownField(&fields, o->display);
    CurrencyStyle style = {
        .accounting = Accounting(locale, o->sign),
        .hidden = o->display == mloc_currencyHidden,
        .name = name,
        .longLength = o->number.notation == mloc_notationCompactLong,
        .spacing = shown != currencyFieldCount ? fields.spacing[shown]
                                               : currencyStartsAlpha | currencyEndsAlpha,
    };
    // A currency's own pattern replaces the standard one alone.
    bool ownPattern = (fields.found & 1u << currencyOwnPattern) != 0 &&
                      IsSymbolDisplay(o->display) && !style.accounting;
    CurrencyTexts texts;
    mlocResult result = InitParts(&c, data, locale, &style, compact, &texts);
    if (result < 0)
    {
        return result;
    }
    mlocSetFormatOptions(&c.formatter, &o->number, currencyStamp);
    // Compact notation rounds by its own rule unless the options say
    // otherwise, as ICU does.
    if (!ShowCurrency(&c.formatter.core, &fields, o->display, ownPattern) ||
        (o->number.precision.kind == mloc_precisionNone && !compact &&
         !CurrencyPrecision(data, packed, o->usage, &c.formatter.precision)))
    {
        return mloc_errorNotFound;
    }
    c.hasOuter = name || o->appendCode;
    c.appended = !name && o->appendCode;
    if (name)
    {
        LongNameOuter(&c.outer, &texts, &names);
        if (mlocInitPluralRules(&c.outer.rules, data, locale, mloc_pluralCardinal) < 0)
        {
            return mloc_errorNotFound;
        }
    }
    else if (o->appendCode)
    {
        AppendOuter(&c.outer, &texts);
    }
    c.outer.textKind = mloc_numberPartCurrency;
    c.outer.patternKind = mloc_numberPartLiteral;
    memcpy(c.outer.code, c.formatter.core.currencyCode, sizeof(c.outer.code));
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &c, sizeof(c));
    return result;
}

// The currency formatter of an opaque value, or false when the library
// did not make it.
static bool ReadCurrency(const mlocCurrencyFormatter* opaque, Currency* c)
{
    if (!mlocReadFormatter(opaque, sizeof(*opaque), currencyStamp, &c->formatter))
    {
        return false;
    }
    memcpy(c, opaque, sizeof(*c));
    return (!c->hasOuter || (c->outer.patternText != nullptr &&
                             c->formatter.pluralRule < mlocPluralRuleSetCount)) &&
           c->chain.symbolCount <= currencyChainUnits && c->chain.nameCount <= currencyChainUnits;
}

// A currency's fields of a kind from the units a formatter kept.
static void GatherKept(const DataSpan* units, uint32_t count, uint32_t kind, uint16_t code,
                       Fields* out)
{
    *out = (Fields){.kind = kind, .code = code};
    for (uint32_t i = 0; i < count && !Gather(out, units[i], nullptr, 0); i++)
    {
    }
}

// A side of a category's outer pattern as an affix pattern (ICU builds
// its currency plural patterns so), into parseMaxAffixText bytes and one
// past them when it does not fit.
static uint8_t OuterSide(const Currency* c, uint32_t category, bool before, uint8_t* out)
{
    const Outer* o = &c->outer;
    const uint8_t* pattern = o->patternText + o->patternStart[category];
    size_t length = o->patternLength[category];
    const uint8_t* number = memchr(pattern, currencyTextNumber, length);
    size_t split = number == nullptr ? length : (size_t)(number - pattern);
    size_t end = before ? split : length;
    size_t n = 0;
    for (size_t at = before ? 0 : (split < length ? split + 1 : length);
         at < end && n <= parseMaxAffixText; at++)
    {
        if (pattern[at] != currencyTextOther)
        {
            out[n++] = pattern[at];
        }
        else if (!c->appended)
        {
            out[n++] = numberAffixCurrencyName;
        }
        else
        {
            for (size_t i = 0; i < sizeof(o->code) && n <= parseMaxAffixText; i++)
            {
                out[n++] = (uint8_t)o->code[i];
            }
        }
    }
    return (uint8_t)n;
}

// What parsing reads of a formatter's currency (ICU's CurrencySymbols):
// its symbol, else its code; the text shown when it is another; and with
// the names data the long name of each category as ICU's getPluralName
// gives it (its own, other's, the name, the code).
static void ParseOf(const Currency* c, bool any, ParseCurrency* out)
{
    memset(out, 0, sizeof(*out));
    const NumberCore* core = &c->formatter.core;
    memcpy(out->code, core->currencyCode, 3);
    uint16_t packed = mlocPackCurrency(out->code, 3);
    Fields f;
    GatherKept(c->chain.symbols, c->chain.symbolCount, kindCurrencySymbols, packed, &f);
    bool symbol = (f.found & 1u << currencySymbol) != 0;
    out->symbol = symbol ? f.text[currencySymbol] : (const uint8_t*)out->code;
    out->symbolLength = symbol ? f.length[currencySymbol] : 3;
    bool other = core->currencyText != nullptr &&
                 (core->currencyLength != out->symbolLength ||
                  memcmp(core->currencyText, out->symbol, out->symbolLength) != 0);
    out->shown = other ? core->currencyText : nullptr;
    out->shownLength = other ? core->currencyLength : 0;
    Fields names;
    GatherKept(c->chain.names, c->chain.nameCount, kindCurrencyNames, packed, &names);
    for (uint32_t category = 0; c->chain.nameCount > 0 && category < 6; category++)
    {
        uint32_t field = NameField(&names, category);
        bool code = field == currencyNameFieldCount;
        out->names[category] = code ? (const uint8_t*)out->code : names.text[field];
        out->nameLengths[category] = code ? 3 : names.length[field];
    }
    out->outer = c->hasOuter;
    out->appended = c->appended;
    // Other's pattern, then each other category's that differs.
    static const uint8_t s_order[6] = {mloc_pluralOther, mloc_pluralZero, mloc_pluralOne,
                                       mloc_pluralTwo,   mloc_pluralFew,  mloc_pluralMany};
    for (uint32_t i = 0; c->hasOuter && i < 6; i++)
    {
        uint32_t n = out->outerCount;
        out->beforeLength[n] = OuterSide(c, s_order[i], true, out->before[n]);
        out->afterLength[n] = OuterSide(c, s_order[i], false, out->after[n]);
        bool seen = false;
        for (uint32_t k = 0; k < n && !seen; k++)
        {
            seen = out->beforeLength[k] == out->beforeLength[n] &&
                   out->afterLength[k] == out->afterLength[n] &&
                   memcmp(out->before[k], out->before[n], out->beforeLength[n]) == 0 &&
                   memcmp(out->after[k], out->after[n], out->afterLength[n]) == 0;
        }
        out->outerCount = (uint8_t)(n + (seen ? 0 : 1));
    }
    out->chain = any ? &c->chain : nullptr;
}

mlocResult mlocParseCurrency(const mlocCurrencyFormatter* formatter, const char* text,
                             size_t length, mlocParseMode mode, uint8_t anyCurrency,
                             mlocDecimal* decimalOut, char* codeOut, size_t* endOut)
{
    Currency c;
    if (!ReadCurrency(formatter, &c) || (text == nullptr && length > 0) || decimalOut == nullptr ||
        codeOut == nullptr || endOut == nullptr || (uint32_t)mode > mloc_parseStrict ||
        anyCurrency > 1 || c.formatter.compactTable != nullptr)
    {
        return mloc_errorInvalid;
    }
    if (anyCurrency == 1 && !c.chain.complete)
    {
        return mloc_errorLimit;
    }
    ParseCurrency currency;
    ParseOf(&c, anyCurrency == 1, &currency);
    return mlocParseWith(&c.formatter, &currency, text, length, mode, decimalOut, codeOut, endOut);
}

mlocResult mlocFormatCurrency(const mlocCurrencyFormatter* formatter, const mlocDecimal* decimal,
                              mlocOutput* output)
{
    Currency c;
    if (!ReadCurrency(formatter, &c))
    {
        return mloc_errorInvalid;
    }
    return mlocFormatWith(&c.formatter, c.hasOuter ? &c.outer : nullptr, decimal, output);
}

mlocResult mlocFormatCurrencyRange(const mlocCurrencyFormatter* formatter, const mlocDecimal* start,
                                   const mlocDecimal* end, mlocOutput* output)
{
    Currency c;
    if (!ReadCurrency(formatter, &c))
    {
        return mloc_errorInvalid;
    }
    return mlocFormatRangeWith(&c.formatter, c.hasOuter ? &c.outer : nullptr, start, end, output);
}

// Writes text into an output: its length, and mloc_errorCapacity when it
// does not fit.
static mlocResult WriteText(const void* text, size_t length, mlocOutput* output)
{
    output->length = length;
    if (output->buffer == nullptr || length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    memcpy(output->buffer, text, length);
    return mloc_success;
}

// Checks a lookup's arguments; the packed code, or 0.
static uint16_t LookupCode(const mlocDataSet* data, const mlocLocale* locale, const char* code,
                           size_t codeLength, const mlocOutput* output)
{
    bool valid = data != nullptr && locale != nullptr && code != nullptr && codeLength == 3 &&
                 output != nullptr && (output->buffer != nullptr || output->capacity == 0);
    return valid ? mlocPackCurrency(code, 3) : 0;
}

// Writes a field, or the code with mloc_fellBack when the chain gives
// none.
static mlocResult WriteField(const Fields* f, uint32_t field, mlocOutput* output)
{
    if (field == currencyFieldCount)
    {
        char letters[3];
        mlocUnpackCurrency(f->code, letters);
        mlocResult r = WriteText(letters, sizeof(letters), output);
        return r == mloc_success ? mloc_fellBack : r;
    }
    return WriteText(f->text[field], f->length[field], output);
}

mlocResult mlocGetCurrencySymbol(const mlocDataSet* data, const mlocLocale* locale,
                                 const char* code, size_t codeLength, mlocCurrencyDisplay display,
                                 mlocOutput* output)
{
    uint16_t packed = LookupCode(data, locale, code, codeLength, output);
    if (packed == 0 || !(IsSymbolDisplay(display) || display == mloc_currencyCode))
    {
        return mloc_errorInvalid;
    }
    Fields fields;
    if (!GatherFields(data, locale, kindCurrencySymbols, packed, &fields))
    {
        return mloc_errorNotFound;
    }
    mlocResult r = WriteField(&fields, ShownField(&fields, display), output);
    // The code shown as asked is no fallback.
    return r == mloc_fellBack && display == mloc_currencyCode ? mloc_success : r;
}

mlocResult mlocGetCurrencyName(const mlocDataSet* data, const mlocLocale* locale, const char* code,
                               size_t codeLength, mlocOutput* output)
{
    uint16_t packed = LookupCode(data, locale, code, codeLength, output);
    if (packed == 0)
    {
        return mloc_errorInvalid;
    }
    Fields names;
    if (!mlocDataSetHasKind(data, kindCurrencyNames) ||
        !GatherFields(data, locale, kindCurrencyNames, packed, &names))
    {
        return mloc_errorNotFound;
    }
    uint32_t field = (names.found & 1u << currencyName) != 0 ? currencyName : currencyFieldCount;
    return WriteField(&names, field, output);
}

mlocResult mlocGetCurrencyDigits(const mlocDataSet* data, const char* code, size_t codeLength,
                                 mlocCurrencyUsage usage, uint8_t* digitsOut,
                                 uint16_t* incrementOut)
{
    uint16_t packed = code == nullptr || codeLength != 3 ? 0 : mlocPackCurrency(code, 3);
    if (data == nullptr || packed == 0 || (uint32_t)usage > mloc_currencyUsageCash ||
        digitsOut == nullptr || incrementOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    CurrencyFraction f;
    if (!FindFraction(data, packed, &f))
    {
        return mloc_errorNotFound;
    }
    bool cash = usage == mloc_currencyUsageCash;
    *digitsOut = cash ? f.cashDigits : f.digits;
    uint8_t rounding = cash ? f.cashRounding : f.rounding;
    *incrementOut = rounding > 1 ? rounding : 1;
    return mloc_success;
}
