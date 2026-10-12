// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// LSRs for language matching (locale_lsr.h). ICU looks likely subtags
// up in a trie of language, then script, then region, each falling back
// to "*", and a subtag is kept as given unless it was found (a macro
// region found is replaced). Its trie also holds, for each "und" and
// region entry, the same answer under that answer's script; the lookups
// below read those from the region entries, so the table stays the one
// of UTS #35.

#include "locale_lsr.h"

#include "bytes.h"
#include "locale_likely.h"
#include "locale_tables.h"

#include <string.h>

static uint32_t FieldLength(const char* field, uint32_t size)
{
    uint32_t n = 0;
    while (n < size && field[n] != '\0')
    {
        n++;
    }
    return n;
}

static uint32_t Und(void)
{
    return mlocPackLanguage("und", 3);
}

static uint32_t ScriptOf(uint64_t tag)
{
    return (uint32_t)(tag >> 11) & 0xFFFFF;
}

// The entry for a language, script and region, with "und"'s entries
// under a script made from its region ones (they win, as in ICU's trie).
static bool Find(const Likely* likely, uint32_t language, uint32_t script, uint32_t region,
                 uint64_t* valueOut)
{
    if (language == Und() && script != 0 && region != 0)
    {
        // An answer for any of und's regions counts under its own script
        // and region.
        uint64_t value = 0;
        uint32_t start = 0;
        uint32_t end = 0;
        mlocLikelyRegionRange(likely, language, &start, &end);
        for (uint32_t i = start; i < end; i++)
        {
            value = mlocLikelyValueAt(likely, i);
            if (ScriptOf(value) == script && (uint32_t)(value & 0x7FF) == region)
            {
                *valueOut = value;
                return true;
            }
        }
    }
    return mlocFindLikely(likely, language, script, region, valueOut);
}

static bool HasScript(const Likely* likely, uint32_t language, uint32_t script)
{
    if (mlocHasLikely(likely, language, script))
    {
        return true;
    }
    if (language != Und())
    {
        return false;
    }
    uint32_t start = 0;
    uint32_t end = 0;
    mlocLikelyRegionRange(likely, language, &start, &end);
    for (uint32_t i = start; i < end; i++)
    {
        if (ScriptOf(mlocLikelyValueAt(likely, i)) == script)
        {
            return true;
        }
    }
    return false;
}

// The given subtags, and what ICU's lookup keeps of them.
typedef struct Given
{
    uint32_t language;
    uint32_t script;
    uint32_t region;
    bool hasLanguage;
} Given;

// ICU's maximize: the language's entries, or "und"'s for one the table
// lacks; the script's within them, or "*"; then the region's, or "*",
// with its fallbacks. Fills the flags and the answer's tag.
static bool Lookup(const Likely* likely, const uint8_t* regionGroups, Given g, uint8_t* flagsOut,
                   uint64_t* valueOut)
{
    uint32_t und = Und();
    bool known = !g.hasLanguage || (g.language != 0 && mlocHasLikely(likely, g.language, 0));
    uint32_t language = known && g.hasLanguage ? g.language : und;
    bool scriptFound = g.script == 0 || HasScript(likely, language, g.script);
    uint32_t script = scriptFound ? g.script : 0;
    bool keepLanguage = g.hasLanguage;
    bool keepScript = g.script != 0;
    bool keepRegion = true;
    if (Find(likely, language, script, g.region, valueOut))
    {
        keepRegion = g.region != 0 && (regionGroups[g.region] & matchMacroRegion) == 0;
    }
    else if (!known && !scriptFound)
    {
        if (!Find(likely, und, 0, 0, valueOut))
        {
            return false;
        }
    }
    else if (!Find(likely, language, script, 0, valueOut))
    {
        keepRegion = g.region != 0;
        if (!Find(likely, und, 0, g.region, valueOut) && !Find(likely, und, 0, 0, valueOut))
        {
            return false;
        }
    }
    *flagsOut =
        (uint8_t)((keepLanguage ? lsrExplicitLanguage : 0) | (keepScript ? lsrExplicitScript : 0) |
                  (keepRegion ? lsrExplicitRegion : 0));
    return true;
}

// The length of a tail's subtag at an offset; the tail may fill its
// bytes without a NUL.
static size_t SubtagLength(const char* tail, size_t size, size_t at)
{
    size_t n = 0;
    while (at + n < size && tail[at + n] != '\0' && tail[at + n] != '-')
    {
        n++;
    }
    return n;
}

// Whether a tail begins with this variant and has no other: the next
// subtag, if any, is a singleton.
static bool OnlyVariant(const char* tail, size_t size, const char* variant)
{
    size_t length = SubtagLength(tail, size, 0);
    if (strlen(variant) != length || memcmp(tail, variant, length) != 0)
    {
        return false;
    }
    // A variant has at most 8 bytes, so the tail goes on past it.
    return tail[length] == '\0' || SubtagLength(tail, size, length + 1) == 1;
}

// A pseudo locale: region XA, XB or XC as given, or a single variant
// psaccent, psbidi or pscrack with the region given or the kind's.
static bool Pseudo(const mlocLocale* locale, Lsr* lsr)
{
    static const char s_regions[3][3] = {"XA", "XB", "XC"};
    static const char* const s_variants[3] = {"psaccent", "psbidi", "pscrack"};
    for (uint8_t k = 0; k < 3; k++)
    {
        if (memcmp(locale->region, s_regions[k], 3) == 0)
        {
            lsr->kind = (uint8_t)(lsrAccents + k);
            lsr->flags = lsrExplicit;
            return true;
        }
    }
    for (uint8_t k = 0; k < 3; k++)
    {
        if (OnlyVariant(locale->tail, sizeof(locale->tail), s_variants[k]))
        {
            lsr->kind = (uint8_t)(lsrAccents + k);
            lsr->flags = lsrExplicit;
            if (lsr->region == 0)
            {
                lsr->region = mlocPackRegion(s_regions[k], 2);
                lsr->flags = lsrExplicitLanguage | lsrExplicitScript;
            }
            return true;
        }
    }
    return false;
}

// ICU's own kinds of LSR, made without likely subtags: an exact "und", a
// private use tag, a pseudo locale.
static bool Special(const mlocLocale* locale, Lsr* lsr)
{
    bool bare =
        memcmp(locale->language, "und\0\0\0\0\0", 8) == 0 && lsr->script == 0 && lsr->region == 0;
    if (bare && (locale->tail[0] == '\0' || (locale->tail[0] == 'x' && locale->tail[1] == '-')))
    {
        lsr->kind = locale->tail[0] == '\0' ? lsrNormal : lsrPrivate;
        lsr->flags = lsrExplicit;
        return true;
    }
    return Pseudo(locale, lsr);
}

bool mlocMakeLsr(const Likely* likely, const uint8_t* regionGroups, const mlocLocale* locale,
                 uint32_t index, Lsr* lsrOut)
{
    Lsr lsr = {.index = index};
    memcpy(&lsr.language, locale->language, 8);
    lsr.script = mlocPackScript(locale->script, FieldLength(locale->script, 4));
    lsr.region = mlocPackRegion(locale->region, FieldLength(locale->region, 4));
    if (Special(locale, &lsr))
    {
        *lsrOut = lsr;
        return true;
    }
    bool und = memcmp(locale->language, "und\0\0\0\0\0", 8) == 0;
    Given g = {
        .language = und ? 0 : mlocPackLanguage(locale->language, FieldLength(locale->language, 8)),
        .script = lsr.script == mlocPackScript("Zzzz", 4) ? 0 : lsr.script,
        .region = lsr.region == mlocPackRegion("ZZ", 2) ? 0 : lsr.region,
        .hasLanguage = !und,
    };
    uint64_t value = 0;
    if (g.hasLanguage && g.script != 0 && g.region != 0)
    {
        lsr.flags = lsrExplicit;
    }
    else if (!Lookup(likely, regionGroups, g, &lsr.flags, &value))
    {
        return false;
    }
    if ((lsr.flags & lsrExplicitLanguage) == 0)
    {
        g.language = (uint32_t)(value >> 31);
        char language[8];
        mlocUnpackLanguage(g.language, language);
        memcpy(&lsr.language, language, 8);
    }
    lsr.packedLanguage = (uint16_t)g.language;
    lsr.script = (lsr.flags & lsrExplicitScript) != 0 ? g.script : ScriptOf(value);
    lsr.region = (lsr.flags & lsrExplicitRegion) != 0 ? g.region : (uint32_t)(value & 0x7FF);
    *lsrOut = lsr;
    return true;
}

bool mlocLikelyOfLanguage(const Likely* likely, const Lsr* lsr, uint32_t* scriptOut,
                          uint32_t* regionOut)
{
    uint64_t value = 0;
    if ((lsr->packedLanguage == 0 || !mlocFindLikely(likely, lsr->packedLanguage, 0, 0, &value)) &&
        !mlocFindLikely(likely, Und(), 0, 0, &value))
    {
        return false;
    }
    *scriptOut = ScriptOf(value);
    *regionOut = (uint32_t)(value & 0x7FF);
    return true;
}
