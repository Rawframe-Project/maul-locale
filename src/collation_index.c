// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_index.h"

#include "bytes.h"
#include "invariant.h"
#include "kinds.h"
#include "locale_fallback.h"

#include "maul-unicode/encoding.h"

#include <string.h>

// Whether a label sorts after another in code point order, which UTF-8's
// bytes keep.
static bool After(const uint8_t* text, uint32_t length, const uint8_t* before,
                  uint32_t beforeLength)
{
    uint32_t common = length < beforeLength ? length : beforeLength;
    int order = memcmp(text, before, common);
    return order > 0 || (order == 0 && length > beforeLength);
}

bool mlocValidIndexLabels(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < 4)
    {
        return false;
    }
    uint32_t count = mlocLoad32(payload.bytes);
    uint32_t at = 4;
    const uint8_t* before = nullptr;
    uint32_t beforeLength = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        if (at >= payload.size)
        {
            return false;
        }
        uint32_t length = payload.bytes[at];
        const uint8_t* text = payload.bytes + at + 1;
        if (length == 0 || length > payload.size - at - 1 ||
            muniValidateUtf8((const char*)text, length).status != muni_success ||
            (before != nullptr && !After(text, length, before, beforeLength)))
        {
            return false;
        }
        before = text;
        beforeLength = length;
        at += 1 + length;
    }
    return at == payload.size;
}

bool mlocFindIndexLabels(const mlocDataSet* data, const mlocLocale* locale, IndexLabels* out)
{
    DataSpan span = {0};
    bool fellBack = false;
    if (!mlocFindLocaleUnit(data, mloc_componentMain, kindCollationIndex, locale, &span, &fellBack,
                            nullptr))
    {
        return false;
    }
    *out = (IndexLabels){span.bytes, span.size, mlocLoad32(span.bytes)};
    return true;
}

bool mlocNextIndexLabel(const IndexLabels* labels, uint32_t* at, const uint8_t** textOut,
                        uint32_t* lengthOut)
{
    if (*at >= labels->size)
    {
        return false;
    }
    *lengthOut = labels->bytes[*at];
    // The validator required the labels to fill the unit exactly.
    MLOC_ASSERT(*lengthOut < labels->size - *at);
    *textOut = labels->bytes + *at + 1;
    *at += 1 + *lengthOut;
    return true;
}
