// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone tables (zone_gen.h), in the layout of src/zone_tables.h.

#include "zone_gen.h"

#include "data_format.h"
#include "kinds.h"
#include "tzdb.h"
#include "zone_compile.h"
#include "zone_ids.h"
#include "zone_tables.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxOffsets = 4096,
};

// The offset (standard plus saving) at each change, without the changes
// that keep it, for comparing two compiles of one zone.
static bool SameOffsets(const CompiledZone* a, const CompiledZone* b)
{
    int32_t offsetA = a->initial.standard + a->initial.save;
    int32_t offsetB = b->initial.standard + b->initial.save;
    int i = 0;
    int j = 0;
    for (;;)
    {
        while (i < a->transitionCount &&
               a->transitions[i].offset.standard + a->transitions[i].offset.save == offsetA)
        {
            i++;
        }
        while (j < b->transitionCount &&
               b->transitions[j].offset.standard + b->transitions[j].offset.save == offsetB)
        {
            j++;
        }
        if (offsetA != offsetB || (i < a->transitionCount) != (j < b->transitionCount))
        {
            return false;
        }
        if (i == a->transitionCount)
        {
            break;
        }
        if (a->transitions[i].at != b->transitions[j].at)
        {
            return false;
        }
        offsetA = a->transitions[i].offset.standard + a->transitions[i].offset.save;
        offsetB = b->transitions[j].offset.standard + b->transitions[j].offset.save;
    }
    if (a->closing != b->closing || (a->closing && a->firstYear != b->firstYear))
    {
        return false;
    }
    for (int k = 0; a->closing && k < 2; k++)
    {
        const CompiledChange* x = &a->changes[k];
        const CompiledChange* y = &b->changes[k];
        if (x->month != y->month || x->dayOfMonth != y->dayOfMonth || x->day != y->day ||
            x->weekday != y->weekday || x->time != y->time ||
            x->offset.standard + x->offset.save != y->offset.standard + y->offset.save)
        {
            return false;
        }
    }
    return true;
}

// Compiles every zone of the rearguard form, each checked to give the
// main form's offsets. Returns the count, or -1 with a message.
static int CompileAll(const char* tzdbDirectory, Tzdb* tzdb, CompiledZone** zonesOut,
                      char* errorOut, size_t errorCapacity)
{
    Tzdb main = {0};
    if (!TzdbRead(tzdbDirectory, true, tzdb, errorOut, errorCapacity))
    {
        return -1;
    }
    if (!TzdbRead(tzdbDirectory, false, &main, errorOut, errorCapacity))
    {
        TzdbFree(tzdb);
        return -1;
    }
    CompiledZone* zones = calloc((size_t)tzdb->zoneCount, sizeof(CompiledZone));
    int ok = zones != nullptr && main.zoneCount == tzdb->zoneCount;
    for (int z = 0; ok && z < tzdb->zoneCount; z++)
    {
        CompiledZone check;
        ok = strcmp(main.zones[z].name, tzdb->zones[z].name) == 0 &&
             ZoneCompile(tzdb, z, &zones[z], errorOut, errorCapacity) &&
             ZoneCompile(&main, z, &check, errorOut, errorCapacity);
        if (ok && !SameOffsets(&zones[z], &check))
        {
            ok = 0;
            snprintf(errorOut, errorCapacity, "zone %s: the rearguard form's offsets differ",
                     tzdb->zones[z].name);
        }
        ZoneFree(&check);
    }
    TzdbFree(&main);
    if (!ok)
    {
        for (int z = 0; zones != nullptr && z < tzdb->zoneCount; z++)
        {
            ZoneFree(&zones[z]);
        }
        free(zones);
        TzdbFree(tzdb);
        return -1;
    }
    *zonesOut = zones;
    return tzdb->zoneCount;
}

static void FreeAll(Tzdb* tzdb, CompiledZone* zones, int count)
{
    for (int z = 0; z < count; z++)
    {
        ZoneFree(&zones[z]);
    }
    free(zones);
    TzdbFree(tzdb);
}

static void PrintZone(const char* name, const CompiledZone* zone, int32_t untilYear)
{
    printf("zone %s %d %d\n", name, (int)zone->initial.standard, (int)zone->initial.save);
    for (int i = 0; i < zone->transitionCount; i++)
    {
        const CompiledTransition* t = &zone->transitions[i];
        printf("at %lld %d %d\n", (long long)t->at, (int)t->offset.standard, (int)t->offset.save);
    }
    for (int32_t year = zone->firstYear; zone->closing && year < untilYear; year++)
    {
        int64_t times[2];
        ZoneClosingTimes(zone, year, times);
        for (int k = 0; k < 2; k++)
        {
            printf("at %lld %d %d\n", (long long)times[k], (int)zone->changes[k].offset.standard,
                   (int)zone->changes[k].offset.save);
        }
    }
}

int PrintZones(const char* tzdbDirectory, int32_t untilYear, char* errorOut, size_t errorCapacity)
{
    Tzdb tzdb;
    CompiledZone* zones = nullptr;
    int count = CompileAll(tzdbDirectory, &tzdb, &zones, errorOut, errorCapacity);
    for (int z = 0; z < count; z++)
    {
        PrintZone(tzdb.zones[z].name, &zones[z], untilYear);
    }
    if (count >= 0)
    {
        FreeAll(&tzdb, zones, count);
    }
    return count >= 0;
}

// The distinct offsets, in the order first met.
typedef struct Offsets
{
    CompiledOffset items[maxOffsets];
    int count;
} Offsets;

static int OffsetIndex(Offsets* o, CompiledOffset offset)
{
    for (int i = 0; i < o->count; i++)
    {
        if (o->items[i].standard == offset.standard && o->items[i].save == offset.save)
        {
            return i;
        }
    }
    if (o->count == maxOffsets)
    {
        fputs("mlocgen: too many zone offsets\n", stderr);
        exit(1);
    }
    o->items[o->count] = offset;
    return o->count++;
}

// A zone's own offsets: the place of each in its list, which holds the
// indexes of the shared ones.
typedef struct Local
{
    int shared[256];
    int count;
} Local;

static int LocalIndex(Local* local, Offsets* o, CompiledOffset offset)
{
    int shared = OffsetIndex(o, offset);
    for (int i = 0; i < local->count; i++)
    {
        if (local->shared[i] == shared)
        {
            return i;
        }
    }
    local->shared[local->count] = shared;
    return local->count++;
}

static bool Narrow(int64_t at)
{
    return at >= INT32_MIN && at <= INT32_MAX;
}

// A growing run of little-endian bytes.
typedef struct Bytes
{
    uint8_t* data;
    size_t size;
    size_t capacity;
} Bytes;

static void PutN(Bytes* b, uint64_t value, int count)
{
    if (b->size + (size_t)count > b->capacity)
    {
        b->capacity = b->capacity == 0 ? 4096 : b->capacity * 2;
        uint8_t* grown = realloc(b->data, b->capacity);
        if (grown == nullptr)
        {
            fputs("mlocgen: cannot build the zones unit\n", stderr);
            exit(1);
        }
        b->data = grown;
    }
    for (int i = 0; i < count; i++)
    {
        b->data[b->size++] = (uint8_t)(value >> (8 * i));
    }
}

// The unit's sections, in the order of zone_tables.h, and their counts.
typedef struct Tables
{
    Offsets offsets;
    Bytes sections[zoneSectionCount];
    uint32_t counts[zoneSectionCount];
    // The tzdb release, NUL-padded, in the header's last eight bytes.
    char release[8];
} Tables;

enum
{
    sectionZones,
    sectionIdentifiers,
    sectionOffsets,
    sectionList,
    sectionNarrow,
    sectionWide,
    sectionTypes,
    sectionClosings,
    sectionText,
    sectionPrimaries,
};

static void Put(Tables* t, int section, uint64_t value, int count)
{
    PutN(&t->sections[section], value, count);
}

static int Limit(char* errorOut, size_t errorCapacity, const char* zone, const char* what)
{
    snprintf(errorOut, errorCapacity, "zone %s: %s exceeds the unit's field", zone, what);
    return 0;
}

static void AddClosing(Tables* t, const CompiledZone* zone)
{
    Put(t, sectionClosings, (uint32_t)zone->firstYear, 4);
    for (int k = 0; k < 2; k++)
    {
        const CompiledChange* c = &zone->changes[k];
        Put(t, sectionClosings, c->month + 1U, 1);
        Put(t, sectionClosings, c->dayOfMonth, 1);
        Put(t, sectionClosings, c->weekday, 1);
        Put(t, sectionClosings, (uint64_t)c->day, 1);
        Put(t, sectionClosings, (uint32_t)c->time, 4);
        Put(t, sectionClosings, (uint64_t)OffsetIndex(&t->offsets, c->offset), 2);
        Put(t, sectionClosings, 0, 2);
    }
    t->counts[sectionClosings]++;
}

// Adds a zone's transitions, offsets, closing rule and record; name is
// its own identifier.
static int AddZone(Tables* t, const char* name, int identifier, const CompiledZone* zone,
                   char* errorOut, size_t errorCapacity)
{
    int count = zone->transitionCount;
    int early = 0;
    while (early < count && !Narrow(zone->transitions[early].at))
    {
        early++;
    }
    int late = 0;
    while (late < count - early && !Narrow(zone->transitions[count - 1 - late].at))
    {
        late++;
    }
    Local local = {0};
    int initial = LocalIndex(&local, &t->offsets, zone->initial);
    uint32_t typeFirst = t->counts[sectionTypes];
    uint32_t narrowFirst = t->counts[sectionNarrow];
    uint32_t wideFirst = t->counts[sectionWide];
    for (int i = 0; i < count; i++)
    {
        const CompiledTransition* tr = &zone->transitions[i];
        bool wide = i < early || i >= count - late;
        if (!wide && !Narrow(tr->at))
        {
            return Limit(errorOut, errorCapacity, name, "a time between narrow ones");
        }
        Put(t, wide ? sectionWide : sectionNarrow, (uint64_t)tr->at, wide ? 8 : 4);
        t->counts[wide ? sectionWide : sectionNarrow]++;
        Put(t, sectionTypes, (uint64_t)LocalIndex(&local, &t->offsets, tr->offset), 1);
        t->counts[sectionTypes]++;
    }
    if (count > UINT16_MAX || early > UINT8_MAX || late > UINT8_MAX || local.count > UINT8_MAX ||
        wideFirst > UINT16_MAX || t->counts[sectionList] > UINT16_MAX ||
        t->counts[sectionClosings] >= UINT8_MAX)
    {
        return Limit(errorOut, errorCapacity, name, "a count or an index");
    }
    Put(t, sectionZones, narrowFirst, 4);
    Put(t, sectionZones, typeFirst, 4);
    Put(t, sectionZones, (uint64_t)count, 2);
    Put(t, sectionZones, wideFirst, 2);
    Put(t, sectionZones, t->counts[sectionList], 2);
    Put(t, sectionZones, (uint64_t)identifier, 2);
    Put(t, sectionZones, (uint64_t)early, 1);
    Put(t, sectionZones, (uint64_t)late, 1);
    Put(t, sectionZones, (uint64_t)local.count, 1);
    Put(t, sectionZones, (uint64_t)initial, 1);
    if (zone->closing)
    {
        AddClosing(t, zone);
    }
    Put(t, sectionZones, zone->closing ? t->counts[sectionClosings] : 0, 1);
    Put(t, sectionZones, 0, 3);
    t->counts[sectionZones]++;
    for (int i = 0; i < local.count; i++)
    {
        Put(t, sectionList, (uint64_t)local.shared[i], 2);
        t->counts[sectionList]++;
    }
    return 1;
}

static void AddIdentifiers(Tables* t, const ZoneIdentifier* ids, int count)
{
    for (int i = 0; i < count; i++)
    {
        size_t length = strlen(ids[i].name);
        Put(t, sectionIdentifiers, t->counts[sectionText], 4);
        Put(t, sectionIdentifiers, length, 1);
        Put(t, sectionIdentifiers, 0, 1);
        Put(t, sectionIdentifiers, (uint64_t)ids[i].zone, 2);
        Put(t, sectionIdentifiers, (uint64_t)ids[i].primary, 2);
        Put(t, sectionIdentifiers, 0, 2);
        for (size_t k = 0; k < length; k++)
        {
            Put(t, sectionText, (uint8_t)ids[i].name[k], 1);
        }
        t->counts[sectionText] += (uint32_t)length;
        t->counts[sectionIdentifiers]++;
    }
    // The primaries in byte order; the identifiers are in folded order.
    static int order[UINT16_MAX];
    int primaries = 0;
    for (int i = 0; i < count; i++)
    {
        if (ids[i].primary == i)
        {
            int at = primaries++;
            for (; at > 0 && strcmp(ids[order[at - 1]].name, ids[i].name) > 0; at--)
            {
                order[at] = order[at - 1];
            }
            order[at] = i;
        }
    }
    for (int i = 0; i < primaries; i++)
    {
        Put(t, sectionPrimaries, (uint64_t)order[i], 2);
    }
    t->counts[sectionPrimaries] = (uint32_t)primaries;
}

// The header, then each section from an 8-byte boundary.
static Bytes Assemble(Tables* t)
{
    for (int i = 0; i < t->offsets.count; i++)
    {
        Put(t, sectionOffsets, (uint32_t)t->offsets.items[i].standard, 4);
        Put(t, sectionOffsets, (uint32_t)t->offsets.items[i].save, 4);
    }
    t->counts[sectionOffsets] = (uint32_t)t->offsets.count;
    Bytes out = {0};
    for (int i = 0; i < zoneSectionCount; i++)
    {
        PutN(&out, t->counts[i], 4);
    }
    for (size_t i = 0; i < sizeof(t->release); i++)
    {
        PutN(&out, (uint8_t)t->release[i], 1);
    }
    for (int i = 0; i < zoneSectionCount; i++)
    {
        while (out.size % 8 != 0)
        {
            PutN(&out, 0, 1);
        }
        for (size_t k = 0; k < t->sections[i].size; k++)
        {
            PutN(&out, t->sections[i].data[k], 1);
        }
        free(t->sections[i].data);
    }
    return out;
}

static bool SameZone(const CompiledZone* a, const CompiledZone* b)
{
    bool same = a->initial.standard == b->initial.standard && a->initial.save == b->initial.save &&
                a->transitionCount == b->transitionCount && a->closing == b->closing &&
                (!a->closing || (a->firstYear == b->firstYear &&
                                 memcmp(a->changes, b->changes, sizeof(a->changes)) == 0));
    for (int i = 0; same && i < a->transitionCount; i++)
    {
        same = a->transitions[i].at == b->transitions[i].at &&
               a->transitions[i].offset.standard == b->transitions[i].offset.standard &&
               a->transitions[i].offset.save == b->transitions[i].offset.save;
    }
    return same;
}

// Each name reads its primary's data, so equal identifiers give equal
// offsets; where the two differ as Zones (Etc/GMT's primary is UTC, a
// Link to Etc/UTC), they must compile the same.
static int ReadPrimaryData(ZoneIdentifier* ids, int count, const CompiledZone* zones,
                           char* errorOut, size_t errorCapacity)
{
    for (int i = 0; i < count; i++)
    {
        int zone = ids[ids[i].primary].zone;
        if (zone != ids[i].zone && !SameZone(&zones[zone], &zones[ids[i].zone]))
        {
            snprintf(errorOut, errorCapacity, "zone %s: its primary's data differs", ids[i].name);
            return 0;
        }
        ids[i].zone = zone;
    }
    return 1;
}

static int IdentifierOf(const ZoneIdentifier* ids, int count, const char* name)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(ids[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// The release a source directory's version file names, at most eight
// bytes; false when it names none.
static bool ReadRelease(const char* tzdbDirectory, char* releaseOut, size_t capacity,
                        char* errorOut, size_t errorCapacity)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/version", tzdbDirectory);
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        snprintf(errorOut, errorCapacity, "cannot read %s", path);
        return false;
    }
    size_t length = fread(releaseOut, 1, capacity - 1, file);
    fclose(file);
    releaseOut[length] = '\0';
    releaseOut[strcspn(releaseOut, "\r\n")] = '\0';
    if (releaseOut[0] == '\0' || strlen(releaseOut) > 8)
    {
        snprintf(errorOut, errorCapacity, "%s names no tzdb release", path);
        return false;
    }
    return true;
}

// The zones unit from a source directory, its release given; a primary
// other than CLDR's fails when strict.
static int BuildUnit(const char* tzdbDirectory, const char* commonDirectory, const char* release,
                     bool strict, Bytes* unitOut, char* errorOut, size_t errorCapacity)
{
    Tzdb tzdb;
    CompiledZone* zones = nullptr;
    int count = CompileAll(tzdbDirectory, &tzdb, &zones, errorOut, errorCapacity);
    if (count < 0)
    {
        return 0;
    }
    ZoneIdentifier* ids = nullptr;
    int idCount = ZoneIdentifiers(&tzdb, commonDirectory, strict, &ids, errorOut, errorCapacity);
    Tables* t = idCount < 0 ? nullptr : calloc(1, sizeof(Tables));
    int ok = t != nullptr && idCount <= UINT16_MAX &&
             ReadPrimaryData(ids, idCount, zones, errorOut, errorCapacity);
    for (int z = 0; ok && z < count; z++)
    {
        ok = AddZone(t, tzdb.zones[z].name, IdentifierOf(ids, idCount, tzdb.zones[z].name),
                     &zones[z], errorOut, errorCapacity);
    }
    if (ok)
    {
        memcpy(t->release, release, strlen(release));
        AddIdentifiers(t, ids, idCount);
        *unitOut = Assemble(t);
    }
    else if (t != nullptr)
    {
        for (int i = 0; i < zoneSectionCount; i++)
        {
            free(t->sections[i].data);
        }
    }
    free(t);
    free(ids);
    FreeAll(&tzdb, zones, count);
    return ok;
}

int GenerateZones(const char* tzdbDirectory, const char* commonDirectory, const char* release,
                  DataFile* file, char* errorOut, size_t errorCapacity)
{
    char found[16];
    if (!ReadRelease(tzdbDirectory, found, sizeof(found), errorOut, errorCapacity))
    {
        return 0;
    }
    if (strcmp(found, release) != 0)
    {
        snprintf(errorOut, errorCapacity, "%s holds tzdb \"%s\", not the pinned %s", tzdbDirectory,
                 found, release);
        return 0;
    }
    Bytes unit = {0};
    if (!BuildUnit(tzdbDirectory, commonDirectory, release, true, &unit, errorOut, errorCapacity))
    {
        return 0;
    }
    DataFileAdd(file, kindZones, nullptr, unit.data, unit.size);
    free(unit.data);
    return 1;
}

int WriteZonesFile(const char* tzdbDirectory, const char* commonDirectory, const char* path,
                   char* errorOut, size_t errorCapacity)
{
    char release[16];
    Bytes unit = {0};
    if (!ReadRelease(tzdbDirectory, release, sizeof(release), errorOut, errorCapacity) ||
        !BuildUnit(tzdbDirectory, commonDirectory, release, false, &unit, errorOut, errorCapacity))
    {
        return 0;
    }
    // The library's own data version, the release aside.
    mlocDataVersion own;
    mlocGetDataVersion(&own);
    DataFileVersion version = {
        .cldr = mlocPackVersion(own.cldr.major, own.cldr.minor, own.cldr.patch),
        .unicode = mlocPackVersion(own.unicode.major, own.unicode.minor, own.unicode.patch),
        .format = own.format,
        .tables = own.tables,
    };
    memcpy(version.tzdb, release, strlen(release));
    DataFile file = {0};
    DataFileAdd(&file, kindZones, nullptr, unit.data, unit.size);
    free(unit.data);
    size_t size = 0;
    uint8_t* bytes = DataFileWrite(&file, &version, &size);
    FILE* out = bytes == nullptr ? nullptr : fopen(path, "wb");
    int ok = out != nullptr && fwrite(bytes, 1, size, out) == size;
    ok = (out != nullptr && fclose(out) == 0) && ok;
    if (!ok)
    {
        snprintf(errorOut, errorCapacity, "cannot write %s", path);
    }
    free(bytes);
    DataFileFree(&file);
    return ok;
}
