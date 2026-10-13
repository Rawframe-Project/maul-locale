// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's inheritance (cldr.h).

#include "cldr.h"

#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHAIN       2048
#define MAX_STEPS       32
#define MAX_ATTRIBUTES  8
#define MAX_NAME        64
#define MAX_VALUE       160
#define MAX_ALIAS_DEPTH 16

static const char* const s_inheritanceMarker = "\xE2\x86\x91\xE2\x86\x91\xE2\x86\x91";
static LdmlFile s_missing;

typedef struct Step
{
    char name[MAX_NAME];
    int attributeCount;
    char attributeNames[MAX_ATTRIBUTES][MAX_NAME];
    char attributeValues[MAX_ATTRIBUTES][MAX_VALUE];
} Step;

typedef struct Chain
{
    int count;
    Step steps[MAX_STEPS];
} Chain;

[[noreturn]] static void Die(const char* format, const char* detail)
{
    fputs("mlocgen: ", stderr);
    fprintf(stderr, format, detail);
    fputc('\n', stderr);
    exit(1);
}

static char* Copy(const char* text)
{
    size_t length = strlen(text) + 1;
    char* copy = malloc(length);
    if (copy == nullptr)
    {
        Die("%s", "out of memory");
    }
    return memcpy(copy, text, length);
}

// Copies at most capacity - 1 bytes of [begin, end); 0 when they do not fit.
static int CopyRange(char* out, size_t capacity, const char* begin, const char* end)
{
    size_t length = (size_t)(end - begin);
    if (length >= capacity)
    {
        return 0;
    }
    memcpy(out, begin, length);
    out[length] = '\0';
    return 1;
}

// One "[@name='value']" or "[@name=\"value\"]" at text; returns the end, or
// null when malformed.
static const char* ParseAttribute(const char* text, Step* step)
{
    if (strncmp(text, "[@", 2) != 0 || step->attributeCount == MAX_ATTRIBUTES)
    {
        return nullptr;
    }
    const char* name = text + 2;
    const char* equals = strchr(name, '=');
    if (equals == nullptr || (equals[1] != '"' && equals[1] != '\''))
    {
        return nullptr;
    }
    const char* value = equals + 2;
    const char* close = strchr(value, equals[1]);
    int i = step->attributeCount;
    if (close == nullptr || close[1] != ']' ||
        !CopyRange(step->attributeNames[i], MAX_NAME, name, equals) ||
        !CopyRange(step->attributeValues[i], MAX_VALUE, value, close))
    {
        return nullptr;
    }
    step->attributeCount++;
    return close + 2;
}

// Parses steps separated by '/', each a name or "..", with attributes;
// values may contain '/'. Leading slashes are skipped.
static int ParseSteps(const char* text, Chain* chain)
{
    while (*text == '/')
    {
        text++;
    }
    while (*text != '\0')
    {
        if (chain->count == MAX_STEPS)
        {
            return 0;
        }
        Step* step = &chain->steps[chain->count++];
        *step = (Step){0};
        size_t length = strcspn(text, "[/");
        if (length == 0 || !CopyRange(step->name, MAX_NAME, text, text + length))
        {
            return 0;
        }
        text += length;
        while (*text == '[')
        {
            text = ParseAttribute(text, step);
            if (text == nullptr)
            {
                return 0;
            }
        }
        if (*text == '/')
        {
            text++;
        }
    }
    return 1;
}

static int CompareIndexes(const void* a, const void* b, const Step* step)
{
    return strcmp(step->attributeNames[*(const int*)a], step->attributeNames[*(const int*)b]);
}

// The canonical form: "//name[@a=\"x\"]...", attributes alphabetical,
// those removed by lateral fallback (an empty name) left out.
static int FormatChain(const Chain* chain, int count, char* out)
{
    size_t length = 1;
    out[0] = '/';
    out[1] = '\0';
    for (int s = 0; s < count; s++)
    {
        const Step* step = &chain->steps[s];
        int order[MAX_ATTRIBUTES];
        for (int i = 0; i < step->attributeCount; i++)
        {
            order[i] = i;
            for (int j = i; j > 0 && CompareIndexes(&order[j], &order[j - 1], step) < 0; j--)
            {
                int swap = order[j];
                order[j] = order[j - 1];
                order[j - 1] = swap;
            }
        }
        int used = snprintf(out + length, MAX_CHAIN - length, "/%s", step->name);
        for (int i = 0; i < step->attributeCount && used >= 0; i++)
        {
            const char* name = step->attributeNames[order[i]];
            size_t at = length + (size_t)used;
            int more = name[0] == '\0' || at >= MAX_CHAIN
                           ? 0
                           : snprintf(out + at, MAX_CHAIN - at, "[@%s=\"%s\"]", name,
                                      step->attributeValues[order[i]]);
            used = more < 0 ? -1 : used + more;
        }
        if (used < 0 || length + (size_t)used >= MAX_CHAIN)
        {
            return 0;
        }
        length += (size_t)used;
    }
    return 1;
}

static void AddParents(Map* parents, const XmlNode* list)
{
    for (int i = 0; i < list->childCount; i++)
    {
        const XmlNode* entry = list->children[i];
        const char* parent = XmlGetAttribute(entry, "parent");
        const char* locales = XmlGetAttribute(entry, "locales");
        if (strcmp(entry->name, "parentLocale") != 0 || parent == nullptr || locales == nullptr)
        {
            continue;
        }
        char* copy = Copy(locales);
        for (char* id = strtok(copy, " "); id != nullptr; id = strtok(nullptr, " "))
        {
            MapPut(parents, id, Copy(parent));
        }
        free(copy);
    }
}

// Reads each language's likely script from likelySubtags.xml.
static int LoadScripts(Cldr* cldr, const char* commonDirectory, char* errorOut,
                       size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/likelySubtags.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    const XmlNode* list = XmlFindChild(root, "likelySubtags");
    for (int i = 0; list != nullptr && i < list->childCount; i++)
    {
        const char* from = XmlGetAttribute(list->children[i], "from");
        const char* to = XmlGetAttribute(list->children[i], "to");
        const char* script = to == nullptr ? nullptr : strchr(to, '_');
        if (from != nullptr && strchr(from, '_') == nullptr && script != nullptr &&
            strlen(script) >= 5)
        {
            char copy[5];
            memcpy(copy, script + 1, 4);
            copy[4] = '\0';
            MapPut(&cldr->scripts, from, Copy(copy));
        }
    }
    XmlFree(root);
    return 1;
}

int CldrOpen(Cldr* cldr, const char* commonDirectory, char* errorOut, size_t errorCapacity)
{
    *cldr = (Cldr){.commonDirectory = Copy(commonDirectory), .threshold = draftContributed};
    char path[4096];
    snprintf(path, sizeof(path), "%s/dtd/ldml.dtd", commonDirectory);
    if (!DtdLoad(&cldr->dtd, path, errorOut, errorCapacity))
    {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/supplemental/supplementalData.xml", commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    if (root == nullptr)
    {
        return 0;
    }
    // The rule localeRules="nonlikelyScript" makes root the parent of a
    // locale whose script is not its language's likely one; CLDR lists
    // every such locale it has a file for, and only files are looked up.
    for (int i = 0; i < root->childCount; i++)
    {
        const XmlNode* list = root->children[i];
        if (strcmp(list->name, "parentLocales") != 0)
        {
            continue;
        }
        const char* component = XmlGetAttribute(list, "component");
        if (component == nullptr)
        {
            AddParents(&cldr->parents, list);
        }
        else if (strstr(component, "collations") != nullptr)
        {
            AddParents(&cldr->collationParents, list);
        }
    }
    XmlFree(root);
    return LoadScripts(cldr, commonDirectory, errorOut, errorCapacity);
}

const char* CldrParent(Cldr* cldr, const char* locale, Component component)
{
    if (strcmp(locale, "root") == 0)
    {
        return nullptr;
    }
    // A component's own parent locales stand alone: they are not merged
    // with the main ones (UTS #35, Parent Locales), so zh_Hant's
    // collation parent is zh, not root.
    const char* parent =
        MapGet(component == componentCollation ? &cldr->collationParents : &cldr->parents, locale);
    if (parent != nullptr)
    {
        return parent;
    }
    const char* cut = strrchr(locale, '_');
    if (cut == nullptr)
    {
        return "root";
    }
    if (component != componentCollation && strchr(locale, '_') == cut && strlen(cut + 1) == 4)
    {
        char language[16];
        const char* likely = CopyRange(language, sizeof(language), locale, cut)
                                 ? MapGet(&cldr->scripts, language)
                                 : nullptr;
        if (likely == nullptr || strcmp(likely, cut + 1) != 0)
        {
            return "root";
        }
    }
    // The truncated id is kept in the parents map so that it lives as
    // long as the Cldr.
    char truncated[256];
    if (!CopyRange(truncated, sizeof(truncated), locale, cut))
    {
        Die("a locale id is too long: %s", locale);
    }
    char key[300];
    snprintf(key, sizeof(key), "#truncated:%s", locale);
    const char* kept = MapGet(&cldr->parents, key);
    if (kept == nullptr)
    {
        MapPut(&cldr->parents, key, Copy(truncated));
        kept = MapGet(&cldr->parents, key);
    }
    return kept;
}

static const char* const s_directories[componentCount] = {"main", "collation", "rbnf"};

static LdmlFile* File(Cldr* cldr, const char* locale, Component component)
{
    LdmlFile* file = MapGet(&cldr->files[component], locale);
    if (file != nullptr)
    {
        return file == &s_missing ? nullptr : file;
    }
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s/%s.xml", cldr->commonDirectory, s_directories[component],
             locale);
    FILE* probe = fopen(path, "rb");
    if (probe == nullptr)
    {
        MapPut(&cldr->files[component], locale, &s_missing);
        return nullptr;
    }
    fclose(probe);
    char error[1024];
    file = LdmlLoad(path, &cldr->dtd, error, sizeof(error));
    if (file == nullptr)
    {
        Die("%s", error);
    }
    MapPut(&cldr->files[component], locale, file);
    return file;
}

const LdmlFile* CldrFile(Cldr* cldr, const char* locale, Component component)
{
    return File(cldr, locale, component);
}

// A value below the draft threshold counts as absent, and so does the
// inheritance marker.
static const Leaf* Accept(const Cldr* cldr, const Leaf* leaf)
{
    if (leaf == nullptr || leaf->draft > cldr->threshold ||
        strcmp(leaf->text, s_inheritanceMarker) == 0)
    {
        return nullptr;
    }
    return leaf;
}

typedef struct Search
{
    Cldr* cldr;
    const char* locale;
    Component component;
} Search;

static const Leaf* Locate(const Search* search, Chain* chain, int depth);

// Root's alias on the deepest element of the chain that has one: the
// chain rewritten, or 0 when none applies.
static int Alias(const LdmlFile* root, const Chain* chain, Chain* target)
{
    char prefix[MAX_CHAIN];
    for (int k = chain->count - 1; k > 0; k--)
    {
        const char* path = FormatChain(chain, k, prefix) ? MapGet(&root->aliases, prefix) : nullptr;
        if (path == nullptr)
        {
            continue;
        }
        Chain relative = {0};
        if (!ParseSteps(path, &relative))
        {
            Die("an alias path cannot be read: %s", path);
        }
        *target = *chain;
        target->count = k;
        for (int i = 0; i < relative.count; i++)
        {
            if (strcmp(relative.steps[i].name, "..") == 0)
            {
                target->count -= target->count > 0;
            }
            else if (target->count < MAX_STEPS)
            {
                target->steps[target->count++] = relative.steps[i];
            }
        }
        for (int i = k; i < chain->count && target->count < MAX_STEPS; i++)
        {
            target->steps[target->count++] = chain->steps[i];
        }
        return 1;
    }
    return 0;
}

// Sets every attribute named name to value, or removes it when value is
// null; returns how many it changed.
static int Rewrite(Chain* chain, const char* name, const char* value)
{
    int changed = 0;
    for (int s = 0; s < chain->count; s++)
    {
        Step* step = &chain->steps[s];
        for (int a = 0; a < step->attributeCount; a++)
        {
            if (strcmp(step->attributeNames[a], name) != 0 ||
                (value != nullptr && strcmp(step->attributeValues[a], value) == 0))
            {
                continue;
            }
            if (value == nullptr)
            {
                step->attributeNames[a][0] = '\0';
            }
            else
            {
                snprintf(step->attributeValues[a], MAX_VALUE, "%s", value);
            }
            changed++;
        }
    }
    return changed;
}

static int Has(const Chain* chain, const char* attribute, const char* element)
{
    for (int s = 0; s < chain->count; s++)
    {
        if (element != nullptr && strcmp(chain->steps[s].name, element) == 0)
        {
            return 1;
        }
        for (int a = 0; attribute != nullptr && a < chain->steps[s].attributeCount; a++)
        {
            if (strcmp(chain->steps[s].attributeNames[a], attribute) == 0)
            {
                return 1;
            }
        }
    }
    return 0;
}

// The fallback CLDR applies when no locale up to root has the chain: an
// alias in root; else, as if root aliased them, the chain without its
// alt, or with its counts made "other" (a currency's displayName with
// count="other" falls to the one without a count). Writes the next chain
// to try and returns 1, or returns 0 when there is none.
static int Fallback(const LdmlFile* root, const Chain* chain, Chain* next)
{
    if (root != nullptr && Alias(root, chain, next))
    {
        return 1;
    }
    *next = *chain;
    if (Has(chain, "alt", nullptr))
    {
        return Rewrite(next, "alt", nullptr) > 0;
    }
    if (!Has(chain, "count", nullptr))
    {
        return 0;
    }
    if (Rewrite(next, "count", "other") > 0)
    {
        return 1;
    }
    return Has(chain, nullptr, "displayName") && Rewrite(next, "count", nullptr) > 0;
}

// CLDR's own resolution (XMLSource.ResolvingSource.getPathLocation): the
// chain along the parent chain, skipping inheritance markers; then the
// fallback chain, looked up again from the locale asked for.
static const Leaf* Locate(const Search* search, Chain* chain, int depth)
{
    if (depth > MAX_ALIAS_DEPTH)
    {
        Die("aliases loop while resolving for %s", search->locale);
    }
    char text[MAX_CHAIN];
    if (!FormatChain(chain, chain->count, text))
    {
        return nullptr;
    }
    const LdmlFile* root = nullptr;
    for (const char* at = search->locale; at != nullptr;
         at = CldrParent(search->cldr, at, search->component))
    {
        const LdmlFile* file = File(search->cldr, at, search->component);
        const Leaf* leaf =
            file == nullptr ? nullptr : Accept(search->cldr, MapGet(&file->leaves, text));
        if (leaf != nullptr)
        {
            return leaf;
        }
        root = strcmp(at, "root") == 0 ? file : root;
    }
    Chain next;
    return Fallback(root, chain, &next) ? Locate(search, &next, depth + 1) : nullptr;
}

const Leaf* CldrResolve(Cldr* cldr, const char* locale, const char* chain, Component component)
{
    Chain parsed = {0};
    if (!ParseSteps(chain, &parsed))
    {
        Die("a chain cannot be read: %s", chain);
    }
    Search search = {cldr, locale, component};
    return Locate(&search, &parsed, 0);
}

static void FreeFile(const char* key, void* value, void* context)
{
    (void)key;
    (void)context;
    if (value != &s_missing)
    {
        LdmlFree(value);
    }
}

static void FreeText(const char* key, void* value, void* context)
{
    (void)key;
    (void)context;
    free(value);
}

void CldrClose(Cldr* cldr)
{
    for (int i = 0; i < componentCount; i++)
    {
        MapEach(&cldr->files[i], FreeFile, nullptr);
        MapFree(&cldr->files[i]);
    }
    MapEach(&cldr->parents, FreeText, nullptr);
    MapEach(&cldr->collationParents, FreeText, nullptr);
    MapFree(&cldr->parents);
    MapFree(&cldr->collationParents);
    MapEach(&cldr->scripts, FreeText, nullptr);
    MapFree(&cldr->scripts);
    DtdFree(&cldr->dtd);
    free(cldr->commonDirectory);
}
