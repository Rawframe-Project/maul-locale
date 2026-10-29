// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Locale distances (locale_distance.h). The table's layout is in
// locale_tables.h. A language pair not in the table is "*": the default
// language distance (0 for equal languages) and the default script and
// region distances below it. A pair found leads to its script run (or
// straight to its region run when its script table was only the default
// one), each run falling back to its "*" entry; an entry without
// children ends the lookup with the defaults.

#include "locale_distance.h"

#include "bytes.h"
#include "invariant.h"
#include "kinds.h"
#include "locale_tables.h"

#include <string.h>

static bool ReadTable(DataSpan span, MatchTable* t)
{
    if (span.size < matchHeaderSize)
    {
        return false;
    }
    const uint8_t* b = span.bytes;
    *t = (MatchTable){
        .paradigmCount = mlocLoad16(b + 4),
        .languageCount = mlocLoad16(b + 6),
        .scriptCount = mlocLoad16(b + 8),
        .regionCount = mlocLoad16(b + 10),
        .groupCount = mlocLoad16(b + 12),
        .textSize = mlocLoad16(b + 14),
        .languageDistance = b[0],
        .scriptDistance = b[1],
        .regionDistance = b[2],
        .minRegionDistance = b[3],
    };
    size_t at = matchHeaderSize;
    t->paradigms = b + at;
    at += (size_t)t->paradigmCount * 8;
    t->scripts = b + at;
    at += (size_t)t->scriptCount * matchScriptSize;
    t->languages = b + at;
    at += (size_t)t->languageCount * matchLanguageSize;
    t->regions = b + at;
    at += (size_t)t->regionCount * matchRegionSize;
    t->groups = b + at;
    at += (size_t)t->groupCount * matchGroupSize;
    t->regionGroups = b + at;
    at += matchRegionSlots;
    t->text = b + at;
    return at + t->textSize == span.size;
}

static uint32_t RegionKey(const MatchTable* t, uint32_t i)
{
    const uint8_t* e = t->regions + (size_t)i * matchRegionSize;
    return (uint32_t)e[0] << 8 | e[1];
}

// A run of region pairs: sorted, unique, "*" first, in bounds.
static bool ValidRegionRun(const MatchTable* t, uint32_t start, uint32_t count)
{
    if ((uint64_t)start + count > t->regionCount)
    {
        return false;
    }
    for (uint32_t i = start; i < start + count; i++)
    {
        if (i == start ? RegionKey(t, i) != 0 : RegionKey(t, i) <= RegionKey(t, i - 1))
        {
            return false;
        }
    }
    return true;
}

static bool ValidEntries(const MatchTable* t)
{
    for (uint32_t i = 0; i < t->regionCount; i++)
    {
        const uint8_t* e = t->regions + (size_t)i * matchRegionSize;
        bool star = e[0] == 0 && e[1] == 0;
        if ((!star && (e[0] < 0x21 || e[0] > 0x7E || e[1] < 0x21 || e[1] > 0x7E)) || e[2] > 100 ||
            e[3] != 0)
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < t->scriptCount; i++)
    {
        const uint8_t* e = t->scripts + (size_t)i * matchScriptSize;
        uint64_t key = mlocLoad64(e);
        bool pair = key != 0;
        if ((pair && (key >> 40 != 0 || !mlocValidPackedScript((uint32_t)(key >> 20)) ||
                      !mlocValidPackedScript((uint32_t)key & 0xFFFFF))) ||
            e[12] > 100 || e[13] != 0 || mlocLoad16(e + 14) != 0 ||
            !ValidRegionRun(t, mlocLoad16(e + 8), mlocLoad16(e + 10)))
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < t->groupCount; i++)
    {
        const uint8_t* e = t->groups + (size_t)i * matchGroupSize;
        if ((uint32_t)mlocLoad16(e) + mlocLoad16(e + 2) > t->textSize)
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < t->textSize; i++)
    {
        if (t->text[i] < 0x21 || t->text[i] > 0x7E)
        {
            return false;
        }
    }
    for (uint32_t slot = 0; slot < matchRegionSlots; slot++)
    {
        if ((t->regionGroups[slot] & ~matchMacroRegion) >= t->groupCount)
        {
            return false;
        }
    }
    return true;
}

// The language pairs: sorted, unique, valid, each leading to a valid
// run of script pairs (sorted, unique, "*" first) or of region pairs.
static bool ValidLanguages(const MatchTable* t)
{
    for (uint32_t i = 0; i < t->languageCount; i++)
    {
        const uint8_t* e = t->languages + (size_t)i * matchLanguageSize;
        uint32_t key = mlocLoad32(e);
        uint32_t start = mlocLoad16(e + 4);
        uint32_t count = mlocLoad16(e + 6);
        if ((i > 0 && key <= mlocLoad32(e - matchLanguageSize)) || key >> 30 != 0 ||
            !mlocValidPackedLanguage(key >> 15) || !mlocValidPackedLanguage(key & 0x7FFF) ||
            e[8] > 100 || e[9] > matchSkipScript || mlocLoad16(e + 10) != 0)
        {
            return false;
        }
        if (e[9] == matchSkipScript)
        {
            if (count == 0 || !ValidRegionRun(t, start, count))
            {
                return false;
            }
            continue;
        }
        if ((uint64_t)start + count > t->scriptCount)
        {
            return false;
        }
        for (uint32_t s = start; s < start + count; s++)
        {
            uint64_t k = mlocLoad64(t->scripts + (size_t)s * matchScriptSize);
            if ((s == start && k != 0) ||
                (s > start && k <= mlocLoad64(t->scripts + (size_t)(s - 1) * matchScriptSize)))
            {
                return false;
            }
        }
    }
    return true;
}

bool mlocValidMatch(const DataView* view, DataSpan payload)
{
    (void)view;
    MatchTable t;
    if (!ReadTable(payload, &t) || t.languageDistance > 100 || t.scriptDistance > 100 ||
        t.regionDistance > 100 || t.minRegionDistance > 100 ||
        t.paradigmCount > matchMaxParadigms || t.groupCount == 0)
    {
        return false;
    }
    for (uint32_t i = 0; i < t.paradigmCount; i++)
    {
        uint64_t tag = mlocLoad64(t.paradigms + (size_t)i * 8);
        if (tag >> 46 != 0 || !mlocValidPackedLanguage((uint32_t)(tag >> 31)) ||
            !mlocValidPackedScript((uint32_t)(tag >> 11) & 0xFFFFF) ||
            !mlocValidPackedRegion((uint32_t)tag & 0x7FF))
        {
            return false;
        }
    }
    return ValidEntries(&t) && ValidLanguages(&t);
}

bool mlocOpenMatchTable(const mlocDataSet* data, MatchTable* tableOut)
{
    DataSpan span;
    return mlocFindDataSetUnit(data, kindLocaleMatch, nullptr, 0, &span) &&
           ReadTable(span, tableOut);
}

// The first language pair at or above a key.
static uint32_t LanguageBound(const MatchTable* t, uint32_t key)
{
    uint32_t low = 0;
    uint32_t high = t->languageCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        if (mlocLoad32(t->languages + (size_t)middle * matchLanguageSize) < key)
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

void mlocWant(const Matcher* matcher, const Lsr* lsr, const mlocLocale* locale, Wanted* wantedOut)
{
    *wantedOut = (Wanted){.lsr = *lsr, .locale = locale};
    if (lsr->packedLanguage != 0)
    {
        wantedOut->start = LanguageBound(matcher->table, (uint32_t)lsr->packedLanguage << 15);
        wantedOut->end = LanguageBound(matcher->table, (uint32_t)(lsr->packedLanguage + 1) << 15);
    }
}

static bool SameLanguage(const Lsr* a, const mlocLocale* aLocale, const Lsr* b,
                         const mlocLocale* bLocale)
{
    if (a->kind != b->kind)
    {
        return false;
    }
    return a->kind == lsrPrivate ? memcmp(aLocale->tail, bLocale->tail, sizeof(aLocale->tail)) == 0
                                 : a->language == b->language;
}

// Pseudo locales prefix their script; a private use tag has none.
static uint32_t ScriptKind(const Lsr* lsr)
{
    return lsr->kind == lsrPrivate ? lsrNormal : lsr->kind;
}

static bool SameScript(const Lsr* a, const Lsr* b)
{
    return ScriptKind(a) == ScriptKind(b) && a->script == b->script;
}

bool mlocSameLsr(const Lsr* a, const mlocLocale* aLocale, const Lsr* b, const mlocLocale* bLocale)
{
    return SameLanguage(a, aLocale, b, bLocale) && SameScript(a, b) && a->region == b->region;
}

// Where a lookup goes on: a run of script or region pairs, or none.
typedef struct Next
{
    uint32_t start;
    uint32_t count;
    bool regions;
} Next;

static const uint8_t* FindLanguagePair(const MatchTable* t, const Wanted* w, uint32_t supported)
{
    uint32_t key = (uint32_t)w->lsr.packedLanguage << 15 | supported;
    uint32_t low = w->start;
    uint32_t high = w->end;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint32_t at = mlocLoad32(t->languages + (size_t)middle * matchLanguageSize);
        if (at == key)
        {
            return t->languages + (size_t)middle * matchLanguageSize;
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
    return nullptr;
}

// The script pair of a run, else its "*" entry, which is first.
static const uint8_t* FindScriptPair(const MatchTable* t, Next run, uint64_t key)
{
    // Distance asks only a run that has its "*" entry first.
    MLOC_ASSERT(run.count > 0);
    const uint8_t* entries = t->scripts + (size_t)run.start * matchScriptSize;
    uint32_t low = key == 0 ? 0 : 1;
    uint32_t high = key == 0 ? 0 : run.count;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint64_t at = mlocLoad64(entries + (size_t)middle * matchScriptSize);
        if (at == key)
        {
            return entries + (size_t)middle * matchScriptSize;
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
    return entries;
}

static uint32_t RegionPair(const MatchTable* t, Next run, uint8_t desired, uint8_t supported)
{
    MLOC_ASSERT(run.count > 0);
    const uint8_t* entries = t->regions + (size_t)run.start * matchRegionSize;
    for (uint32_t i = 1; i < run.count; i++)
    {
        const uint8_t* e = entries + (size_t)i * matchRegionSize;
        if (e[0] == desired && e[1] == supported)
        {
            return e[2];
        }
    }
    return entries[2];
}

static void GroupOf(const MatchTable* t, uint32_t region, const uint8_t** textOut,
                    uint32_t* lengthOut)
{
    MLOC_ASSERT(region < matchRegionSlots);
    const uint8_t* g =
        t->groups + (size_t)(t->regionGroups[region] & ~matchMacroRegion) * matchGroupSize;
    *textOut = t->text + mlocLoad16(g);
    *lengthOut = mlocLoad16(g + 2);
}

// The region distance: the largest over the pairs of the two regions'
// partitions, each pair's own or the run's "*" one.
static uint32_t PartitionDistance(const MatchTable* t, Next run, uint32_t desired,
                                  uint32_t supported, uint32_t limit)
{
    const uint8_t* dText = nullptr;
    const uint8_t* sText = nullptr;
    uint32_t dLength = 0;
    uint32_t sLength = 0;
    GroupOf(t, desired, &dText, &dLength);
    GroupOf(t, supported, &sText, &sLength);
    uint32_t largest = 0;
    for (uint32_t d = 0; d < dLength; d++)
    {
        for (uint32_t s = 0; s < sLength; s++)
        {
            uint32_t distance = RegionPair(t, run, dText[d], sText[s]);
            if (distance > limit)
            {
                return distance;
            }
            largest = distance > largest ? distance : largest;
        }
    }
    return largest;
}

// The language distance of a pair and where the lookup goes on; star
// when the table has no such pair.
static uint32_t LanguageDistance(const Matcher* m, const Wanted* w, const Lsr* s,
                                 const mlocLocale* sLocale, Next* next, bool* starOut)
{
    const MatchTable* t = m->table;
    const uint8_t* pair = w->end > w->start && s->packedLanguage != 0
                              ? FindLanguagePair(t, w, s->packedLanguage)
                              : nullptr;
    *starOut = pair == nullptr;
    if (pair == nullptr)
    {
        *next = (Next){0};
        return SameLanguage(&w->lsr, w->locale, s, sLocale) ? 0 : t->languageDistance;
    }
    *next = (Next){.start = mlocLoad16(pair + 4),
                   .count = mlocLoad16(pair + 6),
                   .regions = pair[9] == matchSkipScript};
    return pair[8];
}

// The distance of a desired and a supported LSR, or one past limit as
// soon as it passes it.
static uint32_t Distance(const Matcher* m, const Wanted* w, const Lsr* s, const mlocLocale* sLocale,
                         uint32_t limit)
{
    const MatchTable* t = m->table;
    Next next;
    bool star = false;
    uint32_t distance = LanguageDistance(m, w, s, sLocale, &next, &star);
    if (m->favorScript)
    {
        distance >>= 2;
    }
    if (distance > limit)
    {
        return limit + 1;
    }
    if (star || next.regions || next.count == 0)
    {
        distance += SameScript(&w->lsr, s) ? 0 : t->scriptDistance;
    }
    else
    {
        bool pair = ScriptKind(&w->lsr) == lsrNormal && ScriptKind(s) == lsrNormal &&
                    w->lsr.script != 0 && s->script != 0;
        uint64_t key = pair ? (uint64_t)w->lsr.script << 20 | s->script : 0;
        const uint8_t* entry = FindScriptPair(t, next, key);
        bool found = key != 0 && mlocLoad64(entry) == key;
        distance += !found && SameScript(&w->lsr, s) ? 0 : entry[12];
        next = (Next){
            .start = mlocLoad16(entry + 8), .count = mlocLoad16(entry + 10), .regions = true};
    }
    if (distance > limit)
    {
        return limit + 1;
    }
    if (w->lsr.region == s->region)
    {
        return distance;
    }
    if (star || next.count == 0)
    {
        return distance + t->regionDistance;
    }
    if (t->minRegionDistance > limit - distance)
    {
        return limit + 1;
    }
    return distance + PartitionDistance(t, next, w->lsr.region, s->region, limit - distance);
}

// ICU's isMatch: whether the pair also matches the other way.
static bool Reverse(const Matcher* m, const Wanted* w, const Lsr* s, const mlocLocale* sLocale,
                    uint32_t shiftedThreshold)
{
    Matcher oneWay = *m;
    oneWay.bothWays = false;
    Wanted reversed;
    mlocWant(&oneWay, s, sLocale, &reversed);
    // The list is the desired LSR alone, its locale the only one.
    Lsr alone = w->lsr;
    alone.index = 0;
    uint32_t shifted = 0;
    return mlocNearest(&oneWay, &reversed, &alone, 1, w->locale, shiftedThreshold, &shifted) >= 0;
}

// ICU's compareLikely: whether a candidate as near as the best has its
// language's likely script (when the scripts differ) or region (when
// only the regions do).
static bool MoreLikely(const Matcher* m, const Lsr* candidate, const mlocLocale* cLocale,
                       const Lsr* best, const mlocLocale* bLocale)
{
    uint32_t script = 0;
    uint32_t region = 0;
    if (!SameLanguage(candidate, cLocale, best, bLocale) ||
        !mlocLikelyOfLanguage(m->likely, candidate, &script, &region))
    {
        return false;
    }
    if (!SameScript(candidate, best))
    {
        return ScriptKind(candidate) == lsrNormal && candidate->script == script;
    }
    return candidate->region != best->region && candidate->region == region;
}

static Lsr LsrAt(const void* lsrs, uint32_t i)
{
    Lsr lsr;
    memcpy(&lsr, (const uint8_t*)lsrs + (size_t)i * sizeof(Lsr), sizeof(Lsr));
    return lsr;
}

// Whether an entry read from the caller's list holds: only its index
// and region reach into arrays.
static bool ValidLsr(const Lsr* s, uint32_t count)
{
    return s->index < count && s->kind <= lsrPrivate && s->flags <= lsrExplicit &&
           s->region < matchRegionSlots;
}

// Whether a supported LSR is surely past the limit: most pairs are of
// two languages the table has no pair for, at the default language
// distance, which most limits pass.
static bool SurelyFar(const Matcher* matcher, const Wanted* wanted, const Lsr* s, uint32_t limit)
{
    bool star = wanted->end == wanted->start || s->packedLanguage == 0;
    bool other = s->kind != wanted->lsr.kind ||
                 (s->kind != lsrPrivate && s->language != wanted->lsr.language);
    uint32_t least = matcher->table->languageDistance >> (matcher->favorScript ? 2 : 0);
    return star && other && least > limit;
}

int32_t mlocNearest(const Matcher* matcher, const Wanted* wanted, const void* lsrs, uint32_t count,
                    const mlocLocale* locales, uint32_t shiftedThreshold, uint32_t* shiftedOut)
{
    int32_t best = -1;
    for (uint32_t i = 0; i < count; i++)
    {
        Lsr s = LsrAt(lsrs, i);
        if (!ValidLsr(&s, count))
        {
            return mlocNearestInvalid;
        }
        const mlocLocale* sLocale = locales + s.index;
        uint32_t limit = (shiftedThreshold + (1u << distanceShift) - 1) >> distanceShift;
        if (SurelyFar(matcher, wanted, &s, limit))
        {
            continue;
        }
        uint32_t distance = Distance(matcher, wanted, &s, sLocale, limit);
        if (distance > limit)
        {
            continue;
        }
        uint32_t shifted = distance << distanceShift;
        if (shifted == 0)
        {
            shifted = (uint32_t)(wanted->lsr.flags ^ s.flags);
        }
        bool better = shifted < shiftedThreshold;
        bool tie = !better && shifted == shiftedThreshold && best >= 0 && distance != 0;
        if ((!better && !tie) ||
            (matcher->bothWays && !Reverse(matcher, wanted, &s, sLocale, shiftedThreshold)))
        {
            continue;
        }
        if (better)
        {
            best = (int32_t)i;
            shiftedThreshold = shifted;
            if (shifted == 0)
            {
                break;
            }
            continue;
        }
        Lsr b = LsrAt(lsrs, (uint32_t)best);
        if (MoreLikely(matcher, &s, sLocale, &b, locales + b.index))
        {
            best = (int32_t)i;
        }
    }
    *shiftedOut = shiftedThreshold;
    return best;
}

uint32_t mlocDefaultDemotion(const Matcher* matcher)
{
    Matcher plain = {.table = matcher->table, .likely = matcher->likely};
    Lsr en = {.script = mlocPackScript("Latn", 4),
              .region = mlocPackRegion("US", 2),
              .packedLanguage = (uint16_t)mlocPackLanguage("en", 2),
              .flags = lsrExplicit};
    memcpy(&en.language, "en\0\0\0\0\0\0", 8);
    Lsr gb = en;
    gb.region = mlocPackRegion("GB", 2);
    static const mlocLocale s_none = {0};
    Wanted wanted;
    mlocWant(&plain, &en, &s_none, &wanted);
    // ICU's threshold for it, and its distance above any.
    uint32_t limit = 50;
    uint32_t distance = Distance(&plain, &wanted, &gb, &s_none, limit);
    return distance > limit ? 100 : distance;
}
