// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A duration formatter made and read (maul-locale/duration.h): the options
// resolved, the names of each unit-style field in its width, the unit list
// of the style, and the separators and hour padding of the locale's hms
// duration pattern, as ICU4X reads them.

#include "duration_format.h"

#include "kinds.h"
#include "plural.h"
#include "plural_rules.h"
#include "unit_names.h"
#include "unit_tables.h"

#include <stddef.h>
#include <string.h>

static_assert(sizeof(DurationFormat) <= sizeof(mlocDurationFormatter),
              "a duration formatter fits its opaque value");
static_assert(offsetof(DurationFormat, formatter) == 0,
              "the formatter begins a duration formatter");

// The run of a letter at a pattern's position: its length, 0 for none.
static size_t Run(const char* text, size_t length, size_t at, char letter)
{
    size_t n = 0;
    while (at + n < length && text[at + n] == letter)
    {
        n++;
    }
    return n;
}

bool mlocReadDurationDigital(const char* text, size_t length, DurationFormat* f, bool* twoDigitOut)
{
    static const char s_letters[3] = {'h', 'm', 's'};
    size_t at = 0;
    size_t hours = 0;
    for (uint32_t i = 0; i < 3; i++)
    {
        size_t run = Run(text, length, at, s_letters[i]);
        if (run < 1 || run > 2)
        {
            return false;
        }
        hours = i == 0 ? run : hours;
        at += run;
        if (i == 2)
        {
            break;
        }
        size_t end = at;
        while (end < length && text[end] != s_letters[i + 1])
        {
            end++;
        }
        if (end == at || end == length || end - at > durationSeparatorMost)
        {
            return false;
        }
        memcpy(f->separator[i], text + at, end - at);
        f->separatorLength[i] = (uint8_t)(end - at);
        at = end;
    }
    *twoDigitOut = hours == 2;
    return at == length;
}

// The field's unit and the names width of a unit style.
static const char* const s_units[mloc_durationFieldCount] = {
    "year",   "month",  "week",        "day",         "hour",
    "minute", "second", "millisecond", "microsecond", "nanosecond"};

static mlocResult LoadNames(DurationFormat* f, const mlocDataSet* data, const mlocLocale* locale)
{
    static const mlocUnitWidth s_widths[4] = {
        [mloc_durationFieldLong] = mloc_unitWidthLong,
        [mloc_durationFieldShort] = mloc_unitWidthShort,
        [mloc_durationFieldNarrow] = mloc_unitWidthNarrow,
    };
    size_t used = 0;
    PatternPool pool = {f->pool, durationPool, &used};
    for (uint32_t i = 0; i < mloc_durationFieldCount; i++)
    {
        if (f->style[i] < mloc_durationFieldLong || f->style[i] > mloc_durationFieldNarrow)
        {
            continue;
        }
        UnitForms forms;
        mlocResult r = mlocLoadUnitForms(data, locale, s_units[i], strlen(s_units[i]),
                                         mlocUnitWidthKind(s_widths[f->style[i]]),
                                         mloc_caseNominative, &forms);
        r = r == mloc_success ? mlocPoolUnitPatterns(&pool, &f->slots[i], &forms) : r;
        if (r != mloc_success)
        {
            return r;
        }
    }
    return mloc_success;
}

// The data the formatter reads: the digital pattern, the names, the list,
// the numbers and the plural rules.
static mlocResult Load(DurationFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                       uint32_t style)
{
    UnitPatternTexts p;
    if (!mlocFindUnitPatterns(data, locale, &p))
    {
        return mloc_errorNotFound;
    }
    bool twoDigit = false;
    if (!mlocReadDurationDigital(p.text[unitPatternHms], p.length[unitPatternHms], f, &twoDigit))
    {
        return mloc_errorFormat;
    }
    if (twoDigit && f->style[mloc_durationHours] == mloc_durationFieldNumeric)
    {
        f->style[mloc_durationHours] = mloc_durationFieldTwoDigit;
    }
    mlocResult r = LoadNames(f, data, locale);
    ListStyle list = style == mloc_durationStyleLong     ? listUnit
                     : style == mloc_durationStyleNarrow ? listUnitNarrow
                                                         : listUnitShort;
    if (r == mloc_success && !mlocFindListPatterns(data, locale, list, &f->list))
    {
        r = mloc_errorNotFound;
    }
    r = r == mloc_success ? mlocInitNumberCore(&f->formatter.core, data, locale, mloc_numberDecimal)
                          : r;
    if (r >= 0 && mlocInitPluralRules(&f->rules, data, locale, mloc_pluralCardinal) < 0)
    {
        r = mloc_errorNotFound;
    }
    return r;
}

mlocResult mlocInitDurationFormatter(mlocDurationFormatter* formatterOut, const mlocDataSet* data,
                                     const mlocLocale* locale, const mlocDurationOptions* options)
{
    static const mlocDurationOptions s_defaults = {0};
    static const DurationFormat s_zero;
    const mlocDurationOptions* o = options == nullptr ? &s_defaults : options;
    DurationFormat f = s_zero;
    if (formatterOut == nullptr || data == nullptr || locale == nullptr ||
        !mlocResolveDurationOptions(o, f.style, f.display))
    {
        return mloc_errorInvalid;
    }
    f.base = (uint8_t)o->style;
    f.fixedFraction = o->fixedFractionalDigits ? 1 : 0;
    f.fractionalDigits = o->fixedFractionalDigits ? o->fractionalDigits : 0;
    mlocResult result = Load(&f, data, locale, (uint32_t)o->style);
    if (result < 0)
    {
        return result;
    }
    f.formatter.pluralRule = mlocPluralRuleOf(&f.rules);
    mlocSetFormatOptions(&f.formatter, &(mlocNumberOptions){0}, durationStamp);
    f.formatter.unscaled = 1;
    f.stamp = durationStamp;
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

bool mlocValidDurationFormat(const DurationFormat* f)
{
    bool valid = f->stamp == durationStamp && f->base <= mloc_durationStyleDigital &&
                 f->fixedFraction <= 1 && f->fractionalDigits <= 9 &&
                 (f->fixedFraction == 1 || f->fractionalDigits == 0) &&
                 f->formatter.pluralRule < mlocPluralRuleSetCount && f->list.rule <= listRuleHebrew;
    for (uint32_t p = 0; p < listPatternCount; p++)
    {
        valid = valid && f->list.text[p] != nullptr;
    }
    for (uint32_t s = 0; s < 2; s++)
    {
        valid =
            valid && f->separatorLength[s] >= 1 && f->separatorLength[s] <= durationSeparatorMost;
    }
    for (uint32_t i = 0; valid && i < mloc_durationFieldCount; i++)
    {
        valid = f->style[i] >= mloc_durationFieldLong && f->style[i] <= durationFieldFraction &&
                f->display[i] >= mloc_durationDisplayAuto &&
                f->display[i] <= mloc_durationDisplayAlways;
        for (uint32_t c = 0; valid && c < unitCategories; c++)
        {
            valid = f->slots[i].start[c] <= durationPool &&
                    f->slots[i].length[c] <= durationPool - f->slots[i].start[c];
        }
    }
    return valid;
}

mlocResult mlocGetDurationOptions(const mlocDurationFormatter* formatter,
                                  mlocDurationOptions* optionsOut)
{
    DurationFormat f;
    if (formatter == nullptr || optionsOut == nullptr ||
        !mlocReadFormatter(formatter, sizeof(*formatter), durationStamp, &f.formatter))
    {
        return mloc_errorInvalid;
    }
    size_t head = sizeof(Formatter);
    memcpy((uint8_t*)&f + head, (const uint8_t*)formatter + head,
           offsetof(DurationFormat, pool) - head);
    if (!mlocValidDurationFormat(&f))
    {
        return mloc_errorInvalid;
    }
    mlocDurationOptions o = {.style = (mlocDurationStyle)f.base,
                             .fixedFractionalDigits = f.fixedFraction == 1,
                             .fractionalDigits = f.fractionalDigits};
    for (uint32_t i = 0; i < mloc_durationFieldCount; i++)
    {
        o.fields[i] = f.style[i] == durationFieldFraction ? mloc_durationFieldNumeric
                                                          : (mlocDurationFieldStyle)f.style[i];
        o.displays[i] = (mlocDurationDisplay)f.display[i];
    }
    *optionsOut = o;
    return mloc_success;
}
