// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone identifiers (zone_ids.h).

#include "zone_ids.h"

#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    // The longest chain of Links the source may have.
    maxLinkDepth = 8,
};

// A CLDR group: the names of one place, and the one CLDR prefers (its
// iana attribute, else its first alias).
typedef struct Groups
{
    XmlNode* root;
    const XmlNode* key;
} Groups;

static int FoldedCompare(const char* a, const char* b)
{
    for (;; a++, b++)
    {
        int x = *a >= 'A' && *a <= 'Z' ? *a + 32 : (unsigned char)*a;
        int y = *b >= 'A' && *b <= 'Z' ? *b + 32 : (unsigned char)*b;
        if (x != y || x == 0)
        {
            return x - y;
        }
    }
}

static int CompareIdentifiers(const void* a, const void* b)
{
    return FoldedCompare(((const ZoneIdentifier*)a)->name, ((const ZoneIdentifier*)b)->name);
}

static int Fail(char* errorOut, size_t errorCapacity, const char* what, const char* name)
{
    snprintf(errorOut, errorCapacity, "zone identifiers: %s: %s", what, name);
    return -1;
}

// Whether a space-separated list holds a name.
static bool ListHas(const char* list, const char* name)
{
    size_t length = strlen(name);
    for (const char* p = list; p != nullptr && *p != '\0';)
    {
        size_t n = strcspn(p, " ");
        if (n == length && memcmp(p, name, n) == 0)
        {
            return true;
        }
        p += n + (p[n] == ' ');
    }
    return false;
}

// The CLDR type whose aliases hold a name, or null.
static const XmlNode* GroupOf(const Groups* g, const char* name)
{
    for (int i = 0; g->key != nullptr && i < g->key->childCount; i++)
    {
        const XmlNode* type = g->key->children[i];
        if (ListHas(XmlGetAttribute(type, "alias"), name))
        {
            return type;
        }
    }
    return nullptr;
}

// A group's preferred name into out, the UTC names read as UTC.
static void GroupPrimary(const XmlNode* type, char* out)
{
    const char* iana = XmlGetAttribute(type, "iana");
    const char* alias = XmlGetAttribute(type, "alias");
    const char* from = iana != nullptr ? iana : alias;
    size_t length = strcspn(from, " ");
    length = length < tzdbMaxName ? length : tzdbMaxName - 1;
    memcpy(out, from, length);
    out[length] = '\0';
    if (strcmp(out, "Etc/UTC") == 0 || strcmp(out, "Etc/GMT") == 0 || strcmp(out, "GMT") == 0)
    {
        strcpy(out, "UTC");
    }
}

static const char* TabCountry(const Tzdb* t, const char* name)
{
    for (int i = 0; i < t->countryCount; i++)
    {
        if (strcmp(t->countries[i].zone, name) == 0)
        {
            return t->countries[i].code;
        }
    }
    return nullptr;
}

// A name's country: its zone.tab row's, else that of a name of its CLDR
// group in zone.tab.
static const char* Country(const Tzdb* t, const Groups* g, const char* name)
{
    const char* code = TabCountry(t, name);
    const XmlNode* type = code == nullptr ? GroupOf(g, name) : nullptr;
    const char* alias = type == nullptr ? nullptr : XmlGetAttribute(type, "alias");
    for (const char* p = alias; code == nullptr && p != nullptr && *p != '\0';)
    {
        char member[tzdbMaxName] = "";
        size_t n = strcspn(p, " ");
        if (n < tzdbMaxName)
        {
            memcpy(member, p, n);
            code = TabCountry(t, member);
        }
        p += n + (p[n] == ' ');
    }
    return code;
}

static int ZoneIndex(const Tzdb* t, const char* name)
{
    for (int low = 0, high = t->zoneCount; low < high;)
    {
        int middle = low + (high - low) / 2;
        int order = strcmp(t->zones[middle].name, name);
        if (order == 0)
        {
            return middle;
        }
        *(order < 0 ? &low : &high) = order < 0 ? middle + 1 : middle;
    }
    return -1;
}

static const char* LinkTarget(const Tzdb* t, const char* name)
{
    for (int low = 0, high = t->linkCount; low < high;)
    {
        int middle = low + (high - low) / 2;
        int order = strcmp(t->links[middle].name, name);
        if (order == 0)
        {
            return t->links[middle].target;
        }
        *(order < 0 ? &low : &high) = order < 0 ? middle + 1 : middle;
    }
    return nullptr;
}

// The Zone a name reaches through its Links, or -1.
static int Resolve(const Tzdb* t, const char* name)
{
    const char* at = name;
    for (int depth = 0; at != nullptr && depth <= maxLinkDepth; depth++)
    {
        int zone = ZoneIndex(t, at);
        if (zone >= 0)
        {
            return zone;
        }
        at = LinkTarget(t, at);
    }
    return -1;
}

static const char* BackzoneTarget(const Tzdb* t, const char* name)
{
    for (int i = 0; i < t->backzoneLinkCount; i++)
    {
        if (strcmp(t->backzoneLinks[i].name, name) == 0)
        {
            return t->backzoneLinks[i].target;
        }
    }
    return nullptr;
}

// ECMA-402's primary identifier of a name, into out (tzdbMaxName bytes).
// Returns false with a message when a country or a backzone Link is
// missing.
static bool PrimaryName(const Tzdb* t, const Groups* g, const char* name, char* out, char* errorOut,
                        size_t errorCapacity)
{
    const char* primary = name;
    int zone = Resolve(t, name);
    if (LinkTarget(t, name) != nullptr && TabCountry(t, name) == nullptr)
    {
        const char* zoneName = t->zones[zone].name;
        const char* nameCountry = Country(t, g, name);
        const char* zoneCountry = Country(t, g, zoneName);
        int lines = 0;
        const char* only = nullptr;
        for (int i = 0; nameCountry != nullptr && i < t->countryCount; i++)
        {
            if (strcmp(t->countries[i].code, nameCountry) == 0)
            {
                lines++;
                only = t->countries[i].zone;
            }
        }
        if (strncmp(zoneName, "Etc/", 4) == 0 ||
            (nameCountry != nullptr && zoneCountry != nullptr &&
             strcmp(nameCountry, zoneCountry) == 0))
        {
            primary = zoneName;
        }
        else if (nameCountry == nullptr)
        {
            Fail(errorOut, errorCapacity, "a Link of no country", name);
            return false;
        }
        else
        {
            primary = lines == 1 ? only : BackzoneTarget(t, name);
        }
        if (primary == nullptr)
        {
            Fail(errorOut, errorCapacity, "no backzone Link for", name);
            return false;
        }
    }
    bool utc = strcmp(primary, "Etc/UTC") == 0 || strcmp(primary, "Etc/GMT") == 0 ||
               strcmp(primary, "GMT") == 0;
    snprintf(out, tzdbMaxName, "%s", utc ? "UTC" : primary);
    return true;
}

static int Find(const ZoneIdentifier* ids, int count, const char* name)
{
    for (int low = 0, high = count; low < high;)
    {
        int middle = low + (high - low) / 2;
        int order = FoldedCompare(ids[middle].name, name);
        if (order == 0)
        {
            return strcmp(ids[middle].name, name) == 0 ? middle : -1;
        }
        *(order < 0 ? &low : &high) = order < 0 ? middle + 1 : middle;
    }
    return -1;
}

// Every name with its zone, sorted with case folded; -1 when a Link
// reaches no Zone or two names differ in case alone.
static int Collect(const Tzdb* t, ZoneIdentifier* ids, char* errorOut, size_t errorCapacity)
{
    int count = 0;
    for (int i = 0; i < t->zoneCount; i++)
    {
        ids[count] = (ZoneIdentifier){.zone = i};
        snprintf(ids[count++].name, tzdbMaxName, "%s", t->zones[i].name);
    }
    for (int i = 0; i < t->linkCount; i++)
    {
        ids[count] = (ZoneIdentifier){.zone = Resolve(t, t->links[i].name)};
        snprintf(ids[count].name, tzdbMaxName, "%s", t->links[i].name);
        if (ids[count++].zone < 0)
        {
            return Fail(errorOut, errorCapacity, "a Link that reaches no Zone", t->links[i].name);
        }
    }
    qsort(ids, (size_t)count, sizeof(ZoneIdentifier), CompareIdentifiers);
    for (int i = 1; i < count; i++)
    {
        if (FoldedCompare(ids[i - 1].name, ids[i].name) == 0)
        {
            return Fail(errorOut, errorCapacity, "a name twice when case is ignored", ids[i].name);
        }
    }
    return count;
}

// Each name's primary, checked against CLDR's.
static int Primaries(const Tzdb* t, const Groups* g, ZoneIdentifier* ids, int count, char* errorOut,
                     size_t errorCapacity)
{
    for (int i = 0; i < count; i++)
    {
        char primary[tzdbMaxName];
        char cldr[tzdbMaxName];
        if (!PrimaryName(t, g, ids[i].name, primary, errorOut, errorCapacity))
        {
            return -1;
        }
        const XmlNode* type = GroupOf(g, ids[i].name);
        if (type != nullptr)
        {
            GroupPrimary(type, cldr);
            if (strcmp(cldr, primary) != 0)
            {
                return Fail(errorOut, errorCapacity, "a primary other than CLDR's for",
                            ids[i].name);
            }
        }
        ids[i].primary = Find(ids, count, primary);
        if (ids[i].primary < 0)
        {
            return Fail(errorOut, errorCapacity, "a primary not among the names for", ids[i].name);
        }
    }
    return count;
}

int ZoneIdentifiers(const Tzdb* tzdb, const char* commonDirectory, ZoneIdentifier** out,
                    char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/bcp47/timezone.xml", commonDirectory);
    Groups g = {.root = XmlParseFile(path, errorOut, errorCapacity)};
    const XmlNode* keyword = g.root == nullptr ? nullptr : XmlFindChild(g.root, "keyword");
    for (int i = 0; keyword != nullptr && i < keyword->childCount && g.key == nullptr; i++)
    {
        const char* name = XmlGetAttribute(keyword->children[i], "name");
        g.key = name != nullptr && strcmp(name, "tz") == 0 ? keyword->children[i] : nullptr;
    }
    ZoneIdentifier* ids = calloc((size_t)(tzdb->zoneCount + tzdb->linkCount), sizeof(*ids));
    int count = -1;
    if (g.key == nullptr || ids == nullptr)
    {
        snprintf(errorOut, errorCapacity, "%s: no tz key", path);
    }
    else
    {
        count = Collect(tzdb, ids, errorOut, errorCapacity);
        count = count < 0 ? -1 : Primaries(tzdb, &g, ids, count, errorOut, errorCapacity);
    }
    XmlFree(g.root);
    if (count < 0)
    {
        free(ids);
        return -1;
    }
    *out = ids;
    return count;
}
