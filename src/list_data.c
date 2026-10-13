// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The list part's units (list_data.h).

#include "list_data.h"

#include "bytes.h"
#include "data_format.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "number_data.h"

#include <string.h>

// Whether a pattern holds "{0}" and "{1}" once each and no other brace.
static bool ValidPattern(const uint8_t* text, uint32_t length)
{
    uint32_t zeros = 0;
    uint32_t ones = 0;
    uint32_t braces = 0;
    for (uint32_t at = 0; at < length; at++)
    {
        if (text[at] == '{' || text[at] == '}')
        {
            braces++;
        }
        if (text[at] == '{' && at + 2 < length && text[at + 2] == '}')
        {
            zeros += text[at + 1] == '0';
            ones += text[at + 1] == '1';
        }
    }
    return zeros == 1 && ones == 1 && braces == 4;
}

bool mlocValidListPatterns(const DataView* view, DataSpan payload)
{
    (void)view;
    if (!mlocValidNumberTable(payload, listSetSize, 0))
    {
        return false;
    }
    NumberTable t = mlocReadNumberTable(payload, listSetSize);
    for (uint32_t s = 0; s < t.stringCount; s++)
    {
        uint32_t start = mlocLoad16(t.offsets + (size_t)s * 2);
        uint32_t end = mlocLoad16(t.offsets + (size_t)s * 2 + 2);
        if (!ValidPattern(t.text + start, end - start))
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < t.recordCount * listPatternCount; i++)
    {
        if (mlocLoad16(t.records + (size_t)i * 2) >= t.stringCount)
        {
            return false;
        }
    }
    return true;
}

bool mlocValidLists(const DataView* view, DataSpan payload)
{
    (void)view;
    return payload.size == listLocaleSize;
}

// The rule of a locale's language, as ICU picks it by the language asked
// for (canonical: Hebrew is "he", not "iw").
static ListRule RuleOf(const mlocLocale* locale)
{
    if (strcmp(locale->language, "es") == 0)
    {
        return listRuleSpanish;
    }
    return strcmp(locale->language, "he") == 0 ? listRuleHebrew : listRuleNone;
}

bool mlocFindListPatterns(const mlocDataSet* data, const mlocLocale* locale, ListStyle style,
                          ListPatterns* out)
{
    DataSpan lists;
    DataSpan sets;
    bool fellBack = false;
    DataView view;
    if (!mlocFindLocaleUnit(data, mloc_componentMain, kindLists, locale, &lists, &fellBack,
                            &view) ||
        !mlocFindDataUnit(&view, kindListPatterns, dataNoLocale, &sets))
    {
        return false;
    }
    NumberTable t = mlocReadNumberTable(sets, listSetSize);
    uint32_t set = mlocLoad16(lists.bytes + 2 * (size_t)style);
    if (set >= t.recordCount)
    {
        return false;
    }
    for (uint32_t p = 0; p < listPatternCount; p++)
    {
        uint32_t s = mlocLoad16(t.records + (size_t)set * listSetSize + 2 * p);
        uint32_t start = mlocLoad16(t.offsets + (size_t)s * 2);
        out->text[p] = (const char*)t.text + start;
        out->length[p] = (uint16_t)(mlocLoad16(t.offsets + (size_t)s * 2 + 2) - start);
    }
    out->rule = RuleOf(locale);
    return true;
}
