// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zones unit (zone_tables.h): its sections, its validator, and its
// records read out of their bytes.

#include "zone_tables.h"

#include "bytes.h"
#include "kinds.h"

enum
{
    secondsPerDay = 86400,
    // The years of Temporal's range, a year either side.
    minClosingYear = -271822,
    maxClosingYear = 275761,
    // A closing change's time passes its day by a week at most.
    maxChangeTime = 8 * secondsPerDay,
};

static uint64_t Align(uint64_t at)
{
    return (at + 7) & ~(uint64_t)7;
}

// The sections in order: their sizes from the counts.
bool mlocZoneTablesOf(DataSpan payload, ZoneTables* out)
{
    if (payload.size < zoneHeaderSize || mlocLoad32(payload.bytes + 36) != 0)
    {
        return false;
    }
    uint32_t counts[9];
    for (int i = 0; i < 9; i++)
    {
        counts[i] = mlocLoad32(payload.bytes + 4 * i);
    }
    static const uint32_t s_sizes[9] = {
        zoneRecordSize, zoneIdentifierSize, zoneOffsetSize, 2, 4, 8, 1, zoneClosingSize, 1};
    const uint8_t** sections[9] = {&out->zones, &out->identifiers, &out->offsets,
                                   &out->list,  &out->narrow,      &out->wide,
                                   &out->types, &out->closings,    &out->text};
    uint64_t at = zoneHeaderSize;
    for (int i = 0; i < 9; i++)
    {
        at = Align(at);
        *sections[i] = payload.bytes + (at <= payload.size ? at : 0);
        at += (uint64_t)counts[i] * s_sizes[i];
    }
    out->zoneCount = counts[0];
    out->identifierCount = counts[1];
    out->offsetCount = counts[2];
    out->listCount = counts[3];
    out->narrowCount = counts[4];
    out->wideCount = counts[5];
    out->typeCount = counts[6];
    out->closingCount = counts[7];
    out->textSize = counts[8];
    return at == payload.size;
}

ZoneRecord mlocZoneRecord(const ZoneTables* t, uint32_t zone)
{
    const uint8_t* p = t->zones + (size_t)zone * zoneRecordSize;
    return (ZoneRecord){.narrowFirst = mlocLoad32(p),
                        .typeFirst = mlocLoad32(p + 4),
                        .transitionCount = mlocLoad16(p + 8),
                        .wideFirst = mlocLoad16(p + 10),
                        .offsetFirst = mlocLoad16(p + 12),
                        .name = mlocLoad16(p + 14),
                        .wideEarly = p[16],
                        .wideLate = p[17],
                        .offsetCount = p[18],
                        .initial = p[19],
                        .closing = p[20]};
}

ZoneOffset mlocZoneOffsetOf(const ZoneTables* t, uint32_t index)
{
    const uint8_t* p = t->offsets + (size_t)index * zoneOffsetSize;
    return (ZoneOffset){(int32_t)mlocLoad32(p), (int32_t)mlocLoad32(p + 4)};
}

int32_t mlocZoneClosingYear(const ZoneTables* t, uint32_t closing)
{
    return (int32_t)mlocLoad32(t->closings + (size_t)closing * zoneClosingSize);
}

ZoneChange mlocZoneClosingChange(const ZoneTables* t, uint32_t closing, uint32_t change)
{
    const uint8_t* p =
        t->closings + (size_t)closing * zoneClosingSize + 4 + change * zoneChangeSize;
    return (ZoneChange){.month = p[0],
                        .dayOfMonth = p[1],
                        .weekday = p[2],
                        .day = p[3],
                        .time = (int32_t)mlocLoad32(p + 4),
                        .offset = mlocLoad16(p + 8)};
}

const char* mlocZoneIdentifierName(const ZoneTables* t, uint32_t identifier, size_t* lengthOut)
{
    const uint8_t* p = t->identifiers + (size_t)identifier * zoneIdentifierSize;
    *lengthOut = p[4];
    return (const char*)t->text + mlocLoad32(p);
}

uint32_t mlocZoneIdentifierZone(const ZoneTables* t, uint32_t identifier)
{
    return mlocLoad16(t->identifiers + (size_t)identifier * zoneIdentifierSize + 6);
}

uint32_t mlocZoneIdentifierPrimary(const ZoneTables* t, uint32_t identifier)
{
    return mlocLoad16(t->identifiers + (size_t)identifier * zoneIdentifierSize + 8);
}

bool mlocFindZoneTables(const mlocDataSet* data, ZoneTables* out)
{
    DataSpan span;
    return mlocFindDataSetUnit(data, kindZones, nullptr, 0, &span) && mlocZoneTablesOf(span, out);
}

static int64_t TimeAt(const ZoneTables* t, const ZoneRecord* z, uint32_t i)
{
    uint32_t late = (uint32_t)z->transitionCount - z->wideLate;
    if (i < z->wideEarly || i >= late)
    {
        uint32_t wide = z->wideFirst + (i < z->wideEarly ? i : z->wideEarly + i - late);
        return (int64_t)mlocLoad64(t->wide + (size_t)wide * 8);
    }
    return (int32_t)mlocLoad32(t->narrow + (size_t)(z->narrowFirst + i - z->wideEarly) * 4);
}

// A zone's ranges within the sections, its offsets' places, its name
// (whose identifier may read its primary's equal data), and its times
// ascending.
static bool ValidZone(const ZoneTables* t, uint32_t zone)
{
    ZoneRecord z = mlocZoneRecord(t, zone);
    const uint8_t* reserved = t->zones + (size_t)zone * zoneRecordSize + 21;
    uint32_t wide = (uint32_t)z.wideEarly + z.wideLate;
    if (reserved[0] != 0 || reserved[1] != 0 || reserved[2] != 0 || wide > z.transitionCount ||
        (uint64_t)z.narrowFirst + z.transitionCount - wide > t->narrowCount ||
        (uint64_t)z.typeFirst + z.transitionCount > t->typeCount ||
        (uint64_t)z.wideFirst + wide > t->wideCount ||
        (uint64_t)z.offsetFirst + z.offsetCount > t->listCount || z.initial >= z.offsetCount ||
        z.closing > t->closingCount || z.name >= t->identifierCount)
    {
        return false;
    }
    for (uint32_t i = 0; i < z.transitionCount; i++)
    {
        if (t->types[z.typeFirst + i] >= z.offsetCount ||
            (i > 0 && TimeAt(t, &z, i - 1) >= TimeAt(t, &z, i)))
        {
            return false;
        }
    }
    return true;
}

static int Folded(uint8_t c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

// Whether an identifier's name sorts strictly after the one before it,
// letters folded.
static bool After(const char* a, size_t aLength, const char* b, size_t bLength)
{
    for (size_t i = 0; i < aLength && i < bLength; i++)
    {
        int x = Folded((uint8_t)a[i]);
        int y = Folded((uint8_t)b[i]);
        if (x != y)
        {
            return y > x;
        }
    }
    return bLength > aLength;
}

static bool ValidIdentifier(const ZoneTables* t, uint32_t identifier)
{
    const uint8_t* p = t->identifiers + (size_t)identifier * zoneIdentifierSize;
    uint32_t zone = mlocZoneIdentifierZone(t, identifier);
    uint32_t primary = mlocZoneIdentifierPrimary(t, identifier);
    if (p[4] == 0 || p[5] != 0 || mlocLoad16(p + 10) != 0 ||
        (uint64_t)mlocLoad32(p) + p[4] > t->textSize || zone >= t->zoneCount ||
        primary >= t->identifierCount || mlocZoneIdentifierPrimary(t, primary) != primary ||
        mlocZoneIdentifierZone(t, primary) != zone)
    {
        return false;
    }
    size_t length = 0;
    const char* name = mlocZoneIdentifierName(t, identifier, &length);
    for (size_t i = 0; i < length; i++)
    {
        if (name[i] <= ' ' || name[i] > '~')
        {
            return false;
        }
    }
    size_t previousLength = 0;
    const char* previous =
        identifier == 0 ? nullptr : mlocZoneIdentifierName(t, identifier - 1, &previousLength);
    return identifier == 0 || After(previous, previousLength, name, length);
}

static bool ValidClosing(const ZoneTables* t, uint32_t closing)
{
    int32_t year = mlocZoneClosingYear(t, closing);
    if (year < minClosingYear || year > maxClosingYear)
    {
        return false;
    }
    for (uint32_t k = 0; k < 2; k++)
    {
        ZoneChange c = mlocZoneClosingChange(t, closing, k);
        const uint8_t* p = t->closings + (size_t)closing * zoneClosingSize + 4 + k * zoneChangeSize;
        if (c.month < 2 || c.month > 11 || c.dayOfMonth < 1 || c.dayOfMonth > 31 || c.weekday > 6 ||
            c.day > zoneOnOrAfter || c.time < -maxChangeTime || c.time > maxChangeTime ||
            c.offset >= t->offsetCount || mlocLoad16(p + 10) != 0)
        {
            return false;
        }
    }
    return true;
}

bool mlocValidZones(const DataView* view, DataSpan payload)
{
    (void)view;
    ZoneTables t;
    if (!mlocZoneTablesOf(payload, &t) || t.zoneCount > UINT16_MAX ||
        t.identifierCount > UINT16_MAX || t.typeCount != t.narrowCount + (uint64_t)t.wideCount)
    {
        return false;
    }
    for (uint32_t i = 0; i < t.offsetCount; i++)
    {
        ZoneOffset o = mlocZoneOffsetOf(&t, i);
        int64_t sum = (int64_t)o.standard + o.save;
        if (o.save < 0 || o.save >= secondsPerDay || sum <= -secondsPerDay || sum >= secondsPerDay)
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < t.listCount; i++)
    {
        if (mlocLoad16(t.list + (size_t)i * 2) >= t.offsetCount)
        {
            return false;
        }
    }
    bool valid = true;
    for (uint32_t i = 0; valid && i < t.identifierCount; i++)
    {
        valid = ValidIdentifier(&t, i);
    }
    for (uint32_t i = 0; valid && i < t.zoneCount; i++)
    {
        valid = ValidZone(&t, i);
    }
    for (uint32_t i = 0; valid && i < t.closingCount; i++)
    {
        valid = ValidClosing(&t, i);
    }
    return valid;
}
