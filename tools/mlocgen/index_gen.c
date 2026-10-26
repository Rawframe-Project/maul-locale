// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "index_gen.h"

#include "kinds.h"
#include "set_pattern.h"
#include "table_gen.h"

#include "maul-unicode/case.h"
#include "maul-unicode/encoding.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxKey = 64,
    maxCodePoints = 4096,
    maxStrings = 256,
    maxLabels = maxCodePoints + maxStrings,
    // A label's UTF-8, which its unit gives one byte.
    maxLabel = 64,
};

typedef struct Label
{
    uint8_t length;
    char text[maxLabel];
} Label;

// An exemplar set as read: its code points and its strings.
typedef struct Exemplars
{
    uint32_t codePoints[maxCodePoints];
    int codePointCount;
    Label strings[maxStrings];
    int stringCount;
} Exemplars;

// A locale's resolved unit.
typedef struct Unit
{
    uint8_t* bytes;
    size_t size;
} Unit;

static const char s_index[] = "//ldml/characters/exemplarCharacters[@type=\"index\"]";
static const char s_standard[] = "//ldml/characters/exemplarCharacters";
// The explicitly empty value (cldr.h).
static const char s_empty[] = "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85";

// The Hangul syllables ICU4C keeps, one a leading consonant.
static const uint32_t s_hangul[] = {0xAC00, 0xB098, 0xB2E4, 0xB77C, 0xB9C8, 0xBC14, 0xC0AC,
                                    0xC544, 0xC790, 0xCC28, 0xCE74, 0xD0C0, 0xD30C, 0xD558};
// The Ethiopic bases ICU4C keeps, each the first of a syllable's eight.
static const uint32_t s_ethiopic[] = {
    0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230, 0x1238, 0x1240, 0x1248, 0x1250,
    0x1258, 0x1260, 0x1268, 0x1270, 0x1278, 0x1280, 0x1288, 0x1290, 0x1298, 0x12A0, 0x12A8,
    0x12B0, 0x12B8, 0x12C0, 0x12C8, 0x12D0, 0x12D8, 0x12E0, 0x12E8, 0x12F0, 0x12F8, 0x1300,
    0x1308, 0x1310, 0x1318, 0x1320, 0x1328, 0x1330, 0x1338, 0x1340, 0x1348, 0x1350, 0x1358};

static void AddCodePoint(Exemplars* e, uint32_t cp)
{
    if (e->codePointCount == maxCodePoints)
    {
        GenDie("an exemplar set of too many code points", "");
    }
    e->codePoints[e->codePointCount++] = cp;
}

static void TakeRange(void* context, uint32_t first, uint32_t last)
{
    for (uint32_t cp = first; cp <= last; cp++)
    {
        AddCodePoint(context, cp);
    }
}

static void TakeString(void* context, const char* text, size_t length)
{
    Exemplars* e = context;
    if (e->stringCount == maxStrings || length > maxLabel)
    {
        GenDie("an exemplar set of too many or too long strings", text);
    }
    Label* s = &e->strings[e->stringCount++];
    s->length = (uint8_t)length;
    memcpy(s->text, text, length);
}

static bool HasIn(const Exemplars* e, uint32_t first, uint32_t last)
{
    for (int i = 0; i < e->codePointCount; i++)
    {
        if (e->codePoints[i] >= first && e->codePoints[i] <= last)
        {
            return true;
        }
    }
    return false;
}

static bool Has(const Exemplars* e, uint32_t cp)
{
    return HasIn(e, cp, cp);
}

// Removes the code points of a range.
static void Remove(Exemplars* e, uint32_t first, uint32_t last)
{
    int out = 0;
    for (int i = 0; i < e->codePointCount; i++)
    {
        if (e->codePoints[i] < first || e->codePoints[i] > last)
        {
            e->codePoints[out++] = e->codePoints[i];
        }
    }
    e->codePointCount = out;
}

// AlphabeticIndex::addIndexExemplars's labels from standard exemplars:
// a to z when one is there or the set is empty, Hangul and Ethiopic cut
// to the syllables above.
static void Synthesize(Exemplars* e)
{
    if (HasIn(e, 0x61, 0x7A) || (e->codePointCount == 0 && e->stringCount == 0))
    {
        TakeRange(e, 0x61, 0x7A);
    }
    if (HasIn(e, 0xAC00, 0xD7A3))
    {
        Remove(e, 0xAC00, 0xD7A3);
        for (size_t i = 0; i < sizeof(s_hangul) / sizeof(s_hangul[0]); i++)
        {
            AddCodePoint(e, s_hangul[i]);
        }
    }
    if (HasIn(e, 0x1200, 0x137F))
    {
        bool kept[sizeof(s_ethiopic) / sizeof(s_ethiopic[0])];
        for (size_t i = 0; i < sizeof(s_ethiopic) / sizeof(s_ethiopic[0]); i++)
        {
            kept[i] = Has(e, s_ethiopic[i]);
        }
        Remove(e, 0x1200, 0x137F);
        for (size_t i = 0; i < sizeof(s_ethiopic) / sizeof(s_ethiopic[0]); i++)
        {
            if (kept[i])
            {
                AddCodePoint(e, s_ethiopic[i]);
            }
        }
    }
}

// The case rules ICU4C's toUpper(locale) takes for a locale's language:
// false for a language whose rules Maul Unicode does not have.
static bool LanguageRules(const char* locale, muniCaseLanguage* out)
{
    size_t length = strcspn(locale, "_");
    *out = muni_caseDefault;
    if ((length == 2 && (memcmp(locale, "tr", 2) == 0 || memcmp(locale, "az", 2) == 0)))
    {
        *out = muni_caseTurkic;
    }
    else if (length == 2 && memcmp(locale, "lt", 2) == 0)
    {
        *out = muni_caseLithuanian;
    }
    return !(length == 2 && (memcmp(locale, "el", 2) == 0 || memcmp(locale, "hy", 2) == 0));
}

static Label Upper(const Label* in, muniCaseLanguage language)
{
    Label out = {0};
    size_t needed = 0;
    muniTextResult r = muniConvertCase(in->text, in->length, muni_caseUpper, language,
                                       muni_convertStrict, out.text, sizeof(out.text), &needed);
    if (r.status != muni_success)
    {
        GenDie("an index label's uppercase does not fit", "");
    }
    out.length = (uint8_t)needed;
    return out;
}

static Label Encoded(uint32_t cp)
{
    Label l = {0};
    size_t size = 0;
    if (muniEncodeUtf8(cp, l.text, &size) != muni_success)
    {
        GenDie("an exemplar that is no scalar value", "");
    }
    l.length = (uint8_t)size;
    return l;
}

// Code point order, which UTF-8's bytes keep.
static int CompareLabels(const void* a, const void* b)
{
    const Label* x = a;
    const Label* y = b;
    size_t common = x->length < y->length ? x->length : y->length;
    int order = memcmp(x->text, y->text, common);
    return order != 0 ? order : (int)x->length - (int)y->length;
}

// The unit of a set's items, upper-cased when they were synthesized.
static Unit UnitOf(const Exemplars* e, bool upper, muniCaseLanguage language)
{
    static Label s_labels[maxLabels];
    int count = 0;
    for (int i = 0; i < e->codePointCount; i++)
    {
        s_labels[count++] = Encoded(e->codePoints[i]);
    }
    for (int i = 0; i < e->stringCount; i++)
    {
        s_labels[count++] = e->strings[i];
    }
    for (int i = 0; upper && i < count; i++)
    {
        s_labels[i] = Upper(&s_labels[i], language);
    }
    GenSort(s_labels, (size_t)count, sizeof(s_labels[0]), CompareLabels);
    Unit unit = {GenAllocate(4 + (size_t)count * (1 + maxLabel)), 4};
    uint32_t distinct = 0;
    for (int i = 0; i < count; i++)
    {
        if (i > 0 && CompareLabels(&s_labels[i], &s_labels[i - 1]) == 0)
        {
            continue;
        }
        unit.bytes[unit.size] = s_labels[i].length;
        memcpy(unit.bytes + unit.size + 1, s_labels[i].text, s_labels[i].length);
        unit.size += 1 + (size_t)s_labels[i].length;
        distinct++;
    }
    GenPut(unit.bytes, distinct, 4);
    return unit;
}

static void ReadSet(const Leaf* leaf, Exemplars* e)
{
    if (strcmp(leaf->text, s_empty) == 0)
    {
        GenDie("an exemplar set explicitly empty", leaf->text);
    }
    GenReadSetPattern(leaf->text, &(SetSink){TakeRange, TakeString, e});
}

static Unit Resolve(Cldr* cldr, const char* locale)
{
    static Exemplars s_set;
    s_set = (Exemplars){0};
    const Leaf* leaf = CldrResolve(cldr, locale, s_index, componentMain);
    if (leaf != nullptr)
    {
        ReadSet(leaf, &s_set);
        return UnitOf(&s_set, false, muni_caseDefault);
    }
    leaf = CldrResolve(cldr, locale, s_standard, componentMain);
    if (leaf != nullptr)
    {
        ReadSet(leaf, &s_set);
    }
    muniCaseLanguage language = muni_caseDefault;
    if (!LanguageRules(locale, &language))
    {
        GenDie("index labels to make with case rules Maul Unicode does not have", locale);
    }
    Synthesize(&s_set);
    return UnitOf(&s_set, true, language);
}

static int IndexOf(const IndexInput* in, const char* id)
{
    for (int i = 0; i < in->count; i++)
    {
        if (strcmp(in->ids[i], id) == 0)
        {
            return i;
        }
    }
    GenDie("a parent locale without a file", id);
}

// The nearest locale along a locale's chain whose key differs from its
// own, or null for root.
static const char* NextKeyed(const IndexInput* in, const char* locale, const char* key)
{
    for (const char* at = CldrParent(in->cldr, locale, componentMain); at != nullptr;
         at = CldrParent(in->cldr, at, componentMain))
    {
        char other[maxKey];
        KeyOf(in->keys, at, other, sizeof(other));
        if (strcmp(other, key) != 0)
        {
            return at;
        }
    }
    return nullptr;
}

static bool Same(const Unit* a, const Unit* b)
{
    return a->size == b->size && memcmp(a->bytes, b->bytes, a->size) == 0;
}

void GenerateIndexUnits(const IndexInput* in, DataFile* file)
{
    Unit* all = GenAllocate(sizeof(Unit) * (size_t)in->count);
    for (int i = 0; i < in->count; i++)
    {
        all[i] = Resolve(in->cldr, in->ids[i]);
    }
    for (int i = 0; i < in->count; i++)
    {
        char key[maxKey];
        KeyOf(in->keys, in->ids[i], key, sizeof(key));
        const char* parent = CldrParent(in->cldr, in->ids[i], componentMain);
        char parentKey[maxKey] = "";
        if (parent != nullptr)
        {
            KeyOf(in->keys, parent, parentKey, sizeof(parentKey));
        }
        if (parent != nullptr && strcmp(parentKey, key) == 0)
        {
            if (!Same(&all[IndexOf(in, parent)], &all[i]))
            {
                GenDie("a locale's index labels differ from those of its parent's key", in->ids[i]);
            }
            continue;
        }
        const char* next = NextKeyed(in, in->ids[i], key);
        if (next == nullptr || !Same(&all[IndexOf(in, next)], &all[i]))
        {
            DataFileAdd(file, kindCollationIndex, key, all[i].bytes, all[i].size);
        }
    }
    for (int i = 0; i < in->count; i++)
    {
        free(all[i].bytes);
    }
    free(all);
}
