// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_duration.h"

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
    double sign = seconds->negative ? -1.0 : 1.0;
    mlocDurationValues values;
    memset(&values, 0, sizeof values);
    values.seconds = sign * (double)whole;
    values.nanoseconds = sign * (double)nanoseconds;
    mlocDateTimeDuration record;
    return mlocMakeDuration(&values, &record) == mloc_success &&
           mlocGetDurationValues(&record, mloc_unitHour, out) == mloc_success;
}

static bool NameIs(const uint8_t* name, size_t length, const char* text)
{
    return length == strlen(text) && memcmp(name, text, length) == 0;
}

// The index of an option's text among count names, or -1.
static int Choice(const NumberOption* option, const char* const* names, int count)
{
    for (int i = 0; option->hasText && i < count; i++)
    {
        if (NameIs(option->text, option->length, names[i]))
        {
            return i;
        }
    }
    return -1;
}

// The fields' names, by mlocDurationField.
static const char* const s_fields[] = {"years",        "months",     "weeks",   "days",
                                       "hours",        "minutes",    "seconds", "milliseconds",
                                       "microseconds", "nanoseconds"};

// A field's style option: mlocDurationFieldStyle's from long, as far as
// the field takes them (years to days the names, hours to seconds also
// numeric and 2-digit, the smaller ones also numeric).
static bool SetField(DurationValue* v, int field, const NumberOption* option)
{
    static const char* const styles[] = {"long", "short", "narrow", "numeric", "2-digit"};
    int taken = field < mloc_durationHours ? 3 : field <= mloc_durationSeconds ? 5 : 4;
    int index = Choice(option, styles, taken);
    if (index < 0)
    {
        return false;
    }
    v->key.fields[field] = (uint8_t)(mloc_durationFieldLong + index);
    v->key.given |= durationGivenField << field;
    return true;
}

// A field's display option.
static bool SetDisplay(DurationValue* v, int field, const NumberOption* option)
{
    static const char* const displays[] = {"auto", "always"};
    int index = Choice(option, displays, 2);
    if (index < 0)
    {
        return false;
    }
    v->key.displays[field] = (uint8_t)(mloc_durationDisplayAuto + index);
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
        static const char* const styles[] = {"short", "long", "narrow", "digital"};
        int index = Choice(option, styles, 4);
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
    return mlocInitDurationFormatter(&out->formatter, data, locale, &options) >= 0;
}

// Writes a value with a formatter made here, off the resolver's frames.
MLOC_NOINLINE static mlocResult WriteMade(const DurationValue* value, const mlocDataSet* data,
                                          const mlocLocale* locale, mlocOutput* output)
{
    mlocDurationFormatter formatter;
    mlocDurationOptions options = OptionsOf(&value->key);
    mlocResult made = mlocInitDurationFormatter(&formatter, data, locale, &options);
    return made < 0 ? made : mlocFormatDuration(&formatter, &value->values, output);
}

mlocMessageFaultKind mlocWriteDurationValue(const DurationValue* value,
                                            const PreparedDuration* prepared,
                                            const mlocDataSet* data, const mlocLocale* locale,
                                            mlocOutput* output)
{
    mlocResult result =
        prepared != NULL && memcmp(&prepared->key, &value->key, sizeof value->key) == 0
            ? mlocFormatDuration(&prepared->formatter, &value->values, output)
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
