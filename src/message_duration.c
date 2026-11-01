// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_duration.h"

#include "invariant.h"

#include "maul-locale/ixdtf.h"
#include "maul-locale/zone.h"

#include <string.h>

// Keeps a formatter made in a call (4 KB) in its own frame, not in every
// write's.
#if defined(_MSC_VER) && !defined(__clang__)
#define MLOC_NOINLINE __declspec(noinline)
#else
#define MLOC_NOINLINE __attribute__((noinline))
#endif

static_assert(sizeof(DurationKey) == offsetof(DurationKey, reserved) + 2,
              "a duration key has no padding");
static_assert(offsetof(PreparedDuration, formatter) % 8 == 0,
              "a prepared duration's formatter is aligned");

bool mlocGoodDuration(const mlocDurationValues* values)
{
    mlocDateTimeDuration record;
    return values != NULL && mlocMakeDuration(values, &record) == mloc_success;
}

DurationValue mlocMakeDurationValue(const mlocDurationValues* values)
{
    DurationValue v;
    memset(&v, 0, sizeof v);
    v.values = *values;
    return v;
}

bool mlocDurationLiteral(const uint8_t* text, size_t length, mlocDurationValues* out)
{
    return mlocParseDuration((const char*)text, length, out) == mloc_success;
}

bool mlocDurationFromSeconds(const mlocDecimal* seconds, mlocDurationValues* out)
{
    if (seconds->kind != mloc_decimalFinite)
    {
        return false;
    }
    // The whole seconds and nine digits of the fraction, from the digits
    // by their powers of ten.
    int32_t top = seconds->exponent + seconds->count - 1;
    if (seconds->count > 0 && top > 15)
    {
        return false;
    }
    uint64_t whole = 0;
    uint64_t nanoseconds = 0;
    for (int32_t p = top > 0 ? top : 0; p >= -9; p--)
    {
        bool held = seconds->count > 0 && p <= top && p >= seconds->exponent;
        // A held power's digit lies between the first and the last.
        MLOC_ASSERT(!held || top - p < seconds->count);
        uint64_t digit = held ? seconds->digits[top - p] : 0;
        if (p >= 0)
        {
            whole = whole * 10 + digit;
        }
        else
        {
            nanoseconds = nanoseconds * 10 + digit;
        }
    }
    if (whole >= (uint64_t)1 << 53)
    {
        return false;
    }
    // Balanced up to hours as Temporal balances a time duration, each
    // field below 2^53 and so exact, a zero field +0 as Temporal's.
    bool negative = seconds->negative != 0;
    uint64_t fields[6] = {whole / 3600,          whole / 60 % 60,           whole % 60,
                          nanoseconds / 1000000, nanoseconds / 1000 % 1000, nanoseconds % 1000};
    double values[6];
    for (int i = 0; i < 6; i++)
    {
        values[i] = fields[i] == 0 ? 0.0 : negative ? -(double)fields[i] : (double)fields[i];
    }
    *out = (mlocDurationValues){0,         0,         0,         0,         values[0],
                                values[1], values[2], values[3], values[4], values[5]};
    return true;
}

static bool NameIs(const uint8_t* name, size_t length, const char* text)
{
    return length == strlen(text) && memcmp(name, text, length) == 0;
}

// The index of an option's text among count names (null entries
// skipped), or -1.
static int Choice(const NumberOption* option, const char* const* names, int count)
{
    for (int i = 0; option->hasText && i < count; i++)
    {
        if (names[i] != NULL && NameIs(option->text, option->length, names[i]))
        {
            return i;
        }
    }
    return -1;
}

// The names of the options and their values, each keyed by the public
// enumeration's value, so that the index a name is found at is the value
// it gives; null for a value no name gives.
static const char* const s_fields[] = {
    [mloc_durationYears] = "years",
    [mloc_durationMonths] = "months",
    [mloc_durationWeeks] = "weeks",
    [mloc_durationDays] = "days",
    [mloc_durationHours] = "hours",
    [mloc_durationMinutes] = "minutes",
    [mloc_durationSeconds] = "seconds",
    [mloc_durationMilliseconds] = "milliseconds",
    [mloc_durationMicroseconds] = "microseconds",
    [mloc_durationNanoseconds] = "nanoseconds",
};
// The default has no name.
static const char* const s_fieldStyles[] = {
    [mloc_durationFieldLong] = "long",        [mloc_durationFieldShort] = "short",
    [mloc_durationFieldNarrow] = "narrow",    [mloc_durationFieldNumeric] = "numeric",
    [mloc_durationFieldTwoDigit] = "2-digit",
};
static const char* const s_displays[] = {
    [mloc_durationDisplayAuto] = "auto",
    [mloc_durationDisplayAlways] = "always",
};
static const char* const s_styles[] = {
    [mloc_durationStyleShort] = "short",
    [mloc_durationStyleLong] = "long",
    [mloc_durationStyleNarrow] = "narrow",
    [mloc_durationStyleDigital] = "digital",
};

static_assert(sizeof(s_fields) / sizeof(s_fields[0]) == mloc_durationFieldCount,
              "a name for each mlocDurationField");
static_assert(sizeof(s_fieldStyles) / sizeof(s_fieldStyles[0]) == mloc_durationFieldTwoDigit + 1,
              "a name for each mlocDurationFieldStyle");
static_assert(sizeof(s_displays) / sizeof(s_displays[0]) == mloc_durationDisplayAlways + 1,
              "a name for each mlocDurationDisplay");
static_assert(sizeof(s_styles) / sizeof(s_styles[0]) == mloc_durationStyleDigital + 1,
              "a name for each mlocDurationStyle");

// The styles a field takes, a bit for each mlocDurationFieldStyle: years
// to days the names, hours to seconds also numeric and 2-digit, the
// smaller ones also numeric.
static uint32_t FieldStyles(int field)
{
    uint32_t names = 1u << mloc_durationFieldLong | 1u << mloc_durationFieldShort |
                     1u << mloc_durationFieldNarrow;
    uint32_t numeric = 1u << mloc_durationFieldNumeric;
    return field < mloc_durationHours      ? names
           : field <= mloc_durationSeconds ? names | numeric | 1u << mloc_durationFieldTwoDigit
                                           : names | numeric;
}

// A field's style option, as far as the field takes it.
static bool SetField(DurationValue* v, int field, const NumberOption* option)
{
    int index =
        Choice(option, s_fieldStyles, (int)(sizeof(s_fieldStyles) / sizeof(s_fieldStyles[0])));
    if (index < 0 || (FieldStyles(field) & 1u << index) == 0)
    {
        return false;
    }
    v->key.fields[field] = (uint8_t)index;
    v->key.given |= durationGivenField << field;
    return true;
}

// A field's display option.
static bool SetDisplay(DurationValue* v, int field, const NumberOption* option)
{
    int index = Choice(option, s_displays, (int)(sizeof(s_displays) / sizeof(s_displays[0])));
    if (index < 0)
    {
        return false;
    }
    v->key.displays[field] = (uint8_t)index;
    v->key.given |= durationGivenDisplay << field;
    return true;
}

// The style or the fractional digits; whether the name was either, and
// in good whether its value was good.
static bool SetWhole(DurationValue* v, const uint8_t* name, size_t length,
                     const NumberOption* option, bool* good)
{
    if (NameIs(name, length, "style"))
    {
        int index = Choice(option, s_styles, (int)(sizeof(s_styles) / sizeof(s_styles[0])));
        *good = index >= 0;
        v->key.style = *good ? (uint8_t)index : v->key.style;
        v->key.given |= *good ? durationGivenStyle : 0;
        return true;
    }
    if (NameIs(name, length, "fractionalDigits"))
    {
        int digits = mlocDigitSize(option);
        *good = digits >= 0 && digits <= 9;
        v->key.fractionalDigits = *good ? (uint8_t)digits : v->key.fractionalDigits;
        v->key.given |= *good ? durationGivenFraction : 0;
        return true;
    }
    return false;
}

mlocMessageFaultKind mlocSetDurationOption(DurationValue* value, const uint8_t* name, size_t length,
                                           const NumberOption* option)
{
    // Set on a copy, kept only for a good literal: they choose the
    // formatter, made once at bind.
    DurationValue set = *value;
    bool good = false;
    bool known = SetWhole(&set, name, length, option, &good);
    for (int field = 0; !known && field < mloc_durationFieldCount; field++)
    {
        size_t size = strlen(s_fields[field]);
        if (length < size || memcmp(name, s_fields[field], size) != 0)
        {
            continue;
        }
        if (length == size)
        {
            known = true;
            good = SetField(&set, field, option);
        }
        else if (NameIs(name + size, length - size, "Display"))
        {
            known = true;
            good = SetDisplay(&set, field, option);
        }
    }
    if (!known)
    {
        return mloc_messageFaultNone;
    }
    if (!good || !option->literal)
    {
        return mloc_messageFaultBadOption;
    }
    *value = set;
    return mloc_messageFaultNone;
}

// The formatter's options for a value's key.
static mlocDurationOptions OptionsOf(const DurationKey* key)
{
    mlocDurationOptions options;
    memset(&options, 0, sizeof options);
    options.style = (mlocDurationStyle)key->style;
    for (int i = 0; i < mloc_durationFieldCount; i++)
    {
        options.fields[i] = (mlocDurationFieldStyle)key->fields[i];
        options.displays[i] = (mlocDurationDisplay)key->displays[i];
    }
    options.fixedFractionalDigits = (key->given & durationGivenFraction) != 0;
    options.fractionalDigits = key->fractionalDigits;
    return options;
}

bool mlocPrepareDuration(const DurationValue* value, const mlocDataSet* data,
                         const mlocLocale* locale, PreparedDuration* out)
{
    memset(out, 0, offsetof(PreparedDuration, formatter));
    out->key = value->key;
    mlocDurationOptions options = OptionsOf(&value->key);
    return mlocInitDurationFormatter(data, locale, &options, &out->formatter) >= 0;
}

// Writes a value with a formatter made here, off the resolver's frames.
MLOC_NOINLINE static mlocResult WriteMade(const DurationValue* value, const mlocDataSet* data,
                                          const mlocLocale* locale, mlocOutput* output)
{
    mlocDurationFormatter formatter;
    mlocDurationOptions options = OptionsOf(&value->key);
    mlocResult made = mlocInitDurationFormatter(data, locale, &options, &formatter);
    return made < 0 ? made : mlocDurationFormatter_Format(&formatter, &value->values, output);
}

mlocMessageFaultKind mlocWriteDurationValue(const DurationValue* value,
                                            const PreparedDuration* prepared,
                                            const mlocDataSet* data, const mlocLocale* locale,
                                            mlocOutput* output)
{
    mlocResult result =
        prepared != NULL && memcmp(&prepared->key, &value->key, sizeof value->key) == 0
            ? mlocDurationFormatter_Format(&prepared->formatter, &value->values, output)
            : WriteMade(value, data, locale, output);
    // A text past the room reports the length it needs.
    return result == mloc_success || (result == mloc_errorCapacity && output->length > 0)
               ? mloc_messageFaultNone
               : mloc_messageFaultUnsupportedOperation;
}

size_t mlocWriteIsoDuration(const mlocDurationValues* values, char* text, size_t capacity)
{
    mlocOutput out = {text, capacity, 0, NULL, NULL};
    return mlocFormatTemporalDuration(values, NULL, &out) == mloc_success ? out.length : 0;
}
