// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit identifiers in their normal form (unit_normal.h), as ICU4C 79's
// MeasureUnitImpl parses, sorts and serializes them.

#include "unit_normal.h"

#include "data_view.h"
#include "invariant.h"
#include "kinds.h"
#include "unit_tables.h"

#include <stdio.h>
#include <string.h>

// The text of a simple unit and of a prefix, as identifiers write them.
void mlocSimpleUnitText(const UnitTable* t, uint16_t simple, const char** text, size_t* length)
{
    const uint8_t* s = mlocSimpleUnit(t, simple);
    *text = (const char*)t->text +
            ((uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24);
    *length = s[simpleLength];
}

static void PrefixText(const UnitTable* t, int8_t prefix, const char** text, size_t* length)
{
    const uint8_t* p = mlocUnitPrefix(t, (uint32_t)prefix);
    *text = (const char*)t->text +
            ((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
    *length = p[4];
}

// Appends text to an identifier being written.
static bool AppendId(char* out, size_t room, size_t* at, const char* text, size_t length)
{
    if (length > room - *at)
    {
        return false;
    }
    memcpy(out + *at, text, length);
    *at += length;
    return true;
}

// Writes a single unit as ICU's appendNeutralIdentifier does.
static bool WritePart(const UnitTable* t, const UnitPart* part, char* out, size_t room, size_t* at)
{
    int power = part->power < 0 ? -part->power : part->power;
    // Each factor's power is 15 at most, merged across the factors.
    MLOC_ASSERT(power >= 1 && power <= unitMaxFactors * 15);
    char head[16] = "";
    if (power == 2 || power == 3)
    {
        snprintf(head, sizeof(head), "%s", power == 2 ? "square-" : "cubic-");
    }
    else if (power > 3)
    {
        snprintf(head, sizeof(head), "pow%d-", power);
    }
    const char* text = nullptr;
    size_t length = 0;
    if (!AppendId(out, room, at, head, strlen(head)))
    {
        return false;
    }
    if (part->prefix >= 0)
    {
        PrefixText(t, part->prefix, &text, &length);
        if (!AppendId(out, room, at, text, length))
        {
            return false;
        }
    }
    mlocSimpleUnitText(t, part->simple, &text, &length);
    return AppendId(out, room, at, text, length);
}

// A constant as ICU writes it: its digits, or 1eN for a power of ten past
// 1000.
static size_t ConstantText(uint64_t constant, char* out, size_t room)
{
    int n = snprintf(out, room, "%llu", (unsigned long long)constant);
    // A 64-bit constant's 20 digits fit the caller's room.
    MLOC_ASSERT(n > 0 && (size_t)n < room);
    size_t zeros = strspn(out + 1, "0");
    if (constant > 1000 && out[0] == '1' && zeros == (size_t)n - 1)
    {
        n = snprintf(out, room, "1e%zu", zeros);
    }
    return (size_t)n;
}

// Writes a normalized product as ICU's MeasureUnitImpl::serialize does:
// "per-" before the first denominator unit, a constant after it (or at
// the end).
bool mlocWriteNormalUnit(const UnitTable* t, const UnitPart* parts, uint32_t count,
                         uint64_t constant, char* out, size_t room, size_t* lengthOut)
{
    size_t at = 0;
    bool beforePer = true;
    bool constantWritten = false;
    char number[24];
    size_t numberLength = constant > 1 ? ConstantText(constant, number, sizeof(number)) : 0;
    for (uint32_t i = 0; i < count; i++)
    {
        bool firstDenominator = beforePer && parts[i].power < 0;
        beforePer = beforePer && !firstDenominator;
        bool ok = true;
        if (firstDenominator)
        {
            ok = AppendId(out, room, &at, at == 0 ? "per-" : "-per-", at == 0 ? 4 : 5);
            if (ok && numberLength > 0)
            {
                ok = AppendId(out, room, &at, number, numberLength) &&
                     AppendId(out, room, &at, "-", 1);
                constantWritten = true;
            }
        }
        else if (at != 0)
        {
            ok = AppendId(out, room, &at, "-", 1);
        }
        if (!ok || !WritePart(t, &parts[i], out, room, &at))
        {
            return false;
        }
    }
    if (!constantWritten && numberLength > 0 &&
        (!AppendId(out, room, &at, "-per-", 5) || !AppendId(out, room, &at, number, numberLength)))
    {
        return false;
    }
    *lengthOut = at;
    return true;
}

// A prefix's power for ordering (a binary power counts thrice) and its
// base.
static int PrefixOrder(const UnitTable* t, int8_t prefix, int* base)
{
    if (prefix < 0)
    {
        *base = 10;
        return 0;
    }
    const uint8_t* p = mlocUnitPrefix(t, (uint32_t)prefix);
    *base = p[6] != 0 ? 1024 : 10;
    return p[6] != 0 ? 3 * p[6] : (int8_t)p[5];
}

// ICU's SingleUnitImpl::compareTo: the numerator first, then the
// quantity's order, the simple unit's index, the larger prefix.
static int ComparePart(const UnitTable* t, const UnitPart* a, const UnitPart* b)
{
    if ((a->power < 0) != (b->power < 0))
    {
        return a->power < 0 ? 1 : -1;
    }
    int qa = mlocSimpleUnit(t, a->simple)[simpleOrder];
    int qb = mlocSimpleUnit(t, b->simple)[simpleOrder];
    if (qa != qb)
    {
        return qa < qb ? -1 : 1;
    }
    if (a->simple != b->simple)
    {
        return a->simple < b->simple ? -1 : 1;
    }
    int baseA = 0;
    int baseB = 0;
    int pa = PrefixOrder(t, a->prefix, &baseA);
    int pb = PrefixOrder(t, b->prefix, &baseB);
    if (pa != pb)
    {
        return pa < pb ? 1 : -1;
    }
    return baseA == baseB ? 0 : (baseA < baseB ? 1 : -1);
}

// Merges single units of one simple unit, prefix and side, then sorts
// them (ICU's appendSingleUnit and serialize).
static void Normalize(const UnitTable* t, UnitId* id)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < id->count; i++)
    {
        UnitPart part = id->parts[i];
        uint32_t j = 0;
        while (j < count &&
               !(id->parts[j].simple == part.simple && id->parts[j].prefix == part.prefix &&
                 (id->parts[j].power < 0) == (part.power < 0)))
        {
            j++;
        }
        if (j < count)
        {
            // At most unitMaxFactors powers of 15 or less: far within int8_t.
            MLOC_ASSERT(id->parts[j].power + part.power >= -unitMaxFactors * 15 &&
                        id->parts[j].power + part.power <= unitMaxFactors * 15);
            id->parts[j].power = (int8_t)(id->parts[j].power + part.power);
            continue;
        }
        id->parts[count++] = part;
    }
    id->count = (uint8_t)count;
    for (uint32_t i = 1; i < count; i++)
    {
        UnitPart part = id->parts[i];
        uint32_t j = i;
        for (; j > 0 && ComparePart(t, &id->parts[j - 1], &part) > 0; j--)
        {
            id->parts[j] = id->parts[j - 1];
        }
        id->parts[j] = part;
    }
}

// ICU's parser knows kilogram as kilo- and gram (measunit_extra.cpp
// leaves it out of its simple units), so its names are built from
// gram's and the prefix's, and it sorts as gram.
static void SplitKilogram(const UnitTable* t, UnitId* id)
{
    int32_t kilogram = mlocFindSimpleUnit(t, "kilogram", 8);
    int32_t gram = mlocFindSimpleUnit(t, "gram", 4);
    int32_t kilo = mlocFindUnitPrefix(t, "kilogram", 8);
    for (uint32_t i = 0; gram >= 0 && kilo >= 0 && i < id->count; i++)
    {
        if (id->parts[i].simple == (uint16_t)kilogram && id->parts[i].prefix < 0)
        {
            id->parts[i].simple = (uint16_t)gram;
            id->parts[i].prefix = (int8_t)kilo;
        }
    }
}

// Parses and normalizes an identifier into a context's table.
mlocResult mlocParseNormalUnit(const mlocDataSet* data, const char* unit, size_t length,
                               UnitTable* table, UnitId* id)
{
    DataSpan span;
    if (!mlocFindDataSetUnit(data, kindUnits, nullptr, 0, &span))
    {
        return mloc_errorNotFound;
    }
    *table = mlocReadUnitTable(span);
    mlocResult r = mlocParseUnitId(table, unit, length, id);
    if (r != mloc_success)
    {
        return r;
    }
    if (id->mixed != 0 || id->numeratorConstant != 1)
    {
        return mloc_errorInvalid;
    }
    for (uint32_t i = 0; i < id->count; i++)
    {
        int power = id->parts[i].power < 0 ? -id->parts[i].power : id->parts[i].power;
        if (id->parts[i].currency != 0 || power > 15)
        {
            return mloc_errorInvalid;
        }
    }
    SplitKilogram(table, id);
    Normalize(table, id);
    return mloc_success;
}

mlocResult mlocNormalizeUnitName(const mlocDataSet* data, const char* unit, size_t length,
                                 char* out, size_t capacity, size_t* lengthOut)
{
    UnitTable table;
    UnitId id;
    mlocResult r = mlocParseNormalUnit(data, unit, length, &table, &id);
    if (r != mloc_success)
    {
        return r;
    }
    return mlocWriteNormalUnit(&table, id.parts, id.count, id.denominatorConstant, out, capacity,
                               lengthOut)
               ? mloc_success
               : mloc_errorCapacity;
}
