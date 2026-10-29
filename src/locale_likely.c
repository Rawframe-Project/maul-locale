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
#include "invariant.h"
#include "kinds.h"
#include "locale_tables.h"

#include "maul-locale/locale.h"

#include <string.h>

enum
{
    likelyHeaderSize = 16,
};

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

// A packed tag whose parts are valid; script and region may be absent
// unless full.
static bool ValidTag(uint64_t tag, bool full)
{
    uint64_t language = tag >> 31;
    uint32_t script = (uint32_t)(tag >> 11) & 0xFFFFF;
    uint32_t region = (uint32_t)tag & 0x7FF;
    // Bits past the 46 of a tag land in the language, which refuses them.
    return language < 1u << 15 && mlocValidPackedLanguage((uint32_t)language) &&
           ((script == 0 && !full) || mlocValidPackedScript(script)) &&
           ((region == 0 && !full) || mlocValidPackedRegion(region));
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
            !mlocValidPackedLanguage(key) || key == und ||
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
    // The validator takes only full tags as answers, so whatever is
    // missing is filled.
    MLOC_ASSERT(((uint32_t)(value >> 11) & 0xFFFFF) != 0 && ((uint32_t)value & 0x7FF) != 0);
    if (und)
    {
        mlocUnpackLanguage((uint32_t)(value >> 31), result.language);
    }
    if (script == 0)
    {
        mlocUnpackScript((uint32_t)(value >> 11) & 0xFFFFF, result.script);
    }
    if (region == 0)
    {
        mlocUnpackRegion((uint32_t)value & 0x7FF, result.region);
    }
    *localeOut = result;
    return mloc_success;
}

// The index of the first general key at or above key.
static uint32_t LowerBound(const Likely* likely, uint64_t key)
{
    uint32_t low = 0;
    uint32_t high = likely->generalCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        if (mlocLoad64(likely->generalKeys + (size_t)middle * 8) < key)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

bool mlocFindLikely(const Likely* likely, uint32_t language, uint32_t script, uint32_t region,
                    uint64_t* valueOut)
{
    if (script == 0 && region == 0 && language != mlocPackLanguage("und", 3))
    {
        return FindLanguage(likely, language, valueOut);
    }
    return FindGeneral(likely, mlocPackTag(language, script, region), valueOut);
}

bool mlocLookupLikely(const Likely* likely, uint32_t language, uint32_t script, uint32_t region,
                      uint64_t* valueOut)
{
    script = script == mlocPackScript("Zzzz", 4) ? 0 : script;
    region = region == mlocPackRegion("ZZ", 2) ? 0 : region;
    return language != 0 && Lookup(likely, language, script, region, valueOut);
}

bool mlocHasLikely(const Likely* likely, uint32_t language, uint32_t script)
{
    uint64_t value = 0;
    if (script == 0 && FindLanguage(likely, language, &value))
    {
        return true;
    }
    // The keys of a language, or of a language and script, are a run.
    uint64_t start = script == 0 ? mlocPackTag(language, 0, 0) : mlocPackTag(language, script, 0);
    uint64_t end =
        script == 0 ? mlocPackTag(language + 1, 0, 0) : mlocPackTag(language, script + 1, 0);
    uint32_t at = LowerBound(likely, start);
    return at < likely->generalCount && mlocLoad64(likely->generalKeys + (size_t)at * 8) < end;
}

void mlocLikelyRegionRange(const Likely* likely, uint32_t language, uint32_t* startOut,
                           uint32_t* endOut)
{
    *startOut = LowerBound(likely, mlocPackTag(language, 0, 1));
    *endOut = LowerBound(likely, mlocPackTag(language, 1, 0));
    MLOC_ASSERT(*startOut <= *endOut);
}

uint64_t mlocLikelyValueAt(const Likely* likely, uint32_t index)
{
    MLOC_ASSERT(index < likely->generalCount);
    return mlocLoad64(likely->generalValues + (size_t)index * 8);
}

bool mlocOpenLikely(const mlocDataSet* data, Likely* likelyOut)
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
    if (!mlocOpenLikely(data, &likely))
    {
        return mloc_errorNotFound;
    }
    return Maximize(&likely, locale, localeOut);
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
    if (!mlocOpenLikely(data, &likely))
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
    // with the other. Each is maximized on the packed fields; a trial
    // never has both a script and a region, so it is always looked up.
    uint32_t language = mlocPackLanguage(max.language, FieldLength(max.language, 8));
    uint32_t script = mlocPackScript(max.script, FieldLength(max.script, 4));
    uint32_t region = mlocPackRegion(max.region, FieldLength(max.region, 4));
    static const bool s_keepScript[2][3] = {{false, false, true}, {false, true, false}};
    for (int t = 0; t < 3; t++)
    {
        bool keepScript = s_keepScript[favor][t];
        bool keepRegion = t > 0 && !keepScript;
        uint32_t trialScript = keepScript ? script : 0;
        uint32_t trialRegion = keepRegion ? region : 0;
        uint64_t value = 0;
        if (!mlocLookupLikely(&likely, language, trialScript, trialRegion, &value) ||
            (trialScript == 0 && ((uint32_t)(value >> 11) & 0xFFFFF) != script) ||
            (trialRegion == 0 && ((uint32_t)value & 0x7FF) != region))
        {
            continue;
        }
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
        memcpy(trial.tail, max.tail, sizeof(trial.tail));
        *localeOut = trial;
        return mloc_success;
    }
    *localeOut = max;
    return mloc_success;
}
