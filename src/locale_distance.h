// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Locale distances (record mloc-0017): the matching table, its
// validator, and the search for the nearest of a list of LSRs, ported
// from ICU's LocaleDistance.getBestIndexAndDistance. Distances are
// shifted by 3 bits; the low bits hold the micro distance of subtags
// one tag gave and the other did not.

#ifndef MLOC_LOCALE_DISTANCE_H
#define MLOC_LOCALE_DISTANCE_H

#include "data_view.h"
#include "locale_likely.h"
#include "locale_lsr.h"

#include "maul-locale/locale.h"

#include <stdint.h>

enum
{
    distanceShift = 3,
};

typedef struct MatchTable
{
    const uint8_t* paradigms;
    const uint8_t* scripts;
    const uint8_t* languages;
    const uint8_t* regions;
    const uint8_t* groups;
    const uint8_t* regionGroups;
    const uint8_t* text;
    uint32_t paradigmCount;
    uint32_t languageCount;
    uint32_t scriptCount;
    uint32_t regionCount;
    uint32_t groupCount;
    uint32_t textSize;
    uint32_t languageDistance;
    uint32_t scriptDistance;
    uint32_t regionDistance;
    uint32_t minRegionDistance;
} MatchTable;

bool mlocValidMatch(const DataView* view, DataSpan payload);

// The first source's matching table, or false.
bool mlocOpenMatchTable(const mlocDataSet* data, MatchTable* tableOut);

typedef struct Matcher
{
    const MatchTable* table;
    const Likely* likely;
    bool favorScript;
    bool bothWays;
} Matcher;

// A desired LSR with the locale it came from (for a private use tag)
// and the run of its language among the table's pairs.
typedef struct Wanted
{
    Lsr lsr;
    const mlocLocale* locale;
    uint32_t start;
    uint32_t end;
} Wanted;

void mlocWant(const Matcher* matcher, const Lsr* lsr, const mlocLocale* locale, Wanted* wantedOut);

// The nearest of count LSRs (stored one after another, as entries are)
// below a shifted threshold, ties broken as ICU breaks them; the locales
// are those the LSRs' indexes name. Returns its position, or -1, and its
// shifted distance.
int32_t mlocNearest(const Matcher* matcher, const Wanted* wanted, const void* lsrs, uint32_t count,
                    const mlocLocale* locales, uint32_t shiftedThreshold, uint32_t* shiftedOut);

// Whether two LSRs are equivalent: the same language, script and region.
bool mlocSameLsr(const Lsr* a, const mlocLocale* aLocale, const Lsr* b, const mlocLocale* bLocale);

// The demotion of each further desired locale: the distance of en-US and
// en-GB.
uint32_t mlocDefaultDemotion(const Matcher* matcher);

#endif
