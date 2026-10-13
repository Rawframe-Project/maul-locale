// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Spell-out formatters (maul-locale/spellout.h, record mloc-0034): a
// rule set found in the locale's groups, the locale's symbols and plural
// rules, and the engine (spellout_format.h).

#include "maul-locale/spellout.h"

#include "bytes.h"
#include "data_view.h"
#include "decimal.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "plural.h"
#include "plural_rules.h"
#include "spellout_format.h"

#include <string.h>

enum
{
    // Marks a spell-out formatter the library made.
    spelloutStamp = 0x53,
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

// The first code point of a symbol's UTF-8 text.
static uint32_t FirstCodePoint(const uint8_t* text, uint32_t length)
{
    if (length == 0)
    {
        return 0;
    }
    uint8_t c = text[0];
    uint32_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    if (n > length)
    {
        return 0;
    }
    uint32_t cp = n == 1 ? c : c & (0x7Fu >> n);
    for (uint32_t i = 1; i < n; i++)
    {
        cp = cp << 6 | (text[i] & 0x3Fu);
    }
    return cp;
}

mlocResult mlocInitSpelloutFormatter(mlocSpelloutFormatter* formatterOut, const mlocDataSet* data,
                                     const mlocLocale* locale, const char* ruleSet, size_t length)
{
    if (formatterOut == nullptr || data == nullptr || locale == nullptr ||
        (ruleSet == nullptr && length != 0) || (ruleSet != nullptr && length == 0))
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
    mlocResult core = mlocInitNumberCore(&f.core, data, locale, mloc_numberDecimal);
    if (core < 0 || mlocFindPluralRule(data, locale, mloc_pluralCardinal, &f.cardinal) < 0 ||
        mlocFindPluralRule(data, locale, mloc_pluralOrdinal, &f.ordinal) < 0)
    {
        return mloc_errorNotFound;
    }
    f.decimalPoint = FirstCodePoint(f.core.symbolText + f.core.symbolStart[numberDecimal],
                                    f.core.symbolLength[numberDecimal]);
    f.stamp = spelloutStamp;
    memcpy(formatterOut, &f, sizeof(f));
    memset((uint8_t*)formatterOut + sizeof(f), 0, sizeof(*formatterOut) - sizeof(f));
    return found == mloc_fellBack || core == mloc_fellBack ? mloc_fellBack : mloc_success;
}

// A formatter the library made, copied out; false for any other.
static bool ReadFormatter(const mlocSpelloutFormatter* opaque, SpelloutFormat* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    if (f->stamp != spelloutStamp || f->group == nullptr || f->groupSize < spelloutHeaderSize ||
        f->core.symbolText == nullptr || f->core.digitLength < 1 ||
        f->core.digitLength > numberDigitSize || f->cardinal >= mlocPluralRuleSetCount ||
        f->ordinal >= mlocPluralRuleSetCount)
    {
        return false;
    }
    return f->set < mlocLoad16(f->group);
}

mlocResult mlocFormatSpellout(const mlocSpelloutFormatter* formatter, const mlocDecimal* decimal,
                              mlocOutput* output)
{
    SpelloutFormat f;
    if (!ReadFormatter(formatter, &f) || decimal == nullptr || output == nullptr ||
        output->part != nullptr || (output->buffer == nullptr && output->capacity != 0) ||
        !mlocValidDecimal(decimal))
    {
        return mloc_errorInvalid;
    }
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    mlocResult r = mlocWriteSpellout(&f, decimal, &w);
    output->length = w.length;
    if (r != mloc_success)
    {
        return r;
    }
    return output->buffer == nullptr || w.length > output->capacity ? mloc_errorCapacity
                                                                    : mloc_success;
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
    if (data == nullptr || locale == nullptr)
    {
        return 0;
    }
    return VisitSets(data, locale, SIZE_MAX, nullptr, nullptr);
}

mlocResult mlocGetSpelloutRuleSet(const mlocDataSet* data, const mlocLocale* locale, size_t index,
                                  const char** nameOut, size_t* lengthOut)
{
    if (data == nullptr || locale == nullptr || nameOut == nullptr || lengthOut == nullptr)
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
