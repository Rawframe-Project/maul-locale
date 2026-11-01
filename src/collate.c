// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "maul-locale/collate.h"

#include "catalog_hash.h"
#include "collation_compare.h"
#include "collation_fast_compare.h"
#include "collation_key.h"
#include "collation_reorder.h"
#include "collation_tables.h"
#include "collator.h"
#include "data_format.h"
#include "data_view.h"
#include "invariant.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "locale_keyword.h"
#include "locale_valid.h"

#include "maul-unicode/properties.h"

#include <stddef.h>
#include <string.h>

enum
{
    // The sort keys' format, a seed of the version: raised whenever the
    // key writer's bytes change.
    keyFormat = 2,
};

// The collation root in the first source that has one, and the keys'
// version: the key format and the root's stored hash, which its
// validator checked when the data was opened.
static bool FindRoot(const mlocDataSet* data, CollationRoot* root, uint64_t* version)
{
    for (size_t i = 0; i < data->count; i++)
    {
        DataView view = mlocViewOf(&data->sources[i]);
        DataSpan span = {0};
        if (mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span))
        {
            if (!mlocViewCollationRoot(span, root))
            {
                return false;
            }
            *version = mlocXxh64(span.bytes + collationRootHashAt, 8, keyFormat);
            return true;
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
    // The unit of types the tailoring was found in.
    const uint8_t* unit;
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
    w->unit = w->hit ? unit.bytes : w->unit;
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

static bool Is(const char* value, size_t length, const char* text);

// ICU4C's CollationLoader's types tried: the asked one, then a
// "search..." one as "search", then the default, then standard.
typedef struct Tries
{
    const char* name;
    size_t length;
    // The default tried.
    bool fallback;
} Tries;

// The next type to try after one the chain lacks; the caller stops once
// standard is tried, so each step is taken at most once (a type equal to
// one tried before only misses again).
static void NextType(Tries* t, const TypeWalk* w)
{
    if (t->length > 6 && memcmp(t->name, "search", 6) == 0)
    {
        t->length = 6;
    }
    else if (!t->fallback)
    {
        *t = (Tries){w->defaultName, w->defaultLength, true};
    }
    else
    {
        t->name = "standard";
        t->length = 8;
    }
}

// The locale's tailoring as ICU4C's CollationLoader finds it: the co
// type along the collation chain, where root always has standard (the
// root order); a missing "search..." type tried as "search", then the
// chain's default type, then standard. A private type is never chosen
// (UTS #35: they are for imports). *found false for the root order;
// *unitOut the unit of types it is in. mloc_errorNotFound when the chain
// needs the parents table and the data has none.
static mlocResult FindTailoring(const mlocDataSet* data, const mlocLocale* locale,
                                DataSpan* tailoringOut, const uint8_t** unitOut, bool* found)
{
    *found = false;
    if (!mlocDataSetHasKind(data, kindCollationTailorings))
    {
        return mloc_success;
    }
    TypeWalk w = {0};
    const char* co = nullptr;
    size_t coLength = mlocFindKeyword(locale, "co", &co);
    if (coLength >= 8 && memcmp(co, "private-", 8) == 0)
    {
        coLength = 0;
    }
    if (!Walk(data, locale, &w, co, coLength))
    {
        return mloc_errorNotFound;
    }
    if (w.defaultLength == 0)
    {
        w.defaultName = "standard";
        w.defaultLength = 8;
    }
    Tries t = {co, coLength, false};
    if (coLength == 0)
    {
        t = (Tries){w.defaultName, w.defaultLength, true};
        if (!Walk(data, locale, &w, t.name, t.length))
        {
            return mloc_errorNotFound;
        }
    }
    while (!w.hit && !Is(t.name, t.length, "standard"))
    {
        NextType(&t, &w);
        if (!Walk(data, locale, &w, t.name, t.length))
        {
            return mloc_errorNotFound;
        }
    }
    *tailoringOut = w.found;
    *unitOut = w.unit;
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
               : Is(word, length, "others") ? MUNI_MAKE_SCRIPT('Z', 'z', 'z', 'z')
               : letters ? MUNI_MAKE_SCRIPT(word[0] - 'a' + 'A', word[1], word[2], word[3])
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

// The radical-stroke Han table in the first source that has one, its
// bytes joining the version; false when none has.
static bool FindHan(const mlocDataSet* data, Collator* c)
{
    for (size_t i = 0; i < data->count; i++)
    {
        DataView view = mlocViewOf(&data->sources[i]);
        DataSpan span = {0};
        if (mlocFindDataUnit(&view, kindCollationHanRadical, dataNoLocale, &span))
        {
            c->han = mlocReadCollationHan(span);
            c->radical = 1;
            c->version = mlocXxh64(span.bytes + collationHanHashAt, 8, c->version);
            return true;
        }
    }
    return false;
}

// The tailoring's settings and reordering over the root's defaults, as
// the -u- keys and options of the call later override them; the version
// takes the stored hash of its unit of types.
static mlocResult FromTailoring(Collator* c, const mlocDataSet* data, DataSpan span,
                                const uint8_t* unit)
{
    CollationTailoring t;
    if (!mlocViewCollationTailoring(span, &t))
    {
        return mloc_errorInvalid;
    }
    c->version = mlocXxh64(unit + collationTypesHashAt, 8, c->version);
    if ((t.settings & collationHanRadical) != 0 && !FindHan(data, c))
    {
        return mloc_errorNotFound;
    }
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
    // The reordering the generator made from the rules' codes, stored.
    if (t.reordering != nullptr)
    {
        mlocCollationLoadReorder(t.reordering, t.reorderRanges, &c->reorder);
    }
    c->reordered = t.reordering != nullptr ? 1 : 0;
    return mloc_success;
}

mlocResult mlocInitCollator(const mlocDataSet* data, const mlocLocale* locale,
                            const mlocCollatorOptions* options, mlocCollator* collatorOut)
{
    mlocCollatorOptions o = options != nullptr ? *options : (mlocCollatorOptions){0};
    if (collatorOut == nullptr || data == nullptr || !mlocLocaleValid(locale) || !OptionsValid(&o))
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
    const uint8_t* unit = nullptr;
    bool tailored = false;
    mlocResult result = FindTailoring(data, locale, &tailoring, &unit, &tailored);
    if (result == mloc_success && tailored)
    {
        result = FromTailoring(&c, data, tailoring, unit);
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
    mlocSetCollatorFastLatin(&c);
    // The collator, then zeros past it: the value's bytes are all set.
    memcpy(collatorOut, &c, sizeof(c));
    memset((uint8_t*)collatorOut + sizeof(c), 0, sizeof(*collatorOut) - sizeof(c));
    return mloc_success;
}

// The general path's parts read, then the texts' identical prefix.
static size_t Prefix(const mlocCollator* collator, Collator* c, NfdText x, NfdText y)
{
    mlocReadCollatorRest(collator, c);
    return mlocCollationIdenticalPrefix(&c->root, &c->settings, x, y);
}

mlocResult mlocCollator_Compare(const mlocCollator* collator, const char* a, size_t aLength,
                                const char* b, size_t bLength, int* orderOut)
{
    Collator c;
    if (orderOut == nullptr || (a == nullptr && aLength > 0) || (b == nullptr && bLength > 0) ||
        !mlocReadCollatorHead(collator, &c))
    {
        return mloc_errorInvalid;
    }
    NfdText x = mlocNfdUtf8((const uint8_t*)(a != nullptr ? a : ""), aLength);
    NfdText y = mlocNfdUtf8((const uint8_t*)(b != nullptr ? b : ""), bLength);
    // The identical prefix skipped before both compares, as ICU4C's compare
    // does. Its unsafe ranges are the root's and the tailoring's, read with
    // the general path's parts, so only texts that begin alike read them
    // before the fast compare.
    bool alike = aLength > 0 && bLength > 0 && x.bytes[0] == y.bytes[0];
    size_t p = alike ? Prefix(collator, &c, x, y) : 0;
    int order = fastLatinBail;
    if (c.fastLatin != 0)
    {
        mlocReadCollatorFast(collator, &c);
        order = mlocFastLatinCompare(&c.fast, x.bytes + p, aLength - p, y.bytes + p, bLength - p);
    }
    if (order == fastLatinBail)
    {
        if (!alike)
        {
            mlocReadCollatorRest(collator, &c);
        }
        order = mlocCollationCompareFrom(&c.root, &c.settings, x, y, p);
    }
    *orderOut = order;
    return mloc_success;
}

mlocResult mlocCollator_CompareUtf16(const mlocCollator* collator, const uint16_t* a,
                                     size_t aLength, const uint16_t* b, size_t bLength,
                                     int* orderOut)
{
    Collator c;
    if (orderOut == nullptr || (a == nullptr && aLength > 0) || (b == nullptr && bLength > 0) ||
        !mlocReadCollatorHead(collator, &c))
    {
        return mloc_errorInvalid;
    }
    static const uint16_t none[1] = {0};
    NfdText x = mlocNfdUtf16(a != nullptr ? a : none, aLength);
    NfdText y = mlocNfdUtf16(b != nullptr ? b : none, bLength);
    bool alike = aLength > 0 && bLength > 0 && x.units[0] == y.units[0];
    size_t p = alike ? Prefix(collator, &c, x, y) : 0;
    int order = fastLatinBail;
    if (c.fastLatin != 0)
    {
        mlocReadCollatorFast(collator, &c);
        order =
            mlocFastLatinCompareUtf16(&c.fast, x.units + p, aLength - p, y.units + p, bLength - p);
    }
    if (order == fastLatinBail)
    {
        if (!alike)
        {
            mlocReadCollatorRest(collator, &c);
        }
        order = mlocCollationCompareFrom(&c.root, &c.settings, x, y, p);
    }
    *orderOut = order;
    return mloc_success;
}

enum
{
    // Holds the key of every word and every line of up to 80 characters
    // in a probe of English text; a longer key is written twice.
    keyScratchSize = 256,
};

// Writes a text's key into keyOut when it fits, else only its length:
// the key goes into a scratch first, and a key longer than the scratch
// is written again straight into keyOut once it is known to fit.
static mlocResult WriteKey(const Collator* c, NfdText text, uint8_t* keyOut, size_t capacity,
                           size_t* lengthOut)
{
    uint8_t scratch[keyScratchSize];
    size_t length =
        mlocCollationSortKeyText(&c->root, &c->settings, text, scratch, sizeof(scratch));
    *lengthOut = length;
    if (length > capacity)
    {
        return mloc_errorCapacity;
    }
    if (length <= sizeof(scratch))
    {
        if (length > 0)
        {
            memcpy(keyOut, scratch, length);
        }
        return mloc_success;
    }
    size_t again = mlocCollationSortKeyText(&c->root, &c->settings, text, keyOut, capacity);
    MLOC_ASSERT(again == length);
    (void)again;
    return mloc_success;
}

mlocResult mlocCollator_GetSortKeyUtf16(const mlocCollator* collator, const uint16_t* text,
                                        size_t length, uint8_t* keyOut, size_t capacity,
                                        size_t* lengthOut)
{
    Collator c;
    if (lengthOut == nullptr || (text == nullptr && length > 0) ||
        (keyOut == nullptr && capacity > 0) || !mlocReadCollatorHead(collator, &c))
    {
        return mloc_errorInvalid;
    }
    mlocReadCollatorRest(collator, &c);
    static const uint16_t none[1] = {0};
    return WriteKey(&c, mlocNfdUtf16(text != nullptr ? text : none, length), keyOut, capacity,
                    lengthOut);
}

mlocResult mlocCollator_GetSortKey(const mlocCollator* collator, const char* text, size_t length,
                                   uint8_t* keyOut, size_t capacity, size_t* lengthOut)
{
    Collator c;
    if (lengthOut == nullptr || (text == nullptr && length > 0) ||
        (keyOut == nullptr && capacity > 0) || !mlocReadCollatorHead(collator, &c))
    {
        return mloc_errorInvalid;
    }
    mlocReadCollatorRest(collator, &c);
    return WriteKey(&c, mlocNfdUtf8((const uint8_t*)(text != nullptr ? text : ""), length), keyOut,
                    capacity, lengthOut);
}

mlocResult mlocCollator_GetVersion(const mlocCollator* collator, uint64_t* versionOut)
{
    Collator c;
    if (versionOut == nullptr || !mlocReadCollatorHead(collator, &c))
    {
        return mloc_errorInvalid;
    }
    *versionOut = c.version;
    return mloc_success;
}
