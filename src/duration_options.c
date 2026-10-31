// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A duration formatter's options resolved (duration_format.h): ECMA-402's
// GetDurationUnitOptions and ValidateDurationUnitStyle, field by field.

#include "duration_format.h"
#include "invariant.h"

enum
{
    // The fields that take numeric styles begin at hours; the fractions
    // at milliseconds.
    firstClock = mloc_durationHours,
    firstFraction = mloc_durationMilliseconds,
};

static bool Numeric(uint32_t style)
{
    return style == mloc_durationFieldNumeric || style == mloc_durationFieldTwoDigit ||
           style == durationFieldFraction;
}

// Whether a field takes a style given: years to days the three widths,
// hours to seconds also numeric and two-digit, the fractions numeric.
static bool Takes(uint32_t field, uint32_t style)
{
    return style <= mloc_durationFieldNarrow ||
           (field >= firstClock && style == mloc_durationFieldNumeric) ||
           (field >= firstClock && field < firstFraction && style == mloc_durationFieldTwoDigit);
}

// A field's default style and display (GetDurationUnitOptions' steps
// for an undefined style).
static void DefaultOf(uint32_t field, uint32_t base, uint32_t previous, uint32_t* style,
                      uint32_t* display)
{
    bool minuteOrSecond = field == mloc_durationMinutes || field == mloc_durationSeconds;
    if (base == mloc_durationStyleDigital)
    {
        *style = field < firstClock ? mloc_durationFieldShort : mloc_durationFieldNumeric;
        *display = field >= firstClock && field < firstFraction ? mloc_durationDisplayAlways
                                                                : mloc_durationDisplayAuto;
    }
    else if (Numeric(previous))
    {
        *style = mloc_durationFieldNumeric;
        *display = minuteOrSecond ? mloc_durationDisplayAlways : mloc_durationDisplayAuto;
    }
    else
    {
        static const uint8_t s_widths[3] = {mloc_durationFieldShort, mloc_durationFieldLong,
                                            mloc_durationFieldNarrow};
        // The style was checked to be at most digital, taken above.
        MLOC_ASSERT(base < 3);
        *style = s_widths[base];
        *display = mloc_durationDisplayAuto;
    }
    // A default resolves to a style and a display of their own.
    MLOC_ASSERT(*style != mloc_durationFieldDefault && *display != mloc_durationDisplayDefault);
}

// ValidateDurationUnitStyle: no display always on a fraction, nothing but
// fractions after one, nothing but numeric fields after a numeric one.
static bool Allowed(uint32_t style, uint32_t display, uint32_t previous)
{
    return !(display == mloc_durationDisplayAlways && style == durationFieldFraction) &&
           !(previous == durationFieldFraction && style != durationFieldFraction) &&
           !(Numeric(previous) && !Numeric(style));
}

// Whether a field's given style and display are in range and its own.
static bool InRange(uint32_t field, uint32_t style, uint32_t display)
{
    return style <= mloc_durationFieldTwoDigit && display <= mloc_durationDisplayAlways &&
           (style == mloc_durationFieldDefault || Takes(field, style));
}

bool mlocResolveDurationOptions(const mlocDurationOptions* o,
                                uint8_t style[mloc_durationFieldCount],
                                uint8_t display[mloc_durationFieldCount])
{
    if ((uint32_t)o->style > mloc_durationStyleDigital || o->fractionalDigits > 9)
    {
        return false;
    }
    uint32_t previous = mloc_durationFieldDefault;
    for (uint32_t i = 0; i < mloc_durationFieldCount; i++)
    {
        uint32_t s = (uint32_t)o->fields[i];
        uint32_t shown = (uint32_t)o->displays[i];
        uint32_t d = mloc_durationDisplayAlways;
        if (!InRange(i, s, shown))
        {
            return false;
        }
        if (s == mloc_durationFieldDefault)
        {
            DefaultOf(i, (uint32_t)o->style, previous, &s, &d);
        }
        if (s == mloc_durationFieldNumeric && i >= firstFraction)
        {
            s = durationFieldFraction;
            d = mloc_durationDisplayAuto;
        }
        d = shown != mloc_durationDisplayDefault ? shown : d;
        if (!Allowed(s, d, previous))
        {
            return false;
        }
        bool padded =
            (i == mloc_durationMinutes || i == mloc_durationSeconds) &&
            (previous == mloc_durationFieldNumeric || previous == mloc_durationFieldTwoDigit);
        s = padded && s == mloc_durationFieldNumeric ? mloc_durationFieldTwoDigit : s;
        style[i] = (uint8_t)s;
        display[i] = (uint8_t)d;
        previous = i >= firstClock && i < mloc_durationNanoseconds ? s : previous;
    }
    return true;
}
