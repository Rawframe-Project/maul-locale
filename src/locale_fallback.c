// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Locale keys and fallback per component. A locale becomes its key once,
// with likely subtags; each next key is the component's explicit parent,
// else the last variant, region or script dropped (a script giving root in
// the main component), else root. The parents table's layout is in
// locale_tables.h.

#include "locale_fallback.h"

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_keyword.h"
#include "locale_likely.h"
#include "locale_tables.h"

#include "maul-locale/locale.h"

#include <string.h>

// A key is at most a language, script, region and a full tail of
// variants with their separators.
#define MAX_KEY 160

// The longest chain: at most 22 variants (112 bytes hold no more), the
// explicit parents, a region, a script and root, with room to spare.
enum
{
    maxChain = 64,
};

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

static uint32_t KeyText(const mlocLocale* step, char* out);

// Whether a text is a key as the library writes one: a locale id that
// parses, without extensions, and that it writes back byte for byte, so
// that what validation follows is what a lookup meets.
static bool ValidKey(const char* text, uint32_t length)
{
    mlocLocale locale;
    if (length == 0 || mlocParseLocale(text, length, &locale).status != mloc_success)
    {
        return false;
    }
    char key[MAX_KEY];
    return KeyText(&locale, key) == length && memcmp(key, text, length) == 0;
}

static bool ChainsEnd(const Parents* p);

bool mlocValidParents(const DataView* view, DataSpan payload)
{
    (void)view;
    Parents p;
    if (!ReadParents(payload, &p) || mlocLoad64(payload.bytes + 8) != 0)
    {
        return false;
    }
    if (p.count > parentMaxEntries)
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
    return ChainsEnd(&p);
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

// Puts the variant posix among a step's variants, in order: CLDR keys
// POSIX English's data en_US_POSIX, which BCP 47 writes en-US-u-va-posix.
static void AddPosix(mlocLocale* step)
{
    size_t length = VariantsLength(step->tail, sizeof(step->tail));
    size_t at = 0;
    while (at < length)
    {
        size_t end = at;
        while (end < length && step->tail[end] != '-')
        {
            end++;
        }
        size_t common = end - at < 5 ? end - at : 5;
        int order = memcmp(step->tail + at, "posix", common);
        if (order == 0 && end - at == 5)
        {
            // The step has the variant already.
            return;
        }
        if (order > 0 || (order == 0 && end - at > 5))
        {
            break;
        }
        at = end + 1;
    }
    at = at > length ? length : at;
    // Six bytes: "posix" and a hyphen; the variants of a parsed locale
    // leave room, its extensions being dropped from the step.
    if (length + 6 > sizeof(step->tail))
    {
        return;
    }
    char rest[sizeof(step->tail)];
    size_t restLength = length - at;
    memcpy(rest, step->tail + at, restLength);
    size_t n = at;
    if (n > 0 && step->tail[n - 1] != '-')
    {
        step->tail[n++] = '-';
    }
    memcpy(step->tail + n, "posix", 5);
    n += 5;
    if (restLength > 0)
    {
        step->tail[n++] = '-';
        memcpy(step->tail + n, rest, restLength);
        n += restLength;
    }
    memset(step->tail + n, 0, sizeof(step->tail) - n);
}

// Takes the variant posix out of a step's variants; false when it has
// none.
static bool RemovePosix(mlocLocale* step)
{
    size_t length = VariantsLength(step->tail, sizeof(step->tail));
    for (size_t at = 0; at < length;)
    {
        size_t end = at;
        while (end < length && step->tail[end] != '-')
        {
            end++;
        }
        if (end - at == 5 && memcmp(step->tail + at, "posix", 5) == 0)
        {
            // The variant and the hyphen after it (or before it, last).
            size_t from = end < length ? at : at > 0 ? at - 1 : 0;
            size_t to = end < length ? end + 1 : end;
            size_t size = FieldLength(step->tail, sizeof(step->tail));
            memmove(step->tail + from, step->tail + to, size - to);
            memset(step->tail + size - (to - from), 0, to - from);
            return true;
        }
        at = end + 1;
    }
    return false;
}

// Whether a locale has -u-va-posix, which its chain reads as the variant.
static bool HasPosixKeyword(const mlocLocale* locale)
{
    const char* va = nullptr;
    return mlocFindKeyword(locale, "va", &va) == 5 && memcmp(va, "posix", 5) == 0;
}

// The script likely subtags give a packed language and region, or 0
// when there is none.
static uint32_t LikelyScript(const Likely* likely, uint32_t language, uint32_t region)
{
    uint64_t value = 0;
    if (likely == nullptr || !mlocLookupLikely(likely, language, 0, region, &value))
    {
        return 0;
    }
    return (uint32_t)(value >> 11) & 0xFFFFF;
}

// A locale's first step: its language, script and region in key form
// and its variants, without its extensions.
static mlocLocale FirstStep(const mlocDataSet* data, const mlocLocale* locale)
{
    mlocLocale step = {0};
    memcpy(step.language, locale->language, 8);
    memcpy(step.region, locale->region, 4);
    memcpy(step.tail, locale->tail, VariantsLength(locale->tail, sizeof(locale->tail)));
    if (HasPosixKeyword(locale))
    {
        AddPosix(&step);
    }
    uint32_t script = mlocPackScript(locale->script, (uint32_t)FieldLength(locale->script, 4));
    uint32_t region =
        script == 0 ? mlocPackRegion(locale->region, (uint32_t)FieldLength(locale->region, 4)) : 0;
    if (script == 0 && region == 0)
    {
        // Its script is its language's likely one, which the key leaves
        // out.
        return step;
    }
    Likely table;
    const Likely* likely = mlocOpenLikely(data, &table) ? &table : nullptr;
    uint32_t language =
        mlocPackLanguage(locale->language, (uint32_t)FieldLength(locale->language, 8));
    if (script == 0)
    {
        script = LikelyScript(likely, language, region);
    }
    if (script != 0 && script != LikelyScript(likely, language, 0))
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

// Whether the chain from every explicit entry reaches root within
// maxChain steps. A chain from any locale either never meets an entry,
// and then only drops subtags until root, or continues as the chain of
// the first entry it meets; so every chain of a valid file ends.
static bool ChainsEnd(const Parents* p)
{
    for (uint32_t i = 0; i < p->count; i++)
    {
        Entry entry = ReadEntry(p, i);
        mlocLocale step;
        if (mlocParseLocale(entry.child, entry.childLength, &step).status != mloc_success)
        {
            return false;
        }
        int depth = 0;
        while (NextStep(p, (mlocComponent)entry.component, &step))
        {
            if (++depth > maxChain)
            {
                return false;
            }
        }
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
    // A parent that keeps POSIX keeps it as the keyword; one that dropped
    // it drops the keyword too, or the chain would never end.
    bool posix = HasPosixKeyword(locale);
    bool keepsPosix = posix && RemovePosix(&step);
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
    if (posix && !keepsPosix && !mlocRemoveKeyword(&step, "va"))
    {
        return mloc_errorCapacity;
    }
    *parentOut = step;
    return mloc_success;
}

// A walk along a locale's chain: each step's key in turn. The parents
// are read only when the first step does not end the walk. Validation
// has made every chain end within maxChain steps.
typedef struct ChainWalk
{
    const mlocDataSet* data;
    mlocComponent component;
    mlocLocale step;
    Parents parents;
    bool parentsOpen;
    int depth;
    char key[MAX_KEY];
    uint32_t length;
} ChainWalk;

static inline void WalkStart(ChainWalk* w, const mlocDataSet* data, mlocComponent component,
                             const mlocLocale* locale)
{
    w->data = data;
    w->component = component;
    w->parentsOpen = false;
    w->depth = 0;
    w->step = FirstStep(data, locale);
    w->length = KeyText(&w->step, w->key);
}

// Moves to the next step; false at the chain's end, with *okOut false
// when the chain is needed and the data has no parents table.
static inline bool WalkNext(ChainWalk* w, bool* okOut)
{
    *okOut = true;
    // Root ends every chain, parents table or none.
    if ((w->length == 4 && memcmp(w->key, "root", 4) == 0) || w->depth == maxChain)
    {
        return false;
    }
    if (!w->parentsOpen && !OpenParents(w->data, &w->parents))
    {
        *okOut = false;
        return false;
    }
    w->parentsOpen = true;
    if (!NextStep(&w->parents, w->component, &w->step))
    {
        return false;
    }
    w->depth++;
    w->length = KeyText(&w->step, w->key);
    return true;
}

bool mlocVisitLocaleUnits(const mlocDataSet* data, mlocComponent component, uint32_t kind,
                          const mlocLocale* locale, LocaleUnitVisitor visit, void* context)
{
    ChainWalk w;
    WalkStart(&w, data, component, locale);
    // The first 32 sources found to hold no unit of the kind, which the
    // later steps skip.
    uint32_t without = 0;
    bool ok = true;
    do
    {
        for (size_t s = 0; s < data->count; s++)
        {
            DataView view = mlocViewOf(&data->sources[s]);
            uint32_t index = 0;
            DataSpan span;
            bool none = false;
            if ((s < 32 && (without >> s & 1) != 0) ||
                !mlocFindDataLocale(&view, w.key, w.length, &index))
            {
                continue;
            }
            if (!mlocFindDataUnitOrKind(&view, kind, index, &span, &none))
            {
                without |= none && s < 32 ? 1u << s : 0;
            }
            else if (visit(context, span, &view, w.depth))
            {
                return true;
            }
        }
    } while (WalkNext(&w, &ok));
    return ok;
}

// The chain's steps in the sources not in without (the first 32's bits).
static inline bool VisitChain(const mlocDataSet* data, mlocComponent component,
                              const mlocLocale* locale, uint32_t without, LocaleStepVisitor visit,
                              void* context)
{
    ChainWalk w;
    WalkStart(&w, data, component, locale);
    bool ok = true;
    do
    {
        for (size_t s = 0; s < data->count; s++)
        {
            DataView view = mlocViewOf(&data->sources[s]);
            uint32_t index = 0;
            if ((s >= 32 || (without >> s & 1) == 0) &&
                mlocFindDataLocale(&view, w.key, w.length, &index) &&
                visit(context, &view, index, w.depth))
            {
                return true;
            }
        }
    } while (WalkNext(&w, &ok));
    return ok;
}

bool mlocVisitLocaleChain(const mlocDataSet* data, mlocComponent component,
                          const mlocLocale* locale, LocaleStepVisitor visit, void* context)
{
    return VisitChain(data, component, locale, 0, visit, context);
}

bool mlocVisitLocaleChainOf(const mlocDataSet* data, mlocComponent component, uint32_t kind,
                            const mlocLocale* locale, LocaleStepVisitor visit, void* context)
{
    uint32_t without = 0;
    for (size_t s = 0; s < data->count && s < 32; s++)
    {
        DataView view = mlocViewOf(&data->sources[s]);
        without |= mlocDataHasKind(&view, kind) ? 0 : 1u << s;
    }
    return VisitChain(data, component, locale, without, visit, context);
}

typedef struct Found
{
    DataSpan* span;
    bool* fellBack;
    DataView* view;
    bool found;
} Found;

static bool TakeFirst(void* context, DataSpan unit, const DataView* view, int depth)
{
    Found* f = context;
    *f->span = unit;
    *f->fellBack = depth > 0;
    if (f->view != nullptr)
    {
        *f->view = *view;
    }
    f->found = true;
    return true;
}

bool mlocFindLocaleUnit(const mlocDataSet* data, mlocComponent component, uint32_t kind,
                        const mlocLocale* locale, DataSpan* spanOut, bool* fellBackOut,
                        DataView* viewOut)
{
    Found f = {.span = spanOut, .fellBack = fellBackOut, .view = viewOut};
    return mlocVisitLocaleUnits(data, component, kind, locale, TakeFirst, &f) && f.found;
}
