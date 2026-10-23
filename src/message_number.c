// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number functions of messages.

#include "message_number.h"

#include "number_precision.h"

#include "maul-locale/currency.h"
#include "maul-locale/plural.h"

#include <stddef.h>
#include <string.h>

// Keeps a formatter made in a call (a unit formatter is 3.5 KB) in its
// own frame, not in every write's.
#if defined(_MSC_VER) && !defined(__clang__)
#define MLOC_NOINLINE __declspec(noinline)
#else
#define MLOC_NOINLINE __attribute__((noinline))
#endif

enum
{
    // The number part's limits.
    integerDigitsLimit = 21,
    significantLimit = 21,
};

static bool IsDigit(uint8_t c)
{
    return c >= '0' && c <= '9';
}

// The end of a run of digits from at.
static size_t SkipDigits(const uint8_t* text, size_t at, size_t length)
{
    while (at < length && IsDigit(text[at]))
    {
        at++;
    }
    return at;
}

bool mlocNumberLiteral(const uint8_t* text, size_t length)
{
    size_t at = 0;
    at += at < length && text[at] == '-';
    if (at >= length || !IsDigit(text[at]) ||
        (text[at] == '0' && at + 1 < length && IsDigit(text[at + 1])))
    {
        return false;
    }
    at = SkipDigits(text, at, length);
    if (at < length && text[at] == '.')
    {
        size_t first = at + 1;
        at = SkipDigits(text, first, length);
        if (at == first)
        {
            return false;
        }
    }
    if (at < length && (text[at] == 'e' || text[at] == 'E'))
    {
        at++;
        at += at < length && (text[at] == '-' || text[at] == '+');
        size_t first = at;
        at = SkipDigits(text, first, length);
        if (at == first)
        {
            return false;
        }
    }
    return at == length;
}

NumberValue mlocMakeNumberValue(NumberFunction function, const mlocDecimal* decimal)
{
    NumberValue v;
    memset(&v, 0, sizeof v);
    v.decimal = *decimal;
    v.function = (uint8_t)function;
    // A currency, unit, spelled-out or relative value does not select.
    v.selectable = function <= numberFunctionOffset;
    v.select = numberSelectPlural;
    v.increment = 1;
    return v;
}

static uint32_t OptionsOf(NumberFunction function);

mlocMessageFaultKind mlocNumberFromValue(NumberFunction function, const NumberValue* operand,
                                         NumberValue* out)
{
    *out = *operand;
    // :offset keeps its operand's function and so its defaults.
    out->function = function == numberFunctionOffset ? operand->function : (uint8_t)function;
    out->selectable = out->function <= numberFunctionOffset;
    out->select = numberSelectPlural;
    out->given &= (uint32_t)~numberGivenSelect;
    if (out->function >= numberFunctionSpellout || operand->function >= numberFunctionSpellout)
    {
        // Into or out of a mloc: function, only the options the function
        // takes pass on.
        out->given &= OptionsOf((NumberFunction)out->function);
        return mloc_messageFaultNone;
    }
    if (out->function == numberFunctionUnit && function != numberFunctionOffset)
    {
        // :unit takes no select either, so drops one without a fault.
        return mloc_messageFaultNone;
    }
    if (out->function == numberFunctionCurrency && function != numberFunctionOffset)
    {
        // :currency takes no select, so drops one without a fault, and
        // its fraction digits are the currency's or its own option's.
        out->given &= (uint32_t)~(numberGivenMinFraction | numberGivenMaxFraction);
        return mloc_messageFaultNone;
    }
    if (function == numberFunctionInteger)
    {
        // :integer keeps none of the operand's fraction digits or its
        // minimum significant digits.
        out->given &= (uint32_t)~(numberGivenMinFraction | numberGivenMaxFraction |
                                  numberGivenMinSignificant);
    }
    if (function == numberFunctionPercent)
    {
        // :percent drops these, select without a fault: it always
        // selects by plural rules.
        out->given &= (uint32_t)~(numberGivenMinInteger | numberGivenIncrement);
        out->increment = 1;
        return mloc_messageFaultNone;
    }
    if ((operand->given & numberGivenSelect) != 0)
    {
        out->selectable = false;
        return mloc_messageFaultBadOption;
    }
    return mloc_messageFaultNone;
}

void mlocUpperCode(const char* code, char* out)
{
    for (int i = 0; i < 3; i++)
    {
        out[i] = code[i] >= 'a' && code[i] <= 'z' ? (char)(code[i] - 'a' + 'A') : code[i];
    }
}

void mlocSetCurrencyCode(NumberValue* value, const char* code)
{
    mlocUpperCode(code, value->code);
    value->given |= numberGivenCurrency;
}

bool mlocUnitText(const char* text, size_t length)
{
    bool start = true;
    for (size_t i = 0; i < length; i++)
    {
        char c = text[i];
        if (c == '-' && !start)
        {
            start = true;
        }
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
        {
            start = false;
        }
        else
        {
            return false;
        }
    }
    return length > 0 && !start;
}

void mlocSetUnit(NumberValue* value, const char* unit, size_t length)
{
    value->unit = unit;
    value->unitLength = (uint32_t)length;
    value->given |= numberGivenUnit;
}

bool mlocFindCurrencyDigits(NumberValue* value, const mlocDataSet* data)
{
    uint8_t digits = 0;
    uint16_t increment = 0;
    if (mlocGetCurrencyDigits(data, value->code, 3, mloc_currencyUsageStandard, &digits,
                              &increment) != mloc_success)
    {
        return false;
    }
    value->currencyDigits = digits;
    return true;
}

bool mlocMakeInteger(NumberValue* value)
{
    return value->decimal.kind != mloc_decimalFinite ||
           mlocRoundDecimal(&value->decimal, 0, 1, mloc_roundHalfExpand) == mloc_success;
}

// The index of text among names, or -1.
static int Choice(const NumberOption* option, const char* const* names, int count)
{
    if (!option->hasText)
    {
        return -1;
    }
    for (int i = 0; i < count; i++)
    {
        size_t length = strlen(names[i]);
        if (option->length == length && memcmp(option->text, names[i], length) == 0)
        {
            return i;
        }
    }
    return -1;
}

// A digit size option's value, 0 to 99, or -1: "0" or one or two digits
// not starting with 0, or a whole number of that range.
int mlocDigitSize(const NumberOption* option)
{
    if (option->hasText)
    {
        const uint8_t* t = option->text;
        if (option->length == 1 && IsDigit(t[0]))
        {
            return t[0] - '0';
        }
        if (option->length == 2 && t[0] >= '1' && t[0] <= '9' && IsDigit(t[1]))
        {
            return (t[0] - '0') * 10 + (t[1] - '0');
        }
        return -1;
    }
    if (option->hasInteger && option->integer >= 0 && option->integer <= 99)
    {
        return (int)option->integer;
    }
    return -1;
}

// A rounding increment, one of ECMA-402's, or 0.
static uint16_t Increment(const NumberOption* option)
{
    static const uint16_t increments[] = {1,   2,   5,   10,   20,   25,   50,  100,
                                          200, 250, 500, 1000, 2000, 2500, 5000};
    int64_t wanted = -1;
    if (option->hasInteger)
    {
        wanted = option->integer;
    }
    else if (option->hasText && option->length > 0 && option->length <= 4 && option->text[0] != '0')
    {
        wanted = 0;
        for (size_t i = 0; i < option->length && wanted >= 0; i++)
        {
            wanted = IsDigit(option->text[i]) ? wanted * 10 + (option->text[i] - '0') : -1;
        }
    }
    for (size_t i = 0; i < sizeof increments / sizeof increments[0]; i++)
    {
        if (increments[i] == wanted)
        {
            return increments[i];
        }
    }
    return 0;
}

static bool NameIs(const uint8_t* name, size_t length, const char* text)
{
    return length == strlen(text) && memcmp(name, text, length) == 0;
}

// Sets a digit size field and its bit; false for a bad value.
static bool SetDigits(NumberValue* v, uint8_t* field, uint32_t bit, const NumberOption* option)
{
    int digits = mlocDigitSize(option);
    if (digits < 0)
    {
        return false;
    }
    *field = (uint8_t)digits;
    v->given |= bit;
    return true;
}

// Sets an enumerated field and its bit; false for a bad value.
static bool SetChoice(NumberValue* v, uint8_t* field, uint32_t bit, const NumberOption* option,
                      const char* const* names, int count)
{
    int index = Choice(option, names, count);
    if (index < 0)
    {
        return false;
    }
    *field = (uint8_t)index;
    v->given |= bit;
    return true;
}

static const char* const s_selects[] = {"plural", "ordinal", "exact"};
// In mlocSignDisplay's order.
static const char* const s_signs[] = {"auto", "always", "never", "exceptZero", "negative"};
// In mlocGrouping's order.
static const char* const s_groupings[] = {"auto", "always", "min2", "never"};
static const char* const s_trailingZeros[] = {"auto", "stripIfInteger"};
static const char* const s_priorities[] = {"auto", "morePrecision", "lessPrecision"};
// UTS #35's names, in s_displayKinds' order ("never" is ICU's hidden).
static const char* const s_displays[] = {"symbol", "narrowSymbol", "code", "never", "name"};
static const mlocCurrencyDisplay s_displayKinds[] = {mloc_currencySymbol, mloc_currencyNarrowSymbol,
                                                     mloc_currencyCode, mloc_currencyHidden,
                                                     mloc_currencyName};
// In mlocCurrencySign's order after its locale choice.
static const char* const s_currencySigns[] = {"standard", "accounting"};
// In mlocUnitWidth's order.
static const char* const s_widths[] = {"short", "narrow", "long"};
// In mlocRelativeWidth's, mlocRelativeNumeric's and mlocRelativeUnit's
// orders.
static const char* const s_styles[] = {"long", "short", "narrow"};
static const char* const s_numerics[] = {"always", "auto"};
static const char* const s_relativeUnits[] = {
    "year",   "quarter", "month",   "week",      "day",      "hour",   "minute",  "second",
    "sunday", "monday",  "tuesday", "wednesday", "thursday", "friday", "saturday"};
// In mlocRoundingMode's order.
static const char* const s_modes[] = {"halfExpand", "halfEven",  "halfTrunc",
                                      "halfCeil",   "halfFloor", "ceil",
                                      "floor",      "expand",    "trunc"};

// The options each function takes, by name and the bit they set;
// another name is ignored.
static const struct
{
    const char* name;
    uint32_t bit;
} s_options[] = {
    {"select", numberGivenSelect},
    {"signDisplay", numberGivenSign},
    {"useGrouping", numberGivenGrouping},
    {"minimumIntegerDigits", numberGivenMinInteger},
    {"minimumFractionDigits", numberGivenMinFraction},
    {"maximumFractionDigits", numberGivenMaxFraction},
    {"minimumSignificantDigits", numberGivenMinSignificant},
    {"maximumSignificantDigits", numberGivenMaxSignificant},
    {"trailingZeroDisplay", numberGivenTrailingZeros},
    {"roundingPriority", numberGivenPriority},
    {"roundingIncrement", numberGivenIncrement},
    {"roundingMode", numberGivenMode},
    {"currency", numberGivenCurrency},
    {"currencyDisplay", numberGivenDisplay},
    {"currencySign", numberGivenCurrencySign},
    {"fractionDigits", numberGivenFractionDigits},
    {"unit", numberGivenUnit},
    {"usage", numberGivenUsage},
    {"unitDisplay", numberGivenWidth},
    {"rules", numberGivenRules},
    {"numberingSystem", numberGivenSystem},
    {"unit", numberGivenRelativeUnit},
    {"style", numberGivenStyle},
    {"numeric", numberGivenNumeric},
};

// The options a function takes (UTS #35 Part 9's lists), as given bits.
static uint32_t OptionsOf(NumberFunction function)
{
    switch (function)
    {
    case numberFunctionInteger:
        return numberGivenSelect | numberGivenSign | numberGivenGrouping | numberGivenMinInteger |
               numberGivenMaxSignificant;
    case numberFunctionPercent:
        return numberGivenSign | numberGivenGrouping | numberGivenMinFraction |
               numberGivenMaxFraction | numberGivenMinSignificant | numberGivenMaxSignificant |
               numberGivenTrailingZeros | numberGivenPriority | numberGivenMode;
    case numberFunctionNumber:
        return 0xFFF;
    case numberFunctionUnit:
        return numberGivenUnit | numberGivenUsage | numberGivenWidth | numberGivenSign |
               numberGivenGrouping | numberGivenMinInteger | numberGivenMinFraction |
               numberGivenMaxFraction | numberGivenMinSignificant | numberGivenMaxSignificant |
               numberGivenPriority | numberGivenIncrement | numberGivenMode;
    case numberFunctionCurrency:
        return numberGivenCurrency | numberGivenDisplay | numberGivenCurrencySign |
               numberGivenGrouping | numberGivenMinInteger | numberGivenFractionDigits |
               numberGivenMinSignificant | numberGivenMaxSignificant | numberGivenTrailingZeros |
               numberGivenPriority | numberGivenIncrement | numberGivenMode;
    case numberFunctionSpellout:
        return numberGivenRules | numberGivenSystem;
    case numberFunctionRelative:
        // The relative formatter's number options: no sign, the sign
        // being the direction.
        return numberGivenRelativeUnit | numberGivenStyle | numberGivenNumeric |
               numberGivenGrouping | numberGivenMinInteger | numberGivenMinFraction |
               numberGivenMaxFraction | numberGivenMinSignificant | numberGivenMaxSignificant |
               numberGivenTrailingZeros | numberGivenPriority | numberGivenMode;
    default:
        return 0;
    }
}

// Sets a currency code from three ASCII letters in any case; false for
// another value.
static bool SetCode(NumberValue* v, const NumberOption* option)
{
    if (!option->hasText || option->length != 3)
    {
        return false;
    }
    for (int i = 0; i < 3; i++)
    {
        uint8_t c = option->text[i] | 0x20;
        if (c < 'a' || c > 'z')
        {
            return false;
        }
    }
    mlocSetCurrencyCode(v, (const char*)option->text);
    return true;
}

// Sets a unit or a usage from text that is lexically one; false for
// another value.
static bool SetUnitText(NumberValue* v, uint32_t bit, const NumberOption* option)
{
    const char* text = (const char*)option->text;
    if (!option->hasText || !mlocUnitText(text, option->length) || option->length > UINT32_MAX)
    {
        return false;
    }
    if (bit == numberGivenUnit)
    {
        mlocSetUnit(v, text, option->length);
        return true;
    }
    v->usage = text;
    v->usageLength = (uint32_t)option->length;
    v->given |= bit;
    return true;
}

// Sets a mloc:spellout value's rule set or numbering system from text
// that is lexically one (as a unit identifier is), the other dropped;
// false for another value.
static bool SetRulesText(NumberValue* v, uint32_t bit, const NumberOption* option)
{
    const char* text = (const char*)option->text;
    if (!option->hasText || !mlocUnitText(text, option->length) || option->length > UINT32_MAX)
    {
        return false;
    }
    v->unit = text;
    v->unitLength = (uint32_t)option->length;
    v->given = (v->given & (uint32_t)~(numberGivenRules | numberGivenSystem)) | bit;
    return true;
}

// Sets one option by its bit; whether its value was good.
static bool SetByBit(NumberValue* v, uint32_t bit, const NumberOption* option)
{
    switch (bit)
    {
    case numberGivenSelect:
        return SetChoice(v, &v->select, bit, option, s_selects, 3);
    case numberGivenSign:
        return SetChoice(v, &v->sign, bit, option, s_signs, 5);
    case numberGivenGrouping:
        return SetChoice(v, &v->grouping, bit, option, s_groupings, 4);
    case numberGivenMinInteger:
        return SetDigits(v, &v->minInteger, bit, option);
    case numberGivenMinFraction:
        return SetDigits(v, &v->minFraction, bit, option);
    case numberGivenMaxFraction:
        return SetDigits(v, &v->maxFraction, bit, option);
    case numberGivenMinSignificant:
        return SetDigits(v, &v->minSignificant, bit, option);
    case numberGivenMaxSignificant:
        return SetDigits(v, &v->maxSignificant, bit, option);
    case numberGivenTrailingZeros:
        return SetChoice(v, &v->trailingZeros, bit, option, s_trailingZeros, 2);
    case numberGivenPriority:
        return SetChoice(v, &v->priority, bit, option, s_priorities, 3);
    case numberGivenMode:
        return SetChoice(v, &v->mode, bit, option, s_modes, 9);
    case numberGivenCurrency:
        return SetCode(v, option);
    case numberGivenUnit:
    case numberGivenUsage:
        return SetUnitText(v, bit, option);
    case numberGivenWidth:
        return SetChoice(v, &v->width, bit, option, s_widths, 3);
    case numberGivenRules:
    case numberGivenSystem:
        return SetRulesText(v, bit, option);
    case numberGivenRelativeUnit:
        return SetChoice(v, &v->relativeUnit, bit, option, s_relativeUnits, 15);
    case numberGivenStyle:
        return SetChoice(v, &v->width, bit, option, s_styles, 3);
    case numberGivenNumeric:
        return SetChoice(v, &v->numeric, bit, option, s_numerics, 2);
    case numberGivenDisplay:
        return SetChoice(v, &v->display, bit, option, s_displays, 5);
    case numberGivenCurrencySign:
        return SetChoice(v, &v->currencySign, bit, option, s_currencySigns, 2);
    case numberGivenFractionDigits:
        if (option->hasText && NameIs(option->text, option->length, "auto"))
        {
            // The currency's digits, over the operand's option.
            v->given &= (uint32_t)~bit;
            return true;
        }
        return SetDigits(v, &v->fractionDigits, bit, option);
    default:
    {
        uint16_t increment = Increment(option);
        if (increment == 0)
        {
            return false;
        }
        v->increment = increment;
        v->given |= bit;
        return true;
    }
    }
}

mlocMessageFaultKind mlocSetNumberOption(NumberValue* value, NumberFunction function,
                                         const uint8_t* name, size_t length,
                                         const NumberOption* option)
{
    uint32_t taken = OptionsOf(function);
    for (size_t i = 0; i < sizeof s_options / sizeof s_options[0]; i++)
    {
        uint32_t bit = s_options[i].bit;
        if ((taken & bit) == 0 || !NameIs(name, length, s_options[i].name))
        {
            continue;
        }
        if ((bit == numberGivenCurrency || bit == numberGivenUnit) && (value->given & bit) != 0)
        {
            // The operand's currency or unit stands (UTS #35: the option
            // must not override it).
            return mloc_messageFaultBadOption;
        }
        if ((bit & numberGivenWords) != 0 && !option->literal)
        {
            // They choose the formatter, made once at bind.
            return mloc_messageFaultBadOption;
        }
        if (bit == numberGivenSelect && !option->literal)
        {
            // Keys depend on it, so a translator must see it.
            value->selectable = false;
            return mloc_messageFaultBadOption;
        }
        return SetByBit(value, bit, option) ? mloc_messageFaultNone : mloc_messageFaultBadOption;
    }
    return mloc_messageFaultNone;
}

// Sets a precision's fraction digits as ECMA-402 does from the
// function's defaults: a missing minimum or maximum follows the other.
static void FractionDigits(const NumberValue* v, uint8_t minDefault, uint8_t maxDefault,
                           mlocPrecision* p)
{
    bool hasMin = (v->given & numberGivenMinFraction) != 0;
    bool hasMax = (v->given & numberGivenMaxFraction) != 0;
    p->minFractionDigits = hasMin ? v->minFraction : minDefault;
    p->maxFractionDigits = hasMax ? v->maxFraction : maxDefault;
    if (!hasMin && hasMax && p->maxFractionDigits < minDefault)
    {
        p->minFractionDigits = p->maxFractionDigits;
    }
    if (hasMin && !hasMax && p->minFractionDigits > maxDefault)
    {
        p->maxFractionDigits = p->minFractionDigits;
    }
}

// A value's precision by ECMA-402's SetNumberFormatDigitOptions, from
// its function's default fraction digits.
static mlocPrecision PrecisionOf(const NumberValue* v)
{
    uint8_t minDefault = 0;
    // ECMA-402's defaults: three for decimals, none for integers and
    // percents; a currency's own digits, or its fractionDigits option.
    uint8_t maxDefault = v->function == numberFunctionNumber || v->function == numberFunctionUnit ||
                                 v->function == numberFunctionRelative
                             ? 3
                             : 0;
    if (v->function == numberFunctionCurrency)
    {
        minDefault =
            (v->given & numberGivenFractionDigits) != 0 ? v->fractionDigits : v->currencyDigits;
        maxDefault = minDefault;
    }
    bool increment = (v->given & numberGivenIncrement) != 0 && v->increment != 1;
    if (increment)
    {
        maxDefault = minDefault;
    }
    bool hasSignificant = (v->given & (numberGivenMinSignificant | numberGivenMaxSignificant)) != 0;
    uint8_t priority = (v->given & numberGivenPriority) != 0 ? v->priority : 0;
    static const mlocPrecisionKind priorities[] = {
        mloc_precisionFraction, mloc_precisionMorePrecision, mloc_precisionLessPrecision};
    mlocPrecision p;
    memset(&p, 0, sizeof p);
    p.kind = priority == 0 && hasSignificant ? mloc_precisionSignificant : priorities[priority];
    p.mode = (v->given & numberGivenMode) != 0 ? (mlocRoundingMode)v->mode : mloc_roundHalfExpand;
    p.trailingZeros = (v->given & numberGivenTrailingZeros) != 0
                          ? (mlocTrailingZeros)v->trailingZeros
                          : mloc_trailingZerosAuto;
    if (p.kind != mloc_precisionFraction)
    {
        p.minSignificantDigits =
            (v->given & numberGivenMinSignificant) != 0 ? v->minSignificant : 1;
        p.maxSignificantDigits =
            (v->given & numberGivenMaxSignificant) != 0 ? v->maxSignificant : significantLimit;
    }
    if (p.kind != mloc_precisionSignificant)
    {
        FractionDigits(v, minDefault, maxDefault, &p);
    }
    if (increment && p.kind == mloc_precisionFraction && p.minFractionDigits == p.maxFractionDigits)
    {
        p.increment = v->increment;
    }
    return p;
}

uint32_t mlocCheckNumberOptions(NumberValue* v)
{
    uint32_t faults = 0;
    if ((v->given & numberGivenMinInteger) != 0 && v->minInteger > integerDigitsLimit)
    {
        v->given &= (uint32_t)~numberGivenMinInteger;
        faults++;
    }
    static const uint32_t significant[] = {numberGivenMinSignificant, numberGivenMaxSignificant};
    uint8_t* fields[] = {&v->minSignificant, &v->maxSignificant};
    for (int i = 0; i < 2; i++)
    {
        if ((v->given & significant[i]) != 0 && (*fields[i] == 0 || *fields[i] > significantLimit))
        {
            v->given &= (uint32_t)~significant[i];
            faults++;
        }
    }
    uint32_t both = numberGivenMinSignificant | numberGivenMaxSignificant;
    if ((v->given & both) == both && v->minSignificant > v->maxSignificant)
    {
        v->given &= (uint32_t)~numberGivenMaxSignificant;
        faults++;
    }
    both = numberGivenMinFraction | numberGivenMaxFraction;
    if ((v->given & both) == both && v->minFraction > v->maxFraction)
    {
        v->given &= (uint32_t)~numberGivenMaxFraction;
        faults++;
    }
    if ((v->given & numberGivenIncrement) != 0 && v->increment != 1)
    {
        mlocPrecision p = PrecisionOf(v);
        if (p.increment == 0)
        {
            // ECMA-402 takes an increment with fraction digits alone, the
            // minimum and maximum equal.
            v->given &= (uint32_t)~numberGivenIncrement;
            v->increment = 1;
            faults++;
        }
    }
    return faults;
}

mlocNumberOptions mlocNumberFormatOptions(const NumberValue* v)
{
    mlocNumberOptions o;
    memset(&o, 0, sizeof o);
    o.grouping =
        (v->given & numberGivenGrouping) != 0 ? (mlocGrouping)v->grouping : mloc_groupingAuto;
    o.signDisplay = (v->given & numberGivenSign) != 0 ? (mlocSignDisplay)v->sign : mloc_signAuto;
    o.minIntegerDigits = (v->given & numberGivenMinInteger) != 0 ? v->minInteger : 0;
    o.precision = PrecisionOf(v);
    o.style = v->function == numberFunctionPercent ? mloc_numberPercent : mloc_numberDecimal;
    uint32_t digits = numberGivenFractionDigits | numberGivenMinFraction | numberGivenMaxFraction |
                      numberGivenMinSignificant | numberGivenMaxSignificant | numberGivenPriority |
                      numberGivenIncrement;
    bool own = v->function == numberFunctionCurrency ||
               (v->function == numberFunctionUnit && (v->given & numberGivenUsage) != 0);
    if (own && (v->given & digits) == 0)
    {
        // The formatter's own: a currency's digits and rounding increment,
        // or a unit preference's rounding; the mode and trailing zeros as
        // given.
        mlocPrecision zero;
        memset(&zero, 0, sizeof zero);
        zero.mode = o.precision.mode;
        zero.trailingZeros = o.precision.trailingZeros;
        o.precision = zero;
    }
    return o;
}

// Writes a currency value with the currency formatter.
static_assert(sizeof(NumberValue) == offsetof(NumberValue, reserved) + 3,
              "a number value has no padding");
static_assert(offsetof(PreparedNumber, formatter) % 8 == 0 && sizeof(PreparedNumber) % 8 == 0,
              "a prepared value's parts stay 8-aligned");

NumberValue mlocNumberKey(const NumberValue* value)
{
    NumberValue key = *value;
    memset(&key.decimal, 0, sizeof key.decimal);
    key.origin = 0;
    key.selectable = false;
    return key;
}

bool mlocNumberFits(const PreparedNumber* prepared, const NumberValue* value)
{
    if (prepared == NULL)
    {
        return false;
    }
    NumberValue key = mlocNumberKey(value);
    return memcmp(&key, &prepared->key, sizeof key) == 0;
}

// A currency formatter's options for a currency value.
static mlocCurrencyOptions CurrencyOptions(const NumberValue* value)
{
    mlocCurrencyOptions options;
    memset(&options, 0, sizeof options);
    options.number = mlocNumberFormatOptions(value);
    options.display = (value->given & numberGivenDisplay) != 0 ? s_displayKinds[value->display]
                                                               : mloc_currencySymbol;
    options.sign = (value->given & numberGivenCurrencySign) != 0
                       ? (mlocCurrencySign)(value->currencySign + 1)
                       : mloc_currencySignLocale;
    return options;
}

// A unit formatter's options for a unit value.
static mlocUnitOptions UnitOptions(const NumberValue* value)
{
    mlocUnitOptions options;
    memset(&options, 0, sizeof options);
    options.number = mlocNumberFormatOptions(value);
    options.width =
        (value->given & numberGivenWidth) != 0 ? (mlocUnitWidth)value->width : mloc_unitWidthShort;
    if ((value->given & numberGivenUsage) != 0)
    {
        options.usage = value->usage;
        options.usageLength = value->usageLength;
    }
    return options;
}

// Writes a unit value with a unit formatter made here: 3.5 KB, off the
// resolver's frames.
MLOC_NOINLINE static mlocResult WriteUnit(const NumberValue* value, const mlocDataSet* data,
                                          const mlocLocale* locale, mlocOutput* output)
{
    mlocUnitOptions options = UnitOptions(value);
    mlocUnitFormatter formatter;
    mlocResult made =
        mlocInitUnitFormatter(&formatter, data, locale, value->unit, value->unitLength, &options);
    return made < 0 ? made : mlocFormatUnit(&formatter, &value->decimal, output);
}

MLOC_NOINLINE static mlocResult WriteCurrency(const NumberValue* value, const mlocDataSet* data,
                                              const mlocLocale* locale, mlocOutput* output)
{
    mlocCurrencyOptions options = CurrencyOptions(value);
    mlocCurrencyFormatter formatter;
    mlocResult made = mlocInitCurrencyFormatter(&formatter, data, locale, value->code, 3, &options);
    return made < 0 ? made : mlocFormatCurrency(&formatter, &value->decimal, output);
}

size_t mlocPreparedSize(PreparedKind kind)
{
    static const size_t sizes[] = {sizeof(mlocNumberFormatter), sizeof(mlocCurrencyFormatter),
                                   sizeof(mlocUnitFormatter), sizeof(mlocSpelloutFormatter),
                                   sizeof(mlocRelativeFormatter)};
    return offsetof(PreparedNumber, formatter) + sizes[kind];
}

bool mlocPrepareNumber(const NumberValue* value, const mlocDataSet* data, const mlocLocale* locale,
                       PreparedNumber* out)
{
    memset(out, 0, offsetof(PreparedNumber, formatter));
    out->key = mlocNumberKey(value);
    out->kind = value->function == numberFunctionCurrency ? preparedCurrency
                : value->function == numberFunctionUnit   ? preparedUnit
                                                          : preparedNumber;
    if (value->selectable && value->select != numberSelectExact)
    {
        mlocPluralType type =
            value->select == numberSelectOrdinal ? mloc_pluralOrdinal : mloc_pluralCardinal;
        out->rulesType =
            mlocInitPluralRules(&out->rules, data, locale, type) < 0 ? 0 : (uint8_t)(1 + type);
    }
    if (out->kind == preparedUnit)
    {
        mlocUnitOptions options = UnitOptions(value);
        return mlocInitUnitFormatter(&out->formatter.unit, data, locale, value->unit,
                                     value->unitLength, &options) >= 0;
    }
    if (out->kind == preparedCurrency)
    {
        mlocCurrencyOptions options = CurrencyOptions(value);
        return mlocInitCurrencyFormatter(&out->formatter.currency, data, locale, value->code, 3,
                                         &options) >= 0;
    }
    mlocNumberOptions options = mlocNumberFormatOptions(value);
    return mlocInitNumberFormatter(&out->formatter.number, data, locale, &options) >= 0;
}

mlocMessageFaultKind mlocWriteNumberValue(const NumberValue* value, const PreparedNumber* prepared,
                                          const mlocDataSet* data, const mlocLocale* locale,
                                          mlocOutput* output)
{
    mlocResult result;
    if (mlocNumberFits(prepared, value))
    {
        result = prepared->kind == preparedUnit
                     ? mlocFormatUnit(&prepared->formatter.unit, &value->decimal, output)
                 : prepared->kind == preparedCurrency
                     ? mlocFormatCurrency(&prepared->formatter.currency, &value->decimal, output)
                     : mlocFormatNumber(&prepared->formatter.number, &value->decimal, output);
    }
    else if (value->function == numberFunctionUnit)
    {
        result = WriteUnit(value, data, locale, output);
    }
    else if (value->function == numberFunctionCurrency)
    {
        result = WriteCurrency(value, data, locale, output);
    }
    else
    {
        mlocNumberOptions options = mlocNumberFormatOptions(value);
        mlocNumberFormatter formatter;
        result = mlocInitNumberFormatter(&formatter, data, locale, &options);
        if (result >= 0)
        {
            result = mlocFormatNumber(&formatter, &value->decimal, output);
        }
    }
    // A text past the room reports the length it needs.
    return result == mloc_success || (result == mloc_errorCapacity && output->length > 0)
               ? mloc_messageFaultNone
               : mloc_messageFaultUnsupportedOperation;
}

static bool Keyword(const uint8_t* key, size_t length, mlocPluralCategory* categoryOut)
{
    static const char* const keywords[] = {"zero", "one", "two", "few", "many", "other"};
    for (int i = 0; i < 6; i++)
    {
        if (NameIs(key, length, keywords[i]))
        {
            *categoryOut = (mlocPluralCategory)i;
            return true;
        }
    }
    return false;
}

// Whether the value rounded as it formats is written plainly as key:
// ASCII digits, "-", "." and the fraction zeros it shows, never an
// exponent; zero unsigned. The key, a number literal, is read as a
// decimal, which compares by its bytes (the same value showing the same
// digits); a key of more digits than a decimal holds matches nothing.
bool mlocExactlyKey(const mlocDecimal* rounded, const uint8_t* key, size_t length)
{
    if (rounded->kind != mloc_decimalFinite || memchr(key, 'e', length) != NULL ||
        memchr(key, 'E', length) != NULL)
    {
        return false;
    }
    mlocDecimal value = *rounded;
    value.negative = value.count == 0 ? 0 : value.negative;
    mlocDecimal written;
    return mlocParseDecimal((const char*)key, length, &written).status == mloc_success &&
           memcmp(&written, &value, sizeof value) == 0;
}

mlocMessageFaultKind mlocMatchNumber(const NumberValue* value, const PreparedNumber* prepared,
                                     const mlocDataSet* data, const mlocLocale* locale,
                                     const uint8_t* key, size_t length, bool* matchOut)
{
    *matchOut = false;
    mlocPluralCategory wanted = mloc_pluralOther;
    bool literal = mlocNumberLiteral(key, length);
    if (!literal && !Keyword(key, length, &wanted))
    {
        return mloc_messageFaultBadVariantKey;
    }
    mlocDecimal rounded = value->decimal;
    mlocPrecision precision = PrecisionOf(value);
    // A percent selects on its value times 100, as it formats.
    if (rounded.kind == mloc_decimalFinite &&
        ((value->function == numberFunctionPercent &&
          mlocScaleDecimal(&rounded, 2) != mloc_success) ||
         mlocApplyPrecision(&precision, &rounded) != mloc_success))
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    if (literal)
    {
        *matchOut = mlocExactlyKey(&rounded, key, length);
        return mloc_messageFaultNone;
    }
    if (value->select == numberSelectExact)
    {
        return mloc_messageFaultNone;
    }
    mlocPluralType type =
        value->select == numberSelectOrdinal ? mloc_pluralOrdinal : mloc_pluralCardinal;
    mlocPluralCategory category = mloc_pluralOther;
    mlocPluralRules made;
    const mlocPluralRules* rules = &made;
    if (mlocNumberFits(prepared, value) && prepared->rulesType == 1 + type)
    {
        rules = &prepared->rules;
    }
    else if (mlocInitPluralRules(&made, data, locale, type) < 0)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    if (mlocSelectPlural(rules, &rounded, &category) != mloc_success)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    *matchOut = category == wanted;
    return mloc_messageFaultNone;
}

bool mlocNumberKeyBetter(const uint8_t* key1, size_t length1, const uint8_t* key2, size_t length2)
{
    return mlocNumberLiteral(key1, length1) && !mlocNumberLiteral(key2, length2);
}
