// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone tables (zone_gen.h), in the layout of src/zone_tables.h.

#include "zone_gen.h"

#include "tzdb.h"
#include "zone_compile.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxOffsets = 4096,
};

typedef struct Text
{
    char* bytes;
    size_t size;
    size_t capacity;
} Text;

[[gnu::format(printf, 2, 3)]] static void Put(Text* text, const char* format, ...)
{
    for (;;)
    {
        va_list arguments;
        va_start(arguments, format);
        size_t room = text->capacity - text->size;
        int written = vsnprintf(text->bytes == nullptr ? nullptr : text->bytes + text->size, room,
                                format, arguments);
        va_end(arguments);
        if (written >= 0 && (size_t)written < room)
        {
            text->size += (size_t)written;
            return;
        }
        size_t next = text->capacity == 0 ? 65536 : text->capacity * 2;
        char* grown = written < 0 ? nullptr : realloc(text->bytes, next);
        if (grown == nullptr)
        {
            fputs("mlocgen: cannot write the zone tables\n", stderr);
            exit(1);
        }
        text->bytes = grown;
        text->capacity = next;
    }
}

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

// A list of values written as C, a dozen to a line.
typedef struct Values
{
    Text text;
    int count;
} Values;

[[gnu::format(printf, 2, 3)]] static void Value(Values* v, const char* format, ...)
{
    char value[64];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(value, sizeof(value), format, arguments);
    va_end(arguments);
    Put(&v->text, "%s%s,", v->count % 12 == 0 ? "\n    " : " ", value);
    v->count++;
}

typedef struct Tables
{
    Offsets offsets;
    Values list;
    Values times;
    Values wide;
    Values types;
    Text closings;
    Text names;
    Text records;
    int closingCount;
    size_t nameSize;
} Tables;

static int Limit(char* errorOut, size_t errorCapacity, const char* zone, const char* what)
{
    snprintf(errorOut, errorCapacity, "zone %s: %s exceeds the table's field", zone, what);
    return 0;
}

static void AddClosing(Tables* t, const CompiledZone* zone)
{
    Put(&t->closings, "\n    {%d, {", (int)zone->firstYear);
    for (int k = 0; k < 2; k++)
    {
        const CompiledChange* c = &zone->changes[k];
        Put(&t->closings, "%s{%d, %d, %d, %d, %d, %d}", k == 0 ? "" : ", ", c->month + 1,
            c->dayOfMonth, c->weekday, (int)c->day, (int)c->time,
            OffsetIndex(&t->offsets, c->offset));
    }
    Put(&t->closings, "}},");
    t->closingCount++;
}

// Adds a zone's transitions, offsets, closing rule, name and record.
static int AddZone(Tables* t, const char* name, const CompiledZone* zone, char* errorOut,
                   size_t errorCapacity)
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
    if (count > UINT16_MAX || early > UINT8_MAX || late > UINT8_MAX || t->wide.count > UINT16_MAX ||
        t->list.count > UINT16_MAX || t->nameSize > UINT16_MAX || t->closingCount >= UINT8_MAX)
    {
        return Limit(errorOut, errorCapacity, name, "a count or an index");
    }
    Put(&t->records, "\n    {%d, %d, %d, %d, %d, %d, %d, ", t->times.count, t->types.count, count,
        t->wide.count, early, late, t->list.count);
    Local local = {0};
    int initial = LocalIndex(&local, &t->offsets, zone->initial);
    for (int i = 0; i < count; i++)
    {
        const CompiledTransition* tr = &zone->transitions[i];
        if (i < early || i >= count - late)
        {
            Value(&t->wide, "INT64_C(%lld)", (long long)tr->at);
        }
        else if (!Narrow(tr->at))
        {
            return Limit(errorOut, errorCapacity, name, "a time between narrow ones");
        }
        else
        {
            Value(&t->times, "%lld", (long long)tr->at);
        }
        Value(&t->types, "%d", LocalIndex(&local, &t->offsets, tr->offset));
    }
    for (int i = 0; i < local.count; i++)
    {
        Value(&t->list, "%d", local.shared[i]);
    }
    if (zone->closing)
    {
        AddClosing(t, zone);
    }
    Put(&t->records, "%d, %d, %d},", initial, zone->closing ? t->closingCount : 0,
        (int)t->nameSize);
    Put(&t->names, "\n    \"%s\\0\"", name);
    t->nameSize += strlen(name) + 1;
    return 1;
}

static void Assemble(Text* out, Tables* t, const char* release, int zoneCount)
{
    Put(out,
        "// SPDX-License-Identifier: MIT\n"
        "// Copyright (c) 2026 Sirac Ozmen\n"
        "//\n"
        "// The time zones of tzdb %s (record mloc-0041), compiled from its\n"
        "// source in the layout of zone_tables.h.\n"
        "// Generated by tools/mlocgen; do not edit.\n"
        "// clang-format off\n"
        "\n"
        "#include \"zone_tables.h\"\n"
        "\n"
        "const char mlocZoneRelease[] = \"%s\";\n"
        "const uint16_t mlocZoneCount = %d;\n",
        release, release, zoneCount);
    Put(out, "\nconst ZoneRecord mlocZones[%d] = {%.*s\n};\n", zoneCount, (int)t->records.size,
        t->records.bytes);
    Put(out, "\nconst ZoneOffset mlocZoneOffsets[%d] = {", t->offsets.count);
    for (int i = 0; i < t->offsets.count; i++)
    {
        Put(out, "%s{%d, %d},", i % 6 == 0 ? "\n    " : " ", (int)t->offsets.items[i].standard,
            (int)t->offsets.items[i].save);
    }
    Put(out, "\n};\n");
    Put(out, "\nconst uint16_t mlocZoneOffsetList[%d] = {%.*s\n};\n", t->list.count,
        (int)t->list.text.size, t->list.text.bytes);
    Put(out, "\nconst int32_t mlocZoneTimes[%d] = {%.*s\n};\n", t->times.count,
        (int)t->times.text.size, t->times.text.bytes);
    Put(out, "\nconst int64_t mlocZoneWideTimes[%d] = {%.*s\n};\n", t->wide.count,
        (int)t->wide.text.size, t->wide.text.bytes);
    Put(out, "\nconst uint8_t mlocZoneTypes[%d] = {%.*s\n};\n", t->types.count,
        (int)t->types.text.size, t->types.text.bytes);
    Put(out, "\nconst ZoneClosing mlocZoneClosings[%d] = {%.*s\n};\n", t->closingCount,
        (int)t->closings.size, t->closings.bytes);
    Put(out, "\nconst char mlocZoneNameText[] =%.*s;\n", (int)t->names.size, t->names.bytes);
}

static void FreeTables(Tables* t)
{
    free(t->list.text.bytes);
    free(t->times.text.bytes);
    free(t->wide.text.bytes);
    free(t->types.text.bytes);
    free(t->closings.bytes);
    free(t->names.bytes);
    free(t->records.bytes);
    free(t);
}

// The release in the source directory's version file must be the pin's.
static int CheckRelease(const char* tzdbDirectory, const char* release, char* errorOut,
                        size_t errorCapacity)
{
    char path[4096];
    char found[16] = "";
    snprintf(path, sizeof(path), "%s/version", tzdbDirectory);
    FILE* file = fopen(path, "rb");
    size_t length = file == nullptr ? 0 : fread(found, 1, sizeof(found) - 1, file);
    if (file != nullptr)
    {
        fclose(file);
    }
    found[length] = '\0';
    found[strcspn(found, "\r\n")] = '\0';
    if (strcmp(found, release) != 0)
    {
        snprintf(errorOut, errorCapacity, "%s holds tzdb \"%s\", not the pinned %s", path, found,
                 release);
        return 0;
    }
    return 1;
}

int GenerateZones(const char* tzdbDirectory, const char* release, char** sourceOut,
                  size_t* sourceSizeOut, char* errorOut, size_t errorCapacity)
{
    if (!CheckRelease(tzdbDirectory, release, errorOut, errorCapacity))
    {
        return 0;
    }
    Tzdb tzdb;
    CompiledZone* zones = nullptr;
    int count = CompileAll(tzdbDirectory, &tzdb, &zones, errorOut, errorCapacity);
    if (count < 0)
    {
        return 0;
    }
    Tables* t = calloc(1, sizeof(Tables));
    int ok = t != nullptr;
    for (int z = 0; ok && z < count; z++)
    {
        ok = AddZone(t, tzdb.zones[z].name, &zones[z], errorOut, errorCapacity);
    }
    if (ok)
    {
        Text out = {0};
        Assemble(&out, t, release, count);
        *sourceOut = out.bytes;
        *sourceSizeOut = out.size;
    }
    if (t != nullptr)
    {
        FreeTables(t);
    }
    FreeAll(&tzdb, zones, count);
    return ok;
}
