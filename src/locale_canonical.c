// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Canonicalization (UTS #35 Annex C): a whole legacy tag replaced, an
// extlang removed, then the alias rules applied to the language,
// script, region and variants until none matches. Rules are found
// through their index (locale_tables.h): each field of the locale
// looks up the rules filed under it, and the first rule in the annex's
// order that matches wins.

#include "locale_canonical.h"

#include "bytes.h"
#include "data_view.h"
#include "invariant.h"
#include "kinds.h"
#include "locale_aliases.h"
#include "locale_keyword.h"
#include "locale_likely.h"
#include "locale_tables.h"

#include "maul-locale/locale.h"

#include <string.h>

#define MAX_VARIANTS   32
#define MAX_ITERATIONS 32

// A language id being canonicalized: packed fields (a language longer
// than 3 letters is kept as text and matches no rule) and the variants
// by name, each with its index in the table or -1.
typedef struct Ident
{
    uint32_t language;
    bool longLanguage;
    char languageText[8];
    uint32_t script;
    uint32_t region;
    int variantCount;
    char variants[MAX_VARIANTS][9];
    int32_t variantIds[MAX_VARIANTS];
} Ident;

static int32_t VariantId(const Aliases* aliases, const char* name)
{
    // The table's names are 8 bytes, NUL-padded, and so is this one.
    char padded[8] = {0};
    for (size_t i = 0; i < 8 && name[i] != '\0'; i++)
    {
        padded[i] = name[i];
    }
    uint32_t low = 0;
    uint32_t high = aliases->variantCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        int order = memcmp(aliases->variants + (size_t)middle * 8, padded, 8);
        if (order == 0)
        {
            return (int32_t)middle;
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
    return -1;
}

static bool HasVariant(const Ident* ident, uint32_t id)
{
    for (int v = 0; v < ident->variantCount; v++)
    {
        if (ident->variantIds[v] == (int32_t)id)
        {
            return true;
        }
    }
    return false;
}

// Whether a rule's type matches, read from its bytes: most rules a key
// files are tried and fail, so the rest of the rule is not unpacked.
static bool MatchesAt(const Ident* ident, const uint8_t* r)
{
    uint32_t language = mlocLoad16(r + 8);
    uint32_t script = mlocLoad32(r);
    uint32_t region = mlocLoad16(r + 10);
    if ((language != 0 && language != ident->language) ||
        (script != 0 && script != ident->script) || (region != 0 && region != ident->region))
    {
        return false;
    }
    for (uint32_t v = 0; v < (r[27] & 15u); v++)
    {
        if (!HasVariant(ident, mlocLoad16(r + 12 + 2 * v)))
        {
            return false;
        }
    }
    return true;
}

// The first rule filed under key that matches, or ruleCount.
static uint32_t FirstUnder(const Aliases* aliases, uint32_t key, const Ident* ident)
{
    for (uint32_t i = KeyStart(aliases, key); i < aliases->ruleCount && KeyAt(aliases, i) == key;
         i++)
    {
        uint32_t index = mlocLoad16(aliases->keyRules + (size_t)i * 2);
        if (MatchesAt(ident, aliases->rules + (size_t)index * aliasRuleSize))
        {
            return index;
        }
    }
    return aliases->ruleCount;
}

// The first matching rule in the annex's order, or ruleCount.
static uint32_t FirstMatch(const Aliases* aliases, const Ident* ident)
{
    uint32_t best = aliases->ruleCount;
    uint32_t keys[3 + MAX_VARIANTS];
    int count = 0;
    MLOC_ASSERT(ident->variantCount <= MAX_VARIANTS);
    if (ident->language != 0)
    {
        keys[count++] = ident->language;
    }
    if (ident->script != 0)
    {
        keys[count++] = 1u << 30 | ident->script;
    }
    if (ident->region != 0)
    {
        keys[count++] = 2u << 30 | ident->region;
    }
    for (int v = 0; v < ident->variantCount; v++)
    {
        if (ident->variantIds[v] >= 0)
        {
            keys[count++] = 3u << 30 | (uint32_t)ident->variantIds[v];
        }
    }
    for (int k = 0; k < count; k++)
    {
        uint32_t found = FirstUnder(aliases, keys[k], ident);
        best = found < best ? found : best;
    }
    return best;
}

static void RemoveVariant(Ident* ident, int at)
{
    for (int v = at; v + 1 < ident->variantCount; v++)
    {
        memcpy(ident->variants[v], ident->variants[v + 1], sizeof(ident->variants[v]));
        ident->variantIds[v] = ident->variantIds[v + 1];
    }
    ident->variantCount--;
}

static bool AddVariant(Ident* ident, const Aliases* aliases, uint32_t id)
{
    if (HasVariant(ident, id))
    {
        return true;
    }
    if (ident->variantCount == MAX_VARIANTS)
    {
        return false;
    }
    // ValidVariants keeps a rule's variants within the table.
    MLOC_ASSERT(id < aliases->variantCount);
    char* name = ident->variants[ident->variantCount];
    memset(name, 0, 9);
    memcpy(name, aliases->variants + (size_t)id * 8, 8);
    ident->variantIds[ident->variantCount++] = (int32_t)id;
    return true;
}

// The region of a replacement with several: the likely region of the
// language (and script) when it is one of them, else the first.
static uint32_t PickRegion(const mlocDataSet* data, const Aliases* aliases, const Rule* rule,
                           const Ident* ident)
{
    uint32_t first = mlocLoad16(aliases->regions + (size_t)rule->regionStart * 2);
    Likely likely;
    if (ident->longLanguage || !mlocOpenLikely(data, &likely))
    {
        return first;
    }
    uint32_t language = ident->language != 0 ? ident->language : mlocPackLanguage("und", 3);
    uint64_t value = 0;
    if (!mlocLookupLikely(&likely, language, ident->script, 0, &value))
    {
        return first;
    }
    uint32_t region = (uint32_t)value & 0x7FF;
    for (uint32_t i = 0; i < rule->regionCount; i++)
    {
        if (mlocLoad16(aliases->regions + (size_t)(rule->regionStart + i) * 2) == region)
        {
            return region;
        }
    }
    return first;
}

// The variants of a rule's Replacement: the type's removed and the
// replacement's added; false when they do not fit.
static bool ApplyVariants(const Aliases* aliases, const Rule* rule, Ident* ident)
{
    for (uint32_t v = 0; v < rule->typeVariantCount; v++)
    {
        for (int at = 0; at < ident->variantCount; at++)
        {
            if (ident->variantIds[at] == (int32_t)rule->typeVariants[v])
            {
                RemoveVariant(ident, at);
                break;
            }
        }
    }
    for (uint32_t v = 0; v < rule->replacementVariantCount; v++)
    {
        if (!AddVariant(ident, aliases, rule->replacementVariants[v]))
        {
            return false;
        }
    }
    return true;
}

// One field of the annex's Replacement: the replacement's value when the
// type has the field, or when the source lacks it and the replacement
// has it. Returns the change bit when the value changed.
static uint32_t ReplaceField(uint32_t* field, bool empty, uint32_t type, uint32_t replacement,
                             uint32_t change)
{
    if (type == 0 && !(empty && replacement != 0))
    {
        return 0;
    }
    uint32_t changed = *field != replacement ? change : 0;
    *field = replacement;
    return changed;
}

// Applies a rule as the annex's Replacement says; returns what changed,
// or ~0u when the variants do not fit.
static uint32_t Apply(const mlocDataSet* data, const Aliases* aliases, const Rule* rule,
                      Ident* ident)
{
    bool emptyLanguage = ident->language == 0 && !ident->longLanguage;
    uint32_t changes = ReplaceField(&ident->language, emptyLanguage, rule->typeLanguage,
                                    rule->replacementLanguage, mloc_changedLanguage);
    changes |= ReplaceField(&ident->script, ident->script == 0, rule->typeScript,
                            rule->replacementScript, mloc_changedScript);
    uint32_t region =
        rule->regionCount > 1 ? PickRegion(data, aliases, rule, ident) : rule->replacementRegion;
    changes |= ReplaceField(&ident->region, ident->region == 0, rule->typeRegion, region,
                            mloc_changedRegion);
    if (rule->typeVariantCount > 0 ||
        (ident->variantCount == 0 && rule->replacementVariantCount > 0))
    {
        if (!ApplyVariants(aliases, rule, ident))
        {
            return ~0u;
        }
        changes |= mloc_changedVariants;
    }
    return changes;
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

// The ident of a parsed locale; the tail's extensions start at
// *restOut (or are empty).
static bool ToIdent(const Aliases* aliases, const mlocLocale* locale, Ident* ident, size_t* restOut)
{
    // The variant arrays are filled up to variantCount only, so that the
    // ident is not cleared whole on every call.
    ident->language = 0;
    ident->longLanguage = false;
    memset(ident->languageText, 0, sizeof(ident->languageText));
    ident->variantCount = 0;
    size_t languageLength = FieldLength(locale->language, 8);
    if (memcmp(locale->language, "und\0", 4) != 0)
    {
        ident->language = mlocPackLanguage(locale->language, (uint32_t)languageLength);
        ident->longLanguage = ident->language == 0;
        memcpy(ident->languageText, locale->language, 8);
    }
    ident->script = mlocPackScript(locale->script, (uint32_t)FieldLength(locale->script, 4));
    ident->region = mlocPackRegion(locale->region, (uint32_t)FieldLength(locale->region, 4));
    // Only the variants are read: the scan stops at the first singleton.
    const char* tail = locale->tail;
    size_t size = sizeof(locale->tail);
    size_t at = 0;
    while (at < size && tail[at] != '\0')
    {
        size_t end = at;
        while (end < size && tail[end] != '\0' && tail[end] != '-')
        {
            end++;
        }
        if (end - at == 1)
        {
            break;
        }
        if (ident->variantCount == MAX_VARIANTS)
        {
            return false;
        }
        // Both callers parse the locale first, and the parser takes
        // variants of 4 to 8 characters.
        MLOC_ASSERT(end - at <= 8);
        char* name = ident->variants[ident->variantCount];
        memcpy(name, locale->tail + at, end - at);
        name[end - at] = '\0';
        ident->variantIds[ident->variantCount++] = VariantId(aliases, name);
        at = end + (end < size && tail[end] == '-');
    }
    *restOut = at;
    return true;
}

static int CompareNames(const char* a, const char* b)
{
    return strcmp(a, b);
}

// Writes an ident's language, script and region into a locale.
static void WriteFields(const Ident* ident, mlocLocale* locale)
{
    memset(locale->language, 0, sizeof(locale->language));
    memset(locale->script, 0, sizeof(locale->script));
    memset(locale->region, 0, sizeof(locale->region));
    if (ident->longLanguage)
    {
        memcpy(locale->language, ident->languageText, 8);
    }
    else if (ident->language != 0)
    {
        mlocUnpackLanguage(ident->language, locale->language);
    }
    else
    {
        memcpy(locale->language, "und", 3);
    }
    if (ident->script != 0)
    {
        mlocUnpackScript(ident->script, locale->script);
    }
    if (ident->region != 0)
    {
        mlocUnpackRegion(ident->region, locale->region);
    }
}

// Writes an ident's variants, sorted, into a locale's tail before the
// extensions, which start at rest.
static bool WriteVariants(Ident* ident, size_t rest, mlocLocale* locale)
{
    for (int i = 1; i < ident->variantCount; i++)
    {
        for (int k = i; k > 0 && CompareNames(ident->variants[k - 1], ident->variants[k]) > 0; k--)
        {
            char swap[9];
            memcpy(swap, ident->variants[k], 9);
            memcpy(ident->variants[k], ident->variants[k - 1], 9);
            memcpy(ident->variants[k - 1], swap, 9);
        }
    }
    char tail[sizeof(locale->tail)] = {0};
    size_t at = 0;
    for (int v = 0; v < ident->variantCount; v++)
    {
        size_t length = strlen(ident->variants[v]);
        if (at + (at > 0) + length > sizeof(tail))
        {
            return false;
        }
        if (at > 0)
        {
            tail[at++] = '-';
        }
        memcpy(tail + at, ident->variants[v], length);
        at += length;
    }
    size_t restLength = FieldLength(locale->tail, sizeof(locale->tail)) - rest;
    if (restLength > 0)
    {
        if (at + (at > 0) + restLength > sizeof(tail))
        {
            return false;
        }
        if (at > 0)
        {
            tail[at++] = '-';
        }
        memcpy(tail + at, locale->tail + rest, restLength);
    }
    memcpy(locale->tail, tail, sizeof(tail));
    return true;
}

// Applies the rules until none matches, adding what changed to
// *changes. Returns mloc_errorLimit when the result does not fit and
// mloc_errorFormat when the rules do not end, which only a crafted data
// file can make them do.
static mlocResult ApplyRules(const mlocDataSet* data, const Aliases* aliases, mlocLocale* locale,
                             uint32_t* changes)
{
    Ident ident;
    size_t rest = 0;
    if (!ToIdent(aliases, locale, &ident, &rest))
    {
        return mloc_errorLimit;
    }
    uint32_t applied = 0;
    for (int iteration = 0;; iteration++)
    {
        uint32_t index = FirstMatch(aliases, &ident);
        if (index == aliases->ruleCount)
        {
            break;
        }
        if (iteration == MAX_ITERATIONS)
        {
            return mloc_errorFormat;
        }
        Rule rule = ReadRule(aliases, index);
        uint32_t one = Apply(data, aliases, &rule, &ident);
        if (one == ~0u)
        {
            return mloc_errorLimit;
        }
        applied |= one;
    }
    // The tail is written again only when the variants changed.
    if ((applied & mloc_changedVariants) != 0 && !WriteVariants(&ident, rest, locale))
    {
        return mloc_errorLimit;
    }
    if (applied != 0)
    {
        WriteFields(&ident, locale);
    }
    *changes |= applied;
    return mloc_success;
}

mlocResult mlocApplyAliases(const mlocDataSet* data, mlocLocale* locale, uint32_t* changes)
{
    DataSpan span;
    Aliases aliases;
    if (!mlocFindDataSetUnit(data, kindLocaleAliases, nullptr, 0, &span) ||
        !ReadAliases(span, &aliases))
    {
        return mloc_errorNotFound;
    }
    return ApplyRules(data, &aliases, locale, changes);
}

// Lowercases text into out with "-" for "_"; false when it does not fit.
static bool Fold(const char* text, size_t length, char* out)
{
    if (length >= MAX_TEXT)
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        char c = text[i] == '_' ? '-' : text[i];
        out[i] = c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
    }
    out[length] = '\0';
    return true;
}

// The replacement of a legacy tag equal to folded, or null.
static const char* Legacy(const Aliases* aliases, const char* folded, size_t length,
                          size_t* replacementLength)
{
    uint32_t low = 0;
    uint32_t high = aliases->legacyCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* entry = aliases->legacy + (size_t)middle * aliasLegacySize;
        int order = CompareText(aliases->text + mlocLoad16(entry), mlocLoad16(entry + 2),
                                (const uint8_t*)folded, length);
        if (order == 0)
        {
            *replacementLength = mlocLoad16(entry + 6);
            return (const char*)aliases->text + mlocLoad16(entry + 4);
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

static bool IsAlphaRun(const char* text, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        if (text[i] < 'a' || text[i] > 'z')
        {
            return false;
        }
    }
    return true;
}

// Rewrites a BCP 47 tag before it is parsed: a legacy tag replaced, an
// extlang removed, "und-" put before a tag that begins with "x" or a
// script. Points *textOut at the text to parse, out or the folded text
// when nothing is rewritten, and returns its length; *shiftOut is what
// to add to an offset in it to point into the text given (or 0 when the
// text was replaced).
static size_t Prepare(const Aliases* aliases, const char* folded, size_t length, char* out,
                      const char** textOut, ptrdiff_t* shiftOut, uint32_t* changes)
{
    *textOut = out;
    *shiftOut = 0;
    size_t replacementLength = 0;
    const char* legacy = Legacy(aliases, folded, length, &replacementLength);
    if (legacy != nullptr)
    {
        memcpy(out, legacy, replacementLength);
        *changes |= mloc_changedLegacy;
        return replacementLength;
    }
    size_t first = strcspn(folded, "-");
    size_t second = first < length ? strcspn(folded + first + 1, "-") : 0;
    if ((first == 2 || first == 3) && second == 3 && IsAlphaRun(folded, first) &&
        IsAlphaRun(folded + first + 1, 3))
    {
        memcpy(out, folded + first + 1, length - first - 1);
        *shiftOut = (ptrdiff_t)first + 1;
        *changes |= mloc_changedExtlang;
        return length - first - 1;
    }
    if ((first == 1 && folded[0] == 'x') || (first == 4 && IsAlphaRun(folded, 4)))
    {
        if (length + 4 >= MAX_TEXT)
        {
            return MAX_TEXT;
        }
        out[0] = 'u';
        out[1] = 'n';
        out[2] = 'd';
        out[3] = '-';
        memcpy(out + 4, folded, length);
        *shiftOut = -4;
        return length + 4;
    }
    *textOut = folded;
    return length;
}

// CLDR's variant POSIX as the keyword -u-va-posix, which UTS #35 makes of
// it. False when the locale with the keyword does not fit.
static bool PosixKeyword(mlocLocale* locale, uint32_t* changes)
{
    size_t size = FieldLength(locale->tail, sizeof(locale->tail));
    for (size_t at = 0; at < size;)
    {
        size_t end = at;
        while (end < size && locale->tail[end] != '-')
        {
            end++;
        }
        if (end - at == 1)
        {
            // The extensions begin: no variant posix.
            return true;
        }
        if (end - at == 5 && memcmp(locale->tail + at, "posix", 5) == 0)
        {
            // The variant and the hyphen after it (or before it, last).
            size_t from = end < size ? at : at > 0 ? at - 1 : 0;
            size_t to = end < size ? end + 1 : end;
            memmove(locale->tail + from, locale->tail + to, size - to);
            memset(locale->tail + size - (to - from), 0, to - from);
            *changes |= mloc_changedVariants;
            return mlocSetKeyword(locale, "va", "posix", 5);
        }
        at = end + 1;
    }
    return true;
}

mlocTextResult mlocCanonicalizeLocale(const mlocDataSet* data, const char* text, size_t length,
                                      mlocLocale* localeOut, mlocLocaleChanges* changesOut)
{
    if (data == nullptr || (text == nullptr && length > 0) || localeOut == nullptr)
    {
        return (mlocTextResult){mloc_errorInvalid, 0};
    }
    DataSpan span;
    Aliases aliases;
    if (!mlocFindDataSetUnit(data, kindLocaleAliases, nullptr, 0, &span) ||
        !ReadAliases(span, &aliases))
    {
        return (mlocTextResult){mloc_errorNotFound, 0};
    }
    char folded[MAX_TEXT];
    char prepared[MAX_TEXT + 4];
    if (!Fold(text, length, folded))
    {
        return (mlocTextResult){mloc_errorLimit, length};
    }
    uint32_t changes = 0;
    ptrdiff_t shift = 0;
    const char* toParse = nullptr;
    size_t parseLength = Prepare(&aliases, folded, length, prepared, &toParse, &shift, &changes);
    if (parseLength >= MAX_TEXT)
    {
        return (mlocTextResult){mloc_errorLimit, length};
    }
    mlocLocale locale;
    mlocTextResult result = mlocParseLocale(toParse, parseLength, &locale);
    if (result.status != mloc_success)
    {
        ptrdiff_t offset = (ptrdiff_t)result.offset + shift;
        result.offset = offset < 0 ? 0 : (size_t)offset > length ? length : (size_t)offset;
        return result;
    }
    mlocResult applied = ApplyRules(data, &aliases, &locale, &changes);
    if (applied == mloc_success)
    {
        applied = mlocCanonicalizeExtensions(data, &locale, &changes);
    }
    if (applied == mloc_success && !PosixKeyword(&locale, &changes))
    {
        applied = mloc_errorLimit;
    }
    if (applied != mloc_success)
    {
        return (mlocTextResult){applied, applied == mloc_errorLimit ? length : 0};
    }
    *localeOut = locale;
    if (changesOut != nullptr)
    {
        *changesOut = changes;
    }
    return (mlocTextResult){mloc_success, length};
}
