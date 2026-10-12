// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Canonicalizing a locale's extensions (UTS #35 Annex C, Processing
// LocaleIds): a t extension's language by the alias rules, u and t key
// values by the bcp47 data's aliases, and the subdivisions "sd" and
// "rg" take by theirs. A u value that becomes "true" is removed, as
// canonical syntax asks. The table's layout is in locale_tables.h.

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_canonical.h"
#include "locale_tables.h"

#include "maul-locale/locale.h"

#include <string.h>

#define MAX_SUBTAGS 64

typedef struct Extensions
{
    const uint8_t* types;
    const uint8_t* subdivisions;
    const uint8_t* text;
    uint32_t typeCount;
    uint32_t subdivisionCount;
    uint32_t textSize;
} Extensions;

static bool ReadExtensions(DataSpan span, Extensions* out)
{
    if (span.size < extensionHeaderSize)
    {
        return false;
    }
    uint64_t types = mlocLoad32(span.bytes);
    uint64_t subdivisions = mlocLoad32(span.bytes + 4);
    uint64_t text = mlocLoad32(span.bytes + 8);
    uint64_t subdivisionsAt = extensionHeaderSize + types * extensionTypeSize;
    uint64_t textAt = subdivisionsAt + subdivisions * extensionSubdivisionSize;
    if (textAt + text != span.size)
    {
        return false;
    }
    *out = (Extensions){
        .types = span.bytes + extensionHeaderSize,
        .subdivisions = span.bytes + subdivisionsAt,
        .text = span.bytes + textAt,
        .typeCount = (uint32_t)types,
        .subdivisionCount = (uint32_t)subdivisions,
        .textSize = (uint32_t)text,
    };
    return true;
}

// A text of the table: its offset and length.
typedef struct Text
{
    const uint8_t* bytes;
    uint32_t length;
} Text;

static bool InText(const Extensions* x, uint32_t offset, uint32_t length)
{
    return (uint64_t)offset + length <= x->textSize;
}

// Whether a text is a value a tag holds: subtags of 3 to 8 lowercase
// letters and digits joined by "-" (or of 1 to 8 for a key's 2).
static bool ValidValue(const uint8_t* text, uint32_t length, uint32_t least)
{
    uint32_t run = 0;
    for (uint32_t i = 0; i <= length; i++)
    {
        if (i == length || text[i] == '-')
        {
            if (run < least || run > 8)
            {
                return false;
            }
            run = 0;
        }
        else if ((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= '0' && text[i] <= '9'))
        {
            run++;
        }
        else
        {
            return false;
        }
    }
    return true;
}

// Orders texts by their bytes, a prefix first. A loop rather than
// memcmp: keys and values are a few bytes, fewer than a call to the
// library takes to start.
static int Compare(const uint8_t* a, uint32_t aLength, const uint8_t* b, uint32_t bLength)
{
    uint32_t n = aLength < bLength ? aLength : bLength;
    for (uint32_t i = 0; i < n; i++)
    {
        if (a[i] != b[i])
        {
            return a[i] < b[i] ? -1 : 1;
        }
    }
    return (aLength > bLength) - (aLength < bLength);
}

// An entry of either list: its key (empty for a subdivision), from and
// to.
typedef struct Entry
{
    const uint8_t* key;
    Text from;
    Text to;
} Entry;

static Entry ReadEntry(const Extensions* x, bool subdivision, uint32_t i)
{
    const uint8_t* e = subdivision ? x->subdivisions + (size_t)i * extensionSubdivisionSize
                                   : x->types + (size_t)i * extensionTypeSize;
    uint32_t fromLength = subdivision ? e[0] : e[2];
    uint32_t toLength = subdivision ? e[1] : e[3];
    return (Entry){
        .key = subdivision ? nullptr : e,
        .from = {x->text + mlocLoad32(e + 4), fromLength},
        .to = {x->text + mlocLoad32(e + 8), toLength},
    };
}

// Whether entry i's offsets, lengths, reserved bytes and texts hold.
static bool ValidEntry(const Extensions* x, bool subdivision, uint32_t i)
{
    const uint8_t* e = subdivision ? x->subdivisions + (size_t)i * extensionSubdivisionSize
                                   : x->types + (size_t)i * extensionTypeSize;
    uint32_t fromLength = subdivision ? e[0] : e[2];
    uint32_t toLength = subdivision ? e[1] : e[3];
    bool reserved = subdivision ? mlocLoad16(e + 2) == 0 : mlocLoad32(e + 12) == 0;
    if (!reserved || !InText(x, mlocLoad32(e + 4), fromLength) ||
        !InText(x, mlocLoad32(e + 8), toLength))
    {
        return false;
    }
    Entry entry = ReadEntry(x, subdivision, i);
    return ValidValue(entry.from.bytes, entry.from.length, 3) &&
           ValidValue(entry.to.bytes, entry.to.length, 3) &&
           (subdivision || ValidValue(entry.key, 2, 2));
}

// Whether entry i comes after entry i - 1: by key, then by from text.
static bool InOrder(const Extensions* x, bool subdivision, uint32_t i)
{
    Entry before = ReadEntry(x, subdivision, i - 1);
    Entry entry = ReadEntry(x, subdivision, i);
    int order = subdivision ? 0 : Compare(before.key, 2, entry.key, 2);
    if (order == 0)
    {
        order = Compare(before.from.bytes, before.from.length, entry.from.bytes, entry.from.length);
    }
    return order < 0;
}

static bool ValidEntries(const Extensions* x, bool subdivision)
{
    uint32_t count = subdivision ? x->subdivisionCount : x->typeCount;
    for (uint32_t i = 0; i < count; i++)
    {
        if (!ValidEntry(x, subdivision, i) || (i > 0 && !InOrder(x, subdivision, i)))
        {
            return false;
        }
    }
    return true;
}

bool mlocValidExtensionAliases(const DataView* view, DataSpan payload)
{
    (void)view;
    Extensions x;
    return ReadExtensions(payload, &x) && mlocLoad32(payload.bytes + 12) == 0 &&
           ValidEntries(&x, false) && ValidEntries(&x, true);
}

// The replacement of a value, or a text of length 0.
static Text Find(const Extensions* x, const char* key, const char* value, uint32_t length)
{
    bool subdivision = key == nullptr;
    uint32_t wanted = subdivision ? 0 : (uint32_t)(uint8_t)key[0] << 8 | (uint8_t)key[1];
    uint32_t low = 0;
    uint32_t high = subdivision ? x->subdivisionCount : x->typeCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        Entry entry = ReadEntry(x, subdivision, middle);
        // A key's two letters compare as one number, highest first.
        int order = 0;
        if (!subdivision)
        {
            uint32_t at = (uint32_t)entry.key[0] << 8 | entry.key[1];
            order = (at > wanted) - (at < wanted);
        }
        order = order != 0
                    ? order
                    : Compare(entry.from.bytes, entry.from.length, (const uint8_t*)value, length);
        if (order == 0)
        {
            return entry.to;
        }
        if (order < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return (Text){nullptr, 0};
}

typedef struct Writer
{
    char* out;
    size_t capacity;
    size_t length;
    bool full;
} Writer;

// Writes "-" (unless first) and the text.
static void Write(Writer* w, const char* text, size_t length)
{
    size_t needed = length + (w->length > 0);
    if (w->full || w->length + needed > w->capacity)
    {
        w->full = true;
        return;
    }
    if (w->length > 0)
    {
        w->out[w->length++] = '-';
    }
    // A loop rather than memcpy: subtags are a few bytes.
    for (size_t i = 0; i < length; i++)
    {
        w->out[w->length + i] = text[i];
    }
    w->length += length;
}

// The tail's subtags, by start and length.
typedef struct Subtags
{
    const char* tail;
    int count;
    uint32_t start[MAX_SUBTAGS];
    uint32_t length[MAX_SUBTAGS];
} Subtags;

static void Split(const char* tail, size_t size, Subtags* s)
{
    s->tail = tail;
    s->count = 0;
    size_t at = 0;
    while (at < size && tail[at] != '\0' && s->count < MAX_SUBTAGS)
    {
        size_t end = at;
        while (end < size && tail[end] != '\0' && tail[end] != '-')
        {
            end++;
        }
        s->start[s->count] = (uint32_t)at;
        s->length[s->count++] = (uint32_t)(end - at);
        at = end + 1;
    }
}

static const char* At(const Subtags* s, int i)
{
    return s->tail + s->start[i];
}

static bool IsSingleton(const Subtags* s, int i)
{
    return i >= s->count || s->length[i] == 1;
}

// The value of a key: subtags first to end (exclusive), as one text in
// the tail; length 0 when there is none.
static uint32_t ValueLength(const Subtags* s, int first, int end)
{
    return first >= end ? 0 : s->start[end - 1] + s->length[end - 1] - s->start[first];
}

// Writes a key and its value, replaced by its alias when it has one; a
// u value that is then "true" is left out. Returns whether it changed.
static bool WriteKeyword(const Extensions* x, const Subtags* s, int key, int first, int end,
                         bool unicode, Writer* w)
{
    const char* name = At(s, key);
    Write(w, name, 2);
    uint32_t length = ValueLength(s, first, end);
    if (length == 0)
    {
        return false;
    }
    bool subdivision = unicode && (memcmp(name, "sd", 2) == 0 || memcmp(name, "rg", 2) == 0);
    Text to = Find(x, subdivision ? nullptr : name, At(s, first), length);
    const char* value = to.length > 0 ? (const char*)to.bytes : At(s, first);
    uint32_t valueLength = to.length > 0 ? to.length : length;
    if (!(unicode && valueLength == 4 && memcmp(value, "true", 4) == 0))
    {
        Write(w, value, valueLength);
    }
    return to.length > 0;
}

// A u extension after its singleton, from subtag *i; returns whether it
// changed.
static bool UnicodeExtension(const Extensions* x, const Subtags* s, int* i, Writer* w)
{
    bool changed = false;
    while (!IsSingleton(s, *i) && s->length[*i] >= 3)
    {
        Write(w, At(s, *i), s->length[*i]);
        (*i)++;
    }
    while (!IsSingleton(s, *i))
    {
        int key = (*i)++;
        int first = *i;
        while (!IsSingleton(s, *i) && s->length[*i] >= 3)
        {
            (*i)++;
        }
        changed |= WriteKeyword(x, s, key, first, *i, true, w);
    }
    return changed;
}

// A t extension's language, canonicalized by the alias rules and written
// in lowercase; returns whether it changed, or fails as they do.
static mlocResult TransformedLanguage(const mlocDataSet* data, const Subtags* s, int first, int end,
                                      Writer* w, bool* changed)
{
    uint32_t length = ValueLength(s, first, end);
    mlocLocale tlang;
    if (mlocParseLocale(At(s, first), length, &tlang).status != mloc_success)
    {
        return mloc_errorData;
    }
    uint32_t changes = 0;
    mlocResult result = mlocApplyAliases(data, &tlang, &changes);
    if (result != mloc_success)
    {
        return result;
    }
    char text[160];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocFormatLocale(&tlang, &output) != mloc_success)
    {
        return mloc_errorCapacity;
    }
    for (size_t k = 0; k < output.length; k++)
    {
        text[k] = text[k] >= 'A' && text[k] <= 'Z' ? (char)(text[k] + 32) : text[k];
    }
    Write(w, text, output.length);
    *changed = changes != 0;
    return mloc_success;
}

// A t extension after its singleton, from subtag *i.
static mlocResult TransformedExtension(const mlocDataSet* data, const Extensions* x,
                                       const Subtags* s, int* i, Writer* w, bool* changed)
{
    int first = *i;
    // A field key is a letter and a digit; the language ends at one.
    while (!IsSingleton(s, *i) &&
           !(s->length[*i] == 2 && At(s, *i)[1] >= '0' && At(s, *i)[1] <= '9'))
    {
        (*i)++;
    }
    if (*i > first)
    {
        mlocResult result = TransformedLanguage(data, s, first, *i, w, changed);
        if (result != mloc_success)
        {
            return result;
        }
    }
    while (!IsSingleton(s, *i))
    {
        int key = (*i)++;
        int values = *i;
        while (!IsSingleton(s, *i) && s->length[*i] >= 3)
        {
            (*i)++;
        }
        *changed |= WriteKeyword(x, s, key, values, *i, false, w);
    }
    return mloc_success;
}

// Copies subtags from *i to the next singleton (to the end for private
// use).
static void CopyExtension(const Subtags* s, int* i, bool privateUse, Writer* w)
{
    while (*i < s->count && (privateUse || s->length[*i] > 1))
    {
        Write(w, At(s, *i), s->length[*i]);
        (*i)++;
    }
}

// Whether the tail has a u or t extension, the only ones this changes.
static bool HasAliasedExtension(const char* tail, size_t size)
{
    for (size_t at = 0; at < size && tail[at] != '\0';)
    {
        size_t end = at;
        while (end < size && tail[end] != '\0' && tail[end] != '-')
        {
            end++;
        }
        if (end - at == 1 && (tail[at] == 'u' || tail[at] == 't'))
        {
            return true;
        }
        if (end - at == 1 && tail[at] == 'x')
        {
            // Private use runs to the end; its subtags are no singletons.
            return false;
        }
        at = end + 1;
    }
    return false;
}

mlocResult mlocCanonicalizeExtensions(const mlocDataSet* data, mlocLocale* locale,
                                      uint32_t* changes)
{
    if (!HasAliasedExtension(locale->tail, sizeof(locale->tail)))
    {
        return mloc_success;
    }
    DataSpan span;
    Extensions x;
    if (!mlocFindDataSetUnit(data, kindLocaleExtensionAliases, nullptr, 0, &span) ||
        !ReadExtensions(span, &x))
    {
        return mloc_errorNotFound;
    }
    Subtags s;
    Split(locale->tail, sizeof(locale->tail), &s);
    char tail[sizeof(locale->tail)];
    Writer w = {tail, sizeof(tail), 0, false};
    int i = 0;
    while (i < s.count && s.length[i] > 1)
    {
        Write(&w, At(&s, i), s.length[i]);
        i++;
    }
    bool changed = false;
    while (i < s.count)
    {
        char singleton = At(&s, i)[0];
        Write(&w, At(&s, i++), 1);
        mlocResult result = mloc_success;
        if (singleton == 'u')
        {
            changed |= UnicodeExtension(&x, &s, &i, &w);
        }
        else if (singleton == 't')
        {
            result = TransformedExtension(data, &x, &s, &i, &w, &changed);
        }
        else
        {
            CopyExtension(&s, &i, singleton == 'x', &w);
        }
        if (result != mloc_success)
        {
            return result;
        }
    }
    if (!changed)
    {
        return mloc_success;
    }
    if (w.full)
    {
        return mloc_errorCapacity;
    }
    memset(locale->tail, 0, sizeof(locale->tail));
    memcpy(locale->tail, tail, w.length);
    *changes |= mloc_changedExtensions;
    return mloc_success;
}
