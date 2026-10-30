// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Language matching: ICU's LocaleMatcher over the LSRs of locale_lsr.h and
// the distances of locale_distance.h. An entry is an LSR, stored as its
// bytes.

#include "bytes.h"
#include "locale_distance.h"
#include "locale_likely.h"
#include "locale_lsr.h"
#include "locale_tables.h"

#include "maul-locale/locale.h"

#include <string.h>

static_assert(sizeof(Lsr) == sizeof(mlocMatchEntry), "an entry holds an LSR");

// The tables, and that the likely subtags answer every locale: they
// fall back to "und", so a table that has it never fails one.
static bool Open(const mlocDataSet* data, Likely* likely, MatchTable* table)
{
    uint64_t und = 0;
    return mlocOpenLikely(data, likely) && mlocOpenMatchTable(data, table) &&
           mlocFindLikely(likely, mlocPackLanguage("und", 3), 0, 0, &und);
}

static Lsr EntryAt(const mlocMatchEntry* entries, size_t i)
{
    Lsr lsr;
    memcpy(&lsr, &entries[i], sizeof(lsr));
    return lsr;
}

static bool IsParadigm(const MatchTable* table, const Lsr* lsr)
{
    if (lsr->kind != lsrNormal || lsr->packedLanguage == 0)
    {
        return false;
    }
    uint64_t tag = mlocPackTag(lsr->packedLanguage, lsr->script, lsr->region);
    for (uint32_t i = 0; i < table->paradigmCount; i++)
    {
        if (mlocLoad64(table->paradigms + (size_t)i * 8) == tag)
        {
            return true;
        }
    }
    return false;
}

// The order an entry is tried in: the default's equivalents, the
// paradigm locales, the rest.
static int Rank(const MatchTable* table, const Lsr* lsr, const mlocLocale* supported,
                const Lsr* defaultLsr, const mlocLocale* defaultLocale)
{
    if (defaultLsr != nullptr &&
        mlocSameLsr(lsr, supported + lsr->index, defaultLsr, defaultLocale))
    {
        return 0;
    }
    return IsParadigm(table, lsr) ? 1 : 2;
}

// Moves the entries of a rank, in their order, to start at `at`;
// returns where the next rank starts.
static size_t Gather(const MatchTable* table, mlocMatchEntry* entries, size_t count, size_t at,
                     int rank, const mlocLocale* supported, const Lsr* defaultLsr,
                     const mlocLocale* defaultLocale)
{
    for (size_t i = at; i < count; i++)
    {
        Lsr lsr = EntryAt(entries, i);
        if (Rank(table, &lsr, supported, defaultLsr, defaultLocale) != rank)
        {
            continue;
        }
        mlocMatchEntry moved = entries[i];
        memmove(&entries[at + 1], &entries[at], (i - at) * sizeof(mlocMatchEntry));
        entries[at++] = moved;
    }
    return at;
}

mlocResult mlocPrepareMatch(const mlocDataSet* data, const mlocLocale* supported, size_t count,
                            const mlocLocale* defaultLocale, mlocMatchEntry* entriesOut)
{
    if (data == nullptr || (count > 0 && (supported == nullptr || entriesOut == nullptr)))
    {
        return mloc_errorInvalid;
    }
    if (count > MLOC_MAX_MATCH_LOCALES)
    {
        return mloc_errorLimit;
    }
    Likely likely;
    MatchTable table;
    if (!Open(data, &likely, &table))
    {
        return mloc_errorNotFound;
    }
    // mlocMakeLsr answers every locale when the table has und's entry,
    // which the validator requires; without it the data has no likely
    // subtags to match by.
    Lsr defaultLsr;
    if (defaultLocale != nullptr &&
        !mlocMakeLsr(&likely, table.regionGroups, defaultLocale, 0, &defaultLsr))
    {
        return mloc_errorNotFound;
    }
    for (size_t i = 0; i < count; i++)
    {
        Lsr lsr;
        if (!mlocMakeLsr(&likely, table.regionGroups, &supported[i], (uint32_t)i, &lsr))
        {
            return mloc_errorNotFound;
        }
        memcpy(&entriesOut[i], &lsr, sizeof(lsr));
    }
    const Lsr* def = defaultLocale != nullptr ? &defaultLsr : nullptr;
    size_t at = Gather(&table, entriesOut, count, 0, 0, supported, def, defaultLocale);
    (void)Gather(&table, entriesOut, count, at, 1, supported, def, defaultLocale);
    return mloc_success;
}

static bool ValidArguments(const mlocDataSet* data, const mlocLocale* supported,
                           const mlocMatchEntry* entries, size_t count, const mlocLocale* desired,
                           size_t desiredCount, const mlocMatchOptions* options,
                           const mlocMatch* matchOut)
{
    if (data == nullptr || matchOut == nullptr ||
        (count > 0 && (supported == nullptr || entries == nullptr)) ||
        (desiredCount > 0 && desired == nullptr))
    {
        return false;
    }
    if (options != nullptr &&
        ((options->flags & ~(mlocMatchFlags)(mloc_matchFavorScript | mloc_matchNoDemotion |
                                             mloc_matchBothWays)) != 0 ||
         options->threshold > 100))
    {
        return false;
    }
    return true;
}

mlocResult mlocMatchLocales(const mlocDataSet* data, const mlocLocale* supported,
                            const mlocMatchEntry* entries, size_t count, const mlocLocale* desired,
                            size_t desiredCount, const mlocMatchOptions* options,
                            mlocMatch* matchOut)
{
    if (!ValidArguments(data, supported, entries, count, desired, desiredCount, options, matchOut))
    {
        return mloc_errorInvalid;
    }
    if (count > MLOC_MAX_MATCH_LOCALES || desiredCount > MLOC_MAX_MATCH_LOCALES)
    {
        return mloc_errorLimit;
    }
    Likely likely;
    MatchTable table;
    if (!Open(data, &likely, &table))
    {
        return mloc_errorNotFound;
    }
    mlocMatchFlags flags = options != nullptr ? options->flags : 0;
    Matcher matcher = {.table = &table,
                       .likely = &likely,
                       .favorScript = (flags & mloc_matchFavorScript) != 0,
                       .bothWays = (flags & mloc_matchBothWays) != 0};
    uint32_t threshold =
        options != nullptr && options->threshold != 0 ? options->threshold : table.scriptDistance;
    uint32_t demotion = (flags & mloc_matchNoDemotion) != 0 ? 0 : mlocDefaultDemotion(&matcher);
    uint32_t best = threshold << distanceShift;
    mlocMatch match = {.supported = -1, .desired = -1};
    for (size_t i = 0; i < desiredCount; i++)
    {
        Lsr lsr;
        if (!mlocMakeLsr(&likely, table.regionGroups, &desired[i], (uint32_t)i, &lsr))
        {
            return mloc_errorNotFound;
        }
        Wanted wanted;
        mlocWant(&matcher, &lsr, &desired[i], &wanted);
        uint32_t shifted = 0;
        int32_t at =
            mlocNearest(&matcher, &wanted, entries, (uint32_t)count, supported, best, &shifted);
        if (at == mlocNearestInvalid)
        {
            return mloc_errorInvalid;
        }
        if (at >= 0)
        {
            best = shifted;
            match = (mlocMatch){.supported = (int32_t)EntryAt(entries, (size_t)at).index,
                                .desired = (int32_t)i};
        }
        // Each further desired locale must be nearer by the demotion.
        if (best <= demotion << distanceShift)
        {
            break;
        }
        best -= demotion << distanceShift;
    }
    *matchOut = match;
    return mloc_success;
}

// A tail's variants and its extensions, each extension from its
// singleton to the next (private use to the end).
typedef struct Parts
{
    const char* variants;
    size_t variantsLength;
    const char* extensions[36];
    size_t lengths[36];
} Parts;

static int SingletonSlot(char c)
{
    return c >= 'a' && c <= 'z' ? c - 'a' : c >= '0' && c <= '9' ? 26 + (c - '0') : -1;
}

static size_t FieldLength(const char* field, size_t size)
{
    size_t n = 0;
    while (n < size && field[n] != '\0')
    {
        n++;
    }
    return n;
}

// False for a singleton no locale has.
static bool Split(const char* tail, size_t size, Parts* parts)
{
    *parts = (Parts){.variants = tail};
    size_t length = FieldLength(tail, size);
    size_t at = 0;
    int current = -1;
    while (at < length)
    {
        size_t subtag = 0;
        while (at + subtag < length && tail[at + subtag] != '-')
        {
            subtag++;
        }
        if (subtag == 1 && current != SingletonSlot('x'))
        {
            current = SingletonSlot(tail[at]);
            if (current < 0)
            {
                return false;
            }
            parts->extensions[current] = tail + at;
        }
        if (current < 0)
        {
            parts->variantsLength = at + subtag;
        }
        else
        {
            parts->lengths[current] = (size_t)(tail + at + subtag - parts->extensions[current]);
        }
        at += subtag + 1;
    }
    return true;
}

// Appends a subtag run after a "-"; the text holds a language, script,
// region and two full tails with room to spare.
static void Append(char* text, size_t* length, const char* part, size_t size)
{
    if (size == 0)
    {
        return;
    }
    if (*length > 0)
    {
        text[(*length)++] = '-';
    }
    memcpy(text + *length, part, size);
    *length += size;
}

mlocResult mlocCombineMatch(const mlocLocale* desired, const mlocLocale* supported,
                            mlocLocale* localeOut)
{
    if (desired == nullptr || supported == nullptr || localeOut == nullptr ||
        mlocLocalesOverlap(desired, localeOut) || mlocLocalesOverlap(supported, localeOut))
    {
        return mloc_errorInvalid;
    }
    Parts d;
    Parts s;
    if (!Split(desired->tail, sizeof(desired->tail), &d) ||
        !Split(supported->tail, sizeof(supported->tail), &s))
    {
        return mloc_errorInvalid;
    }
    const Parts* variants = d.variantsLength > 0 ? &d : &s;
    const char* region = desired->region[0] != '\0' ? desired->region : supported->region;
    char text[512] = {0};
    static_assert(sizeof(text) > 8 + 4 + 4 + 3 * sizeof(desired->tail) + 40, "the text holds all");
    size_t length = 0;
    Append(text, &length, supported->language, FieldLength(supported->language, 8));
    Append(text, &length, supported->script, FieldLength(supported->script, 4));
    Append(text, &length, region, FieldLength(region, 4));
    Append(text, &length, variants->variants, variants->variantsLength);
    for (int i = 0; i < 36; i++)
    {
        const Parts* from = d.extensions[i] != nullptr ? &d : &s;
        if (from->extensions[i] != nullptr)
        {
            Append(text, &length, from->extensions[i], from->lengths[i]);
        }
    }
    mlocLocale combined;
    mlocResult parsed = mlocParseLocale(text, length, &combined).status;
    if (parsed != mloc_success)
    {
        // Only a locale mlocParseLocale did not make has text it refuses.
        return parsed == mloc_errorLimit ? mloc_errorLimit : mloc_errorInvalid;
    }
    *localeOut = combined;
    return mloc_success;
}
