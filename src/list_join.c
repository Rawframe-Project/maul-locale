// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Items joined by list patterns (list_join.h). A list of n items nests
// n - 1 patterns, the first item innermost: each pattern is the text
// before its "{0}" and the text after it, the item it adds at its "{1}".
// So the text is each pattern's part before "{0}", from the outermost,
// then the first item, then each pattern's part after "{0}", from the
// innermost, and no copy of a partial result is made.

#include "list_join.h"

#include "invariant.h"

#include <string.h>

typedef struct Pattern
{
    const char* text;
    size_t length;
    size_t zero;
} Pattern;

// Whether an item starts with "i" or "hi" (not "hia" nor "hie"), any
// case: Spanish's "y" becomes "e".
static bool BeforeE(const ListItem* item)
{
    const char* t = item->text;
    size_t n = item->length;
    if (n > 1 && (t[0] == 'h' || t[0] == 'H') && (t[1] == 'i' || t[1] == 'I'))
    {
        return n == 2 || (t[2] != 'a' && t[2] != 'A' && t[2] != 'e' && t[2] != 'E');
    }
    return n > 0 && (t[0] == 'i' || t[0] == 'I');
}

// Whether an item starts with "o", "ho" or "8", any case, or is "11"
// alone or before a space: Spanish's "o" becomes "u".
static bool BeforeU(const ListItem* item)
{
    const char* t = item->text;
    size_t n = item->length;
    if (n > 0 && (t[0] == 'o' || t[0] == 'O' || t[0] == '8'))
    {
        return true;
    }
    if (n > 1 && (t[0] == 'h' || t[0] == 'H') && (t[1] == 'o' || t[1] == 'O'))
    {
        return true;
    }
    return n >= 2 && t[0] == '1' && t[1] == '1' && (n == 2 || t[2] == ' ');
}

// Whether a code point is of the Hebrew script (Scripts.txt, Unicode 18).
static bool Hebrew(uint32_t c)
{
    static const uint32_t s_ranges[][2] = {
        {0x0591, 0x05C9}, {0x05D0, 0x05EA}, {0x05EF, 0x05F4}, {0xFB1D, 0xFB36}, {0xFB38, 0xFB3C},
        {0xFB3E, 0xFB3E}, {0xFB40, 0xFB41}, {0xFB43, 0xFB44}, {0xFB46, 0xFB4F},
    };
    for (size_t i = 0; i < sizeof(s_ranges) / sizeof(s_ranges[0]); i++)
    {
        if (c >= s_ranges[i][0] && c <= s_ranges[i][1])
        {
            return true;
        }
    }
    return false;
}

// Whether an item's first code point is not Hebrew: Hebrew's vav takes a
// dash.
static bool BeforeDash(const ListItem* item)
{
    const uint8_t* t = (const uint8_t*)item->text;
    size_t n = item->length;
    if (n == 0)
    {
        return false;
    }
    // Hebrew's code points are of two or three bytes.
    uint32_t c = t[0] >= 0xC0 && t[0] < 0xE0 && n >= 2 ? (t[0] & 0x1Fu) << 6 | (t[1] & 0x3Fu)
                 : t[0] >= 0xE0 && t[0] < 0xF0 && n >= 3
                     ? (t[0] & 0x0Fu) << 12 | (t[1] & 0x3Fu) << 6 | (t[2] & 0x3Fu)
                     : 0;
    return !Hebrew(c);
}

static bool Is(const char* text, size_t length, const char* pattern)
{
    return length == strlen(pattern) && memcmp(text, pattern, length) == 0;
}

// The pattern adding item k of count: "two" or the end one with the
// language's change for the item, else the start or a middle one.
static Pattern PatternFor(const ListPatterns* p, const ListItem* items, uint32_t k, uint32_t count)
{
    MLOC_ASSERT(k >= 1 && k < count);
    uint32_t which = count == 2       ? listPatternTwo
                     : k == count - 1 ? listPatternEnd
                     : k == 1         ? listPatternStart
                                      : listPatternMiddle;
    Pattern out = {p->text[which], p->length[which], 0};
    bool contextual = which == listPatternTwo || which == listPatternEnd;
    if (contextual && p->rule == listRuleSpanish)
    {
        if (Is(out.text, out.length, "{0} y {1}") && BeforeE(&items[k]))
        {
            out.text = "{0} e {1}";
        }
        else if (Is(out.text, out.length, "{0} o {1}") && BeforeU(&items[k]))
        {
            out.text = "{0} u {1}";
        }
    }
    else if (contextual && p->rule == listRuleHebrew &&
             Is(out.text, out.length, "{0} \xD7\x95{1}") && BeforeDash(&items[k]))
    {
        out.text = "{0} \xD7\x95-{1}";
        out.length++;
    }
    while (out.zero + 3 <= out.length && memcmp(out.text + out.zero, "{0}", 3) != 0)
    {
        out.zero++;
    }
    // The data's patterns hold "{0}" (mlocValidListPatterns), and so do
    // the changed ones above.
    MLOC_ASSERT(out.zero + 3 <= out.length);
    return out;
}

static void WriteItem(const ListItem* items, uint32_t k, ListItemWriter write, void* context,
                      NumberWriter* writer)
{
    if (write != nullptr)
    {
        write(context, k, writer);
    }
    else
    {
        mlocWriteLiteral(writer, items[k].text, items[k].length);
    }
}

// Writes a part of a pattern, item k at its "{1}".
static void WriteSpan(const Pattern* p, size_t from, size_t to, const ListItem* items, uint32_t k,
                      ListItemWriter write, void* context, NumberWriter* writer)
{
    MLOC_ASSERT(from <= to && to <= p->length);
    size_t start = from;
    for (size_t at = from; at + 3 <= to; at++)
    {
        if (memcmp(p->text + at, "{1}", 3) == 0)
        {
            mlocWriteLiteral(writer, p->text + start, at - start);
            WriteItem(items, k, write, context, writer);
            start = at + 3;
            at += 2;
        }
    }
    mlocWriteLiteral(writer, p->text + start, to - start);
}

void mlocWriteList(const ListPatterns* patterns, const ListItem* items, uint32_t count,
                   ListItemWriter write, void* context, NumberWriter* writer)
{
    // Its callers write nothing for no items (list.c, duration_write.c),
    // and a unit formatter has a part; k below would wrap at 0.
    MLOC_ASSERT(count >= 1);
    for (uint32_t k = count - 1; k >= 1; k--)
    {
        Pattern p = PatternFor(patterns, items, k, count);
        WriteSpan(&p, 0, p.zero, items, k, write, context, writer);
    }
    WriteItem(items, 0, write, context, writer);
    for (uint32_t k = 1; k < count; k++)
    {
        Pattern p = PatternFor(patterns, items, k, count);
        WriteSpan(&p, p.zero + 3, p.length, items, k, write, context, writer);
    }
}
