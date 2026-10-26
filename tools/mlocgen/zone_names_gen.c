// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone names (zone_names_gen.h). The codes are bcp47/timezone.xml's
// ids sorted as the zone codes unit sorts them; a zone's names are those
// of its CLDR id (the first alias). A locale's value of a field is CLDR's
// resolution of it ("∅∅∅" filed as an empty text), a code's city falling
// back to the one its identifier implies. Locales are taken parent
// first, so a key's chain is filed before it, as the names are.

#include "zone_names_gen.h"

#include "kinds.h"
#include "table_gen.h"
#include "tzdb.h"
#include "xml.h"
#include "zone_name_tables.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxCodes = zoneNameMaxCodes,
    maxMetazones = zoneNameMaxMetazones,
    maxPeriods = 4096,
    maxPreferred = 2048,
    maxId = 16,
    maxKey = 64,
    maxChain = 256,
};

static const char s_emptyMarker[] = "\xE2\x88\x85\xE2\x88\x85\xE2\x88\x85";
static const char s_prefix[] = "//ldml/dates/timeZoneNames/";

// The paths of ZoneNameField's fields under a zone or metazone.
static const char* const s_fieldPaths[zoneFieldCount] = {
    "exemplarCity",  "long/generic",   "long/standard",  "long/daylight",
    "short/generic", "short/standard", "short/daylight",
};

// The elements of ZoneNameFormat's fields; the hour format's two halves
// share one.
static const char* const s_formatPaths[zoneFormatCount] = {
    "gmtFormat",
    "hourFormat",
    "hourFormat",
    "gmtUnknownFormat",
    "regionFormat",
    "regionFormat[@type=\"standard\"]",
    "regionFormat[@type=\"daylight\"]",
    "fallbackFormat",
};

typedef struct ZoneCode
{
    char id[maxId];
    // CLDR's id of the zone, which the locale files and metaZones.xml
    // name it by.
    char name[tzdbMaxName];
    char region[3];
    bool deprecated;
    uint8_t flags;
    int firstPeriod;
    int periodCount;
} ZoneCode;

typedef struct Period
{
    int code;
    int64_t from;
    int64_t to;
    int metazone;
    uint8_t flags;
    int standard;
    int daylight;
} Period;

typedef struct Preferred
{
    int metazone;
    char region[3];
    int code;
} Preferred;

typedef struct Metazone
{
    char* id;
    int golden;
    int firstPreferred;
    int preferredCount;
} Metazone;

// The fields a locale files, per item (as many rows as items); null where
// none.
typedef struct Filed
{
    const char*** rows;
} Filed;

typedef struct Zones
{
    const NamesInput* input;
    ZoneCode codes[maxCodes];
    int codeCount;
    // Every alias of a live id to its code plus one.
    Map aliases;
    Metazone metazones[maxMetazones];
    int metazoneCount;
    Period periods[maxPeriods];
    int periodCount;
    Preferred preferred[maxPreferred];
    int preferredCount;
    // A locale id to its Filed.
    Map filed;
} Zones;

static int Fail(char* errorOut, size_t errorCapacity, const char* what, const char* name)
{
    snprintf(errorOut, errorCapacity, "zone names: %s: %s", what, name);
    return 0;
}

static char* Copy(const char* text, size_t length)
{
    char* copy = GenAllocate(length + 1);
    memcpy(copy, text, length);
    return copy;
}

static int CompareCodes(const void* a, const void* b)
{
    return strcmp(((const ZoneCode*)a)->id, ((const ZoneCode*)b)->id);
}

static int CodeOf(const Zones* z, const char* name)
{
    uintptr_t code = (uintptr_t)MapGet(&z->aliases, name);
    if (code == 0)
    {
        GenDie("a zone no BCP 47 id lists", name);
    }
    return (int)code - 1;
}

// One type of the tz key: its id, CLDR's name for it and its region.
static void AddType(Zones* z, const XmlNode* type)
{
    const char* id = XmlGetAttribute(type, "name");
    const char* list = XmlGetAttribute(type, "alias");
    const char* deprecated = XmlGetAttribute(type, "deprecated");
    const char* region = XmlGetAttribute(type, "region");
    if (id == nullptr || strcmp(id, "unk") == 0)
    {
        return;
    }
    if (z->codeCount == maxCodes || strlen(id) >= maxId)
    {
        GenDie("too many zone ids or one too long", id);
    }
    ZoneCode* c = &z->codes[z->codeCount++];
    snprintf(c->id, sizeof(c->id), "%s", id);
    c->deprecated = deprecated != nullptr && strcmp(deprecated, "true") == 0;
    if (c->deprecated || list == nullptr)
    {
        c->deprecated = true;
        return;
    }
    // The first alias: MapAliases refuses any alias of the list that does not
    // fit tzdbMaxName, this one included, before the names are used.
    snprintf(c->name, sizeof(c->name), "%.*s", (int)strcspn(list, " "), list);
    if (region != nullptr && !GenCopy(c->region, sizeof(c->region), region, strlen(region)))
    {
        GenDie("a zone's region not two letters", id);
    }
    else if (strlen(id) == 5)
    {
        c->region[0] = (char)(id[0] - 32);
        c->region[1] = (char)(id[1] - 32);
    }
    if (c->region[0] != '\0' && (strlen(c->region) != 2 || c->region[0] < 'A' ||
                                 c->region[0] > 'Z' || c->region[1] < 'A' || c->region[1] > 'Z'))
    {
        GenDie("a zone's region not two letters", id);
    }
}

// The aliases of each live id, after sorting; 0 and a message for an
// alias longer than the generator holds.
static int MapAliases(Zones* z, const XmlNode* key, char* errorOut, size_t errorCapacity)
{
    for (int i = 0; i < key->childCount; i++)
    {
        const char* id = XmlGetAttribute(key->children[i], "name");
        const char* list = XmlGetAttribute(key->children[i], "alias");
        ZoneCode probe = {0};
        // AddType stopped on any id of maxId bytes or more.
        snprintf(probe.id, sizeof(probe.id), "%s", id != nullptr ? id : "");
        const ZoneCode* c =
            bsearch(&probe, z->codes, (size_t)z->codeCount, sizeof(ZoneCode), CompareCodes);
        for (const char* at = list; c != nullptr && !c->deprecated && *at != '\0';)
        {
            size_t length = strcspn(at, " ");
            char name[tzdbMaxName];
            if (!GenCopy(name, sizeof(name), at, length))
            {
                return Fail(errorOut, errorCapacity, "an alias longer than the generator holds",
                            at);
            }
            MapPut(&z->aliases, name, (void*)(uintptr_t)(c - z->codes + 1));
            at += length + (at[length] == ' ');
        }
    }
    return 1;
}

static int ReadCodes(Zones* z, char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/bcp47/timezone.xml", z->input->commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    const XmlNode* keyword = root == nullptr ? nullptr : XmlFindChild(root, "keyword");
    const XmlNode* key = nullptr;
    for (int i = 0; keyword != nullptr && i < keyword->childCount && key == nullptr; i++)
    {
        const char* name = XmlGetAttribute(keyword->children[i], "name");
        key = name != nullptr && strcmp(name, "tz") == 0 ? keyword->children[i] : nullptr;
    }
    if (key == nullptr)
    {
        XmlFree(root);
        return root == nullptr ? 0 : Fail(errorOut, errorCapacity, "no tz key", path);
    }
    for (int i = 0; i < key->childCount; i++)
    {
        AddType(z, key->children[i]);
    }
    qsort(z->codes, (size_t)z->codeCount, sizeof(ZoneCode), CompareCodes);
    int ok = MapAliases(z, key, errorOut, errorCapacity);
    XmlFree(root);
    return ok;
}

// Seconds since 1970 of "YYYY-MM-DD HH:MM", or with ":SS", in universal
// time.
static int64_t SecondsOf(const char* text)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int read = sscanf(text, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
    if (read < 5 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 ||
        second > 59)
    {
        GenDie("a metazone period's time the generator cannot read", text);
    }
    // Days from 1970, by the civil calendar's 400-year cycle.
    int y = year - (month <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    int yearOfEra = y - era * 400;
    int dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    int64_t days = (int64_t)era * 146097 + dayOfEra - 719468;
    return days * 86400 + hour * 3600 + minute * 60 + second;
}

// Minutes of "+01", "-07" or "+05:30".
static int OffsetOf(const char* text)
{
    int hours = 0;
    int minutes = 0;
    if (text == nullptr || (text[0] != '+' && text[0] != '-') ||
        sscanf(text + 1, "%d:%d", &hours, &minutes) < 1 || hours > 18 || minutes > 59)
    {
        GenDie("a metazone offset the generator cannot read", text != nullptr ? text : "?");
    }
    return (text[0] == '-' ? -1 : 1) * (hours * 60 + minutes);
}

static int MetazoneOf(Zones* z, const char* id)
{
    for (int i = 0; i < z->metazoneCount; i++)
    {
        if (strcmp(z->metazones[i].id, id) == 0)
        {
            return i;
        }
    }
    if (z->metazoneCount == maxMetazones)
    {
        GenDie("too many metazones", id);
    }
    z->metazones[z->metazoneCount].id = Copy(id, strlen(id));
    z->metazones[z->metazoneCount].golden = -1;
    return z->metazoneCount++;
}

static void AddPeriod(Zones* z, int code, const XmlNode* uses)
{
    const char* metazone = XmlGetAttribute(uses, "mzone");
    const char* from = XmlGetAttribute(uses, "from");
    const char* to = XmlGetAttribute(uses, "to");
    const char* standard = XmlGetAttribute(uses, "stdOffset");
    const char* daylight = XmlGetAttribute(uses, "dstOffset");
    if (metazone == nullptr || z->periodCount == maxPeriods)
    {
        GenDie("a metazone period without a metazone, or too many", z->codes[code].name);
    }
    Period* p = &z->periods[z->periodCount++];
    p->code = code;
    p->metazone = MetazoneOf(z, metazone);
    p->from = from != nullptr ? SecondsOf(from) : INT64_MIN;
    p->to = to != nullptr ? SecondsOf(to) : INT64_MAX;
    if (standard != nullptr || daylight != nullptr)
    {
        p->flags = zoneMetaOffsets;
        p->standard = OffsetOf(standard);
        p->daylight = OffsetOf(daylight);
    }
    if (p->from >= p->to)
    {
        GenDie("a metazone period that ends before it starts", z->codes[code].name);
    }
}

static int ComparePeriods(const void* a, const void* b)
{
    const Period* x = a;
    const Period* y = b;
    if (x->code != y->code)
    {
        return x->code < y->code ? -1 : 1;
    }
    return x->from < y->from ? -1 : x->from > y->from;
}

static int ComparePreferred(const void* a, const void* b)
{
    const Preferred* x = a;
    const Preferred* y = b;
    return x->metazone != y->metazone ? x->metazone - y->metazone : strcmp(x->region, y->region);
}

static void AddMapping(Zones* z, const XmlNode* map)
{
    const char* metazone = XmlGetAttribute(map, "other");
    const char* region = XmlGetAttribute(map, "territory");
    const char* zone = XmlGetAttribute(map, "type");
    if (metazone == nullptr || region == nullptr || zone == nullptr)
    {
        GenDie("a metazone mapping without its attributes", metazone != nullptr ? metazone : "?");
    }
    int m = MetazoneOf(z, metazone);
    int code = CodeOf(z, zone);
    if (strcmp(region, "001") == 0)
    {
        if (z->metazones[m].golden >= 0)
        {
            GenDie("a metazone with two golden zones", metazone);
        }
        z->metazones[m].golden = code;
        return;
    }
    if (z->preferredCount == maxPreferred || strlen(region) != 2)
    {
        GenDie("too many preferred zones, or a region not two letters", region);
    }
    Preferred* p = &z->preferred[z->preferredCount++];
    p->metazone = m;
    p->code = code;
    snprintf(p->region, sizeof(p->region), "%s", region);
}

// The metazones in id order, their periods and preferred zones renumbered.
static void SortMetazones(Zones* z)
{
    int order[maxMetazones];
    int place[maxMetazones];
    for (int i = 0; i < z->metazoneCount; i++)
    {
        order[i] = i;
    }
    for (int i = 1; i < z->metazoneCount; i++)
    {
        for (int k = i;
             k > 0 && strcmp(z->metazones[order[k - 1]].id, z->metazones[order[k]].id) > 0; k--)
        {
            int swap = order[k];
            order[k] = order[k - 1];
            order[k - 1] = swap;
        }
    }
    Metazone sorted[maxMetazones];
    for (int i = 0; i < z->metazoneCount; i++)
    {
        sorted[i] = z->metazones[order[i]];
        place[order[i]] = i;
    }
    memcpy(z->metazones, sorted, sizeof(Metazone) * (size_t)z->metazoneCount);
    for (int i = 0; i < z->periodCount; i++)
    {
        z->periods[i].metazone = place[z->periods[i].metazone];
    }
    for (int i = 0; i < z->preferredCount; i++)
    {
        z->preferred[i].metazone = place[z->preferred[i].metazone];
    }
}

// Each code's periods in order, each metazone's preferred zones.
static void ArrangePeriods(Zones* z)
{
    SortMetazones(z);
    qsort(z->periods, (size_t)z->periodCount, sizeof(Period), ComparePeriods);
    for (int i = 0; i < z->periodCount; i++)
    {
        ZoneCode* c = &z->codes[z->periods[i].code];
        if (c->periodCount == 0)
        {
            c->firstPeriod = i;
        }
        else if (z->periods[i - 1].to > z->periods[i].from)
        {
            GenDie("a zone's metazone periods overlap", c->name);
        }
        if (++c->periodCount > 0xFF)
        {
            GenDie("a zone with too many metazone periods", c->name);
        }
    }
    qsort(z->preferred, (size_t)z->preferredCount, sizeof(Preferred), ComparePreferred);
    for (int i = 0; i < z->preferredCount; i++)
    {
        Metazone* m = &z->metazones[z->preferred[i].metazone];
        if (i > 0 && ComparePreferred(&z->preferred[i - 1], &z->preferred[i]) == 0)
        {
            GenDie("a metazone with two preferred zones in a region", m->id);
        }
        m->firstPreferred = m->preferredCount++ == 0 ? i : m->firstPreferred;
    }
    for (int i = 0; i < z->metazoneCount; i++)
    {
        if (z->metazones[i].golden < 0)
        {
            GenDie("a metazone without a golden zone", z->metazones[i].id);
        }
    }
}

// The flag of a code its region names: its region's only live code, or
// the region's primary zone.
static void FlagCountries(Zones* z, const XmlNode* primaries)
{
    for (int i = 0; i < z->codeCount; i++)
    {
        ZoneCode* c = &z->codes[i];
        int count = 0;
        for (int k = 0; c->region[0] != '\0' && k < z->codeCount; k++)
        {
            count += strcmp(z->codes[k].region, c->region) == 0;
        }
        c->flags = count == 1 ? zoneMetaCountry : 0;
    }
    for (int i = 0; primaries != nullptr && i < primaries->childCount; i++)
    {
        const XmlNode* p = primaries->children[i];
        if (strcmp(p->name, "primaryZone") == 0)
        {
            z->codes[CodeOf(z, p->text)].flags |= zoneMetaCountry;
        }
    }
}

static int ReadMetazones(Zones* z, char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/supplemental/metaZones.xml", z->input->commonDirectory);
    XmlNode* root = XmlParseFile(path, errorOut, errorCapacity);
    const XmlNode* meta = root == nullptr ? nullptr : XmlFindChild(root, "metaZones");
    const XmlNode* info = meta == nullptr ? nullptr : XmlFindChild(meta, "metazoneInfo");
    const XmlNode* maps = meta == nullptr ? nullptr : XmlFindChild(meta, "mapTimezones");
    if (info == nullptr || maps == nullptr)
    {
        XmlFree(root);
        return root == nullptr ? 0 : Fail(errorOut, errorCapacity, "no metazones", path);
    }
    for (int i = 0; i < info->childCount; i++)
    {
        const XmlNode* zone = info->children[i];
        int code = CodeOf(z, XmlGetAttribute(zone, "type"));
        for (int k = 0; k < zone->childCount; k++)
        {
            AddPeriod(z, code, zone->children[k]);
        }
    }
    for (int i = 0; i < maps->childCount; i++)
    {
        AddMapping(z, maps->children[i]);
    }
    FlagCountries(z, XmlFindChild(root, "primaryZones"));
    XmlFree(root);
    ArrangePeriods(z);
    return 1;
}

static void AddMetaUnit(const Zones* z, DataFile* file)
{
    size_t size = zoneMetaHeaderSize + zoneMetaCodeSize * (size_t)z->codeCount +
                  zoneMetaPeriodSize * (size_t)z->periodCount +
                  zoneMetaZoneSize * (size_t)z->metazoneCount +
                  zoneMetaPreferredSize * (size_t)z->preferredCount;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, (uint64_t)z->codeCount, 4);
    GenPut(unit + 4, (uint64_t)z->periodCount, 4);
    GenPut(unit + 8, (uint64_t)z->metazoneCount, 4);
    GenPut(unit + 12, (uint64_t)z->preferredCount, 4);
    uint8_t* at = unit + zoneMetaHeaderSize;
    for (int i = 0; i < z->codeCount; i++, at += zoneMetaCodeSize)
    {
        const ZoneCode* c = &z->codes[i];
        memcpy(at, c->region, 2);
        GenPut(at + 2, c->flags, 1);
        GenPut(at + 3, (uint64_t)c->periodCount, 1);
        GenPut(at + 4, (uint64_t)(c->periodCount > 0 ? c->firstPeriod : 0), 2);
    }
    for (int i = 0; i < z->periodCount; i++, at += zoneMetaPeriodSize)
    {
        const Period* p = &z->periods[i];
        GenPut(at, (uint64_t)p->from, 8);
        GenPut(at + 8, (uint64_t)p->to, 8);
        GenPut(at + 16, (uint64_t)p->metazone, 2);
        GenPut(at + 18, p->flags, 1);
        GenPut(at + 20, (uint16_t)p->standard, 2);
        GenPut(at + 22, (uint16_t)p->daylight, 2);
    }
    for (int i = 0; i < z->metazoneCount; i++, at += zoneMetaZoneSize)
    {
        const Metazone* m = &z->metazones[i];
        GenPut(at, (uint64_t)m->golden, 2);
        GenPut(at + 2, (uint64_t)(m->preferredCount > 0 ? m->firstPreferred : 0), 2);
        GenPut(at + 4, (uint64_t)m->preferredCount, 2);
    }
    for (int i = 0; i < z->preferredCount; i++, at += zoneMetaPreferredSize)
    {
        memcpy(at, z->preferred[i].region, 2);
        GenPut(at + 2, (uint64_t)z->preferred[i].code, 2);
    }
    DataFileAdd(file, kindZoneMeta, nullptr, unit, size);
    free(unit);
}

// The items: the formats, Etc/Unknown, the codes, the metazones.
static int ItemCount(const Zones* z)
{
    return zoneNameCodeBase + z->codeCount + z->metazoneCount;
}

static int ItemKey(const Zones* z, int item)
{
    int metazone = item - zoneNameCodeBase - z->codeCount;
    return metazone >= 0 ? zoneNameMetazoneBase + metazone : item;
}

static int FieldCount(const Zones* z, int item)
{
    if (item == zoneNameFormats)
    {
        return zoneFormatCount;
    }
    bool deprecated = item >= zoneNameCodeBase && item - zoneNameCodeBase < z->codeCount &&
                      z->codes[item - zoneNameCodeBase].deprecated;
    return deprecated ? 0 : zoneFieldCount;
}

// The chain of an item's field, or false for none (a metazone's city).
static bool ChainOf(const Zones* z, int item, int field, char* out, size_t capacity)
{
    if (item == zoneNameFormats)
    {
        snprintf(out, capacity, "%s%s", s_prefix, s_formatPaths[field]);
        return true;
    }
    int code = item - zoneNameCodeBase;
    if (item == zoneNameUnknown || code < z->codeCount)
    {
        snprintf(out, capacity, "%szone[@type=\"%s\"]/%s", s_prefix,
                 item == zoneNameUnknown ? "Etc/Unknown" : z->codes[code].name,
                 s_fieldPaths[field]);
        return true;
    }
    snprintf(out, capacity, "%smetazone[@type=\"%s\"]/%s", s_prefix,
             z->metazones[code - z->codeCount].id, s_fieldPaths[field]);
    return field != zoneFieldCity;
}

// The city an identifier implies: its last part, '_' as spaces.
static const char* ImpliedCity(const ZoneCode* c)
{
    const char* last = strrchr(c->name, '/');
    if (c->region[0] == '\0' || last == nullptr || last[1] == '\0')
    {
        return nullptr;
    }
    char* city = Copy(last + 1, strlen(last + 1));
    for (char* at = city; *at != '\0'; at++)
    {
        *at = *at == '_' ? ' ' : *at;
    }
    return city;
}

// CLDR's value of a field at a locale ("∅∅∅" as empty), or null.
static const char* Target(Zones* z, const char* locale, int item, int field)
{
    char chain[maxChain];
    if (!ChainOf(z, item, field, chain, sizeof(chain)))
    {
        return nullptr;
    }
    const Leaf* leaf = CldrResolve(z->input->cldr, locale, chain, componentMain);
    const char* value = leaf != nullptr ? leaf->text : nullptr;
    if (value != nullptr && strcmp(value, s_emptyMarker) == 0)
    {
        return "";
    }
    int code = item - zoneNameCodeBase;
    if (value == nullptr && field == zoneFieldCity && code >= 0 && code < z->codeCount)
    {
        return ImpliedCity(&z->codes[code]);
    }
    if (value != nullptr && item == zoneNameFormats &&
        (field == zoneFormatHourPositive || field == zoneFormatHourNegative))
    {
        const char* semicolon = strchr(value, ';');
        if (semicolon == nullptr)
        {
            GenDie("an hour format without its negative half", value);
        }
        return field == zoneFormatHourPositive ? Copy(value, (size_t)(semicolon - value))
                                               : Copy(semicolon + 1, strlen(semicolon + 1));
    }
    return value;
}

// Whether a value is text the unit holds, a format holding its
// placeholders.
static bool ValidValue(int item, int field, const char* value)
{
    for (const unsigned char* at = (const unsigned char*)value; *at != '\0';)
    {
        int length = *at < 0x20 ? 0 : GenCharLength(at);
        if (length == 0)
        {
            return false;
        }
        at += length;
    }
    // A format holds no quotes, so placeholders are all the run time reads
    // in it (ICU's SimpleFormatter would read quotes).
    if (item != zoneNameFormats)
    {
        return true;
    }
    if (strchr(value, '\'') != nullptr)
    {
        return false;
    }
    bool zero = strstr(value, "{0}") != nullptr;
    bool one = strstr(value, "{1}") != nullptr;
    switch (field)
    {
    case zoneFormatHourPositive:
    case zoneFormatHourNegative:
        return strchr(value, 'H') != nullptr && strchr(value, 'm') != nullptr;
    case zoneFormatGmtUnknown:
        return value[0] != '\0';
    case zoneFormatFallback:
        return zero && one;
    default:
        return zero;
    }
}

// The first value of a field filed along a locale's chain.
static const char* FiledValue(Zones* z, const char* locale, int item, int field)
{
    for (const char* at = locale; at != nullptr; at = CldrParent(z->input->cldr, at, componentMain))
    {
        const Filed* filed = MapGet(&z->filed, at);
        const char* const* fields = filed != nullptr ? filed->rows[item] : nullptr;
        if (fields != nullptr && fields[field] != nullptr)
        {
            return fields[field];
        }
    }
    return nullptr;
}

// Files a value at a locale, or checks a locale that files nothing. A
// value CLDR leaves out where the chain gives one is filed as empty.
static void Take(Zones* z, Filed* filed, const char* locale, int item, int field, const char* value)
{
    const char* looked = FiledValue(z, locale, item, field);
    bool none = value == nullptr || value[0] == '\0';
    bool lookedNone = looked == nullptr || looked[0] == '\0';
    if ((none && lookedNone) || (!none && !lookedNone && strcmp(looked, value) == 0))
    {
        return;
    }
    if (filed == nullptr)
    {
        GenDie("a locale's zone names differ from those of its parent's key", locale);
    }
    if (value != nullptr && !ValidValue(item, field, value))
    {
        GenDie("a zone name of a control or malformed byte, or a format without its "
               "placeholders",
               value);
    }
    if (filed->rows[item] == nullptr)
    {
        filed->rows[item] = GenAllocate(sizeof(char*) * zoneNameFieldsPerItem);
    }
    filed->rows[item][field] = value != nullptr ? value : "";
}

// Adds a locale key's unit.
static void AddUnit(const Zones* z, const Filed* filed, const char* key, DataFile* file)
{
    size_t count = 0;
    size_t text = 0;
    for (int item = 0; item < ItemCount(z); item++)
    {
        for (int f = 0; filed->rows[item] != nullptr && f < zoneNameFieldsPerItem; f++)
        {
            const char* value = filed->rows[item][f];
            count += value != nullptr;
            text += value != nullptr ? strlen(value) : 0;
        }
    }
    if (count == 0)
    {
        return;
    }
    size_t size = zoneNameHeaderSize + 2 * count + 4 * (count + 1) + text;
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, count, 4);
    GenPut(unit + 4, text, 4);
    uint8_t* keys = unit + zoneNameHeaderSize;
    uint8_t* offsets = keys + 2 * count;
    uint8_t* textAt = offsets + 4 * (count + 1);
    size_t entry = 0;
    size_t at = 0;
    for (int item = 0; item < ItemCount(z); item++)
    {
        for (int f = 0; filed->rows[item] != nullptr && f < zoneNameFieldsPerItem; f++)
        {
            const char* value = filed->rows[item][f];
            if (value == nullptr)
            {
                continue;
            }
            size_t length = strlen(value);
            GenPut(keys + 2 * entry, (uint64_t)(ItemKey(z, item) * zoneNameFieldsPerItem + f), 2);
            GenPut(offsets + 4 * entry, at, 4);
            memcpy(textAt + at, value, length);
            at += length;
            entry++;
        }
    }
    GenPut(offsets + 4 * count, at, 4);
    DataFileAdd(file, kindZoneNames, key, unit, size);
    free(unit);
}

// Takes a locale after its parent.
static void TakeChain(Zones* z, const char* locale, DataFile* file)
{
    if (MapGet(&z->filed, locale) != nullptr)
    {
        return;
    }
    const NamesInput* input = z->input;
    const char* parent = CldrParent(input->cldr, locale, componentMain);
    if (parent != nullptr)
    {
        TakeChain(z, parent, file);
    }
    char key[maxKey];
    char parentKey[maxKey] = "";
    KeyOf(input->keys, locale, key, sizeof(key));
    if (parent != nullptr)
    {
        KeyOf(input->keys, parent, parentKey, sizeof(parentKey));
    }
    Filed* filed = GenAllocate(sizeof(Filed));
    filed->rows = GenAllocate(sizeof(*filed->rows) * (size_t)ItemCount(z));
    bool own = strcmp(key, parentKey) != 0;
    MapPut(&z->filed, locale, filed);
    for (int item = 0; item < ItemCount(z); item++)
    {
        for (int field = 0; field < FieldCount(z, item); field++)
        {
            Take(z, own ? filed : nullptr, locale, item, field, Target(z, locale, item, field));
        }
    }
    if (own)
    {
        AddUnit(z, filed, key, file);
    }
}

int GenerateZoneNames(const NamesInput* input, DataFile* file, char* errorOut, size_t errorCapacity)
{
    static Zones s_zones;
    s_zones = (Zones){.input = input};
    Zones* z = &s_zones;
    if (!ReadCodes(z, errorOut, errorCapacity) || !ReadMetazones(z, errorOut, errorCapacity))
    {
        return 0;
    }
    AddMetaUnit(z, file);
    for (int i = 0; i < input->count; i++)
    {
        TakeChain(z, input->ids[i], file);
    }
    return 1;
}
