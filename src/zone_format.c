// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone formatter (maul-locale/datetime.h): a zone at an instant
// written by a style, falling back as ICU4C's TimeZoneFormat and
// TimeZoneGenericNames fall back: a specific name, else the offset; a
// generic name (the standard one where no daylight time lies within 184
// days, qualified by the fallback format where the metazone's preferred
// zone for the locale's region shows another offset), else the location,
// else the offset; the location by the country or city; the city, else
// Etc/Unknown's.

#include "zone_format.h"

#include "bytes.h"
#include "date_tables.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "names_data.h"
#include "zone_code_tables.h"
#include "zone_handle.h"
#include "zone_local.h"
#include "zone_name_tables.h"
#include "zone_rules.h"
#include "zone_tables.h"

#include <string.h>

enum
{
    // Marks a zone format the library made.
    zoneStamp = 0x5A,
    // ICU4C's reach around an instant for a daylight period that keeps a
    // generic name from the standard one: 184 days, in seconds.
    daylightReach = 184 * 86400,
    secondsPerDay = 86400,
};

// A text in the data.
typedef struct ZoneText
{
    const uint8_t* bytes;
    uint32_t length;
} ZoneText;

static ZoneMeta ReadMeta(DataSpan unit)
{
    ZoneMeta m;
    m.codeCount = mlocLoad32(unit.bytes);
    m.periodCount = mlocLoad32(unit.bytes + 4);
    m.metazoneCount = mlocLoad32(unit.bytes + 8);
    m.preferredCount = mlocLoad32(unit.bytes + 12);
    m.codes = unit.bytes + zoneMetaHeaderSize;
    m.periods = m.codes + (size_t)m.codeCount * zoneMetaCodeSize;
    m.metazones = m.periods + (size_t)m.periodCount * zoneMetaPeriodSize;
    m.preferred = m.metazones + (size_t)m.metazoneCount * zoneMetaZoneSize;
    return m;
}

// A code's metazone period at an instant, or null; the code is below
// the count (the formatter's own, or one the validator bounded) or -1.
static const uint8_t* PeriodAt(const ZoneMeta* m, int32_t code, int64_t seconds)
{
    if (code < 0)
    {
        return nullptr;
    }
    const uint8_t* c = m->codes + (size_t)code * zoneMetaCodeSize;
    for (uint32_t k = 0; k < c[3]; k++)
    {
        const uint8_t* p = m->periods + (size_t)(mlocLoad16(c + 4) + k) * zoneMetaPeriodSize;
        if (seconds >= (int64_t)mlocLoad64(p) && seconds < (int64_t)mlocLoad64(p + 8))
        {
            return p;
        }
    }
    return nullptr;
}

// An offset read by the standard and daylight offsets CLDR gives its
// period, where it gives them: the daylight one is daylight time even
// where the zones unit counts it standard (Punta Arenas), as ICU4C reads
// it.
static ZoneOffset Classify(const uint8_t* period, ZoneOffset offset)
{
    if (period == nullptr || period[18] != zoneMetaOffsets)
    {
        return offset;
    }
    int32_t total = offset.standard + offset.save;
    int32_t standard = (int16_t)mlocLoad16(period + 20) * 60;
    int32_t daylight = (int16_t)mlocLoad16(period + 22) * 60;
    return total == daylight ? (ZoneOffset){standard, daylight - standard} : offset;
}

// A metazone's preferred zone in a region, else its golden zone.
static int32_t ReferenceZone(const ZoneMeta* m, int32_t metazone, const char* region)
{
    const uint8_t* z = m->metazones + (size_t)metazone * zoneMetaZoneSize;
    for (uint32_t k = 0; k < mlocLoad16(z + 4); k++)
    {
        const uint8_t* p = m->preferred + (size_t)(mlocLoad16(z + 2) + k) * zoneMetaPreferredSize;
        if (memcmp(p, region, 2) == 0 && region[2] == '\0')
        {
            return (int32_t)mlocLoad16(p + 2);
        }
    }
    return (int32_t)mlocLoad16(z);
}

// A code's region (two letters), or false for none.
static bool CodeRegion(const ZoneAt* a, char region[3])
{
    region[0] = '\0';
    if (a->code >= 0)
    {
        const uint8_t* c = a->meta.codes + (size_t)a->code * zoneMetaCodeSize;
        region[0] = (char)c[0];
        region[1] = (char)c[1];
        region[2] = '\0';
    }
    return region[0] != '\0';
}

// The text filed at a key first along the chain; false for none or an
// empty one ("∅∅∅").
static bool FindKey(const ZoneFormat* f, uint32_t key, ZoneText* out)
{
    *out = (ZoneText){0};
    for (uint32_t i = 0; i < f->nameCount; i++)
    {
        const uint8_t* unit = f->names[i].bytes;
        uint32_t count = mlocLoad32(unit);
        const uint8_t* keys = unit + zoneNameHeaderSize;
        const uint8_t* offsets = keys + 2 * (size_t)count;
        uint32_t low = 0;
        uint32_t high = count;
        while (low < high)
        {
            uint32_t middle = low + (high - low) / 2;
            if (mlocLoad16(keys + 2 * (size_t)middle) < key)
            {
                low = middle + 1;
            }
            else
            {
                high = middle;
            }
        }
        if (low < count && mlocLoad16(keys + 2 * (size_t)low) == key)
        {
            uint32_t start = mlocLoad32(offsets + 4 * (size_t)low);
            out->bytes = offsets + 4 * ((size_t)count + 1) + start;
            out->length = mlocLoad32(offsets + 4 * (size_t)low + 4) - start;
            return out->length > 0;
        }
    }
    return false;
}

static bool FormatText(const ZoneFormat* f, ZoneNameFormat field, ZoneText* out)
{
    return FindKey(f, zoneNameFormats * zoneNameFieldsPerItem + (uint32_t)field, out);
}

// A code's field; false for a code the keys cannot hold.
static bool CodeText(const ZoneFormat* f, int32_t code, ZoneNameField field, ZoneText* out)
{
    *out = (ZoneText){0};
    return code >= 0 && code < zoneNameMaxCodes &&
           FindKey(f, (zoneNameCodeBase + (uint32_t)code) * zoneNameFieldsPerItem + (uint32_t)field,
                   out);
}

static bool MetazoneText(const ZoneFormat* f, int32_t metazone, ZoneNameField field, ZoneText* out)
{
    *out = (ZoneText){0};
    return metazone >= 0 &&
           FindKey(f,
                   (zoneNameMetazoneBase + (uint32_t)metazone) * zoneNameFieldsPerItem +
                       (uint32_t)field,
                   out);
}

static void Write(NumberWriter* w, const void* bytes, size_t length)
{
    mlocWritePart(w, mloc_datePartTimeZone, bytes, length);
}

// Writes a pattern with its {0} and {1} replaced.
static void WritePattern(NumberWriter* w, ZoneText pattern, ZoneText zero, ZoneText one)
{
    uint32_t at = 0;
    uint32_t from = 0;
    while (at + 3 <= pattern.length)
    {
        const uint8_t* p = pattern.bytes + at;
        if (p[0] == '{' && (p[1] == '0' || p[1] == '1') && p[2] == '}')
        {
            Write(w, pattern.bytes + from, at - from);
            ZoneText arg = p[1] == '0' ? zero : one;
            Write(w, arg.bytes, arg.length);
            at += 3;
            from = at;
            continue;
        }
        at++;
    }
    Write(w, pattern.bytes + from, pattern.length - from);
}

static ZoneText Literal(const char* text)
{
    return (ZoneText){(const uint8_t*)text, (uint32_t)strlen(text)};
}

// The digits of n (0 to 99), at least minimum of them.
static void WriteDigits(const ZoneFormat* f, NumberWriter* w, uint32_t n, uint32_t minimum)
{
    if (n >= 10 || minimum > 1)
    {
        Write(w, f->digits + (size_t)(n / 10) * numberDigitSize, f->digitLength);
    }
    Write(w, f->digits + (size_t)(n % 10) * numberDigitSize, f->digitLength);
}

// Where a half of the hour format holds its fields: the last H (and an H
// before it) before "mm", and "mm".
typedef struct OffsetPattern
{
    ZoneText text;
    uint32_t hourStart;
    uint32_t hourEnd;
    uint32_t minutes;
} OffsetPattern;

// An hour format's parts; the validator saw an H before its first mm.
static void ReadOffsetPattern(ZoneText text, OffsetPattern* out)
{
    out->text = text;
    uint32_t i = 0;
    while (memcmp(text.bytes + i, "mm", 2) != 0)
    {
        i++;
    }
    uint32_t h = i;
    while (text.bytes[h - 1] != 'H')
    {
        h--;
    }
    out->minutes = i;
    out->hourEnd = h;
    out->hourStart = h >= 2 && text.bytes[h - 2] == 'H' ? h - 2 : h - 1;
}

// Writes an offset by a half of the hour format, as ICU4C expands and
// truncates it: the hours alone (the text up to them), with the minutes,
// or with the seconds after the minutes behind the hours' separator.
static void WriteOffsetFields(const ZoneFormat* f, NumberWriter* w, const OffsetPattern* p,
                              uint32_t seconds, bool shortForm)
{
    uint32_t h = seconds / 3600;
    uint32_t m = seconds / 60 % 60;
    uint32_t s = seconds % 60;
    const uint8_t* t = p->text.bytes;
    Write(w, t, p->hourStart);
    WriteDigits(f, w, h, shortForm ? 1 : 2);
    if (s == 0 && m == 0 && shortForm)
    {
        return;
    }
    Write(w, t + p->hourEnd, p->minutes - p->hourEnd);
    WriteDigits(f, w, m, 2);
    if (s != 0)
    {
        Write(w, t + p->hourEnd, p->minutes - p->hourEnd);
        WriteDigits(f, w, s, 2);
    }
    Write(w, t + p->minutes + 2, p->text.length - p->minutes - 2);
}

// The localized GMT format: gmtFormat around the hour format's half.
static void WriteOffset(const ZoneFormat* f, NumberWriter* w, int32_t offset, bool shortForm)
{
    ZoneText gmt;
    ZoneText half;
    OffsetPattern p = {0};
    bool negative = offset < 0;
    if (!FormatText(f, zoneFormatGmt, &gmt))
    {
        gmt = Literal("GMT{0}");
    }
    if (!FormatText(f, negative ? zoneFormatHourNegative : zoneFormatHourPositive, &half))
    {
        half = Literal(negative ? "-HH:mm" : "+HH:mm");
    }
    ReadOffsetPattern(half, &p);
    // The validator saw the GMT format's {0}.
    uint32_t at = 0;
    while (memcmp(gmt.bytes + at, "{0}", 3) != 0)
    {
        at++;
    }
    Write(w, gmt.bytes, at);
    WriteOffsetFields(f, w, &p, (uint32_t)(negative ? -(int64_t)offset : offset), shortForm);
    Write(w, gmt.bytes + at + 3, gmt.length - at - 3);
}

// The zone's code: its identifier's id, else its primary identifier's.
static int32_t CodeOf(const ZoneAt* a, const mlocTimeZone* zone)
{
    if (!a->named)
    {
        return -1;
    }
    size_t length = 0;
    const char* name = mlocZoneIdentifierName(&a->t, zone->identifier, &length);
    int32_t code = mlocZoneCodeOfIdentifier(&a->codes, name, length);
    if (code < 0)
    {
        uint32_t primary = mlocZoneIdentifierPrimary(&a->t, zone->identifier);
        name = mlocZoneIdentifierName(&a->t, primary, &length);
        code = mlocZoneCodeOfIdentifier(&a->codes, name, length);
    }
    return code >= 0 && (uint32_t)code < a->meta.codeCount ? code : -1;
}

bool mlocReadZoneAt(const ZoneFormat* f, const mlocTimeZone* zone, const mlocInstant* instant,
                    ZoneAt* a)
{
    memset(a, 0, sizeof(*a));
    if (!mlocReadTimeZone(zone, &a->t, &a->named) || !mlocZoneCodeTablesOf(f->codes, &a->codes))
    {
        return false;
    }
    a->meta = ReadMeta(f->meta);
    a->seconds = instant->seconds;
    if (a->named)
    {
        a->index = mlocZoneIdentifierZone(&a->t, zone->identifier);
        a->offset = mlocZoneOffsetAt(&a->t, a->index, a->seconds);
    }
    else
    {
        a->offset = (ZoneOffset){(int32_t)zone->offsetMinutes * 60, 0};
    }
    a->code = CodeOf(a, zone);
    const uint8_t* period = PeriodAt(&a->meta, a->code, a->seconds);
    a->metazone = period != nullptr ? (int32_t)mlocLoad16(period + 16) : -1;
    a->offset = Classify(period, a->offset);
    return true;
}

static int32_t TotalOffset(const ZoneAt* a)
{
    return a->offset.standard + a->offset.save;
}

// "zzzz" or "z": the zone's name of its time at the instant, else its
// metazone's.
static bool WriteSpecific(const ZoneFormat* f, const ZoneAt* a, bool longForm, NumberWriter* w)
{
    bool daylight = a->offset.save != 0;
    ZoneNameField field = longForm ? (daylight ? zoneFieldLongDaylight : zoneFieldLongStandard)
                                   : (daylight ? zoneFieldShortDaylight : zoneFieldShortStandard);
    ZoneText name;
    if (!CodeText(f, a->code, field, &name) && !MetazoneText(f, a->metazone, field, &name))
    {
        return false;
    }
    Write(w, name.bytes, name.length);
    return true;
}

// Whether a named zone keeps standard time within 184 days of the
// instant either way, as ICU4C asks before taking a standard name for a
// generic one.
static bool StandardAround(const ZoneAt* a)
{
    int64_t at = 0;
    ZoneOffset offset = {0};
    if (a->offset.save != 0)
    {
        return false;
    }
    if (mlocZonePreviousTransition(&a->t, a->index, a->seconds, &at, &offset) &&
        a->seconds - at < daylightReach && mlocZoneOffsetAt(&a->t, a->index, at - 1).save != 0)
    {
        return false;
    }
    return !(mlocZoneNextTransition(&a->t, a->index, a->seconds, &at, &offset) &&
             at - a->seconds < daylightReach && offset.save != 0);
}

static bool SameIgnoringCase(ZoneText x, ZoneText y)
{
    if (x.length != y.length)
    {
        return false;
    }
    for (uint32_t i = 0; i < x.length; i++)
    {
        uint8_t c = x.bytes[i];
        uint8_t d = y.bytes[i];
        c = c >= 'A' && c <= 'Z' ? (uint8_t)(c + 32) : c;
        d = d >= 'A' && d <= 'Z' ? (uint8_t)(d + 32) : d;
        if (c != d)
        {
            return false;
        }
    }
    return true;
}

// A zone's offset at a local time as ICU4C reads one: the later reading
// where the local time repeats, the earlier where it is skipped.
static ZoneOffset OffsetAtLocal(const ZoneTables* t, uint32_t zone, int64_t local)
{
    ZoneOffset after = mlocZoneOffsetAt(t, zone, local + secondsPerDay);
    ZoneOffset before = mlocZoneOffsetAt(t, zone, local - secondsPerDay);
    ZoneOffset at = mlocZoneOffsetAt(t, zone, local - after.standard - after.save);
    if (at.standard + at.save == after.standard + after.save)
    {
        return at;
    }
    at = mlocZoneOffsetAt(t, zone, local - before.standard - before.save);
    return at.standard + at.save == before.standard + before.save ? at : before;
}

// Whether the metazone's preferred zone for the region shows the zone's
// wall time with another offset, which qualifies its generic name (a
// zone in a metazone is a named one).
static bool OtherReference(const ZoneFormat* f, const ZoneAt* a)
{
    int32_t reference = ReferenceZone(&a->meta, a->metazone, f->region);
    if (reference == a->code || (uint32_t)reference >= a->codes.codeCount)
    {
        return false;
    }
    ZoneCode code = mlocZoneCode(&a->codes, (uint32_t)reference);
    int32_t identifier =
        mlocZoneFindIdentifier(&a->t, (const char*)a->codes.text + code.name, code.nameLength);
    if (identifier < 0)
    {
        return false;
    }
    uint32_t zone = mlocZoneIdentifierZone(&a->t, (uint32_t)identifier);
    ZoneOffset other = Classify(PeriodAt(&a->meta, reference, a->seconds),
                                OffsetAtLocal(&a->t, zone, a->seconds + TotalOffset(a)));
    return other.standard != a->offset.standard || other.save != a->offset.save;
}

// A region's name in the locale, else its code.
static ZoneText RegionName(const ZoneFormat* f, const char region[3])
{
    char code[5] = {'r', ':', region[0], region[1], '\0'};
    NameText name;
    if (mlocFindName(f->data, &f->locale, code, 4, nameFieldName, &name))
    {
        return (ZoneText){name.text, name.length};
    }
    return (ZoneText){(const uint8_t*)region, 2};
}

// The zone's city: the locale's, else the one its identifier implies.
static bool City(const ZoneFormat* f, const ZoneAt* a, ZoneText* out)
{
    return CodeText(f, a->code, zoneFieldCity, out);
}

// "Pacific Time (Canada)": the metazone's name qualified by the zone's
// country where it is the metazone's preferred zone there, else by its
// city, else by its identifier.
static void WritePartial(const ZoneFormat* f, const ZoneAt* a, ZoneText name, NumberWriter* w)
{
    ZoneText pattern;
    ZoneText location;
    char region[3];
    if (!FormatText(f, zoneFormatFallback, &pattern))
    {
        pattern = Literal("{1} ({0})");
    }
    if (CodeRegion(a, region) && ReferenceZone(&a->meta, a->metazone, region) == a->code)
    {
        location = RegionName(f, region);
    }
    else if (!City(f, a, &location))
    {
        ZoneCode code = mlocZoneCode(&a->codes, (uint32_t)a->code);
        location = (ZoneText){a->codes.text + code.name, code.nameLength};
    }
    WritePattern(w, pattern, location, name);
}

// "Pacific Time", "PT": the zone's generic name, else the standard name
// of a zone in standard time all around, else its metazone's generic
// name, qualified where the reference zone differs.
static bool WriteGeneric(const ZoneFormat* f, const ZoneAt* a, bool longForm, NumberWriter* w)
{
    ZoneNameField generic = longForm ? zoneFieldLongGeneric : zoneFieldShortGeneric;
    ZoneNameField standard = longForm ? zoneFieldLongStandard : zoneFieldShortStandard;
    ZoneText name;
    ZoneText metazone;
    if (CodeText(f, a->code, generic, &name))
    {
        Write(w, name.bytes, name.length);
        return true;
    }
    if (a->metazone < 0)
    {
        return false;
    }
    bool hasGeneric = MetazoneText(f, a->metazone, generic, &metazone);
    if (StandardAround(a) &&
        (CodeText(f, a->code, standard, &name) || MetazoneText(f, a->metazone, standard, &name)) &&
        !(hasGeneric && SameIgnoringCase(name, metazone)))
    {
        Write(w, name.bytes, name.length);
        return true;
    }
    if (!hasGeneric)
    {
        return false;
    }
    if (OtherReference(f, a))
    {
        WritePartial(f, a, metazone, w);
    }
    else
    {
        Write(w, metazone.bytes, metazone.length);
    }
    return true;
}

// "Los Angeles Time", "Japan Time": the region format around the country
// for its only or primary zone, else around the city.
static bool WriteLocation(const ZoneFormat* f, const ZoneAt* a, NumberWriter* w)
{
    char region[3];
    ZoneText pattern;
    ZoneText location;
    if (!CodeRegion(a, region))
    {
        return false;
    }
    if (!FormatText(f, zoneFormatRegion, &pattern))
    {
        pattern = Literal("{0}");
    }
    bool country = (a->meta.codes[(size_t)a->code * zoneMetaCodeSize + 2] & zoneMetaCountry) != 0;
    if (country)
    {
        location = RegionName(f, region);
    }
    else if (!City(f, a, &location))
    {
        return false;
    }
    WritePattern(w, pattern, location, (ZoneText){0});
    return true;
}

// "Los Angeles": the city, else Etc/Unknown's, else "Unknown".
static void WriteCity(const ZoneFormat* f, const ZoneAt* a, NumberWriter* w)
{
    ZoneText city;
    if (!City(f, a, &city) &&
        !FindKey(f, zoneNameUnknown * zoneNameFieldsPerItem + zoneFieldCity, &city))
    {
        city = Literal("Unknown");
    }
    Write(w, city.bytes, city.length);
}

static bool WriteStyle(const ZoneFormat* f, mlocZoneStyle style, const ZoneAt* a, NumberWriter* w)
{
    switch (style)
    {
    case mloc_zoneStyleSpecificLong:
    case mloc_zoneStyleSpecificShort:
        return WriteSpecific(f, a, style == mloc_zoneStyleSpecificLong, w);
    case mloc_zoneStyleGenericLong:
    case mloc_zoneStyleGenericShort:
        return WriteGeneric(f, a, style == mloc_zoneStyleGenericLong, w) || WriteLocation(f, a, w);
    case mloc_zoneStyleLocation:
        return WriteLocation(f, a, w);
    case mloc_zoneStyleCity:
        WriteCity(f, a, w);
        return true;
    default:
        return false;
    }
}

bool mlocReadZonedLocal(const ZoneFormat* f, const mlocTimeZone* zone, const mlocInstant* instant,
                        ZoneAt* at, mlocDateTime* localOut)
{
    return zone != nullptr && instant != nullptr && mlocValidInstant(instant) &&
           mlocReadZoneAt(f, zone, instant, at) &&
           mlocLocalDateTime(instant, TotalOffset(at), mloc_calendarIso8601, localOut) ==
               mloc_success;
}

void mlocWriteZone(const ZoneFormat* f, mlocZoneStyle style, const ZoneAt* a, NumberWriter* w)
{
    if (!WriteStyle(f, style, a, w))
    {
        bool shortForm = style == mloc_zoneStyleSpecificShort ||
                         style == mloc_zoneStyleGenericShort || style == mloc_zoneStyleOffsetShort;
        WriteOffset(f, w, TotalOffset(a), shortForm);
    }
}

void mlocWriteZonePattern(const ZoneFormat* f, const uint8_t* pattern, uint32_t length,
                          const ZoneAt* zone, const PatternSpan* span, NumberWriter* w)
{
    uint32_t at = 0;
    while (at < length)
    {
        w->source = at >= span->first && at < span->end ? span->source : 0;
        if (pattern[at] == datePatternField)
        {
            mlocWriteZone(f, mlocZoneFieldStyle(pattern[at + 1], pattern[at + 2]), zone, w);
            at += 3;
            continue;
        }
        uint32_t end = at;
        while (end < length && pattern[end] >= 0x20)
        {
            end++;
        }
        mlocWritePart(w, mloc_datePartLiteral, pattern + at, end - at);
        at = end;
    }
    w->source = 0;
}

typedef struct ChainWalk
{
    ZoneFormat* f;
    bool full;
} ChainWalk;

static bool TakeStep(void* context, const DataView* view, uint32_t locale, int depth)
{
    (void)depth;
    ChainWalk* walk = context;
    DataSpan unit;
    if (!mlocFindDataUnit(view, kindZoneNames, locale, &unit))
    {
        return false;
    }
    if (walk->f->nameCount == zoneChainMost)
    {
        walk->full = true;
        return true;
    }
    walk->f->names[walk->f->nameCount++] = unit;
    return false;
}

// The region generic names are qualified for: the locale's, else its
// likely one, else 001.
static void TargetRegion(ZoneFormat* f, const mlocDataSet* data, const mlocLocale* locale)
{
    mlocLocale likely;
    const char* region = locale->region;
    if (region[0] == '\0' && mlocMaximizeLocale(data, locale, &likely) == mloc_success)
    {
        region = likely.region;
    }
    memcpy(f->region, region[0] != '\0' ? region : "001", 3);
    f->region[3] = '\0';
}

mlocResult mlocMakeZoneFormat(ZoneFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                              mlocZoneStyle style)
{
    memset(f, 0, sizeof(*f));
    NumberCore core;
    ChainWalk walk = {f, false};
    ZoneCodeTables codes;
    mlocResult digits = mlocInitNumberCore(&core, data, locale, mloc_numberDecimal);
    if (digits < 0 || !mlocFindDataSetUnit(data, kindZoneMeta, nullptr, 0, &f->meta) ||
        !mlocFindZoneCodeTables(data, &codes) ||
        !mlocFindDataSetUnit(data, kindZoneCodes, nullptr, 0, &f->codes) ||
        !mlocVisitLocaleChainOf(data, mloc_componentMain, kindZoneNames, locale, TakeStep, &walk) ||
        f->nameCount == 0)
    {
        return mloc_errorNotFound;
    }
    if (walk.full)
    {
        return mloc_errorData;
    }
    f->data = data;
    f->locale = *locale;
    f->style = (uint8_t)style;
    memcpy(f->digits, core.digits, sizeof(f->digits));
    f->digitLength = core.digitLength;
    TargetRegion(f, data, locale);
    f->stamp = zoneStamp;
    return digits;
}

bool mlocZoneFormatSound(const ZoneFormat* f)
{
    return f->stamp == zoneStamp && f->nameCount > 0 && f->nameCount <= zoneChainMost &&
           f->meta.bytes != nullptr && f->codes.bytes != nullptr && f->data != nullptr &&
           f->style > mloc_zoneStyleNone && f->style <= mloc_zoneStyleOffsetShort &&
           f->digitLength > 0 && f->digitLength <= numberDigitSize;
}

static_assert(sizeof(ZoneFormat) <= sizeof(mlocZoneFormatter), "a zone formatter fits its value");

mlocResult mlocInitZoneFormatter(mlocZoneFormatter* formatterOut, const mlocDataSet* data,
                                 const mlocLocale* locale, mlocZoneStyle style)
{
    if (formatterOut == nullptr || data == nullptr || locale == nullptr ||
        style <= mloc_zoneStyleNone || style > mloc_zoneStyleOffsetShort)
    {
        return mloc_errorInvalid;
    }
    ZoneFormat f;
    mlocResult result = mlocMakeZoneFormat(&f, data, locale, style);
    if (result < 0)
    {
        return result;
    }
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

mlocResult mlocFormatZone(const mlocZoneFormatter* formatter, const mlocTimeZone* zone,
                          const mlocInstant* instant, mlocOutput* output)
{
    ZoneFormat f;
    ZoneTables t;
    bool named = false;
    if (formatter == nullptr || zone == nullptr || instant == nullptr || output == nullptr ||
        (output->buffer == nullptr && output->capacity != 0) || !mlocValidInstant(instant) ||
        !mlocReadTimeZone(zone, &t, &named))
    {
        return mloc_errorInvalid;
    }
    memcpy(&f, formatter, sizeof(f));
    if (!mlocZoneFormatSound(&f))
    {
        return mloc_errorInvalid;
    }
    // Zone names units that disagree with the zone write nothing.
    ZoneAt a;
    bool read = mlocReadZoneAt(&f, zone, instant, &a);
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    if (read)
    {
        mlocWriteZone(&f, (mlocZoneStyle)f.style, &a, &w);
    }
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        if (read)
        {
            mlocWriteZone(&f, (mlocZoneStyle)f.style, &a, &parts);
        }
        mlocEndParts(&parts);
    }
    return mloc_success;
}
