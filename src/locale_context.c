// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A locale's context transforms (src/locale_context.h).

#include "locale_context.h"

#include "invariant.h"
#include "kinds.h"
#include "locale_fallback.h"

bool mlocValidLocaleContext(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size != contextUsageCount)
    {
        return false;
    }
    for (uint32_t i = 0; i < payload.size; i++)
    {
        if ((payload.bytes[i] & ~contextAll) != 0)
        {
            return false;
        }
    }
    return true;
}

uint8_t mlocGetLocaleContext(const mlocDataSet* data, const mlocLocale* locale, ContextUsage usage)
{
    DataSpan unit;
    bool fellBack = false;
    DataView view;
    if (!mlocFindLocaleUnit(data, mloc_componentMain, kindLocaleContext, locale, &unit, &fellBack,
                            &view))
    {
        return 0;
    }
    // The validator took a byte for each usage.
    MLOC_ASSERT((uint32_t)usage < unit.size);
    return unit.bytes[usage];
}
