// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number functions of messages (record mloc-0045).

#include "message_number.h"

#include "number_precision.h"

#include "maul-locale/plural.h"

#include <string.h>

enum
{
    // The number part's limits (record mloc-0021).
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
    v.selectable = true;
    v.select = numberSelectPlural;
    v.increment = 1;
    return v;
}

mlocMessageFaultKind mlocNumberFromValue(NumberFunction function, const NumberValue* operand,
                                         NumberValue* out)
{
    *out = *operand;
    out->function = (uint8_t)function;
    out->selectable = true;
    out->select = numberSelectPlural;
    out->given &= (uint16_t)~numberGivenSelect;
    if (function == numberFunctionInteger)
    {
        // :integer keeps none of the operand's fraction digits or its
        // minimum significant digits.
        out->given &= (uint16_t)~(numberGivenMinFraction | numberGivenMaxFraction |
                                  numberGivenMinSignificant);
    }
    if ((operand->given & numberGivenSelect) != 0)
    {
        out->selectable = false;
        return mloc_messageFaultBadOption;
    }
    return mloc_messageFaultNone;
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
static int DigitSize(const NumberOption* option)
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
static bool SetDigits(NumberValue* v, uint8_t* field, uint16_t bit, const NumberOption* option)
{
    int digits = DigitSize(option);
    if (digits < 0)
    {
        return false;
    }
    *field = (uint8_t)digits;
    v->given |= bit;
    return true;
}

// Sets an enumerated field and its bit; false for a bad value.
static bool SetChoice(NumberValue* v, uint8_t* field, uint16_t bit, const NumberOption* option,
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
// In mlocRoundingMode's order.
static const char* const s_modes[] = {"halfExpand", "halfEven",  "halfTrunc",
                                      "halfCeil",   "halfFloor", "ceil",
                                      "floor",      "expand",    "trunc"};

// Sets the options :number and :integer share; -1 for a name they do
// not, else whether the value was good.
static int SetSharedOption(NumberValue* v, const uint8_t* name, size_t length,
                           const NumberOption* option)
{
    if (NameIs(name, length, "signDisplay"))
    {
        return SetChoice(v, &v->sign, numberGivenSign, option, s_signs, 5);
    }
    if (NameIs(name, length, "useGrouping"))
    {
        return SetChoice(v, &v->grouping, numberGivenGrouping, option, s_groupings, 4);
    }
    if (NameIs(name, length, "minimumIntegerDigits"))
    {
        return SetDigits(v, &v->minInteger, numberGivenMinInteger, option);
    }
    if (NameIs(name, length, "maximumSignificantDigits"))
    {
        return SetDigits(v, &v->maxSignificant, numberGivenMaxSignificant, option);
    }
    return -1;
}

// Sets the options only :number has; -1 for a name it does not.
static int SetNumberOnlyOption(NumberValue* v, const uint8_t* name, size_t length,
                               const NumberOption* option)
{
    if (NameIs(name, length, "minimumFractionDigits"))
    {
        return SetDigits(v, &v->minFraction, numberGivenMinFraction, option);
    }
    if (NameIs(name, length, "maximumFractionDigits"))
    {
        return SetDigits(v, &v->maxFraction, numberGivenMaxFraction, option);
    }
    if (NameIs(name, length, "minimumSignificantDigits"))
    {
        return SetDigits(v, &v->minSignificant, numberGivenMinSignificant, option);
    }
    if (NameIs(name, length, "trailingZeroDisplay"))
    {
        return SetChoice(v, &v->trailingZeros, numberGivenTrailingZeros, option, s_trailingZeros,
                         2);
    }
    if (NameIs(name, length, "roundingPriority"))
    {
        return SetChoice(v, &v->priority, numberGivenPriority, option, s_priorities, 3);
    }
    if (NameIs(name, length, "roundingMode"))
    {
        return SetChoice(v, &v->mode, numberGivenMode, option, s_modes, 9);
    }
    if (NameIs(name, length, "roundingIncrement"))
    {
        uint16_t increment = Increment(option);
        if (increment == 0)
        {
            return false;
        }
        v->increment = increment;
        v->given |= numberGivenIncrement;
        return true;
    }
    return -1;
}

mlocMessageFaultKind mlocSetNumberOption(NumberValue* value, const uint8_t* name, size_t length,
                                         const NumberOption* option)
{
    int good = -1;
    if (NameIs(name, length, "select"))
    {
        if (!option->literal)
        {
            // Keys depend on it, so a translator must see it.
            value->selectable = false;
            return mloc_messageFaultBadOption;
        }
        good = SetChoice(value, &value->select, numberGivenSelect, option, s_selects, 3);
    }
    if (good < 0)
    {
        good = SetSharedOption(value, name, length, option);
    }
    if (good < 0 && value->function == numberFunctionNumber)
    {
        good = SetNumberOnlyOption(value, name, length, option);
    }
    return good == 0 ? mloc_messageFaultBadOption : mloc_messageFaultNone;
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
    uint8_t maxDefault = v->function == numberFunctionNumber ? 3 : 0;
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
        v->given &= (uint16_t)~numberGivenMinInteger;
        faults++;
    }
    static const uint16_t significant[] = {numberGivenMinSignificant, numberGivenMaxSignificant};
    uint8_t* fields[] = {&v->minSignificant, &v->maxSignificant};
    for (int i = 0; i < 2; i++)
    {
        if ((v->given & significant[i]) != 0 && (*fields[i] == 0 || *fields[i] > significantLimit))
        {
            v->given &= (uint16_t)~significant[i];
            faults++;
        }
    }
    uint16_t both = numberGivenMinSignificant | numberGivenMaxSignificant;
    if ((v->given & both) == both && v->minSignificant > v->maxSignificant)
    {
        v->given &= (uint16_t)~numberGivenMaxSignificant;
        faults++;
    }
    both = numberGivenMinFraction | numberGivenMaxFraction;
    if ((v->given & both) == both && v->minFraction > v->maxFraction)
    {
        v->given &= (uint16_t)~numberGivenMaxFraction;
        faults++;
    }
    if ((v->given & numberGivenIncrement) != 0 && v->increment != 1)
    {
        mlocPrecision p = PrecisionOf(v);
        if (p.increment == 0)
        {
            // ECMA-402 takes an increment with fraction digits alone, the
            // minimum and maximum equal.
            v->given &= (uint16_t)~numberGivenIncrement;
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
    return o;
}

mlocMessageFaultKind mlocWriteNumberValue(const NumberValue* value, const mlocDataSet* data,
                                          const mlocLocale* locale, mlocOutput* output)
{
    mlocNumberOptions options = mlocNumberFormatOptions(value);
    mlocNumberFormatter formatter;
    if (mlocInitNumberFormatter(&formatter, data, locale, &options) < 0)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    mlocResult result = mlocFormatNumber(&formatter, &value->decimal, output);
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
static bool ExactlyKey(const mlocDecimal* rounded, const uint8_t* key, size_t length)
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

mlocMessageFaultKind mlocMatchNumber(const NumberValue* value, const mlocDataSet* data,
                                     const mlocLocale* locale, const uint8_t* key, size_t length,
                                     bool* matchOut)
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
    if (rounded.kind == mloc_decimalFinite &&
        mlocApplyPrecision(&precision, &rounded) != mloc_success)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    if (literal)
    {
        *matchOut = ExactlyKey(&rounded, key, length);
        return mloc_messageFaultNone;
    }
    if (value->select == numberSelectExact)
    {
        return mloc_messageFaultNone;
    }
    mlocPluralRules rules;
    mlocPluralType type =
        value->select == numberSelectOrdinal ? mloc_pluralOrdinal : mloc_pluralCardinal;
    mlocPluralCategory category = mloc_pluralOther;
    if (mlocInitPluralRules(&rules, data, locale, type) < 0 ||
        mlocSelectPlural(&rules, &rounded, &category) != mloc_success)
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
