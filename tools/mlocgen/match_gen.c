// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The match unit (match_gen.h). A node holds the distance of the pairs
// so far and its cells, one per (desired, supported) pair of the next
// subtag; "*" is the pair of any two. Partitions come from
// match_regions.h.

#include "match_gen.h"

#include "kinds.h"
#include "locale_tables.h"
#include "map.h"
#include "match_regions.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANY "*"

enum
{
    maxRules = 1024,
    maxSubtag = 16,
    // ICU's flag on a language pair whose script table is only the
    // default one, followed by region rules.
    skipScript = 0x80,
};

typedef struct Node Node;

typedef struct Cell
{
    char desired[maxSubtag];
    char supported[maxSubtag];
    Node* node;
} Cell;

struct Node
{
    int distance;
    Cell* cells;
    int count;
    int capacity;
};

// A rule's pairs from a level on, in either direction.
typedef struct Key
{
    const char* pairs[6];
    int depth;
    int distance;
    bool reversed;
} Key;

typedef struct Rule
{
    char desired[3][maxSubtag];
    char supported[3][maxSubtag];
    int size;
    int distance;
    bool oneway;
} Rule;

typedef struct Builder
{
    Node* root;
    int minRegionDistance;
} Builder;

[[noreturn]] static void Die(const char* message, const char* detail)
{
    fprintf(stderr, "mlocgen: %s: %s\n", message, detail);
    exit(1);
}

static Node* NewNode(int distance)
{
    Node* node = calloc(1, sizeof(Node));
    if (node == nullptr)
    {
        Die("out of memory", "match");
    }
    node->distance = distance;
    return node;
}

static void FreeNode(Node* node)
{
    for (int i = 0; i < node->count; i++)
    {
        FreeNode(node->cells[i].node);
    }
    free(node->cells);
    free(node);
}

static Node* Get(const Node* node, const char* desired, const char* supported)
{
    for (int i = 0; i < node->count; i++)
    {
        if (strcmp(node->cells[i].desired, desired) == 0 &&
            strcmp(node->cells[i].supported, supported) == 0)
        {
            return node->cells[i].node;
        }
    }
    return nullptr;
}

static Node* Any(const Node* node)
{
    return Get(node, ANY, ANY);
}

static Node* Put(Node* node, const char* desired, const char* supported, int distance)
{
    if (node->count == node->capacity)
    {
        node->capacity = node->capacity == 0 ? 8 : node->capacity * 2;
        node->cells = realloc(node->cells, (size_t)node->capacity * sizeof(Cell));
        if (node->cells == nullptr)
        {
            Die("out of memory", "match");
        }
    }
    Cell* cell = &node->cells[node->count++];
    snprintf(cell->desired, sizeof(cell->desired), "%s", desired);
    snprintf(cell->supported, sizeof(cell->supported), "%s", supported);
    cell->node = NewNode(distance);
    return cell->node;
}

static const char* Desired(const Key* key, int level)
{
    return key->pairs[2 * level + (key->reversed ? 1 : 0)];
}

static const char* Supported(const Key* key, int level)
{
    return key->pairs[2 * level + (key->reversed ? 0 : 1)];
}

static void PutIfAbsent(Node* node, const Key* key, int level)
{
    if (Get(node, Desired(key, level), Supported(key, level)) == nullptr)
    {
        Put(node, Desired(key, level), Supported(key, level), key->distance);
    }
}

// The node of a pair, made when missing from the "*" one: its distance
// (0 for a pair of equal subtags) and a copy of its cells' distances.
static Node* Intermediate(Node* node, const Key* key, int level)
{
    const char* desired = Desired(key, level);
    const char* supported = Supported(key, level);
    Node* found = Get(node, desired, supported);
    if (found != nullptr)
    {
        return found;
    }
    Node* any = Any(node);
    if (any == nullptr)
    {
        Die("a rule before its level's default rule", desired);
    }
    found = Put(node, desired, supported, strcmp(desired, supported) == 0 ? 0 : any->distance);
    for (int i = 0; i < any->count; i++)
    {
        Put(found, any->cells[i].desired, any->cells[i].supported, any->cells[i].node->distance);
    }
    return found;
}

static void AddExplicit(Node* node, const Key* key, int level)
{
    if (level == key->depth - 1)
    {
        PutIfAbsent(node, key, level);
        return;
    }
    AddExplicit(Intermediate(node, key, level), key, level + 1);
}

// Puts a rule with a "*" under every pair it matches.
static void AddWildcard(Node* node, const Key* key, int level)
{
    if (level == key->depth - 1)
    {
        PutIfAbsent(node, key, level);
        return;
    }
    if (strcmp(Desired(key, level), ANY) == 0)
    {
        for (int i = 0; i < node->count; i++)
        {
            AddWildcard(node->cells[i].node, key, level + 1);
        }
        return;
    }
    Node* next = Get(node, Desired(key, level), Supported(key, level));
    if (next != nullptr)
    {
        AddWildcard(next, key, level + 1);
    }
}

static void AddMapping(Builder* builder, const Key* key)
{
    AddExplicit(builder->root, key, 0);
    for (int level = 0; level < key->depth; level++)
    {
        if (strcmp(Desired(key, level), ANY) == 0)
        {
            AddWildcard(builder->root, key, 0);
            return;
        }
    }
}

static void AddDistance(Builder* builder, const Rule* rule, const char* desiredPartition,
                        const char* supportedPartition)
{
    Key key = {.depth = rule->size, .distance = rule->distance};
    bool symmetrical = true;
    for (int level = 0; level < rule->size; level++)
    {
        key.pairs[2 * level] = level == 2 ? desiredPartition : rule->desired[level];
        key.pairs[2 * level + 1] = level == 2 ? supportedPartition : rule->supported[level];
        symmetrical &= strcmp(key.pairs[2 * level], key.pairs[2 * level + 1]) == 0;
    }
    if (rule->size == 3 && rule->distance < builder->minRegionDistance)
    {
        builder->minRegionDistance = rule->distance;
    }
    AddMapping(builder, &key);
    if (!rule->oneway && !symmetrical)
    {
        key.reversed = true;
        AddMapping(builder, &key);
    }
}

// A rule with a region adds each pair of the partitions of its two
// region variables.
static void AddRule(Builder* builder, const Rule* rule, const Partitions* partitions)
{
    if (rule->size < 3)
    {
        AddDistance(builder, rule, nullptr, nullptr);
        return;
    }
    const char* desired = PartitionsOf(partitions, rule->desired[2]);
    const char* supported = PartitionsOf(partitions, rule->supported[2]);
    for (const char* d = desired; *d != '\0'; d++)
    {
        for (const char* s = supported; *s != '\0'; s++)
        {
            char dp[2] = {*d, '\0'};
            char sp[2] = {*s, '\0'};
            AddDistance(builder, rule, strcmp(desired, ANY) == 0 ? ANY : dp,
                        strcmp(supported, ANY) == 0 ? ANY : sp);
        }
    }
}

// Drops a script table holding only the default region one, then a
// language's script table holding only the default script one, which
// becomes skipped when region rules follow it.
static void Prune(Node* root, int scriptDistance, int regionDistance)
{
    for (int l = 0; l < root->count; l++)
    {
        Node* language = root->cells[l].node;
        for (int s = 0; s < language->count; s++)
        {
            Node* script = language->cells[s].node;
            Node* any = Any(script);
            if (script->count == 1 && (any == nullptr || any->distance == regionDistance))
            {
                if (any == nullptr)
                {
                    Die("a script table without a default region", language->cells[s].desired);
                }
                FreeNode(any);
                script->count = 0;
            }
        }
        Node* any = Any(language);
        if (language->count == 1 && any != nullptr && any->distance == scriptDistance)
        {
            if (any->count == 0)
            {
                FreeNode(any);
                language->count = 0;
            }
            else
            {
                language->distance |= skipScript;
            }
        }
    }
}

static void SplitSpec(const char* text, char (*parts)[maxSubtag], int* sizeOut)
{
    int size = 0;
    for (const char* at = text;; size++)
    {
        size_t length = strcspn(at, "_");
        if (size == 3 || length == 0 || length >= maxSubtag)
        {
            Die("a language match spec not of 1 to 3 subtags", text);
        }
        snprintf(parts[size], maxSubtag, "%.*s", (int)length, at);
        if (at[length] == '\0')
        {
            *sizeOut = size + 1;
            return;
        }
        at += length + 1;
    }
}

// Reads a rule, its region subtags made variables, and checks what ICU
// checks: both sides alike, "*" on both or neither side of each level,
// and all "*" after a "*" language.
static void ReadRule(const XmlNode* match, Partitions* partitions, Rule* rule)
{
    const char* desired = XmlGetAttribute(match, "desired");
    const char* supported = XmlGetAttribute(match, "supported");
    const char* distance = XmlGetAttribute(match, "distance");
    const char* oneway = XmlGetAttribute(match, "oneway");
    if (desired == nullptr || supported == nullptr || distance == nullptr)
    {
        Die("a languageMatch without desired, supported or distance", "languageInfo.xml");
    }
    int supportedSize = 0;
    SplitSpec(desired, rule->desired, &rule->size);
    SplitSpec(supported, rule->supported, &supportedSize);
    char* end = nullptr;
    long value = strtol(distance, &end, 10);
    if (supportedSize != rule->size || *end != '\0' || value < 0 || value > 100)
    {
        Die("a languageMatch with uneven sides or a bad distance", desired);
    }
    rule->distance = (int)value;
    rule->oneway = oneway != nullptr && strcmp(oneway, "true") == 0;
    bool anyLanguage = strcmp(rule->desired[0], ANY) == 0;
    for (int level = 0; level < rule->size; level++)
    {
        bool d = strcmp(rule->desired[level], ANY) == 0;
        bool s = strcmp(rule->supported[level], ANY) == 0;
        if (d != s || (anyLanguage && !d))
        {
            Die("a languageMatch with a lone \"*\"", desired);
        }
    }
    if (rule->size == 3)
    {
        char variable[maxSubtag];
        snprintf(variable, sizeof(variable), "%s",
                 PartitionsEnsureVariable(partitions, rule->desired[2]));
        snprintf(rule->desired[2], maxSubtag, "%s", variable);
        snprintf(variable, sizeof(variable), "%s",
                 PartitionsEnsureVariable(partitions, rule->supported[2]));
        snprintf(rule->supported[2], maxSubtag, "%s", variable);
    }
}

// Reads the variables, then the rules in order, checking that the sizes
// only grow and each growth follows a default rule.
static int ReadRules(const XmlNode* matches, Partitions* partitions, Rule* rules)
{
    for (int i = 0; i < matches->childCount; i++)
    {
        const XmlNode* child = matches->children[i];
        if (strcmp(child->name, "matchVariable") == 0)
        {
            const char* id = XmlGetAttribute(child, "id");
            const char* value = XmlGetAttribute(child, "value");
            if (id == nullptr || value == nullptr)
            {
                Die("a matchVariable without id or value", "languageInfo.xml");
            }
            PartitionsAddVariable(partitions, id, value);
        }
    }
    int count = 0;
    for (int i = 0; i < matches->childCount; i++)
    {
        const XmlNode* child = matches->children[i];
        if (strcmp(child->name, "languageMatch") != 0)
        {
            continue;
        }
        if (count == maxRules)
        {
            Die("too many languageMatch rules", "languageInfo.xml");
        }
        Rule* rule = &rules[count];
        ReadRule(child, partitions, rule);
        int previous = count == 0 ? 1 : rules[count - 1].size;
        if (rule->size < previous ||
            (rule->size > previous &&
             (count == 0 || strcmp(rules[count - 1].desired[0], ANY) != 0)))
        {
            Die("languageMatch rules out of order or without a default rule before", "rule");
        }
        count++;
    }
    return count;
}

static void Store(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

static uint64_t PackPair(const Cell* cell, int level)
{
    if (strcmp(cell->desired, ANY) == 0)
    {
        return 0;
    }
    if (level == 0)
    {
        uint32_t d = mlocPackLanguage(cell->desired, (uint32_t)strlen(cell->desired));
        uint32_t s = mlocPackLanguage(cell->supported, (uint32_t)strlen(cell->supported));
        if (d == 0 || s == 0)
        {
            Die("a languageMatch language of other than 2 or 3 letters", cell->desired);
        }
        return (uint64_t)d << 15 | s;
    }
    if (level == 1)
    {
        uint32_t d = mlocPackScript(cell->desired, (uint32_t)strlen(cell->desired));
        uint32_t s = mlocPackScript(cell->supported, (uint32_t)strlen(cell->supported));
        if (d == 0 || s == 0)
        {
            Die("a languageMatch script of other than 4 letters", cell->desired);
        }
        return (uint64_t)d << 20 | s;
    }
    return (uint64_t)(uint8_t)cell->desired[0] << 8 | (uint8_t)cell->supported[0];
}

static int CompareCells(const Cell* a, const Cell* b, int level)
{
    uint64_t x = PackPair(a, level);
    uint64_t y = PackPair(b, level);
    return x < y ? -1 : x > y;
}

static int CompareLanguages(const void* a, const void* b)
{
    return CompareCells(a, b, 0);
}

static int CompareScripts(const void* a, const void* b)
{
    return CompareCells(a, b, 1);
}

static int CompareRegions(const void* a, const void* b)
{
    return CompareCells(a, b, 2);
}

static void SortCells(Node* node, int level)
{
    static int (*const s_compare[3])(const void*, const void*) = {CompareLanguages, CompareScripts,
                                                                  CompareRegions};
    qsort(node->cells, (size_t)node->count, sizeof(Cell), s_compare[level]);
}

typedef struct Table
{
    const Node* root;
    const Partitions* partitions;
    const uint64_t* paradigms;
    int paradigmCount;
    int distances[4];
    size_t languageCount;
    size_t scriptCount;
    size_t regionCount;
    size_t textSize;
} Table;

// The children of a language pair: its regions when its script table is
// skipped, else its scripts.
static Node* Children(Node* language)
{
    return (language->distance & skipScript) != 0 ? Any(language) : language;
}

// Sorts every run and counts the entries of each level.
static void Measure(Table* table, Node* root)
{
    SortCells(root, 0);
    for (int l = 0; l < root->count; l++)
    {
        Node* language = root->cells[l].node;
        Node* children = Children(language);
        if ((language->distance & skipScript) != 0)
        {
            SortCells(children, 2);
            table->regionCount += (size_t)children->count;
            continue;
        }
        SortCells(children, 1);
        table->scriptCount += (size_t)children->count;
        for (int s = 0; s < children->count; s++)
        {
            SortCells(children->cells[s].node, 2);
            table->regionCount += (size_t)children->cells[s].node->count;
        }
    }
    table->languageCount = (size_t)root->count;
    for (int g = 0; g < table->partitions->groupCount; g++)
    {
        table->textSize += strlen(table->partitions->groups[g]);
    }
}

typedef struct Cursor
{
    uint8_t* scripts;
    uint8_t* languages;
    uint8_t* regions;
    size_t script;
    size_t region;
} Cursor;

static size_t WriteRegions(Cursor* at, const Node* node)
{
    size_t start = at->region;
    for (int r = 0; r < node->count; r++)
    {
        uint8_t* entry = at->regions + at->region++ * matchRegionSize;
        uint64_t pair = PackPair(&node->cells[r], 2);
        entry[0] = (uint8_t)(pair >> 8);
        entry[1] = (uint8_t)pair;
        entry[2] = (uint8_t)node->cells[r].node->distance;
    }
    return start;
}

static size_t WriteScripts(Cursor* at, const Node* node)
{
    size_t start = at->script;
    for (int s = 0; s < node->count; s++)
    {
        const Node* script = node->cells[s].node;
        uint8_t* entry = at->scripts + at->script++ * matchScriptSize;
        Store(entry, PackPair(&node->cells[s], 1), 8);
        Store(entry + 8, WriteRegions(at, script), 2);
        Store(entry + 10, (uint64_t)script->count, 2);
        entry[12] = (uint8_t)script->distance;
    }
    return start;
}

static void WriteLevels(Cursor* at, const Node* root)
{
    for (int l = 0; l < root->count; l++)
    {
        Node* language = root->cells[l].node;
        bool skip = (language->distance & skipScript) != 0;
        const Node* children = Children(language);
        uint8_t* entry = at->languages + (size_t)l * matchLanguageSize;
        Store(entry, PackPair(&root->cells[l], 0), 4);
        Store(entry + 4, skip ? WriteRegions(at, children) : WriteScripts(at, children), 2);
        Store(entry + 6, (uint64_t)children->count, 2);
        entry[8] = (uint8_t)(language->distance & ~skipScript);
        entry[9] = skip ? matchSkipScript : 0;
    }
}

static uint8_t* WriteTable(const Table* table, size_t* sizeOut)
{
    size_t scripts = matchHeaderSize + (size_t)table->paradigmCount * 8;
    size_t languages = scripts + table->scriptCount * matchScriptSize;
    size_t regions = languages + table->languageCount * matchLanguageSize;
    size_t groups = regions + table->regionCount * matchRegionSize;
    const Partitions* partitions = table->partitions;
    size_t slots = groups + (size_t)partitions->groupCount * matchGroupSize;
    size_t text = slots + matchRegionSlots;
    uint8_t* out = calloc(text + table->textSize, 1);
    if (out == nullptr)
    {
        Die("out of memory", "match");
    }
    for (int i = 0; i < 4; i++)
    {
        out[i] = (uint8_t)table->distances[i];
    }
    const size_t counts[6] = {
        (size_t)table->paradigmCount, table->languageCount,           table->scriptCount,
        table->regionCount,           (size_t)partitions->groupCount, table->textSize};
    for (int i = 0; i < 6; i++)
    {
        Store(out + 4 + 2 * i, counts[i], 2);
    }
    for (int i = 0; i < table->paradigmCount; i++)
    {
        Store(out + matchHeaderSize + 8 * (size_t)i, table->paradigms[i], 8);
    }
    Cursor at = {.scripts = out + scripts, .languages = out + languages, .regions = out + regions};
    WriteLevels(&at, table->root);
    size_t offset = 0;
    for (int g = 0; g < partitions->groupCount; g++)
    {
        size_t length = strlen(partitions->groups[g]);
        Store(out + groups + (size_t)g * matchGroupSize, offset, 2);
        Store(out + groups + (size_t)g * matchGroupSize + 2, length, 2);
        memcpy(out + text + offset, partitions->groups[g], length);
        offset += length;
    }
    for (uint32_t slot = 0; slot < matchRegionSlots; slot++)
    {
        out[slots + slot] =
            (uint8_t)(partitions->groupOf[slot] | (partitions->macro[slot] ? matchMacroRegion : 0));
    }
    *sizeOut = text + table->textSize;
    return out;
}

// The parts of a CLDR id: language, then a script of 4 letters, then a
// region; absent ones empty.
static void SplitId(const char* id, char* language, char* script, char* region)
{
    char parts[3][maxSubtag] = {{0}};
    int size = 0;
    SplitSpec(id, parts, &size);
    snprintf(language, maxSubtag, "%s", parts[0]);
    script[0] = region[0] = '\0';
    for (int i = 1; i < size; i++)
    {
        snprintf(strlen(parts[i]) == 4 ? script : region, maxSubtag, "%s", parts[i]);
    }
}

// A paradigm locale with likely subtags added (UTS #35's lookup order),
// packed.
static uint64_t Maximize(const Map* likely, const char* id)
{
    char language[maxSubtag], script[maxSubtag], region[maxSubtag];
    SplitId(id, language, script, region);
    char tries[4][3 * maxSubtag];
    snprintf(tries[0], sizeof(tries[0]), "%s_%s_%s", language, script, region);
    snprintf(tries[1], sizeof(tries[1]), "%s_%s", language, region);
    snprintf(tries[2], sizeof(tries[2]), "%s_%s", language, script);
    snprintf(tries[3], sizeof(tries[3]), "%s", language);
    const bool usable[4] = {script[0] != '\0' && region[0] != '\0', region[0] != '\0',
                            script[0] != '\0', true};
    for (int t = 0; t < 4; t++)
    {
        const char* to = usable[t] ? MapGet(likely, tries[t]) : nullptr;
        if (to == nullptr)
        {
            continue;
        }
        char l[maxSubtag], s[maxSubtag], r[maxSubtag];
        SplitId(to, l, s, r);
        const char* lf = strcmp(language, "und") == 0 ? l : language;
        const char* sf = script[0] != '\0' ? script : s;
        const char* rf = region[0] != '\0' ? region : r;
        uint32_t lp = mlocPackLanguage(lf, (uint32_t)strlen(lf));
        uint32_t sp = mlocPackScript(sf, (uint32_t)strlen(sf));
        uint32_t rp = mlocPackRegion(rf, (uint32_t)strlen(rf));
        if (lp == 0 || sp == 0 || rp == 0)
        {
            Die("a paradigm locale that does not maximize", id);
        }
        return mlocPackTag(lp, sp, rp);
    }
    Die("a paradigm locale without likely subtags", id);
}

// The paradigm locales, maximized, each once in their order.
static int ReadParadigms(const XmlNode* matches, const Map* likely, uint64_t* paradigms)
{
    const XmlNode* node = XmlFindChild(matches, "paradigmLocales");
    const char* locales = node == nullptr ? nullptr : XmlGetAttribute(node, "locales");
    if (locales == nullptr)
    {
        Die("no paradigmLocales", "languageInfo.xml");
    }
    int count = 0;
    for (const char* at = locales + strspn(locales, " "); *at != '\0';)
    {
        size_t length = strcspn(at, " ");
        char id[3 * maxSubtag];
        snprintf(id, sizeof(id), "%.*s", (int)(length < sizeof(id) - 1 ? length : sizeof(id) - 1),
                 at);
        uint64_t tag = Maximize(likely, id);
        bool seen = false;
        for (int i = 0; i < count; i++)
        {
            seen |= paradigms[i] == tag;
        }
        if (!seen)
        {
            if (count == matchMaxParadigms)
            {
                Die("too many paradigm locales", id);
            }
            paradigms[count++] = tag;
        }
        at += length;
        at += strspn(at, " ");
    }
    if (count % 2 != 0)
    {
        Die("unpaired paradigm locales", locales);
    }
    return count;
}

static void ReadLikely(const XmlNode* root, Map* likely)
{
    const XmlNode* list = XmlFindChild(root, "likelySubtags");
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        const char* from = XmlGetAttribute(list->children[i], "from");
        const char* to = XmlGetAttribute(list->children[i], "to");
        if (from != nullptr && to != nullptr)
        {
            // The tree outlives the map, so its text is used as it is.
            MapPut(likely, from, (void*)to);
        }
    }
}

static const XmlNode* WrittenMatches(const XmlNode* root)
{
    const XmlNode* matching = XmlFindChild(root, "languageMatching");
    for (int i = 0; matching != nullptr && i < matching->childCount; i++)
    {
        const XmlNode* child = matching->children[i];
        const char* type = XmlGetAttribute(child, "type");
        if (strcmp(child->name, "languageMatches") == 0 && type != nullptr &&
            strcmp(type, "written_new") == 0)
        {
            return child;
        }
    }
    Die("no languageMatches of type written_new", "languageInfo.xml");
}

// Builds the table from the rules: the defaults, then the pruning, then
// the default language pair dropped (ICU keeps its distances apart).
static Node* Build(const Rule* rules, int count, const Partitions* partitions, int* distances)
{
    Builder builder = {.root = NewNode(-1), .minRegionDistance = 100};
    for (int i = 0; i < count; i++)
    {
        AddRule(&builder, &rules[i], partitions);
    }
    Node* language = Any(builder.root);
    Node* script = language == nullptr ? nullptr : Any(language);
    Node* region = script == nullptr ? nullptr : Any(script);
    if (region == nullptr)
    {
        Die("no default language, script or region rule", "languageInfo.xml");
    }
    distances[0] = language->distance;
    distances[1] = script->distance;
    distances[2] = region->distance;
    distances[3] = builder.minRegionDistance;
    Prune(builder.root, distances[1], distances[2]);
    if (Any(builder.root)->count != 0)
    {
        Die("the default language rule keeps a table", "languageInfo.xml");
    }
    for (int i = 0; i < builder.root->count; i++)
    {
        if (strcmp(builder.root->cells[i].desired, ANY) == 0)
        {
            FreeNode(builder.root->cells[i].node);
            builder.root->cells[i] = builder.root->cells[--builder.root->count];
            break;
        }
    }
    return builder.root;
}

static XmlNode* ParseSupplemental(const char* commonDirectory, const char* name, char* errorOut,
                                  size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/%s", commonDirectory, name);
    return XmlParseFile(path, errorOut, errorCapacity);
}

int GenerateMatchUnit(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity)
{
    XmlNode* supplemental =
        ParseSupplemental(commonDirectory, "supplementalData.xml", errorOut, errorCapacity);
    XmlNode* info = supplemental == nullptr ? nullptr
                                            : ParseSupplemental(commonDirectory, "languageInfo.xml",
                                                                errorOut, errorCapacity);
    XmlNode* likelyRoot = info == nullptr ? nullptr
                                          : ParseSupplemental(commonDirectory, "likelySubtags.xml",
                                                              errorOut, errorCapacity);
    static Partitions s_partitions;
    static Rule s_rules[maxRules];
    if (likelyRoot == nullptr ||
        !PartitionsOpen(&s_partitions, supplemental, errorOut, errorCapacity))
    {
        XmlFree(supplemental);
        XmlFree(info);
        XmlFree(likelyRoot);
        return 0;
    }
    const XmlNode* matches = WrittenMatches(info);
    int count = ReadRules(matches, &s_partitions, s_rules);
    PartitionsBuild(&s_partitions);
    Map likely = {0};
    ReadLikely(likelyRoot, &likely);
    uint64_t paradigms[matchMaxParadigms];
    Table table = {.partitions = &s_partitions,
                   .paradigms = paradigms,
                   .paradigmCount = ReadParadigms(matches, &likely, paradigms)};
    Node* root = Build(s_rules, count, &s_partitions, table.distances);
    table.root = root;
    Measure(&table, root);
    if (table.scriptCount > 0xFFFF || table.regionCount > 0xFFFF || table.languageCount > 0xFFFF)
    {
        Die("a match table too large for its 16-bit counts", "languageInfo.xml");
    }
    size_t size = 0;
    uint8_t* unit = WriteTable(&table, &size);
    DataFileAdd(file, kindLocaleMatch, nullptr, unit, size);
    free(unit);
    FreeNode(root);
    MapFree(&likely);
    PartitionsClose(&s_partitions);
    XmlFree(supplemental);
    XmlFree(info);
    XmlFree(likelyRoot);
    return 1;
}
