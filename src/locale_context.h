// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A locale's context transforms (CLDR's contextTransforms), shared by the
// library and the generator: a unit per locale key where the chain would
// give others, a byte per usage in the order below, each a set of the
// contexts in which the usage's first word is titlecased. A locale whose
// chain has no unit titlecases in none.

#ifndef MLOC_LOCALE_CONTEXT_H
#define MLOC_LOCALE_CONTEXT_H

#include "data_view.h"

#include "maul-locale/locale.h"

#include <stdint.h>

// The usages, the unit's bytes.
typedef enum ContextUsage
{
    // number-spellout: the words of spelled-out numbers.
    contextNumberSpellout = 0,
    contextUsageCount = 1,
} ContextUsage;

// The contexts of a usage's byte.
enum
{
    contextListOrMenu = 1,
    contextStandAlone = 2,
    contextAll = 3,
};

// Whether a context transforms unit has a byte per usage and no other
// bits.
bool mlocValidLocaleContext(const DataView* view, DataSpan payload);

// The contexts in which a usage titlecases for a locale.
uint8_t mlocGetLocaleContext(const mlocDataSet* data, const mlocLocale* locale, ContextUsage usage);

#endif
