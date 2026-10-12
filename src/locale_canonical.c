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
#include "kinds.h"
#include "locale_tables.h"

#include "maul-locale/locale.h"

#include <string.h>

#define MAX_TEXT       256
#define MAX_VARIANTS   32
#define MAX_ITERATIONS 32

typedef struct Aliases
{
    const uint8_t* rules;
    const uint8_t* keys;
    const uint8_t* keyRules;
    const uint8_t* variants;
    const uint8_t* regions;
    const uint8_t* legacy;
    const uint8_t* text;
    uint32_t ruleCount;
    uint32_t variantCount;
    uint32_t regionCount;
    uint32_t legacyCount;
    uint32_t textSize;
} Aliases;

// The table's parts, or false when its counts do not give its size.
static bool ReadAliases(DataSpan span, Aliases* out)
{
    if (span.size < aliasHeaderSize)
    {
        return false;
    }
    uint64_t rules = mlocLoad32(span.bytes);
    uint64_t keys = mlocLoad32(span.bytes + 4);
    uint64_t variants = mlocLoad32(span.bytes + 8);
    uint64_t regions = mlocLoad32(span.bytes + 12);
    uint64_t legacy = mlocLoad32(span.bytes + 16);
    uint64_t text = mlocLoad32(span.bytes + 20);
    uint64_t keysAt = aliasHeaderSize + rules * aliasRuleSize;
    uint64_t variantsAt = (keysAt + rules * 6 + 7) / 8 * 8;
    uint64_t regionsAt = variantsAt + variants * 8;
    uint64_t legacyAt = regionsAt + regions * 2;
    uint64_t textAt = legacyAt + legacy * aliasLegacySize;
    if (keys != rules || rules > 0xFFFF || variants > 0xFFFF || textAt + text != span.size)
    {
        return false;
    }
    *out = (Aliases){
        .rules = span.bytes + aliasHeaderSize,
        .keys = span.bytes + keysAt,
        .keyRules = span.bytes + keysAt + rules * 4,
        .variants = span.bytes + variantsAt,
        .regions = span.bytes + regionsAt,
        .legacy = span.bytes + legacyAt,
        .text = span.bytes + textAt,
        .ruleCount = (uint32_t)rules,
        .variantCount = (uint32_t)variants,
        .regionCount = (uint32_t)regions,
        .legacyCount = (uint32_t)legacy,
        .textSize = (uint32_t)text,
    };
    return true;
}

// One rule, unpacked.
typedef struct Rule
{
    uint32_t typeScript;
    uint32_t replacementScript;
    uint32_t typeLanguage;
    uint32_t typeRegion;
    uint32_t typeVariants[aliasMaxVariants];
    uint32_t replacementLanguage;
    uint32_t replacementRegion;
    uint32_t replacementVariants[aliasMaxVariants];
    uint32_t regionStart;
    uint32_t regionCount;
    uint32_t typeVariantCount;
    uint32_t replacementVariantCount;
} Rule;

static Rule ReadRule(const Aliases* aliases, uint32_t index)
{
    const uint8_t* r = aliases->rules + (size_t)index * aliasRuleSize;
    return (Rule){
        .typeScript = mlocLoad32(r),
        .replacementScript = mlocLoad32(r + 4),
        .typeLanguage = mlocLoad16(r + 8),
        .typeRegion = mlocLoad16(r + 10),
        .typeVariants = {mlocLoad16(r + 12), mlocLoad16(r + 14)},
        .replacementLanguage = mlocLoad16(r + 16),
        .replacementRegion = mlocLoad16(r + 18),
        .replacementVariants = {mlocLoad16(r + 20), mlocLoad16(r + 22)},
        .regionStart = mlocLoad16(r + 24),
        .regionCount = r[26],
        .typeVariantCount = r[27] & 15u,
        .replacementVariantCount = r[27] >> 4,
    };
}

// The key a rule is filed under: its type's first field.
static uint32_t RuleKey(const Rule* rule)
{
    if (rule->typeLanguage != 0)
    {
        return rule->typeLanguage;
    }
    if (rule->typeScript != 0)
    {
        return 1u << 30 | rule->typeScript;
    }
    if (rule->typeRegion != 0)
    {
        return 2u << 30 | rule->typeRegion;
    }
    return 3u << 30 | rule->typeVariants[0];
}

static bool Optional(uint32_t packed, bool (*valid)(uint32_t))
{
    return packed == 0 || valid(packed);
}

static bool ValidVariants(const Aliases* aliases, const uint32_t* variants, uint32_t count)
{
    // The count comes from 4 bits of the file; the rule holds two.
    if (count > aliasMaxVariants)
    {
        return false;
    }
    for (uint32_t v = 0; v < count; v++)
    {
        if (variants[v] >= aliases->variantCount || (v > 0 && variants[v] <= variants[v - 1]))
        {
            return false;
        }
    }
    return true;
}

// A variant name as the parser writes one: 5 to 8 lowercase letters and
// digits, or a digit and 3 of them.
static bool ValidVariantName(const uint8_t* name, uint32_t length)
{
    for (uint32_t k = 0; k < length; k++)
    {
        if (!((name[k] >= 'a' && name[k] <= 'z') || (name[k] >= '0' && name[k] <= '9')))
        {
            return false;
        }
    }
    return length >= 5 || (length == 4 && name[0] >= '0' && name[0] <= '9');
}

static bool ValidRule(const Aliases* aliases, const Rule* rule)
{
    bool fields = Optional(rule->typeLanguage, mlocValidPackedLanguage) &&
                  Optional(rule->replacementLanguage, mlocValidPackedLanguage) &&
                  Optional(rule->typeScript, mlocValidPackedScript) &&
                  Optional(rule->replacementScript, mlocValidPackedScript) &&
                  Optional(rule->typeRegion, mlocValidPackedRegion) &&
                  Optional(rule->replacementRegion, mlocValidPackedRegion);
    bool empty = rule->typeLanguage == 0 && rule->typeScript == 0 && rule->typeRegion == 0 &&
                 rule->typeVariantCount == 0;
    return fields && !empty && ValidVariants(aliases, rule->typeVariants, rule->typeVariantCount) &&
           ValidVariants(aliases, rule->replacementVariants, rule->replacementVariantCount) &&
           rule->regionCount != 1 &&
           (uint64_t)rule->regionStart + rule->regionCount <= aliases->regionCount;
}

static bool ValidTables(const Aliases* aliases)
{
    for (uint32_t i = 0; i < aliases->variantCount; i++)
    {
        const uint8_t* name = aliases->variants + (size_t)i * 8;
        uint32_t length = 0;
        while (length < 8 && name[length] != 0)
        {
            length++;
        }
        bool sorted = i == 0 || memcmp(name - 8, name, 8) < 0;
        for (uint32_t k = length; k < 8; k++)
        {
            sorted = sorted && name[k] == 0;
        }
        if (!ValidVariantName(name, length) || !sorted)
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < aliases->regionCount; i++)
    {
        if (!mlocValidPackedRegion(mlocLoad16(aliases->regions + (size_t)i * 2)))
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < aliases->legacyCount; i++)
    {
        const uint8_t* entry = aliases->legacy + (size_t)i * aliasLegacySize;
        uint32_t typeLength = mlocLoad16(entry + 2);
        uint32_t replacementLength = mlocLoad16(entry + 6);
        if ((uint32_t)mlocLoad16(entry) + typeLength > aliases->textSize ||
            (uint32_t)mlocLoad16(entry + 4) + replacementLength > aliases->textSize ||
            typeLength == 0 || replacementLength == 0 || typeLength >= MAX_TEXT ||
            replacementLength >= MAX_TEXT)
        {
            return false;
        }
    }
    return true;
}

bool mlocValidAliases(const DataView* view, DataSpan payload)
{
    (void)view;
    Aliases aliases;
    if (!ReadAliases(payload, &aliases) || mlocLoad64(payload.bytes + 24) != 0 ||
        !ValidTables(&aliases))
    {
        return false;
    }
    uint64_t previous = 0;
    for (uint32_t i = 0; i < aliases.ruleCount; i++)
    {
        Rule rule = ReadRule(&aliases, i);
        uint32_t key = mlocLoad32(aliases.keys + (size_t)i * 4);
        uint32_t keyRule = mlocLoad16(aliases.keyRules + (size_t)i * 2);
        uint64_t sortKey = (uint64_t)key << 16 | keyRule;
        if (!ValidRule(&aliases, &rule) || keyRule >= aliases.ruleCount ||
            (i > 0 && sortKey <= previous))
        {
            return false;
        }
        Rule filed = ReadRule(&aliases, keyRule);
        if (RuleKey(&filed) != key)
        {
            return false;
        }
        previous = sortKey;
    }
    return true;
}

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

static bool Matches(const Ident* ident, const Rule* rule)
{
    if ((rule->typeLanguage != 0 && rule->typeLanguage != ident->language) ||
        (rule->typeScript != 0 && rule->typeScript != ident->script) ||
        (rule->typeRegion != 0 && rule->typeRegion != ident->region))
    {
        return false;
    }
    for (uint32_t v = 0; v < rule->typeVariantCount; v++)
    {
        if (!HasVariant(ident, rule->typeVariants[v]))
        {
            return false;
        }
    }
    return true;
}

// The first rule filed under key that matches, or ruleCount.
static uint32_t FirstUnder(const Aliases* aliases, uint32_t key, const Ident* ident)
{
    uint32_t low = 0;
    uint32_t high = aliases->ruleCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        if (mlocLoad32(aliases->keys + (size_t)middle * 4) < key)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    for (uint32_t i = low;
         i < aliases->ruleCount && mlocLoad32(aliases->keys + (size_t)i * 4) == key; i++)
    {
        uint32_t index = mlocLoad16(aliases->keyRules + (size_t)i * 2);
        Rule rule = ReadRule(aliases, index);
        if (Matches(ident, &rule))
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
    mlocLocale probe = {0};
    if (ident->longLanguage)
    {
        memcpy(probe.language, ident->languageText, 8);
    }
    else if (ident->language != 0)
    {
        mlocUnpackLanguage(ident->language, probe.language);
    }
    else
    {
        memcpy(probe.language, "und", 3);
    }
    if (ident->script != 0)
    {
        mlocUnpackScript(ident->script, probe.script);
    }
    mlocLocale likely;
    if (mlocMaximizeLocale(data, &probe, &likely) != mloc_success)
    {
        return first;
    }
    uint32_t region = mlocPackRegion(likely.region, likely.region[2] != '\0' ? 3 : 2);
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
    *ident = (Ident){0};
    size_t languageLength = FieldLength(locale->language, 8);
    if (memcmp(locale->language, "und\0", 4) != 0)
    {
        ident->language = mlocPackLanguage(locale->language, (uint32_t)languageLength);
        ident->longLanguage = ident->language == 0;
        memcpy(ident->languageText, locale->language, 8);
    }
    ident->script = mlocPackScript(locale->script, (uint32_t)FieldLength(locale->script, 4));
    ident->region = mlocPackRegion(locale->region, (uint32_t)FieldLength(locale->region, 4));
    size_t tailLength = FieldLength(locale->tail, sizeof(locale->tail));
    size_t at = 0;
    while (at < tailLength)
    {
        size_t end = at;
        while (end < tailLength && locale->tail[end] != '-')
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
        char* name = ident->variants[ident->variantCount];
        memcpy(name, locale->tail + at, end - at);
        name[end - at] = '\0';
        ident->variantIds[ident->variantCount++] = VariantId(aliases, name);
        at = end + (end < tailLength);
    }
    *restOut = at;
    return true;
}

static int CompareNames(const char* a, const char* b)
{
    return strcmp(a, b);
}

// Writes an ident back with the extensions kept from the old tail.
static bool FromIdent(Ident* ident, const mlocLocale* old, size_t rest, mlocLocale* locale)
{
    mlocLocale result = {0};
    if (ident->longLanguage)
    {
        memcpy(result.language, ident->languageText, 8);
    }
    else if (ident->language != 0)
    {
        mlocUnpackLanguage(ident->language, result.language);
    }
    else
    {
        memcpy(result.language, "und", 3);
    }
    if (ident->script != 0)
    {
        mlocUnpackScript(ident->script, result.script);
    }
    if (ident->region != 0)
    {
        mlocUnpackRegion(ident->region, result.region);
    }
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
    size_t at = 0;
    size_t capacity = sizeof(result.tail);
    for (int v = 0; v < ident->variantCount; v++)
    {
        size_t length = strlen(ident->variants[v]);
        if (at + (at > 0) + length > capacity)
        {
            return false;
        }
        if (at > 0)
        {
            result.tail[at++] = '-';
        }
        memcpy(result.tail + at, ident->variants[v], length);
        at += length;
    }
    size_t restLength = FieldLength(old->tail, sizeof(old->tail)) - rest;
    if (restLength > 0)
    {
        if (at + (at > 0) + restLength > capacity)
        {
            return false;
        }
        if (at > 0)
        {
            result.tail[at++] = '-';
        }
        memcpy(result.tail + at, old->tail + rest, restLength);
    }
    *locale = result;
    return true;
}

// Applies the rules until none matches. Returns the changes, or ~0u
// when they do not end or the result does not fit.
static uint32_t ApplyRules(const mlocDataSet* data, const Aliases* aliases, mlocLocale* locale)
{
    Ident ident;
    size_t rest = 0;
    if (!ToIdent(aliases, locale, &ident, &rest))
    {
        return ~0u;
    }
    uint32_t changes = 0;
    for (int iteration = 0;; iteration++)
    {
        uint32_t index = FirstMatch(aliases, &ident);
        if (index == aliases->ruleCount)
        {
            break;
        }
        Rule rule = ReadRule(aliases, index);
        uint32_t applied = Apply(data, aliases, &rule, &ident);
        if (applied == ~0u || iteration == MAX_ITERATIONS)
        {
            return ~0u;
        }
        changes |= applied;
    }
    if (changes != 0 && !FromIdent(&ident, locale, rest, locale))
    {
        return ~0u;
    }
    return changes;
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
    for (uint32_t i = 0; i < aliases->legacyCount; i++)
    {
        const uint8_t* entry = aliases->legacy + (size_t)i * aliasLegacySize;
        if (mlocLoad16(entry + 2) == length &&
            memcmp(aliases->text + mlocLoad16(entry), folded, length) == 0)
        {
            *replacementLength = mlocLoad16(entry + 6);
            return (const char*)aliases->text + mlocLoad16(entry + 4);
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
// script. Writes the text to parse into out and returns its length;
// *shiftOut is what to add to an offset in it to point into the text
// given (or 0 when the text was replaced).
static size_t Prepare(const Aliases* aliases, const char* folded, size_t length, char* out,
                      ptrdiff_t* shiftOut, uint32_t* changes)
{
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
    memcpy(out, folded, length);
    return length;
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
        return (mlocTextResult){mloc_errorCapacity, length};
    }
    uint32_t changes = 0;
    ptrdiff_t shift = 0;
    size_t preparedLength = Prepare(&aliases, folded, length, prepared, &shift, &changes);
    if (preparedLength >= MAX_TEXT)
    {
        return (mlocTextResult){mloc_errorCapacity, length};
    }
    mlocLocale locale;
    mlocTextResult result = mlocParseLocale(prepared, preparedLength, &locale);
    if (result.status != mloc_success)
    {
        ptrdiff_t offset = (ptrdiff_t)result.offset + shift;
        result.offset = offset < 0 ? 0 : (size_t)offset > length ? length : (size_t)offset;
        return result;
    }
    uint32_t applied = ApplyRules(data, &aliases, &locale);
    if (applied == ~0u)
    {
        return (mlocTextResult){mloc_errorCapacity, length};
    }
    *localeOut = locale;
    if (changesOut != nullptr)
    {
        *changesOut = changes | applied;
    }
    return (mlocTextResult){mloc_success, length};
}
