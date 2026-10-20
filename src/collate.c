// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "maul-locale/collate.h"

#include "catalog_hash.h"
#include "collation_compare.h"
#include "collation_key.h"
#include "collation_reorder.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "locale_keyword.h"

#include "maul-unicode/properties.h"

#include <stddef.h>
#include <string.h>

enum
{
    collatorStamp = 0x43,
    // The sort keys' format, a seed of the version: raised whenever the
    // key writer's bytes change.
    keyFormat = 1,
};

typedef struct Collator
{
    uint32_t stamp;
    // 1 when the reordering is in use (a byte, read only after the stamp).
    uint8_t reordered;
    // 1 when the locale's tailoring maps code points.
    uint8_t tailored;
    // The keys' version: the format, the root unit's bytes and the
    // tailoring's, hashed.
    uint64_t version;
    CollationRoot root;
    CollationRoot tailoring;
    // Its reorder pointer is set at each compare: a collator is copied.
    CollationSettings settings;
    CollationReorder reorder;
} Collator;

static_assert(sizeof(Collator) <= sizeof(mlocCollator), "a collator fits its value");
static_assert(offsetof(Collator, stamp) == 0, "the stamp is read first, alone");

// The collation root in the first source that has one, and the keys'
// version over its bytes.
static bool FindRoot(const mlocDataSet* data, CollationRoot* root, uint64_t* version)
{
    for (size_t i = 0; i < data->count; i++)
    {
        DataView view = mlocViewOf(&data->sources[i]);
        DataSpan span = {0};
        if (mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span))
        {
            *version = mlocXxh64(span.bytes, span.size, keyFormat);
            return mlocReadCollationRoot(span, root);
        }
    }
    return false;
}

// A walk along a locale's collation chain for a type, which also finds
// the chain's first default type.
typedef struct TypeWalk
{
    const char* name;
    size_t length;
    DataSpan found;
    bool hit;
    const char* defaultName;
    size_t defaultLength;
} TypeWalk;

static bool VisitTypes(void* context, DataSpan unit, const DataView* view, int depth)
{
    (void)view;
    (void)depth;
    TypeWalk* w = context;
    CollationTypes t = mlocReadCollationTypes(unit);
    if (w->defaultLength == 0 && t.defaultLength > 0)
    {
        w->defaultName = (const char*)t.defaultName;
        w->defaultLength = t.defaultLength;
    }
    w->hit = w->length > 0 && mlocFindCollationType(&t, w->name, w->length, &w->found);
    return w->hit;
}

static bool Walk(const mlocDataSet* data, const mlocLocale* locale, TypeWalk* w, const char* name,
                 size_t length)
{
    w->name = name;
    w->length = length;
    w->hit = false;
    return mlocVisitLocaleUnits(data, mloc_componentCollation, kindCollationTailorings, locale,
                                VisitTypes, w);
}

// The locale's tailoring as ICU4C's loader finds it: the co type along
// the collation chain, else the chain's default type, else standard;
// *found false for the root order. mloc_errorNotFound when the chain
// needs the parents table and the data has none.
static mlocResult FindTailoring(const mlocDataSet* data, const mlocLocale* locale,
                                DataSpan* tailoringOut, bool* found)
{
    *found = false;
    if (!mlocDataSetHasKind(data, kindCollationTailorings))
    {
        return mloc_success;
    }
    TypeWalk w = {0};
    const char* co = nullptr;
    size_t coLength = mlocFindKeyword(locale, "co", &co);
    if (!Walk(data, locale, &w, co, coLength))
    {
        return mloc_errorNotFound;
    }
    static const char s_standard[] = "standard";
    const char* next = w.defaultLength > 0 ? w.defaultName : s_standard;
    size_t nextLength = w.defaultLength > 0 ? w.defaultLength : sizeof(s_standard) - 1;
    for (int tries = 0; !w.hit && tries < 2; tries++)
    {
        bool tried = coLength == nextLength && memcmp(co, next, nextLength) == 0;
        if (!tried && !Walk(data, locale, &w, next, nextLength))
        {
            return mloc_errorNotFound;
        }
        co = next;
        coLength = nextLength;
        next = s_standard;
        nextLength = sizeof(s_standard) - 1;
    }
    *tailoringOut = w.found;
    *found = w.hit;
    return mloc_success;
}

static bool Is(const char* value, size_t length, const char* text)
{
    return length == strlen(text) && memcmp(value, text, length) == 0;
}

// The index of the value among the texts plus one, or 0.
static uint32_t Choice(const char* value, size_t length, const char* const* texts, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
    {
        if (Is(value, length, texts[i]))
        {
            return i + 1;
        }
    }
    return 0;
}

static char Lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

// A reorder code from its text (any case): a group, others, or a script.
static bool Code(const char* text, size_t length, uint32_t* codeOut)
{
    static const char* const groups[] = {"space", "punct", "symbol", "currency", "digit"};
    char word[8];
    if (length == 0 || length > sizeof(word))
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        word[i] = Lower(text[i]);
    }
    uint32_t group = Choice(word, length, groups, 5);
    bool letters = length == 4;
    for (size_t i = 0; letters && i < 4; i++)
    {
        letters = word[i] >= 'a' && word[i] <= 'z';
    }
    *codeOut = group != 0                   ? collationGroupSpace + group - 1
               : Is(word, length, "others") ? MUNI_SCRIPT('Z', 'z', 'z', 'z')
               : letters ? MUNI_SCRIPT(word[0] - 'a' + 'A', word[1], word[2], word[3])
                         : 0;
    return *codeOut != 0;
}

// The reordering a kr text asks for; mloc_errorInvalid for a text that
// is not a list of codes.
static mlocResult Reorder(const CollationRoot* root, const char* text, size_t length,
                          CollationReorder* out, bool* changes)
{
    uint32_t codes[collationMaxReorderCodes];
    size_t count = 0;
    for (size_t at = 0; at < length;)
    {
        size_t n = 0;
        while (at + n < length && text[at + n] != '-')
        {
            n++;
        }
        if (count == collationMaxReorderCodes || !Code(text + at, n, &codes[count]))
        {
            return mloc_errorInvalid;
        }
        count++;
        at += n + 1;
        if (at == length && text[length - 1] == '-')
        {
            return mloc_errorInvalid;
        }
    }
    return mlocCollationMakeReorder(root, codes, count, out, changes);
}

// The settings the locale's -u keys ask for over the root's defaults; a
// value that is not one of a key's is passed over.
static void FromLocale(Collator* c, const mlocLocale* locale)
{
    static const char* const strengths[] = {"level1", "level2", "level3", "level4", "identic"};
    static const char* const alternates[] = {"noignore", "shifted"};
    static const char* const groups[] = {"space", "punct", "symbol", "currency"};
    static const char* const cases[] = {"false", "lower", "upper"};
    static const char* const flags[] = {"false", "true"};
    CollationSettings* s = &c->settings;
    const char* v = nullptr;
    size_t n = mlocFindKeyword(locale, "ks", &v);
    uint32_t k = Choice(v, n, strengths, 5);
    s->strength = k != 0 ? k : s->strength;
    n = mlocFindKeyword(locale, "ka", &v);
    k = Choice(v, n, alternates, 2);
    s->shifted = k != 0 ? k == 2 : s->shifted;
    n = mlocFindKeyword(locale, "kv", &v);
    k = Choice(v, n, groups, 4);
    s->variableTop =
        k != 0 ? mlocCollationLastPrimary(&c->root, collationGroupSpace + k - 1) : s->variableTop;
    n = mlocFindKeyword(locale, "kf", &v);
    k = Choice(v, n, cases, 3);
    s->caseFirst = k != 0 ? k - 1 : s->caseFirst;
    // A key alone is "true", which canonical form leaves out.
    const char* const switches[] = {"kc", "kn", "kb"};
    bool* fields[] = {&s->caseLevel, &s->numeric, &s->backwards};
    for (size_t i = 0; i < 3; i++)
    {
        n = mlocFindKeyword(locale, switches[i], &v);
        k = n == 0 && mlocHasKeyword(locale, switches[i]) ? 2 : Choice(v, n, flags, 2);
        *fields[i] = k != 0 ? k == 2 : *fields[i];
    }
    n = mlocFindKeyword(locale, "kr", &v);
    CollationReorder made;
    bool changes = false;
    if (n > 0 && Reorder(&c->root, v, n, &made, &changes) == mloc_success)
    {
        c->reorder = made;
        c->reordered = changes ? 1 : 0;
    }
}

static bool OptionsValid(const mlocCollatorOptions* o)
{
    return (uint32_t)o->strength <= mloc_strengthIdentical &&
           (uint32_t)o->alternate <= mloc_alternateShifted &&
           (uint32_t)o->maxVariable <= mloc_maxVariableCurrency &&
           (uint32_t)o->caseFirst <= mloc_caseFirstUpper &&
           (uint32_t)o->caseLevel <= mloc_switchOn && (uint32_t)o->numeric <= mloc_switchOn &&
           (uint32_t)o->backwards <= mloc_switchOn &&
           (o->reorder != nullptr || o->reorderLength == 0);
}

// The options of the call over the locale's.
static mlocResult FromOptions(Collator* c, const mlocCollatorOptions* o)
{
    CollationSettings* s = &c->settings;
    s->strength = o->strength != mloc_strengthOfLocale ? (uint32_t)o->strength : s->strength;
    s->shifted =
        o->alternate != mloc_alternateOfLocale ? o->alternate == mloc_alternateShifted : s->shifted;
    s->variableTop =
        o->maxVariable != mloc_maxVariableOfLocale
            ? mlocCollationLastPrimary(&c->root, collationGroupSpace + (uint32_t)o->maxVariable - 1)
            : s->variableTop;
    s->caseFirst =
        o->caseFirst != mloc_caseFirstOfLocale ? (uint32_t)o->caseFirst - 1 : s->caseFirst;
    s->caseLevel =
        o->caseLevel != mloc_switchOfLocale ? o->caseLevel == mloc_switchOn : s->caseLevel;
    s->numeric = o->numeric != mloc_switchOfLocale ? o->numeric == mloc_switchOn : s->numeric;
    s->backwards =
        o->backwards != mloc_switchOfLocale ? o->backwards == mloc_switchOn : s->backwards;
    if (o->reorder == nullptr)
    {
        return mloc_success;
    }
    CollationReorder made;
    bool changes = false;
    mlocResult result = Reorder(&c->root, o->reorder, o->reorderLength, &made, &changes);
    if (result == mloc_success)
    {
        c->reorder = made;
        c->reordered = changes ? 1 : 0;
    }
    return result;
}

// The tailoring's settings and reordering over the root's defaults, as
// the -u- keys and options of the call later override them.
static mlocResult FromTailoring(Collator* c, DataSpan span)
{
    CollationTailoring t;
    if (!mlocReadCollationTailoring(span, &t))
    {
        return mloc_errorInvalid;
    }
    c->version = mlocXxh64(span.bytes, span.size, c->version);
    if (t.mappedCount > 0)
    {
        c->tailoring = t.view;
        c->tailored = 1;
    }
    uint32_t w = t.settings;
    mlocCollatorOptions o = {
        .strength = (mlocCollationStrength)(w & 7),
        .alternate = (mlocCollationAlternate)(w >> 3 & 3),
        .maxVariable = (mlocCollationMaxVariable)(w >> 5 & 7),
        .caseFirst = (mlocCollationCaseFirst)(w >> 8 & 3),
        .caseLevel = (mlocCollationSwitch)(w >> 10 & 3),
        .numeric = (mlocCollationSwitch)(w >> 12 & 3),
        .backwards = (mlocCollationSwitch)(w >> 14 & 3),
    };
    mlocResult result = FromOptions(c, &o);
    if (result != mloc_success || (w & collationReorderGiven) == 0)
    {
        return result;
    }
    uint32_t codes[collationMaxReorderCodes];
    for (uint32_t i = 0; i < t.reorderCount; i++)
    {
        codes[i] = mlocLoad32(t.reorder + (size_t)i * 4);
    }
    bool changes = false;
    result = mlocCollationMakeReorder(&c->root, codes, t.reorderCount, &c->reorder, &changes);
    c->reordered = result == mloc_success && changes ? 1 : 0;
    return result;
}

mlocResult mlocInitCollator(mlocCollator* collatorOut, const mlocDataSet* data,
                            const mlocLocale* locale, const mlocCollatorOptions* options)
{
    mlocCollatorOptions o = options != nullptr ? *options : (mlocCollatorOptions){0};
    if (collatorOut == nullptr || data == nullptr || locale == nullptr || !OptionsValid(&o))
    {
        return mloc_errorInvalid;
    }
    static Collator s_zero;
    Collator c = s_zero;
    if (!FindRoot(data, &c.root, &c.version))
    {
        return mloc_errorNotFound;
    }
    c.stamp = collatorStamp;
    c.settings = mlocCollationDefaults(&c.root);
    DataSpan tailoring = {0};
    bool tailored = false;
    mlocResult result = FindTailoring(data, locale, &tailoring, &tailored);
    if (result == mloc_success && tailored)
    {
        result = FromTailoring(&c, tailoring);
    }
    if (result != mloc_success)
    {
        return result;
    }
    FromLocale(&c, locale);
    result = FromOptions(&c, &o);
    if (result != mloc_success)
    {
        return result;
    }
    memset(collatorOut, 0, sizeof(*collatorOut));
    memcpy(collatorOut, &c, sizeof(c));
    return mloc_success;
}

// A collator the library made, copied out with its settings pointing at
// its reordering; false for any other.
static bool ReadCollator(const mlocCollator* collator, Collator* c)
{
    uint32_t stamp = 0;
    if (collator == nullptr)
    {
        return false;
    }
    memcpy(&stamp, collator, sizeof(stamp));
    if (stamp != collatorStamp)
    {
        return false;
    }
    memcpy(c, collator, sizeof(*c));
    c->settings.reorder = c->reordered != 0 ? &c->reorder : nullptr;
    c->settings.tailoring = c->tailored != 0 ? &c->tailoring : nullptr;
    return true;
}

mlocResult mlocCompare(const mlocCollator* collator, const char* a, size_t aLength, const char* b,
                       size_t bLength, int* orderOut)
{
    Collator c;
    if (orderOut == nullptr || (a == nullptr && aLength > 0) || (b == nullptr && bLength > 0) ||
        !ReadCollator(collator, &c))
    {
        return mloc_errorInvalid;
    }
    *orderOut = mlocCollationCompare(&c.root, &c.settings, (const uint8_t*)(a != nullptr ? a : ""),
                                     aLength, (const uint8_t*)(b != nullptr ? b : ""), bLength);
    return mloc_success;
}

mlocResult mlocGetSortKey(const mlocCollator* collator, const char* text, size_t length,
                          uint8_t* key, size_t capacity, size_t* lengthOut)
{
    Collator c;
    if (lengthOut == nullptr || (text == nullptr && length > 0) ||
        (key == nullptr && capacity > 0) || !ReadCollator(collator, &c))
    {
        return mloc_errorInvalid;
    }
    *lengthOut = mlocCollationSortKey(
        &c.root, &c.settings, (const uint8_t*)(text != nullptr ? text : ""), length, key, capacity);
    return *lengthOut <= capacity ? mloc_success : mloc_errorCapacity;
}

mlocResult mlocGetCollationVersion(const mlocCollator* collator, uint64_t* versionOut)
{
    Collator c;
    if (versionOut == nullptr || !ReadCollator(collator, &c))
    {
        return mloc_errorInvalid;
    }
    *versionOut = c.version;
    return mloc_success;
}
