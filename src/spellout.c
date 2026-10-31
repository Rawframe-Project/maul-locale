// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Spell-out formatters (maul-locale/spellout.h): a rule set found in the
// locale's groups, the locale's symbols and plural rules, and the engine
// (spellout_format.h).

#include "maul-locale/spellout.h"

#include "bytes.h"
#include "data_view.h"
#include "decimal.h"
#include "invariant.h"
#include "kinds.h"
#include "locale_context.h"
#include "locale_fallback.h"
#include "locale_valid.h"
#include "plural.h"
#include "plural_rules.h"
#include "spellout_data.h"
#include "spellout_format.h"
#include "spellout_parse.h"

#include <stddef.h>
#include <string.h>

enum
{
    // Marks a spell-out formatter the library made.
    spelloutStamp = 0x53,
    // Bytes of text a format writes before the caller's buffer, more than
    // most texts take; a longer one (Burmese words from 10^17, Finnish
    // ordinals in some cases) is written twice.
    spelloutScratchSize = 512,
};

static_assert(sizeof(SpelloutFormat) <= sizeof(mlocSpelloutFormatter),
              "a spell-out formatter fits its opaque value");

// The groups a rule set's name is looked for in, in order.
static const uint32_t s_groupKinds[spelloutGroupCount] = {kindSpelloutRules, kindSpelloutOrdinals,
                                                          kindSpelloutNumbering};

// A group's unit, the first along the locale's chain, and whether it is
// root's for a locale that is not root.
static bool FindGroup(const mlocDataSet* data, const mlocLocale* locale, uint32_t kind,
                      DataSpan* spanOut, bool* rootOut)
{
    bool fellBack = false;
    DataView view;
    if (!mlocFindLocaleUnit(data, mloc_componentMain, kind, locale, spanOut, &fellBack, &view))
    {
        return false;
    }
    uint32_t root = 0;
    DataSpan rootSpan;
    *rootOut = fellBack && mlocFindDataLocale(&view, "root", 4, &root) &&
               mlocFindDataUnit(&view, kind, root, &rootSpan) && rootSpan.bytes == spanOut->bytes;
    return true;
}

static bool NameIs(const SpelloutRules* r, const uint8_t* set, const char* name, size_t length)
{
    return set[2] == length && memcmp(r->text + mlocLoad16(set), name, length) == 0;
}

// The public rule set of a name in a group, or -1.
static int32_t FindSet(DataSpan group, const char* name, size_t length)
{
    SpelloutRules r = mlocReadSpelloutRules(group);
    for (uint32_t s = 0; s < r.setCount; s++)
    {
        const uint8_t* set = r.sets + (size_t)s * spelloutSetSize;
        if ((set[3] & spelloutSetPublic) != 0 && NameIs(&r, set, name, length))
        {
            return (int32_t)s;
        }
    }
    return -1;
}

// Finds the group and set of a name (the spell-out group's default for
// none); mloc_fellBack when that group is root's.
static mlocResult FindRuleSet(const mlocDataSet* data, const mlocLocale* locale, const char* name,
                              size_t length, SpelloutFormat* f)
{
    for (int g = 0; g < spelloutGroupCount; g++)
    {
        DataSpan group;
        bool root = false;
        if (!FindGroup(data, locale, s_groupKinds[g], &group, &root))
        {
            if (g == 0)
            {
                return mloc_errorNotFound;
            }
            continue;
        }
        int32_t set = name == nullptr ? (int32_t)mlocReadSpelloutRules(group).defaultSet
                                      : FindSet(group, name, length);
        if (set >= 0)
        {
            f->group = group.bytes;
            f->groupSize = group.size;
            f->set = (uint16_t)set;
            return root && g != spelloutGroupNumbering ? mloc_fellBack : mloc_success;
        }
    }
    return mloc_errorNotFound;
}

// The public rule set of a name in one group along the locale's chain.
static mlocResult FindNamedSet(const mlocDataSet* data, const mlocLocale* locale, int group,
                               const char* name, size_t length, SpelloutFormat* f)
{
    DataSpan span;
    bool root = false;
    if (group < 0 || group >= spelloutGroupCount ||
        !FindGroup(data, locale, s_groupKinds[group], &span, &root))
    {
        return mloc_errorNotFound;
    }
    int32_t set = FindSet(span, name, length);
    if (set < 0)
    {
        return mloc_errorNotFound;
    }
    f->group = span.bytes;
    f->groupSize = span.size;
    f->set = (uint16_t)set;
    return mloc_success;
}

// Whether the first word is titlecased in a context, as ICU decides: at
// the beginning of a sentence always, in a list or menu or alone as the
// locale's number-spellout transforms say.
static bool Capitalizes(const mlocDataSet* data, const mlocLocale* locale,
                        mlocCapitalization capitalization)
{
    uint8_t contexts =
        capitalization == mloc_capitalizationNone || capitalization == mloc_capitalizationSentence
            ? 0
            : mlocGetLocaleContext(data, locale, contextNumberSpellout);
    return capitalization == mloc_capitalizationSentence ||
           (capitalization == mloc_capitalizationListOrMenu &&
            (contexts & contextListOrMenu) != 0) ||
           (capitalization == mloc_capitalizationStandAlone && (contexts & contextStandAlone) != 0);
}

// Completes a formatter whose rule set is found: the locale's symbols,
// plural rules and capitalization.
static mlocResult Complete(SpelloutFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                           const mlocSpelloutOptions* options, mlocResult found,
                           mlocSpelloutFormatter* formatterOut)
{
    mlocResult core = mlocInitNumberCore(&f->core, data, locale, mloc_numberDecimal);
    if (core < 0 || mlocFindPluralRule(data, locale, mloc_pluralCardinal, &f->cardinal) < 0 ||
        mlocFindPluralRule(data, locale, mloc_pluralOrdinal, &f->ordinal) < 0)
    {
        return mloc_errorNotFound;
    }
    if (options != nullptr && Capitalizes(data, locale, options->capitalization))
    {
        DataSpan titlecase;
        if (!mlocFindDataSetUnit(data, kindSpelloutTitlecase, nullptr, 0, &titlecase))
        {
            return mloc_errorNotFound;
        }
        f->capitalize = true;
        f->turkic = strcmp(locale->language, "tr") == 0 || strcmp(locale->language, "az") == 0;
        f->titlecase = titlecase.bytes;
        f->titlecaseSize = titlecase.size;
    }
    f->decimalPoint =
        mlocSpelloutFirstCodePoint(f->core.symbolText + f->core.symbolStart[numberDecimal],
                                   f->core.symbolLength[numberDecimal]);
    f->stamp = spelloutStamp;
    memcpy(formatterOut, f, sizeof(*f));
    memset((uint8_t*)formatterOut + sizeof(*f), 0, sizeof(*formatterOut) - sizeof(*f));
    return found == mloc_fellBack || core == mloc_fellBack ? mloc_fellBack : mloc_success;
}

static bool ValidOptions(const mlocSpelloutOptions* options)
{
    return options == nullptr ||
           ((uint32_t)options->capitalization <= (uint32_t)mloc_capitalizationStandAlone);
}

mlocResult mlocInitSpelloutFormatter(const mlocDataSet* data, const mlocLocale* locale,
                                     const char* ruleSet, size_t length,
                                     const mlocSpelloutOptions* options,
                                     mlocSpelloutFormatter* formatterOut)
{
    if (formatterOut == nullptr || data == nullptr || !mlocLocaleValid(locale) ||
        (ruleSet == nullptr && length != 0) || (ruleSet != nullptr && length == 0) ||
        !ValidOptions(options))
    {
        return mloc_errorInvalid;
    }
    if (ruleSet != nullptr && length >= 2 && memcmp(ruleSet, "%%", 2) == 0)
    {
        return mloc_errorInvalid;
    }
    SpelloutFormat f;
    memset(&f, 0, sizeof(f));
    mlocResult found = FindRuleSet(data, locale, ruleSet, length, &f);
    if (found < 0)
    {
        return found;
    }
    return Complete(&f, data, locale, options, found, formatterOut);
}

mlocResult mlocInitNumberingSystemFormatter(const mlocDataSet* data, const mlocLocale* locale,
                                            const char* system, size_t length,
                                            const mlocSpelloutOptions* options,
                                            mlocSpelloutFormatter* formatterOut)
{
    if (formatterOut == nullptr || data == nullptr || !mlocLocaleValid(locale) ||
        system == nullptr || !ValidOptions(options))
    {
        return mloc_errorInvalid;
    }
    DataSpan systems;
    SpelloutSystem s;
    if (!mlocFindDataSetUnit(data, kindSpelloutSystems, nullptr, 0, &systems) ||
        !mlocFindSpelloutSystem(systems, system, length, &s))
    {
        return mloc_errorNotFound;
    }
    // The locale the system names ("zh_Hant"), else the one asked for.
    mlocLocale named;
    if (s.localeLength > 0)
    {
        char tag[16];
        if (s.localeLength >= sizeof(tag))
        {
            return mloc_errorNotFound;
        }
        for (size_t i = 0; i < s.localeLength; i++)
        {
            tag[i] = s.locale[i] == '_' ? '-' : s.locale[i];
        }
        if (mlocParseLocale(tag, s.localeLength, &named).status != mloc_success)
        {
            return mloc_errorNotFound;
        }
        locale = &named;
    }
    SpelloutFormat f;
    memset(&f, 0, sizeof(f));
    mlocResult found = FindNamedSet(data, locale, s.group, s.name, s.nameLength, &f);
    if (found < 0)
    {
        return found;
    }
    return Complete(&f, data, locale, options, found, formatterOut);
}

// A formatter the library made, copied out, with its core or (to format,
// which seldom reads it) without; false for any other.
static bool ReadFormatter(const mlocSpelloutFormatter* opaque, bool withCore, SpelloutFormat* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    if (withCore)
    {
        memcpy(f, opaque, sizeof(*f));
    }
    else
    {
        size_t at = offsetof(SpelloutFormat, source);
        memcpy((uint8_t*)f + at, (const uint8_t*)opaque + at, sizeof(*f) - at);
    }
    if ((withCore && !mlocValidSpelloutCore(&f->core)) || f->source != nullptr)
    {
        return false;
    }
    f->source = withCore ? nullptr : opaque;
    if (f->stamp != spelloutStamp || f->group == nullptr || f->groupSize < spelloutHeaderSize ||
        f->cardinal >= mlocPluralRuleSetCount || f->ordinal >= mlocPluralRuleSetCount ||
        (f->capitalize &&
         (f->titlecase == nullptr || f->titlecaseSize < spelloutTitlecaseHeaderSize)))
    {
        return false;
    }
    return f->set < mlocLoad16(f->group);
}

// Titlecases the text's first code point, a lowercase letter, moving the
// rest when its length changes (only when the whole text is written; the
// length is the one needed either way).
static void Capitalize(const SpelloutFormat* f, uint32_t first, NumberWriter* w)
{
    static const uint8_t s_dottedI[] = {0xC4, 0xB0};
    const uint8_t* title = nullptr;
    uint32_t titleLength = 0;
    if (f->turkic && first == 'i')
    {
        title = s_dottedI;
        titleLength = sizeof(s_dottedI);
    }
    else if (first == 0 || !mlocSpelloutTitlecase((DataSpan){f->titlecase, f->titlecaseSize}, first,
                                                  &title, &titleLength))
    {
        return;
    }
    size_t old = first < 0x80 ? 1 : first < 0x800 ? 2 : first < 0x10000 ? 3 : 4;
    size_t length = w->length - old + titleLength;
    if (w->buffer != nullptr && w->length <= w->capacity && length <= w->capacity)
    {
        memmove(w->buffer + titleLength, w->buffer + old, w->length - old);
        memcpy(w->buffer, title, titleLength);
    }
    w->length = length;
}

// Writes the text of a value, titlecased when the formatter asks; whether
// the writer's buffer holds all of it goes to wholeOut.
static mlocResult WriteText(const SpelloutFormat* f, const mlocDecimal* decimal, NumberWriter* w,
                            bool* wholeOut)
{
    uint32_t first = 0;
    mlocResult r = mlocWriteSpellout(f, decimal, w, &first);
    bool whole = w->length <= w->capacity;
    if (r == mloc_success && f->capitalize)
    {
        Capitalize(f, first, w);
    }
    *wholeOut = whole && w->length <= w->capacity;
    return r;
}

mlocResult mlocSpelloutFormatter_Format(const mlocSpelloutFormatter* formatter,
                                        const mlocDecimal* decimal, mlocOutput* textOut)
{
    SpelloutFormat f;
    if (!ReadFormatter(formatter, false, &f) || decimal == nullptr || textOut == nullptr ||
        textOut->part != nullptr || (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocValidDecimal(decimal))
    {
        return mloc_errorInvalid;
    }
    // The rules can fail after writing text, so the text is written here
    // first and reaches the caller's buffer only once all of it is.
    char scratch[spelloutScratchSize];
    NumberWriter w = {.buffer = scratch, .capacity = sizeof(scratch)};
    bool whole = false;
    mlocResult r = WriteText(&f, decimal, &w, &whole);
    if (r != mloc_success)
    {
        return r;
    }
    if (textOut->buffer != nullptr && w.length <= textOut->capacity)
    {
        if (whole)
        {
            memcpy(textOut->buffer, scratch, w.length);
        }
        else
        {
            // Longer than the scratch: written again, where it succeeded.
            NumberWriter direct = {.buffer = textOut->buffer, .capacity = textOut->capacity};
            mlocResult again = WriteText(&f, decimal, &direct, &whole);
            MLOC_ASSERT(again == mloc_success && direct.length == w.length && whole);
            (void)again;
        }
    }
    textOut->length = w.length;
    return textOut->buffer == nullptr || w.length > textOut->capacity ? mloc_errorCapacity
                                                                      : mloc_success;
}

mlocResult mlocSpelloutFormatter_Parse(const mlocSpelloutFormatter* formatter, const char* text,
                                       size_t length, mlocSpelloutWorkspace* workspace,
                                       mlocDecimal* decimalOut, size_t* endOut)
{
    SpelloutFormat f;
    if (!ReadFormatter(formatter, true, &f) || (text == nullptr && length != 0) ||
        workspace == nullptr || decimalOut == nullptr || endOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    return mlocReadSpellout(&f, (const uint8_t*)text, length, workspace, decimalOut, endOut);
}

// The public sets of the locale's spell-out and ordinal suffix groups:
// the index-th one's name, or the count.
static size_t VisitSets(const mlocDataSet* data, const mlocLocale* locale, size_t index,
                        const char** nameOut, size_t* lengthOut)
{
    size_t count = 0;
    for (int g = 0; g < spelloutGroupNumbering; g++)
    {
        DataSpan group;
        bool root = false;
        if (!FindGroup(data, locale, s_groupKinds[g], &group, &root))
        {
            continue;
        }
        SpelloutRules r = mlocReadSpelloutRules(group);
        for (uint32_t s = 0; s < r.setCount; s++)
        {
            const uint8_t* set = r.sets + (size_t)s * spelloutSetSize;
            if ((set[3] & spelloutSetPublic) == 0)
            {
                continue;
            }
            if (count == index && nameOut != nullptr)
            {
                *nameOut = (const char*)r.text + mlocLoad16(set);
                *lengthOut = set[2];
            }
            count++;
        }
    }
    return count;
}

size_t mlocGetSpelloutRuleSetCount(const mlocDataSet* data, const mlocLocale* locale)
{
    if (data == nullptr || !mlocLocaleValid(locale))
    {
        return 0;
    }
    return VisitSets(data, locale, SIZE_MAX, nullptr, nullptr);
}

mlocResult mlocGetSpelloutRuleSet(const mlocDataSet* data, const mlocLocale* locale, size_t index,
                                  const char** nameOut, size_t* lengthOut)
{
    if (data == nullptr || !mlocLocaleValid(locale) || nameOut == nullptr || lengthOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    const char* name = nullptr;
    size_t length = 0;
    if (index >= VisitSets(data, locale, index, &name, &length))
    {
        return mloc_errorNotFound;
    }
    *nameOut = name;
    *lengthOut = length;
    return mloc_success;
}
