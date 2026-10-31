// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The list formatter (maul-locale/list.h): a locale's patterns of a style
// kept in the formatter, items joined by list_join.

#include "maul-locale/list.h"

#include "invariant.h"
#include "list_data.h"
#include "list_join.h"
#include "locale_valid.h"
#include "number_core.h"
#include "number_output.h"

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
    if (formatterOut == nullptr || data == nullptr || !mlocLocaleValid(locale) ||
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

typedef struct ListWriting
{
    const ListFormat* f;
    const mlocListItem* items;
    size_t count;
} ListWriting;

// Writes a list's text, or in the parts pass reports its parts by their
// list kinds, through a writer of its own.
static mlocResult WriteListText(const void* context, NumberWriter* w)
{
    const ListWriting* g = context;
    // mlocListFormatter_Format refuses more, so the count fits 32 bits.
    MLOC_ASSERT(g->count <= MLOC_MAX_LIST_ITEMS);
    if (g->count == 0)
    {
        return mloc_success;
    }
    if (w->parts == nullptr)
    {
        mlocWriteList(&g->f->patterns, g->items, (uint32_t)g->count, WriteItem, (void*)g->items, w);
        return mloc_success;
    }
    mlocOutput report = {.part = ReportPart, .context = (void*)w->parts};
    NumberWriter parts = {.parts = &report};
    mlocWriteList(&g->f->patterns, g->items, (uint32_t)g->count, WriteItem, (void*)g->items,
                  &parts);
    mlocEndParts(&parts);
    return mloc_success;
}

mlocResult mlocListFormatter_Format(const mlocListFormatter* formatter, const mlocListItem* items,
                                    size_t count, mlocOutput* textOut)
{
    ListFormat f;
    if (!ReadFormatter(formatter, &f) || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0) || (items == nullptr && count > 0))
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
    ListWriting g = {&f, items, count};
    return mlocWriteOutput(WriteListText, &g, textOut);
}
