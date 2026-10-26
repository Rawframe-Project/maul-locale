// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The alias rules of UTS #35 Annex C from supplemental/
// supplementalMetadata.xml, preprocessed as the annex says: language,
// script, region and variant aliases as rules on the four fields, in the
// annex's order, with the language aliases whose type is no valid
// language id kept apart as legacy rules for whole tags. The layout is
// src/locale_tables.h's.

#include "alias_gen.h"

#include "kinds.h"
#include "locale_tables.h"
#include "table_gen.h"
#include "xml.h"

#include "maul-locale/locale.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RULES   2048
#define MAX_REGIONS 32
#define MAX_LEGACY  64

typedef struct Fields
{
    char language[9];
    char script[5];
    char region[4];
    int variantCount;
    char variants[aliasMaxVariants][9];
} Fields;

typedef struct Rule
{
    Fields type;
    Fields replacement;
    int regionCount;
    char regions[MAX_REGIONS][4];
} Rule;

typedef struct Legacy
{
    char type[64];
    char replacement[64];
} Legacy;

typedef struct Aliases
{
    int ruleCount;
    Rule rules[MAX_RULES];
    int legacyCount;
    Legacy legacy[MAX_LEGACY];
    int variantCount;
    char variants[64][9];
} Aliases;

[[noreturn]] static void Die(const char* message, const char* detail)
{
    fprintf(stderr, "mlocgen: %s: %s\n", message, detail);
    exit(1);
}

// CLDR's id with "-" for "_", lowercased, after a prefix; the generator
// stops on one longer than out holds.
static void Normalize(const char* prefix, const char* id, char* out, size_t capacity)
{
    if (strlen(prefix) + strlen(id) >= capacity)
    {
        Die("an alias id longer than the generator holds", id);
    }
    size_t at = 0;
    for (const char* p = prefix; *p != '\0'; p++)
    {
        out[at++] = *p;
    }
    for (const char* p = id; *p != '\0'; p++)
    {
        char c = *p == '_' ? '-' : *p;
        out[at++] = c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
    }
    out[at] = '\0';
}

// The next token of text split at sep, or null; the text is cut in
// place and the cursor moves past the token.
static char* NextToken(char** cursor, char sep)
{
    char* token = *cursor;
    while (*token == sep)
    {
        token++;
    }
    if (*token == '\0')
    {
        return nullptr;
    }
    char* end = strchr(token, sep);
    *cursor = end == nullptr ? token + strlen(token) : end + 1;
    if (end != nullptr)
    {
        *end = '\0';
    }
    return token;
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

// The fields of a language id, or 0 when it is none (or has
// extensions, which no rule may have).
static int ToFields(const char* id, Fields* fields)
{
    mlocLocale locale;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success)
    {
        return 0;
    }
    *fields = (Fields){0};
    if (strncmp(locale.language, "und", 8) != 0)
    {
        memcpy(fields->language, locale.language, FieldLength(locale.language, 8));
    }
    memcpy(fields->script, locale.script, FieldLength(locale.script, 4));
    memcpy(fields->region, locale.region, FieldLength(locale.region, 4));
    char tail[113] = {0};
    memcpy(tail, locale.tail, FieldLength(locale.tail, sizeof(locale.tail)));
    char* cursor = tail;
    for (char* subtag = NextToken(&cursor, '-'); subtag != nullptr;
         subtag = NextToken(&cursor, '-'))
    {
        if (strlen(subtag) == 1)
        {
            return 0;
        }
        if (fields->variantCount == aliasMaxVariants)
        {
            Die("an alias with more variants than the table holds", id);
        }
        strcpy(fields->variants[fields->variantCount++], subtag);
    }
    return 1;
}

static int Count(const Fields* f)
{
    return (f->language[0] != 0) + (f->script[0] != 0) + (f->region[0] != 0) + f->variantCount;
}

// UTS #35 Annex C's order: more items first, then the fields that are
// not empty, in the order language, script, region, variants, then the
// values by code point.
static int CompareRules(const void* a, const void* b)
{
    const Fields* x = &((const Rule*)a)->type;
    const Fields* y = &((const Rule*)b)->type;
    if (Count(x) != Count(y))
    {
        return Count(y) - Count(x);
    }
    const char* xs[3] = {x->language, x->script, x->region};
    const char* ys[3] = {y->language, y->script, y->region};
    for (int i = 0; i < 3; i++)
    {
        if ((xs[i][0] != 0) != (ys[i][0] != 0))
        {
            return xs[i][0] != 0 ? -1 : 1;
        }
    }
    if ((x->variantCount > 0) != (y->variantCount > 0))
    {
        return x->variantCount > 0 ? -1 : 1;
    }
    for (int i = 0; i < 3; i++)
    {
        int order = strcmp(xs[i], ys[i]);
        if (order != 0)
        {
            return order;
        }
    }
    for (int i = 0; i < x->variantCount && i < y->variantCount; i++)
    {
        int order = strcmp(x->variants[i], y->variants[i]);
        if (order != 0)
        {
            return order;
        }
    }
    return x->variantCount - y->variantCount;
}

static void AddLegacy(Aliases* aliases, const char* type, const char* replacement)
{
    if (aliases->legacyCount == MAX_LEGACY)
    {
        Die("more legacy aliases than the generator holds", type);
    }
    Legacy* legacy = &aliases->legacy[aliases->legacyCount++];
    Normalize("", type, legacy->type, sizeof(legacy->type));
    Normalize("", replacement, legacy->replacement, sizeof(legacy->replacement));
    mlocLocale check;
    if (mlocParseLocale(legacy->replacement, strlen(legacy->replacement), &check).status !=
        mloc_success)
    {
        Die("a legacy alias whose replacement is no locale id", replacement);
    }
}

// One alias element: a rule, a legacy rule, or nothing for a type the
// annex discards.
static void AddAlias(Aliases* aliases, const XmlNode* node)
{
    const char* type = XmlGetAttribute(node, "type");
    const char* replacement = XmlGetAttribute(node, "replacement");
    if (type == nullptr || replacement == nullptr)
    {
        Die("an alias without its type or replacement", node->name);
    }
    bool language = strcmp(node->name, "languageAlias") == 0;
    const char* prefix = language ? "" : "und-";
    char id[128];
    Normalize(prefix, type, id, sizeof(id));
    Rule rule = {0};
    if (!ToFields(id, &rule.type))
    {
        if (language)
        {
            AddLegacy(aliases, type, replacement);
        }
        return;
    }
    char list[256];
    if (!GenCopy(list, sizeof(list), replacement, strlen(replacement)))
    {
        Die("an alias replacement list longer than the generator holds", type);
    }
    char* cursor = list;
    int replacements = 0;
    for (char* one = NextToken(&cursor, ' '); one != nullptr; one = NextToken(&cursor, ' '))
    {
        Normalize(prefix, one, id, sizeof(id));
        Fields fields;
        if (!ToFields(id, &fields))
        {
            Die("an alias whose replacement is no language id", replacement);
        }
        if (replacements++ == 0)
        {
            rule.replacement = fields;
        }
        else if (fields.region[0] == 0 || fields.language[0] != 0 || fields.script[0] != 0 ||
                 fields.variantCount != 0)
        {
            Die("an alias with several replacements that are not all regions", replacement);
        }
        if (fields.region[0] != 0)
        {
            if (rule.regionCount == MAX_REGIONS)
            {
                Die("an alias with more regions than the table holds", replacement);
            }
            strcpy(rule.regions[rule.regionCount++], fields.region);
        }
    }
    // Only several regions need the list; one is the replacement's own.
    rule.regionCount = rule.regionCount > 1 ? rule.regionCount : 0;
    if (aliases->ruleCount == MAX_RULES)
    {
        Die("more alias rules than the generator holds", type);
    }
    aliases->rules[aliases->ruleCount++] = rule;
}

static int VariantIndex(Aliases* aliases, const char* variant)
{
    for (int i = 0; i < aliases->variantCount; i++)
    {
        if (strcmp(aliases->variants[i], variant) == 0)
        {
            return i;
        }
    }
    return -1;
}

static int CompareVariants(const void* a, const void* b)
{
    return strcmp(a, b);
}

static void CollectVariants(Aliases* aliases)
{
    for (int r = 0; r < aliases->ruleCount; r++)
    {
        const Fields* both[2] = {&aliases->rules[r].type, &aliases->rules[r].replacement};
        for (int f = 0; f < 2; f++)
        {
            for (int v = 0; v < both[f]->variantCount; v++)
            {
                if (VariantIndex(aliases, both[f]->variants[v]) < 0)
                {
                    if (aliases->variantCount == 64)
                    {
                        Die("more alias variants than the generator holds", both[f]->variants[v]);
                    }
                    strcpy(aliases->variants[aliases->variantCount++], both[f]->variants[v]);
                }
            }
        }
    }
    GenSort(aliases->variants, (size_t)aliases->variantCount, sizeof(aliases->variants[0]),
            CompareVariants);
}

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

static uint32_t Language(const char* language)
{
    uint32_t length = (uint32_t)strlen(language);
    uint32_t packed = mlocPackLanguage(language, length);
    if (length > 0 && packed == 0)
    {
        Die("an alias language longer than the table holds", language);
    }
    return packed;
}

// Writes one rule's 32 bytes; regions go from regionStart on.
static void WriteRule(Aliases* aliases, const Rule* rule, uint8_t* out, uint32_t regionStart)
{
    const Fields* t = &rule->type;
    const Fields* r = &rule->replacement;
    Put(out, mlocPackScript(t->script, (uint32_t)strlen(t->script)), 4);
    Put(out + 4, mlocPackScript(r->script, (uint32_t)strlen(r->script)), 4);
    Put(out + 8, Language(t->language), 2);
    Put(out + 10, mlocPackRegion(t->region, (uint32_t)strlen(t->region)), 2);
    for (int v = 0; v < t->variantCount; v++)
    {
        Put(out + 12 + v * 2, (uint32_t)VariantIndex(aliases, t->variants[v]), 2);
    }
    Put(out + 16, Language(r->language), 2);
    Put(out + 18, mlocPackRegion(r->region, (uint32_t)strlen(r->region)), 2);
    for (int v = 0; v < r->variantCount; v++)
    {
        Put(out + 20 + v * 2, (uint32_t)VariantIndex(aliases, r->variants[v]), 2);
    }
    Put(out + 24, rule->regionCount > 0 ? regionStart : 0, 2);
    out[26] = (uint8_t)rule->regionCount;
    out[27] = (uint8_t)(t->variantCount | r->variantCount << 4);
}

// The key a rule is indexed under: its type's first field.
static uint32_t Key(Aliases* aliases, const Rule* rule)
{
    const Fields* t = &rule->type;
    if (t->language[0] != 0)
    {
        return Language(t->language);
    }
    if (t->script[0] != 0)
    {
        return 1u << 30 | mlocPackScript(t->script, 4);
    }
    if (t->region[0] != 0)
    {
        return 2u << 30 | mlocPackRegion(t->region, (uint32_t)strlen(t->region));
    }
    return 3u << 30 | (uint32_t)VariantIndex(aliases, t->variants[0]);
}

static int CompareKeys(const void* a, const void* b)
{
    uint64_t x = *(const uint64_t*)a;
    uint64_t y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}

static int CompareLegacy(const void* a, const void* b)
{
    return strcmp(((const Legacy*)a)->type, ((const Legacy*)b)->type);
}

// Twice the keys of the sorted (key << 16 | rule) list, rounded up to
// a power of two: a probe then reads one or two slots, and a 0 always
// ends it.
static uint32_t SlotCount(const uint64_t* keys, uint32_t count)
{
    uint32_t distinct = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        distinct += i == 0 || keys[i] >> 16 != keys[i - 1] >> 16;
    }
    uint32_t slots = 2;
    while (slots < 2 * distinct)
    {
        slots *= 2;
    }
    if (slots > 65536)
    {
        Die("too many alias keys", "aliases");
    }
    return slots;
}

// Files a key's first index in the first free slot from its own.
static void PutSlot(uint8_t* slots, uint32_t slotCount, uint32_t key, uint32_t index)
{
    uint32_t at = mlocAliasSlot(key, slotCount);
    while (slots[at * 2] != 0 || slots[at * 2 + 1] != 0)
    {
        at = (at + 1) & (slotCount - 1);
    }
    Put(slots + at * 2, index + 1, 2);
}

static uint8_t* WriteAliases(Aliases* aliases, size_t* sizeOut)
{
    uint32_t rules = (uint32_t)aliases->ruleCount;
    uint32_t regions = 0;
    uint32_t text = 0;
    for (int r = 0; r < aliases->ruleCount; r++)
    {
        regions += (uint32_t)aliases->rules[r].regionCount;
    }
    for (int i = 0; i < aliases->legacyCount; i++)
    {
        text +=
            (uint32_t)(strlen(aliases->legacy[i].type) + strlen(aliases->legacy[i].replacement));
    }
    uint64_t* keys = calloc(rules + 1, sizeof(uint64_t));
    if (keys == nullptr)
    {
        Die("out of memory", "aliases");
    }
    for (uint32_t r = 0; r < rules; r++)
    {
        keys[r] = (uint64_t)Key(aliases, &aliases->rules[r]) << 16 | r;
    }
    GenSort(keys, rules, sizeof(uint64_t), CompareKeys);
    uint32_t slots = SlotCount(keys, rules);
    size_t keysAt = aliasHeaderSize + (size_t)rules * aliasRuleSize;
    size_t slotsAt = keysAt + (size_t)rules * 6;
    size_t variantsAt = (slotsAt + (size_t)slots * 2 + 7) / 8 * 8;
    size_t regionsAt = variantsAt + (size_t)aliases->variantCount * 8;
    size_t legacyAt = regionsAt + (size_t)regions * 2;
    size_t textAt = legacyAt + (size_t)aliases->legacyCount * aliasLegacySize;
    size_t size = textAt + text;
    uint8_t* out = calloc(size, 1);
    if (out == nullptr)
    {
        Die("out of memory", "aliases");
    }
    uint32_t counts[7] = {
        rules, rules, (uint32_t)aliases->variantCount, regions, (uint32_t)aliases->legacyCount,
        text,  slots};
    for (int i = 0; i < 7; i++)
    {
        Put(out + i * 4, counts[i], 4);
    }
    uint32_t region = 0;
    for (uint32_t r = 0; r < rules; r++)
    {
        const Rule* rule = &aliases->rules[r];
        WriteRule(aliases, rule, out + aliasHeaderSize + (size_t)r * aliasRuleSize, region);
        for (int k = 0; k < rule->regionCount; k++)
        {
            const char* code = rule->regions[k];
            Put(out + regionsAt + (size_t)region++ * 2,
                mlocPackRegion(code, (uint32_t)strlen(code)), 2);
        }
    }
    for (uint32_t i = 0; i < rules; i++)
    {
        Put(out + keysAt + (size_t)i * 4, keys[i] >> 16, 4);
        Put(out + keysAt + (size_t)rules * 4 + (size_t)i * 2, keys[i] & 0xFFFF, 2);
        if (i == 0 || keys[i] >> 16 != keys[i - 1] >> 16)
        {
            PutSlot(out + slotsAt, slots, (uint32_t)(keys[i] >> 16), i);
        }
    }
    for (int v = 0; v < aliases->variantCount; v++)
    {
        memcpy(out + variantsAt + (size_t)v * 8, aliases->variants[v],
               strlen(aliases->variants[v]));
    }
    GenSort(aliases->legacy, (size_t)aliases->legacyCount, sizeof(Legacy), CompareLegacy);
    uint32_t at = 0;
    for (int i = 0; i < aliases->legacyCount; i++)
    {
        const Legacy* legacy = &aliases->legacy[i];
        uint8_t* entry = out + legacyAt + (size_t)i * aliasLegacySize;
        uint32_t typeLength = (uint32_t)strlen(legacy->type);
        uint32_t replacementLength = (uint32_t)strlen(legacy->replacement);
        Put(entry, at, 2);
        Put(entry + 2, typeLength, 2);
        Put(entry + 4, at + typeLength, 2);
        Put(entry + 6, replacementLength, 2);
        memcpy(out + textAt + at, legacy->type, typeLength);
        memcpy(out + textAt + at + typeLength, legacy->replacement, replacementLength);
        at += typeLength + replacementLength;
    }
    free(keys);
    *sizeOut = size;
    return out;
}

int GenerateAliasUnit(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/supplementalMetadata.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    const XmlNode* metadata = XmlFindChild(root, "metadata");
    const XmlNode* list = metadata == nullptr ? nullptr : XmlFindChild(metadata, "alias");
    if (list == nullptr)
    {
        snprintf(errorOut, errorCapacity, "supplementalMetadata.xml has no metadata/alias");
        XmlFree(root);
        return 0;
    }
    Aliases* aliases = calloc(1, sizeof(Aliases));
    if (aliases == nullptr)
    {
        Die("out of memory", "aliases");
    }
    static const char* const s_kinds[] = {"languageAlias", "scriptAlias", "territoryAlias",
                                          "variantAlias"};
    for (int i = 0; i < list->childCount; i++)
    {
        for (int k = 0; k < 4; k++)
        {
            if (strcmp(list->children[i]->name, s_kinds[k]) == 0)
            {
                AddAlias(aliases, list->children[i]);
            }
        }
    }
    XmlFree(root);
    GenSort(aliases->rules, (size_t)aliases->ruleCount, sizeof(Rule), CompareRules);
    CollectVariants(aliases);
    size_t size = 0;
    uint8_t* unit = WriteAliases(aliases, &size);
    DataFileAdd(file, kindLocaleAliases, nullptr, unit, size);
    free(unit);
    free(aliases);
    return 1;
}
