// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Likely subtags (UTS #35, "Likely Subtags"): adding them by looking up
// the language with its script and region, with its script, with its
// region, then alone; removing them by trying the shorter forms. The
// table's layout is in locale_tables.h.

#include "locale_likely.h"

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_tables.h"

#include "maul-locale/locale.h"

#include <string.h>

enum
{
    likelyHeaderSize = 16,
};

typedef struct Likely
{
    const uint8_t* generalKeys;
    const uint8_t* generalValues;
    const uint8_t* languageValues;
    const uint8_t* languageKeys;
    uint32_t generalCount;
    uint32_t languageCount;
} Likely;

static Likely ReadLikely(DataSpan span)
{
    uint32_t languageCount = mlocLoad32(span.bytes);
    uint32_t generalCount = mlocLoad32(span.bytes + 4);
    const uint8_t* generalKeys = span.bytes + likelyHeaderSize;
    const uint8_t* generalValues = generalKeys + (size_t)generalCount * 8;
    const uint8_t* languageValues = generalValues + (size_t)generalCount * 8;
    return (Likely){
        .generalKeys = generalKeys,
        .generalValues = generalValues,
        .languageValues = languageValues,
        .languageKeys = languageValues + (size_t)languageCount * 4,
        .generalCount = generalCount,
        .languageCount = languageCount,
    };
}

// Whether count 5-bit groups, highest first, hold letters: the first
// `required` must, and the rest may be zero once one is.
static bool Letters(uint64_t packed, int count, int required)
{
    for (int i = 0; i < count; i++)
    {
        uint32_t letter = (uint32_t)(packed >> (5 * (count - 1 - i))) & 31;
        if (letter > 26 || (i < required && letter == 0))
        {
            return false;
        }
        if (letter == 0 && i + 1 < count && ((packed >> (5 * (count - 2 - i))) & 31) != 0)
        {
            return false;
        }
    }
    return true;
}

static bool ValidLanguage(uint32_t language)
{
    return language < 1u << 15 && Letters(language, 3, 2);
}

static bool ValidScript(uint32_t script)
{
    return script < 1u << 20 && Letters(script, 4, 4);
}

static bool ValidRegion(uint32_t region)
{
    return (region >= 1024 && region < 2024) || (region < 1024 && Letters(region, 2, 2));
}

// A packed tag whose parts are valid; script and region may be absent
// unless full.
static bool ValidTag(uint64_t tag, bool full)
{
    uint64_t language = tag >> 31;
    uint32_t script = (uint32_t)(tag >> 11) & 0xFFFFF;
    uint32_t region = (uint32_t)tag & 0x7FF;
    // Bits past the 46 of a tag land in the language, which refuses them.
    return language < 1u << 15 && ValidLanguage((uint32_t)language) &&
           ((script == 0 && !full) || ValidScript(script)) &&
           ((region == 0 && !full) || ValidRegion(region));
}

bool mlocValidLikely(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < likelyHeaderSize || mlocLoad64(payload.bytes + 8) != 0)
    {
        return false;
    }
    uint64_t languageCount = mlocLoad32(payload.bytes);
    uint64_t generalCount = mlocLoad32(payload.bytes + 4);
    if (payload.size != likelyHeaderSize + generalCount * 16 + languageCount * 6)
    {
        return false;
    }
    Likely likely = ReadLikely(payload);
    for (uint32_t i = 0; i < likely.generalCount; i++)
    {
        uint64_t key = mlocLoad64(likely.generalKeys + (size_t)i * 8);
        if ((i > 0 && key <= mlocLoad64(likely.generalKeys + (size_t)(i - 1) * 8)) ||
            !ValidTag(key, false) ||
            !ValidTag(mlocLoad64(likely.generalValues + (size_t)i * 8), true))
        {
            return false;
        }
    }
    uint32_t und = mlocPackLanguage("und", 3);
    for (uint32_t i = 0; i < likely.languageCount; i++)
    {
        uint32_t key = mlocLoad16(likely.languageKeys + (size_t)i * 2);
        uint32_t value = mlocLoad32(likely.languageValues + (size_t)i * 4);
        if ((i > 0 && key <= mlocLoad16(likely.languageKeys + (size_t)(i - 1) * 2)) ||
            !ValidLanguage(key) || key == und ||
            !ValidTag(mlocPackTag(key, value >> 11, value & 0x7FF), true) || value >> 31 != 0)
        {
            return false;
        }
    }
    return true;
}

static bool FindGeneral(const Likely* likely, uint64_t key, uint64_t* valueOut)
{
    uint32_t low = 0;
    uint32_t high = likely->generalCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint64_t at = mlocLoad64(likely->generalKeys + (size_t)middle * 8);
        if (at == key)
        {
            *valueOut = mlocLoad64(likely->generalValues + (size_t)middle * 8);
            return true;
        }
        if (at < key)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return false;
}

static bool FindLanguage(const Likely* likely, uint32_t language, uint64_t* valueOut)
{
    uint32_t low = 0;
    uint32_t high = likely->languageCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint32_t at = mlocLoad16(likely->languageKeys + (size_t)middle * 2);
        if (at == language)
        {
            uint32_t value = mlocLoad32(likely->languageValues + (size_t)middle * 4);
            *valueOut = mlocPackTag(language, value >> 11, value & 0x7FF);
            return true;
        }
        if (at < language)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return false;
}

static uint32_t FieldLength(const char* field, uint32_t size)
{
    uint32_t n = 0;
    while (n < size && field[n] != '\0')
    {
        n++;
    }
    return n;
}

static void UnpackLanguage(uint32_t packed, char* language)
{
    memset(language, 0, 8);
    for (int i = 0; i < 3; i++)
    {
        uint32_t letter = packed >> (5 * (2 - i)) & 31;
        language[i] = letter == 0 ? '\0' : (char)('a' + letter - 1);
    }
}

static void UnpackScript(uint32_t packed, char* script)
{
    for (int i = 0; i < 4; i++)
    {
        char base = i == 0 ? 'A' : 'a';
        script[i] = (char)(base + (packed >> (5 * (3 - i)) & 31) - 1);
    }
}

static void UnpackRegion(uint32_t packed, char* region)
{
    memset(region, 0, 4);
    if (packed >= 1024)
    {
        uint32_t number = packed - 1024;
        region[0] = (char)('0' + number / 100);
        region[1] = (char)('0' + number / 10 % 10);
        region[2] = (char)('0' + number % 10);
        return;
    }
    region[0] = (char)('A' + (packed >> 5) - 1);
    region[1] = (char)('A' + (packed & 31) - 1);
}

// The table's answer for a language, script and region, the latter two
// possibly 0, in UTS #35's order.
static bool Lookup(const Likely* likely, uint32_t language, uint32_t script, uint32_t region,
                   uint64_t* valueOut)
{
    bool und = language == mlocPackLanguage("und", 3);
    return (script != 0 && region != 0 &&
            FindGeneral(likely, mlocPackTag(language, script, region), valueOut)) ||
           (script != 0 && FindGeneral(likely, mlocPackTag(language, script, 0), valueOut)) ||
           (region != 0 && FindGeneral(likely, mlocPackTag(language, 0, region), valueOut)) ||
           (und ? FindGeneral(likely, mlocPackTag(language, 0, 0), valueOut)
                : FindLanguage(likely, language, valueOut));
}

// Maximizes with the table given; the locale may be the output.
static mlocResult Maximize(const Likely* likely, const mlocLocale* locale, mlocLocale* localeOut)
{
    mlocLocale result = *locale;
    if (memcmp(result.script, "Zzzz", 4) == 0)
    {
        memset(result.script, 0, 4);
    }
    if (memcmp(result.region, "ZZ\0\0", 4) == 0)
    {
        memset(result.region, 0, 4);
    }
    bool und = memcmp(result.language, "und\0\0\0\0\0", 8) == 0;
    uint32_t script = mlocPackScript(result.script, FieldLength(result.script, 4));
    uint32_t region = mlocPackRegion(result.region, FieldLength(result.region, 4));
    if (!und && script != 0 && region != 0)
    {
        *localeOut = result;
        return mloc_success;
    }
    uint32_t language = mlocPackLanguage(result.language, FieldLength(result.language, 8));
    uint64_t value = 0;
    if (language == 0 || !Lookup(likely, language, script, region, &value))
    {
        return mloc_errorNotFound;
    }
    if (und)
    {
        UnpackLanguage((uint32_t)(value >> 31), result.language);
    }
    if (script == 0)
    {
        UnpackScript((uint32_t)(value >> 11) & 0xFFFFF, result.script);
    }
    if (region == 0)
    {
        UnpackRegion((uint32_t)value & 0x7FF, result.region);
    }
    *localeOut = result;
    return mloc_success;
}

static bool OpenLikely(const mlocDataSet* data, Likely* likelyOut)
{
    DataSpan span;
    if (!mlocFindDataSetUnit(data, kindLocaleLikely, nullptr, 0, &span))
    {
        return false;
    }
    *likelyOut = ReadLikely(span);
    return true;
}

mlocResult mlocMaximizeLocale(const mlocDataSet* data, const mlocLocale* locale,
                              mlocLocale* localeOut)
{
    if (data == nullptr || locale == nullptr || localeOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    Likely likely;
    if (!OpenLikely(data, &likely))
    {
        return mloc_errorNotFound;
    }
    return Maximize(&likely, locale, localeOut);
}

// Whether two locales have the same language, script and region.
static bool SameIdentity(const mlocLocale* a, const mlocLocale* b)
{
    return memcmp(a->language, b->language, 8) == 0 && memcmp(a->script, b->script, 4) == 0 &&
           memcmp(a->region, b->region, 4) == 0;
}

mlocResult mlocMinimizeLocale(const mlocDataSet* data, const mlocLocale* locale, mlocFavor favor,
                              mlocLocale* localeOut)
{
    if (data == nullptr || locale == nullptr || localeOut == nullptr ||
        (favor != mloc_favorRegion && favor != mloc_favorScript))
    {
        return mloc_errorInvalid;
    }
    Likely likely;
    if (!OpenLikely(data, &likely))
    {
        return mloc_errorNotFound;
    }
    mlocLocale max;
    mlocResult result = Maximize(&likely, locale, &max);
    if (result != mloc_success)
    {
        return result;
    }
    // The trials: the language alone, then with the favored subtag, then
    // with the other.
    static const bool s_keepScript[2][3] = {{false, false, true}, {false, true, false}};
    for (int t = 0; t < 3; t++)
    {
        bool keepScript = s_keepScript[favor][t];
        bool keepRegion = t > 0 && !keepScript;
        mlocLocale trial = {0};
        memcpy(trial.language, max.language, 8);
        if (keepScript)
        {
            memcpy(trial.script, max.script, 4);
        }
        if (keepRegion)
        {
            memcpy(trial.region, max.region, 4);
        }
        mlocLocale back;
        if (Maximize(&likely, &trial, &back) == mloc_success && SameIdentity(&back, &max))
        {
            memcpy(trial.tail, max.tail, sizeof(trial.tail));
            *localeOut = trial;
            return mloc_success;
        }
    }
    *localeOut = max;
    return mloc_success;
}
