// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Relative time (maul-locale/relative.h): an amount's magnitude written by
// the number formatting in the pattern of its direction and plural
// category, the patterns read along the locale's chain from relative.mloc
// (relative_tables.h), or a near amount's name.

#include "relative_format.h"

#include "bytes.h"
#include "data_view.h"
#include "decimal.h"
#include "invariant.h"
#include "kinds.h"
#include "locale_context.h"
#include "locale_fallback.h"
#include "locale_valid.h"
#include "number_core.h"
#include "number_format.h"
#include "number_output.h"
#include "plural.h"
#include "plural_rules.h"
#include "relative_tables.h"
#include "spellout_data.h"

#include <stddef.h>
#include <string.h>

static_assert(sizeof(RelativeFormat) <= sizeof(mlocRelativeFormatter),
              "a relative time formatter fits its opaque value");
static_assert(offsetof(RelativeFormat, formatter) == 0, "the formatter begins it");

typedef struct ChainWalk
{
    RelativeFormat* f;
    bool full;
} ChainWalk;

static bool TakeStep(void* context, const DataView* view, uint32_t locale, int depth)
{
    (void)depth;
    ChainWalk* walk = context;
    DataSpan unit;
    if (!mlocFindDataUnit(view, kindRelative, locale, &unit))
    {
        return false;
    }
    if (walk->f->unitCount == relativeChainMost)
    {
        walk->full = true;
        return true;
    }
    walk->f->units[walk->f->unitCount++] = unit;
    return false;
}

static bool ValidOptions(const mlocRelativeOptions* o)
{
    return mlocValidFormatOptions(&o->number) && o->number.style == mloc_numberDecimal &&
           o->number.signDisplay == mloc_signAuto &&
           (uint32_t)o->width <= mloc_relativeWidthNarrow &&
           (uint32_t)o->numeric <= mloc_relativeNumericAuto &&
           (uint32_t)o->capitalization <= mloc_capitalizationStandAlone;
}

// Whether the formatter titlecases: at a sentence's start always, in the
// other contexts where the locale's relative transforms name them.
static bool Capitalizes(const mlocDataSet* data, const mlocLocale* locale, mlocCapitalization c)
{
    if (c == mloc_capitalizationSentence)
    {
        return true;
    }
    uint8_t contexts =
        c == mloc_capitalizationNone ? 0 : mlocGetLocaleContext(data, locale, contextRelative);
    return (c == mloc_capitalizationListOrMenu && (contexts & contextListOrMenu) != 0) ||
           (c == mloc_capitalizationStandAlone && (contexts & contextStandAlone) != 0);
}

mlocResult mlocInitRelativeFormatter(const mlocDataSet* data, const mlocLocale* locale,
                                     const mlocRelativeOptions* options,
                                     mlocRelativeFormatter* formatterOut)
{
    static const mlocRelativeOptions s_defaults = {0};
    const mlocRelativeOptions* o = options != nullptr ? options : &s_defaults;
    if (formatterOut == nullptr || !mlocValidDataSet(data) || !mlocLocaleValid(locale) ||
        !ValidOptions(o))
    {
        return mloc_errorInvalid;
    }
    RelativeFormat f;
    memset(&f, 0, sizeof(f));
    ChainWalk walk = {&f, false};
    if (!mlocVisitLocaleChainOf(data, mloc_componentMain, kindRelative, locale, TakeStep, &walk) ||
        f.unitCount == 0)
    {
        return mloc_errorNotFound;
    }
    if (walk.full)
    {
        return mloc_errorFormat;
    }
    Formatter* n = &f.formatter;
    bool compact = o->number.notation == mloc_notationCompactShort ||
                   o->number.notation == mloc_notationCompactLong;
    mlocResult result = compact
                            ? mlocInitCompactCore(&n->core, data, locale,
                                                  o->number.notation == mloc_notationCompactLong,
                                                  &n->compactTable, &n->compactPatterns)
                            : mlocInitNumberCore(&n->core, data, locale, mloc_numberDecimal);
    mlocPluralRules rules;
    if (result < 0 || mlocInitPluralRules(data, locale, mloc_pluralCardinal, &rules) < 0)
    {
        return result < 0 ? result : mloc_errorNotFound;
    }
    n->pluralRule = mlocPluralRuleOf(&rules);
    mlocSetFormatOptions(n, &o->number, relativeStamp);
    f.width = (uint8_t)o->width;
    f.numeric = (uint8_t)o->numeric;
    if (Capitalizes(data, locale, o->capitalization) &&
        mlocFindDataSetUnit(data, kindRelativeTitlecase, nullptr, 0, &f.titlecase))
    {
        f.capitalize = 1;
        f.turkic = strcmp(locale->language, "tr") == 0 || strcmp(locale->language, "az") == 0;
    }
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

bool mlocReadRelativeFormat(const mlocRelativeFormatter* opaque, RelativeFormat* f)
{
    if (opaque == nullptr ||
        !mlocReadFormatter(opaque, sizeof(*opaque), relativeStamp, &f->formatter))
    {
        return false;
    }
    size_t head = sizeof(Formatter);
    memcpy((uint8_t*)f + head, (const uint8_t*)opaque + head, sizeof(*f) - head);
    return f->unitCount > 0 && f->unitCount <= relativeChainMost &&
           f->width <= mloc_relativeWidthNarrow && f->numeric <= mloc_relativeNumericAuto &&
           f->capitalize <= 1 && f->formatter.pluralRule < mlocPluralRuleSetCount &&
           (!f->capitalize || f->titlecase.bytes != nullptr);
}

// A unit's string at a key: true when the unit files the key.
static bool FiledText(DataSpan unit, uint32_t key, DataSpan* out)
{
    uint32_t count = mlocLoad32(unit.bytes);
    uint32_t stringCount = mlocLoad32(unit.bytes + 4);
    const uint8_t* keys = unit.bytes + relativeHeaderSize;
    const uint8_t* strings = keys + 2 * (size_t)count;
    const uint8_t* offsets = strings + 2 * (size_t)count;
    uint32_t low = 0;
    uint32_t high = count;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        if (mlocLoad16(keys + 2 * (size_t)middle) < key)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    if (low == count || mlocLoad16(keys + 2 * (size_t)low) != key)
    {
        return false;
    }
    uint32_t string = mlocLoad16(strings + 2 * (size_t)low);
    // mlocValidRelative checked each entry's string.
    MLOC_ASSERT(string < stringCount);
    uint32_t start = mlocLoad32(offsets + 4 * (size_t)string);
    out->bytes = offsets + 4 * ((size_t)stringCount + 1) + start;
    out->size = mlocLoad32(offsets + 4 * (size_t)string + 4) - start;
    return true;
}

// A slot's text: the width's first along the chain, else the next wider
// width's; false for none or an empty one.
static bool Lookup(const RelativeFormat* f, uint32_t unit, uint32_t slot, DataSpan* out)
{
    // The unit was checked with the amount, the width when the formatter
    // was read, and the callers' slots are a name's or a direction's.
    MLOC_ASSERT(unit < relativeUnitCount && f->width < relativeWidthCount && slot < relativeSlots);
    for (uint32_t w = f->width + 1; w-- > 0;)
    {
        uint32_t key = (unit * relativeWidthCount + w) * relativeSlots + slot;
        for (uint32_t i = 0; i < f->unitCount; i++)
        {
            if (FiledText(f->units[i], key, out))
            {
                return out->size > 0;
            }
        }
    }
    return false;
}

// The titlecase of text's first code point when the formatter
// capitalizes and it is a lowercase letter, and the bytes it replaces;
// else no bytes for none.
static size_t TitleOf(const RelativeFormat* f, DataSpan text, const uint8_t** title,
                      uint32_t* titleLength)
{
    static const uint8_t s_dottedI[] = {0xC4, 0xB0};
    static const uint8_t s_masks[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    *title = text.bytes;
    *titleLength = 0;
    if (!f->capitalize || text.size == 0)
    {
        return 0;
    }
    uint8_t lead = text.bytes[0];
    size_t size = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
    uint32_t cp = lead & s_masks[size];
    for (size_t i = 1; i < size && i < text.size; i++)
    {
        cp = cp << 6 | (text.bytes[i] & 0x3Fu);
    }
    if (f->turkic && cp == 'i')
    {
        *title = s_dottedI;
        *titleLength = sizeof(s_dottedI);
        return size;
    }
    if (!mlocSpelloutTitlecase(f->titlecase, cp, title, titleLength))
    {
        *title = text.bytes;
        *titleLength = 0;
        return 0;
    }
    return size;
}

// A text of the data as it is written: its first letter's titlecase when
// the formatter capitalizes (no bytes otherwise), then the rest.
typedef struct TitledText
{
    const uint8_t* title;
    uint32_t titleLength;
    DataSpan rest;
} TitledText;

static TitledText Titled(const RelativeFormat* f, DataSpan text)
{
    TitledText t;
    size_t old = TitleOf(f, text, &t.title, &t.titleLength);
    // The data's text is well-formed UTF-8 (mlocValidRelative), so its first
    // code point ends within it.
    MLOC_ASSERT(old <= text.size);
    t.rest = (DataSpan){text.bytes + old, text.size - (uint32_t)old};
    return t;
}

// The offset an amount names under auto, from -2 to 2, or false: an
// integer of at most one digit, its trailing zeros aside.
static bool NamedOffset(const mlocDecimal* d, int32_t* offset)
{
    if (d->count == 0)
    {
        *offset = 0;
        return true;
    }
    if (d->count != 1 || d->exponent != 0 || d->digits[0] > 2)
    {
        return false;
    }
    *offset = d->negative != 0 ? -(int32_t)d->digits[0] : (int32_t)d->digits[0];
    return true;
}

// What an amount of a unit writes: a name, or the pattern of its
// direction and its magnitude's category with the magnitude at the
// pattern's "{0}". Both are written from the data, whatever their length.
typedef struct RelativeChoice
{
    TitledText text;
    // Where the "{0}" begins in the text's rest; the rest's size for a
    // name or a pattern that writes no number.
    uint32_t numberAt;
    // The magnitude, rounded once for every pass of the writing.
    Prepared number;
} RelativeChoice;

static mlocResult Choose(const RelativeFormat* f, const mlocDecimal* amount, uint32_t unit,
                         RelativeChoice* c)
{
    int32_t offset = 0;
    DataSpan text;
    if (f->numeric == mloc_relativeNumericAuto && NamedOffset(amount, &offset) &&
        Lookup(f, unit, (uint32_t)(offset + 2), &text))
    {
        c->text = Titled(f, text);
        c->numberAt = c->text.rest.size;
        return mloc_success;
    }
    uint32_t base = amount->negative != 0 ? relativePast : relativeFuture;
    mlocDecimal magnitude = *amount;
    magnitude.negative = 0;
    uint32_t category = mloc_pluralOther;
    mlocResult r = mlocPrepareDecimal(&f->formatter, &magnitude, &c->number, &category);
    if (r != mloc_success)
    {
        return r;
    }
    // Other's pattern where the chain lacks the category's.
    if (!Lookup(f, unit, base + category, &text) && !Lookup(f, unit, base + 5, &text))
    {
        return mloc_errorNotFound;
    }
    // A titlecased first letter is not a "{0}"'s brace.
    c->text = Titled(f, text);
    const uint8_t* rest = c->text.rest.bytes;
    c->numberAt = c->text.rest.size;
    for (uint32_t i = 0; i + 3 <= c->text.rest.size; i++)
    {
        if (memcmp(rest + i, "{0}", 3) == 0)
        {
            c->numberAt = i;
            break;
        }
    }
    return mloc_success;
}

// Writes a choice: the text as literal parts, the magnitude's parts
// where its "{0}" was.
static mlocResult WriteChoice(const void* context, NumberWriter* w)
{
    const RelativeChoice* c = context;
    const TitledText* t = &c->text;
    mlocWritePart(w, mloc_numberPartLiteral, t->title, t->titleLength);
    mlocWritePart(w, mloc_numberPartLiteral, t->rest.bytes, c->numberAt);
    if (c->numberAt < t->rest.size)
    {
        mlocWritePrepared(&c->number, w);
        mlocWritePart(w, mloc_numberPartLiteral, t->rest.bytes + c->numberAt + 3,
                      t->rest.size - c->numberAt - 3);
    }
    return mloc_success;
}

bool mlocValidRelativeAmount(const mlocDecimal* amount, uint32_t unit)
{
    return amount != nullptr && mlocValidDecimal(amount) && amount->kind == mloc_decimalFinite &&
           unit < relativeUnitCount;
}

mlocResult mlocWriteRelative(const RelativeFormat* f, const mlocDecimal* amount, uint32_t unit,
                             uint32_t kind, NumberWriter* w)
{
    RelativeChoice c;
    mlocResult r = Choose(f, amount, unit, &c);
    if (r != mloc_success)
    {
        return r;
    }
    // The text is written in place; a parts pass counts its length and
    // reports it as one part, reading no bytes.
    if (w->parts == nullptr)
    {
        return WriteChoice(&c, w);
    }
    NumberWriter count = {0};
    (void)WriteChoice(&c, &count);
    mlocWritePart(w, kind, nullptr, count.length);
    return mloc_success;
}

mlocResult mlocRelativeFormatter_Format(const mlocRelativeFormatter* formatter,
                                        const mlocDecimal* amount, mlocRelativeUnit unit,
                                        mlocOutput* textOut)
{
    RelativeFormat f;
    if (!mlocReadRelativeFormat(formatter, &f) || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocValidRelativeAmount(amount, (uint32_t)unit))
    {
        return mloc_errorInvalid;
    }
    RelativeChoice c;
    mlocResult r = Choose(&f, amount, (uint32_t)unit, &c);
    return r != mloc_success ? r : mlocWriteOutput(WriteChoice, &c, textOut);
}
