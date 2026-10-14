// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// How the names part writes (names_text.h).

#include "names_text.h"

#include "data_view.h"
#include "kinds.h"
#include "spellout_data.h"

#include <string.h>

static void Put(NameWriter* w, uint8_t byte)
{
    if (w->length < sizeof(w->head))
    {
        w->head[w->length] = byte;
    }
    if (w->buffer != nullptr && w->length < w->capacity)
    {
        w->buffer[w->length] = (char)byte;
    }
    w->length++;
}

void mlocNameAppend(NameWriter* w, const uint8_t* text, size_t length, bool brackets)
{
    for (size_t i = 0; i < length; i++)
    {
        uint8_t c = text[i];
        if (brackets && (c == '(' || c == ')'))
        {
            c = c == '(' ? '[' : ']';
        }
        else if (brackets && c == 0xEF && i + 2 < length && text[i + 1] == 0xBC &&
                 (text[i + 2] == 0x88 || text[i + 2] == 0x89))
        {
            // U+FF08 and U+FF09 become U+FF3B and U+FF3D.
            Put(w, 0xEF);
            Put(w, 0xBC);
            Put(w, text[i + 2] == 0x88 ? 0xBB : 0xBD);
            i += 2;
            continue;
        }
        Put(w, c);
    }
}

bool mlocNameCapitalizes(const mlocDataSet* data, const mlocLocale* locale, ContextUsage usage,
                         mlocCapitalization capitalization)
{
    if (capitalization == mloc_capitalizationSentence)
    {
        return true;
    }
    if (usage >= contextUsageCount || capitalization == mloc_capitalizationNone)
    {
        return false;
    }
    uint8_t contexts = mlocGetLocaleContext(data, locale, usage);
    return (capitalization == mloc_capitalizationListOrMenu &&
            (contexts & contextListOrMenu) != 0) ||
           (capitalization == mloc_capitalizationStandAlone && (contexts & contextStandAlone) != 0);
}

// The first code point of valid UTF-8 text and its length in bytes; 0
// when the text is shorter than its first character.
static uint32_t First(const uint8_t* text, size_t length, size_t* sizeOut)
{
    uint32_t size = text[0] < 0x80 ? 1 : text[0] < 0xE0 ? 2 : text[0] < 0xF0 ? 3 : 4;
    if (length < size)
    {
        *sizeOut = 0;
        return 0;
    }
    static const uint8_t s_masks[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t cp = text[0] & s_masks[size];
    for (uint32_t i = 1; i < size; i++)
    {
        cp = cp << 6 | (text[i] & 0x3Fu);
    }
    *sizeOut = size;
    return cp;
}

// A code point's titlecase, or false when it has none other.
static bool Titlecase(const mlocDataSet* data, const mlocLocale* locale, uint32_t cp,
                      const uint8_t** textOut, uint32_t* lengthOut)
{
    static const uint8_t s_dottedI[] = {0xC4, 0xB0};
    static const char s_upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    bool turkic = strcmp(locale->language, "tr") == 0 || strcmp(locale->language, "az") == 0;
    if (turkic && cp == 'i')
    {
        *textOut = s_dottedI;
        *lengthOut = sizeof(s_dottedI);
        return true;
    }
    if (cp < 0x80)
    {
        if (cp < 'a' || cp > 'z')
        {
            return false;
        }
        *textOut = (const uint8_t*)s_upper + (cp - 'a');
        *lengthOut = 1;
        return true;
    }
    DataSpan unit;
    return mlocFindDataSetUnit(data, kindNameTitlecase, nullptr, 0, &unit) &&
           mlocSpelloutTitlecase(unit, cp, textOut, lengthOut);
}

void mlocNameCapitalize(const mlocDataSet* data, const mlocLocale* locale, NameWriter* w)
{
    size_t size = 0;
    size_t kept = w->length < sizeof(w->head) ? w->length : sizeof(w->head);
    uint32_t cp = kept == 0 ? 0 : First(w->head, kept, &size);
    const uint8_t* title = nullptr;
    uint32_t titleLength = 0;
    if (size == 0 || !Titlecase(data, locale, cp, &title, &titleLength))
    {
        return;
    }
    size_t length = w->length - size + titleLength;
    if (w->buffer != nullptr && w->length <= w->capacity && length <= w->capacity)
    {
        memmove(w->buffer + titleLength, w->buffer + size, w->length - size);
        memcpy(w->buffer, title, titleLength);
    }
    w->length = length;
}

mlocResult mlocNameFinish(const NameWriter* w, mlocOutput* output)
{
    output->length = w->length;
    return w->length > w->capacity ? mloc_errorCapacity : mloc_success;
}
