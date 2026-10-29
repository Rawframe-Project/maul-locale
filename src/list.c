// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The list formatter (maul-locale/list.h): a locale's patterns of a style
// kept in the formatter, items joined by list_join.

#include "maul-locale/list.h"

#include "list_data.h"
#include "list_join.h"
#include "number_core.h"

#include <string.h>

enum
{
    // Marks a list formatter the library made.
    listStamp = 0x4C,
    // The part kinds items are written with: two, alternating, so that
    // items a pattern puts side by side ("{0}{1}") stay two parts.
    listKindItem = 0x400,
};

static_assert((int)listOrShort == mloc_listOr * 3 + mloc_listShort &&
                  (int)listUnitNarrow == mloc_listUnit * 3 + mloc_listNarrow,
              "a style is its type's three widths in order");

typedef struct ListFormat
{
    ListPatterns patterns;
    uint8_t stamp;
} ListFormat;

static_assert(sizeof(ListFormat) <= sizeof(mlocListFormatter), "a list formatter fits its value");

mlocResult mlocInitListFormatter(const mlocDataSet* data, const mlocLocale* locale,
                                 const mlocListOptions* options, mlocListFormatter* formatterOut)
{
    mlocListOptions o = options != nullptr ? *options : (mlocListOptions){0};
    if (formatterOut == nullptr || data == nullptr || locale == nullptr ||
        (uint32_t)o.type > mloc_listUnit || (uint32_t)o.width > mloc_listNarrow)
    {
        return mloc_errorInvalid;
    }
    ListFormat f;
    memset(&f, 0, sizeof(f));
    if (!mlocFindListPatterns(data, locale, (ListStyle)(o.type * 3 + o.width), &f.patterns))
    {
        return mloc_errorNotFound;
    }
    f.stamp = listStamp;
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return mloc_success;
}

// A formatter the library made, copied out; false for any other.
static bool ReadFormatter(const mlocListFormatter* opaque, ListFormat* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    bool texts = true;
    for (uint32_t i = 0; i < listPatternCount; i++)
    {
        texts = texts && f->patterns.text[i] != nullptr;
    }
    return f->stamp == listStamp && texts && f->patterns.rule <= listRuleHebrew;
}

static void WriteItem(void* context, uint32_t index, NumberWriter* writer)
{
    const mlocListItem* items = context;
    mlocWritePart(writer, listKindItem + (index & 1), (const uint8_t*)items[index].text,
                  items[index].length);
}

// Reports a part to the caller's output by its list kind.
static void ReportPart(void* context, uint32_t kind, size_t start, size_t length)
{
    const mlocOutput* output = context;
    output->part(output->context,
                 kind == mloc_numberPartLiteral ? mloc_listPartLiteral : mloc_listPartElement,
                 start, length);
}

mlocResult mlocListFormatter_Format(const mlocListFormatter* formatter, const mlocListItem* items,
                                    size_t count, mlocOutput* output)
{
    ListFormat f;
    if (!ReadFormatter(formatter, &f) || output == nullptr ||
        (output->buffer == nullptr && output->capacity != 0) || (items == nullptr && count > 0))
    {
        return mloc_errorInvalid;
    }
    if (count > MLOC_MAX_LIST_ITEMS)
    {
        return mloc_errorLimit;
    }
    for (size_t i = 0; i < count; i++)
    {
        if (items[i].text == nullptr && items[i].length > 0)
        {
            return mloc_errorInvalid;
        }
    }
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    if (count > 0)
    {
        mlocWriteList(&f.patterns, items, (uint32_t)count, WriteItem, (void*)items, &w);
    }
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr && count > 0)
    {
        mlocOutput report = {.part = ReportPart, .context = output};
        NumberWriter parts = {.parts = &report};
        mlocWriteList(&f.patterns, items, (uint32_t)count, WriteItem, (void*)items, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}
