// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Locale keys and fallback per component (record mloc-0016). A locale
// becomes its key once, with likely subtags; each next key is the
// component's explicit parent, else the last variant, region or script
// dropped (a script giving root in the main component), else root. The
// parents table's layout is in locale_tables.h.

#include "locale_fallback.h"

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_tables.h"

#include "maul-locale/locale.h"

#include <string.h>

// A key is at most a language, script, region and a full tail of
// variants with their separators.
#define MAX_KEY 160

typedef struct Parents
{
    const uint8_t* entries;
    const uint8_t* text;
    uint32_t count;
    uint32_t textSize;
} Parents;

static bool ReadParents(DataSpan span, Parents* out)
{
    if (span.size < parentHeaderSize)
    {
        return false;
    }
    uint64_t count = mlocLoad32(span.bytes);
    uint64_t text = mlocLoad32(span.bytes + 4);
    uint64_t textAt = parentHeaderSize + count * parentEntrySize;
    if (textAt + text != span.size)
    {
        return false;
    }
    *out = (Parents){span.bytes + parentHeaderSize, span.bytes + textAt, (uint32_t)count,
                     (uint32_t)text};
    return true;
}

typedef struct Entry
{
    uint32_t component;
    const char* child;
    uint32_t childLength;
    const char* parent;
    uint32_t parentLength;
} Entry;

static Entry ReadEntry(const Parents* p, uint32_t i)
{
    const uint8_t* e = p->entries + (size_t)i * parentEntrySize;
    return (Entry){e[0], (const char*)p->text + mlocLoad32(e + 4), e[1],
                   (const char*)p->text + mlocLoad32(e + 8), e[2]};
}

static int Compare(const char* a, uint32_t aLength, const char* b, uint32_t bLength)
{
    int order = memcmp(a, b, aLength < bLength ? aLength : bLength);
    return order != 0 ? order : (aLength > bLength) - (aLength < bLength);
}

// Whether a text is a key as the generator writes one: a locale id with
// "_" that parses.
static bool ValidKey(const char* text, uint32_t length)
{
    mlocLocale locale;
    if (length == 0 || mlocParseLocale(text, length, &locale).status != mloc_success)
    {
        return false;
    }
    for (uint32_t i = 0; i < length; i++)
    {
        if (text[i] == '-')
        {
            return false;
        }
    }
    return true;
}

static const Entry* FindParent(const Parents* p, uint32_t component, const char* key,
                               uint32_t length, Entry* found);

// Whether following explicit parents from every entry ends, so that a
// file's chains always reach root.
static bool ParentsEnd(const Parents* p)
{
    for (uint32_t i = 0; i < p->count; i++)
    {
        Entry entry = ReadEntry(p, i);
        uint32_t steps = 0;
        Entry next;
        while (FindParent(p, entry.component, entry.parent, entry.parentLength, &next) != nullptr)
        {
            if (++steps > p->count)
            {
                return false;
            }
            entry = next;
        }
    }
    return true;
}

bool mlocValidParents(const DataView* view, DataSpan payload)
{
    (void)view;
    Parents p;
    if (!ReadParents(payload, &p) || mlocLoad64(payload.bytes + 8) != 0)
    {
        return false;
    }
    for (uint32_t i = 0; i < p.count; i++)
    {
        const uint8_t* e = p.entries + (size_t)i * parentEntrySize;
        if (e[0] > mloc_componentGrammar || e[3] != 0 ||
            (uint64_t)mlocLoad32(e + 4) + e[1] > p.textSize ||
            (uint64_t)mlocLoad32(e + 8) + e[2] > p.textSize)
        {
            return false;
        }
        Entry entry = ReadEntry(&p, i);
        if (!ValidKey(entry.child, entry.childLength) ||
            !ValidKey(entry.parent, entry.parentLength))
        {
            return false;
        }
        if (i > 0)
        {
            Entry before = ReadEntry(&p, i - 1);
            int order =
                before.component != entry.component
                    ? (before.component < entry.component ? -1 : 1)
                    : Compare(before.child, before.childLength, entry.child, entry.childLength);
            if (order >= 0)
            {
                return false;
            }
        }
    }
    return ParentsEnd(&p);
}

// The explicit parent of a key in a component, or null.
static const Entry* FindParent(const Parents* p, uint32_t component, const char* key,
                               uint32_t length, Entry* found)
{
    uint32_t low = 0;
    uint32_t high = p->count;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        Entry entry = ReadEntry(p, middle);
        int order = entry.component != component
                        ? (entry.component < component ? -1 : 1)
                        : Compare(entry.child, entry.childLength, key, length);
        if (order == 0)
        {
            *found = entry;
            return found;
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
    return nullptr;
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

// The length of a tail's variants: the subtags before its first
// singleton.
static size_t VariantsLength(const char* tail, size_t size)
{
    size_t length = FieldLength(tail, size);
    size_t at = 0;
    while (at < length)
    {
        size_t end = at;
        while (end < length && tail[end] != '-')
        {
            end++;
        }
        if (end - at == 1)
        {
            return at == 0 ? 0 : at - 1;
        }
        at = end + 1;
    }
    return length;
}

// Writes a step's key: "root" for und alone, else the CLDR id with "_"
// and the variants in uppercase. Returns its length.
static uint32_t KeyText(const mlocLocale* step, char* out)
{
    size_t language = FieldLength(step->language, 8);
    size_t script = FieldLength(step->script, 4);
    size_t region = FieldLength(step->region, 4);
    size_t variants = VariantsLength(step->tail, sizeof(step->tail));
    if (language == 3 && memcmp(step->language, "und", 3) == 0 && script == 0 && region == 0 &&
        variants == 0)
    {
        // The key is not NUL-ended; its length is returned.
        static const char s_root[4] = {'r', 'o', 'o', 't'};
        memcpy(out, s_root, sizeof(s_root));
        return 4;
    }
    size_t at = 0;
    const char* parts[3] = {step->language, step->script, step->region};
    size_t lengths[3] = {language, script, region};
    for (int i = 0; i < 3; i++)
    {
        if (lengths[i] > 0)
        {
            if (at > 0)
            {
                out[at++] = '_';
            }
            memcpy(out + at, parts[i], lengths[i]);
            at += lengths[i];
        }
    }
    if (variants > 0)
    {
        out[at++] = '_';
        for (size_t i = 0; i < variants; i++)
        {
            char c = step->tail[i];
            out[at++] = c == '-' ? '_' : (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
        }
    }
    return (uint32_t)at;
}

// The script likely subtags give a locale, or 0 when there is none.
static uint32_t LikelyScript(const mlocDataSet* data, const mlocLocale* locale)
{
    mlocLocale probe = {0};
    memcpy(probe.language, locale->language, 8);
    memcpy(probe.script, locale->script, 4);
    memcpy(probe.region, locale->region, 4);
    mlocLocale likely;
    if (mlocMaximizeLocale(data, &probe, &likely) != mloc_success)
    {
        return 0;
    }
    return mlocPackScript(likely.script, 4);
}

// A locale's first step: its language, script and region in key form
// and its variants, without its extensions.
static mlocLocale FirstStep(const mlocDataSet* data, const mlocLocale* locale)
{
    mlocLocale step = {0};
    memcpy(step.language, locale->language, 8);
    memcpy(step.region, locale->region, 4);
    memcpy(step.tail, locale->tail, VariantsLength(locale->tail, sizeof(locale->tail)));
    uint32_t script = mlocPackScript(locale->script, (uint32_t)FieldLength(locale->script, 4));
    if (script == 0)
    {
        script = LikelyScript(data, locale);
    }
    mlocLocale alone = {0};
    memcpy(alone.language, locale->language, 8);
    if (script != 0 && script != LikelyScript(data, &alone))
    {
        mlocUnpackScript(script, step.script);
    }
    return step;
}

// The step after one, or false after root. Fails as parsing does on a
// parent the table names, which validation has already parsed.
static bool NextStep(const Parents* parents, mlocComponent component, mlocLocale* step)
{
    char key[MAX_KEY];
    uint32_t length = KeyText(step, key);
    if (length == 4 && memcmp(key, "root", 4) == 0)
    {
        return false;
    }
    Entry entry;
    if (FindParent(parents, component, key, length, &entry) != nullptr)
    {
        return mlocParseLocale(entry.parent, entry.parentLength, step).status == mloc_success;
    }
    size_t variants = VariantsLength(step->tail, sizeof(step->tail));
    if (variants > 0)
    {
        size_t cut = variants;
        while (cut > 0 && step->tail[cut - 1] != '-')
        {
            cut--;
        }
        memset(step->tail + (cut > 0 ? cut - 1 : 0), 0,
               sizeof(step->tail) - (cut > 0 ? cut - 1 : 0));
        return true;
    }
    if (step->region[0] != '\0')
    {
        memset(step->region, 0, sizeof(step->region));
        return true;
    }
    bool script = step->script[0] != '\0';
    memset(step->script, 0, sizeof(step->script));
    if (!script || component == mloc_componentMain)
    {
        mlocLocale root = {0};
        memcpy(root.language, "und", 3);
        *step = root;
    }
    return true;
}

static bool OpenParents(const mlocDataSet* data, Parents* parents)
{
    DataSpan span;
    return mlocFindDataSetUnit(data, kindLocaleParents, nullptr, 0, &span) &&
           ReadParents(span, parents);
}

mlocResult mlocGetParentLocale(const mlocDataSet* data, mlocComponent component,
                               const mlocLocale* locale, mlocLocale* parentOut)
{
    if (data == nullptr || locale == nullptr || parentOut == nullptr ||
        component > mloc_componentGrammar)
    {
        return mloc_errorInvalid;
    }
    Parents parents;
    if (!OpenParents(data, &parents))
    {
        return mloc_errorNotFound;
    }
    mlocLocale step = FirstStep(data, locale);
    if (!NextStep(&parents, component, &step))
    {
        return mloc_errorNotFound;
    }
    // The extensions come back after the parent's variants.
    size_t variants = FieldLength(step.tail, sizeof(step.tail));
    size_t own = VariantsLength(locale->tail, sizeof(locale->tail));
    size_t tail = FieldLength(locale->tail, sizeof(locale->tail));
    if (tail > own)
    {
        size_t start = own == 0 ? 0 : own + 1;
        size_t length = tail - start;
        if (variants + (variants > 0) + length > sizeof(step.tail))
        {
            return mloc_errorCapacity;
        }
        if (variants > 0)
        {
            step.tail[variants++] = '-';
        }
        memcpy(step.tail + variants, locale->tail + start, length);
    }
    *parentOut = step;
    return mloc_success;
}

bool mlocFindLocaleUnit(const mlocDataSet* data, mlocComponent component, uint32_t kind,
                        const mlocLocale* locale, DataSpan* spanOut, bool* fellBackOut)
{
    Parents parents;
    if (!OpenParents(data, &parents))
    {
        return false;
    }
    // A chain is at most 22 variants, explicit parents, a region, a
    // script and root; the bound only stops data whose parents loop.
    mlocLocale step = FirstStep(data, locale);
    for (int depth = 0; depth < 64; depth++)
    {
        char key[MAX_KEY];
        uint32_t length = KeyText(&step, key);
        for (size_t s = 0; s < data->count; s++)
        {
            DataView view = mlocViewOf(&data->sources[s]);
            uint32_t index = 0;
            if (mlocFindDataLocale(&view, key, length, &index) &&
                mlocFindDataUnit(&view, kind, index, spanOut))
            {
                *fellBackOut = depth > 0;
                return true;
            }
        }
        if (!NextStep(&parents, component, &step))
        {
            return false;
        }
    }
    return false;
}
