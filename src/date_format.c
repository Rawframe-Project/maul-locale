// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date formatter (maul-locale/datetime.h): the locale's calendar from
// its preferences, the set of its options' patterns from dates.mloc, its
// digits and those its patterns name; a date read in that calendar and
// written by the pattern of the year style its year needs.

#include "date_format.h"

#include "bytes.h"
#include "calendar_arith.h"
#include "date_range.h"
#include "invariant.h"

#include "maul-locale/datetime.h"

#include <string.h>

enum
{
    // Marks a date formatter the library made.
    dateStamp = 0x44,
    dateStandard = 3,
    // The glue's styles, as the standard date formats' lengths.
    glueFull = 0,
    glueLong = 1,
    glueMedium = 2,
    glueShort = 3,
};

// A date formatter: its state, and with a zone the zone's, the atTime
// glue and the zone's pattern.
typedef struct ZonedDateFormat
{
    DateFormat date;
    ZoneFormat zone;
    const uint8_t* glue;
    uint32_t glueLength;
    const uint8_t* zonePattern;
    uint32_t zonePatternLength;
    // The calendar's time set, whose atTime glue of each length joins a
    // range's prefixed dates to the zone.
    const uint8_t* timeSet;
} ZonedDateFormat;

static_assert(sizeof(ZonedDateFormat) <= sizeof(mlocDateFormatter),
              "a date formatter fits its value");

// The set entry of each field set (mlocDateFieldSet).
static const uint8_t s_entries[] = {
    dateYearMonthDay, dateYearMonthDayWeekday, dateMonthDay, dateMonthDayWeekday,
    dateDay,          dateDayWeekday,          dateWeekday,  dateMonth,
    dateYear,         dateYearMonth,
};

bool mlocValidDateOptions(const mlocDateOptions* o)
{
    bool day = o->style == mloc_dateStyleNone && o->fields <= mloc_dateFieldSetWeekday;
    return (uint32_t)o->fields <= mloc_dateFieldSetYearMonth &&
           (uint32_t)o->length <= mloc_dateLengthShort &&
           (uint32_t)o->yearStyle <= mloc_yearStyleWithEra &&
           (uint32_t)o->style <= mloc_dateStyleShort &&
           (uint32_t)o->zone <= mloc_zoneStyleOffsetShort && (o->zone == mloc_zoneStyleNone || day);
}

uint32_t mlocDateGlueStyle(const mlocDateOptions* o, uint32_t flags)
{
    if (o->style != mloc_dateStyleNone)
    {
        return (uint32_t)o->style - 1;
    }
    mlocDateFieldSet s = o->fields;
    bool month = s == mloc_dateFieldSetYearMonthDay || s == mloc_dateFieldSetYearMonthDayWeekday ||
                 s == mloc_dateFieldSetMonthDay || s == mloc_dateFieldSetMonthDayWeekday;
    bool weekday = s == mloc_dateFieldSetYearMonthDayWeekday ||
                   s == mloc_dateFieldSetMonthDayWeekday || s == mloc_dateFieldSetDayWeekday;
    uint32_t code = flags >> (dateTimeMonthShift + 2 * (uint32_t)o->length) & 3;
    if (!month || code == 0)
    {
        return glueShort;
    }
    return code == 1 ? glueMedium : weekday ? glueFull : glueLong;
}

bool mlocValidGlue(const uint8_t* glue, uint32_t length)
{
    bool time = false;
    bool date = false;
    for (uint32_t at = 0; at + 3 <= length; at++)
    {
        time = time || memcmp(glue + at, "{0}", 3) == 0;
        date = date || memcmp(glue + at, "{1}", 3) == 0;
    }
    return time && date;
}

// The numbering system a pattern names for its fields, or none.
static bool OverrideOf(const uint8_t* pattern, uint32_t length, const char** id, size_t* idLength)
{
    for (uint32_t at = 0; at < length;)
    {
        if (pattern[at] == datePatternOverride)
        {
            *id = (const char*)pattern + at + 4;
            *idLength = pattern[at + 3];
            return true;
        }
        at += pattern[at] == datePatternField ? 3 : pattern[at] < 0x20 ? 4u + pattern[at + 3] : 1;
    }
    return false;
}

// The formatter's patterns: the standard format's, or the field set's at
// its length in the three year styles (one for a set without a year).
static bool FindPatterns(DateFormat* f, const DatePatterns* p, const uint8_t* set,
                         const mlocDateOptions* o)
{
    for (uint32_t i = 0; i < dateFormatPatterns; i++)
    {
        uint32_t index = 0;
        if (o->style != mloc_dateStyleNone)
        {
            index = mlocLoad16(set + 2 * (dateEntryCount + (size_t)o->style - 1));
        }
        else
        {
            uint32_t entry = s_entries[o->fields];
            bool year = entry >= dateYearMonthDay;
            uint32_t row = mlocLoad16(set + 2 * (size_t)(entry + (year ? i : 0)));
            // A set's entries are rows (mlocValidDatePatterns).
            MLOC_ASSERT(row < p->rowCount);
            index = mlocLoad16(p->rows + (size_t)row * dateRowSize + 2 * (size_t)o->length);
        }
        DataSpan pattern;
        if (!mlocDatePattern(p, index, &pattern) || !mlocWritableDate(pattern.bytes, pattern.size))
        {
            return false;
        }
        f->pattern[i] = pattern.bytes;
        f->patternLength[i] = pattern.size;
    }
    return true;
}

// A kept pattern's pair and prefix indexes (i, the year style for a
// field set with a year) in an interval date part: a standard format's,
// or its row's at its length.
static const uint8_t* IndexesOf(const DatePatterns* p, const uint8_t* part,
                                const mlocDateOptions* o, uint32_t i)
{
    if (o->style != mloc_dateStyleNone)
    {
        return part + 2 * dateEntryCount + 6 * (size_t)(o->style - 1);
    }
    uint32_t entry = s_entries[o->fields];
    bool year = entry >= dateYearMonthDay;
    uint32_t row = mlocLoad16(part + 2 * (size_t)(entry + (year ? i : 0)));
    // A date part's entries are interval rows (ValidIntervals).
    MLOC_ASSERT(row < p->intervalRowCount);
    return p->intervalRows + (size_t)row * dateIntervalRowSize + 4 * (size_t)o->length;
}

// The formatter's interval patterns: each kept pattern's pair, a standard
// format's own pattern, and the fallback.
static bool FindRange(DateFormat* f, const DatePatterns* p, const uint8_t* intervals,
                      const mlocDateOptions* o)
{
    DateRange* r = &f->range;
    r->table = (PatternTable){p->offsets, p->text, p->patternCount};
    // An interval set's date part, and the pairs and prefixes of date
    // parts and rows, are within their sections (ValidIntervals).
    MLOC_ASSERT(mlocLoad16(intervals) < p->datePartCount);
    const uint8_t* part = p->dateParts + (size_t)mlocLoad16(intervals) * dateIntervalDateSize;
    // The validator saw the fallback hold both values.
    DataSpan fallback;
    if (!mlocTablePattern(&r->table, mlocLoad16(intervals + 4), &fallback))
    {
        return false;
    }
    r->fallback = fallback.bytes;
    r->fallbackLength = fallback.size;
    if (o->style != mloc_dateStyleNone)
    {
        DataSpan own;
        size_t at = 2 * dateEntryCount + 6 * (size_t)(o->style - 1) + 4;
        if (!mlocTablePattern(&r->table, mlocLoad16(part + at), &own))
        {
            return false;
        }
        r->own = own.bytes;
        r->ownLength = own.size;
    }
    for (uint32_t i = 0; i < dateFormatPatterns; i++)
    {
        const uint8_t* at = IndexesOf(p, part, o, i);
        MLOC_ASSERT(mlocLoad16(at) < p->pairCount && mlocLoad16(at + 2) < p->prefixCount);
        r->pairs[i] = p->pairs + (size_t)mlocLoad16(at) * dateIntervalPairSize;
        r->prefixes[i] = p->prefixes + (size_t)mlocLoad16(at + 2) * dateIntervalPrefixSize;
    }
    return true;
}

// The digits of the system the patterns name, or its spell-out rules.
static mlocResult FindOverride(DateFormat* f, const mlocDataSet* data, const mlocLocale* locale)
{
    for (uint32_t i = 0; i < dateFormatPatterns; i++)
    {
        const char* id = nullptr;
        size_t length = 0;
        if (!OverrideOf(f->pattern[i], f->patternLength[i], &id, &length))
        {
            continue;
        }
        if (mlocNumberSystemDigits(data, id, length, f->overrideDigits, &f->overrideDigitLength))
        {
            return mloc_success;
        }
        mlocResult result =
            mlocInitNumberingSystemFormatter(data, locale, id, length, nullptr, &f->spellout);
        if (result < 0)
        {
            return mloc_errorNotFound;
        }
        f->hasSpellout = 1;
        return mloc_success;
    }
    return mloc_success;
}

mlocResult mlocReadDateLocale(DateLocale* l, const mlocDataSet* data, const mlocLocale* locale)
{
    if (mlocGetLocalePreferences(data, locale, &l->preferences) != mloc_success)
    {
        return mloc_errorNotFound;
    }
    l->digits = mlocInitNumberCore(&l->core, data, locale, mloc_numberDecimal);
    uint32_t calendar = l->preferences.calendar;
    l->calendar = calendar == mloc_calendarIso8601 ? mloc_calendarGregorian : calendar;
    if (l->digits < 0 || !mlocFindDateUnits(data, locale, &l->units) ||
        !mlocDateNamesOf(&l->units, l->calendar, &l->nameSet) ||
        !mlocDateNamesOf(&l->units, mloc_calendarGregorian, &l->gregorianSet))
    {
        return mloc_errorNotFound;
    }
    return l->digits;
}

static mlocResult Resolve(DateFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                          const DateLocale* l, const mlocDateOptions* o)
{
    const DatePatterns* patterns = &l->units.patterns;
    const uint8_t* set = nullptr;
    const uint8_t* intervals = nullptr;
    if (!mlocDateSetOf(&l->units, l->calendar, &set, &intervals))
    {
        return mloc_errorNotFound;
    }
    if (!FindPatterns(f, patterns, set, o) || !FindRange(f, patterns, intervals, o))
    {
        return mloc_errorFormat;
    }
    f->names = l->units.names;
    f->nameSet = l->nameSet;
    f->gregorianSet = l->gregorianSet;
    memcpy(f->digits, l->core.digits, sizeof(f->digits));
    f->digitLength = l->core.digitLength;
    uint32_t minus = l->core.symbolLength[numberMinus];
    f->minusLength = (uint8_t)(minus <= sizeof(f->minus) ? minus : 0);
    memcpy(f->minus, l->core.symbolText + l->core.symbolStart[numberMinus], f->minusLength);
    f->calendar = (uint8_t)l->calendar;
    f->firstDay = l->preferences.firstDay;
    f->minimalDays = l->preferences.minimalDays;
    f->yearStyle = o->style != mloc_dateStyleNone ? dateStandard : (uint8_t)o->yearStyle;
    mlocResult override = FindOverride(f, data, locale);
    return override < 0 ? override : l->digits;
}

mlocResult mlocMakeDateFormat(DateFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                              const DateLocale* shared, const mlocDateOptions* o)
{
    static_assert(sizeof(s_entries) == mloc_dateFieldSetYearMonth + 1, "an entry per field set");
    memset(f, 0, sizeof(*f));
    DateLocale own;
    mlocResult result = shared != nullptr ? shared->digits : mlocReadDateLocale(&own, data, locale);
    result = result < 0 ? result : Resolve(f, data, locale, shared != nullptr ? shared : &own, o);
    f->stamp = result < 0 ? 0 : dateStamp;
    return result;
}

// Whether a pattern holds literal text and zone fields only.
static bool ZoneOnly(const uint8_t* pattern, uint32_t length)
{
    for (uint32_t at = 0; at < length;)
    {
        if (pattern[at] >= 0x20)
        {
            at++;
            continue;
        }
        if (pattern[at] != datePatternField ||
            mlocZoneFieldStyle(pattern[at + 1], pattern[at + 2]) == mloc_zoneStyleNone)
        {
            return false;
        }
        at += 3;
    }
    return true;
}

// The zone of a date formatter: its names, the calendar's atTime glue of
// the date's length and the zone's pattern alone, as ICU4C composes a
// date with a zone.
static mlocResult ResolveZone(ZonedDateFormat* f, const mlocDataSet* data, const mlocLocale* locale,
                              const mlocDateOptions* o)
{
    DatePatterns patterns;
    const uint8_t* set = nullptr;
    DataSpan glue = {0};
    DataSpan zone;
    if (!mlocFindTimeSet(data, locale, f->date.calendar, &patterns, &set, nullptr))
    {
        return mloc_errorNotFound;
    }
    // Every length's glue checked: the formatter writes its own length's.
    uint32_t style = mlocDateGlueStyle(o, mlocLoad16(set + 2 * dateTimeFlags));
    for (uint32_t length = 0; length < dateStandardCount; length++)
    {
        DataSpan other;
        if (!mlocDatePattern(&patterns, mlocLoad16(set + 2 * (dateTimeAtGlue + length)), &other) ||
            !mlocValidGlue(other.bytes, other.size))
        {
            return mloc_errorFormat;
        }
        if (length == style)
        {
            glue = other;
        }
    }
    uint32_t form = (uint32_t)o->zone - 1;
    if (!mlocDatePattern(&patterns, mlocLoad16(set + 2 * (dateTimeZoneAlone + form)), &zone) ||
        !ZoneOnly(zone.bytes, zone.size))
    {
        return mloc_errorFormat;
    }
    f->glue = glue.bytes;
    f->glueLength = glue.size;
    f->zonePattern = zone.bytes;
    f->zonePatternLength = zone.size;
    f->timeSet = set;
    return mlocMakeZoneFormat(&f->zone, data, locale, o->zone);
}

mlocResult mlocInitDateFormatter(const mlocDataSet* data, const mlocLocale* locale,
                                 const mlocDateOptions* options, mlocDateFormatter* formatterOut)
{
    mlocDateOptions o = options != nullptr ? *options : (mlocDateOptions){0};
    if (formatterOut == nullptr || data == nullptr || locale == nullptr ||
        !mlocValidDateOptions(&o))
    {
        return mloc_errorInvalid;
    }
    ZonedDateFormat f;
    memset(&f, 0, sizeof(f));
    mlocResult result = mlocMakeDateFormat(&f.date, data, locale, nullptr, &o);
    if (result >= 0 && o.zone != mloc_zoneStyleNone)
    {
        // The zone reads the locale's digits as the date does: one result.
        result = ResolveZone(&f, data, locale, &o);
    }
    if (result < 0)
    {
        return result;
    }
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

// Whether a year needs its era shown beside it: a year of an era before
// the calendar's main one (before the common era, before the Republic of
// China, before the Hijra, before the Incarnation), or a Japanese date
// before Meiji.
static bool NeedsEra(uint32_t calendar, const mlocDateFields* fields)
{
    static const char* const s_main[dateCalendarCount] = {"ce", "ce", "be",    "roc", "",   "am",
                                                          "am", "aa", "shaka", "ap",  "am", "ah",
                                                          "ah", "ah", "ah",    "ah",  "",   ""};
    if (calendar == mloc_calendarJapanese)
    {
        return fields->eraLength == 2 || fields->eraLength == 3;
    }
    const char* main = s_main[calendar];
    return fields->eraLength != 0 &&
           (strlen(main) != fields->eraLength || memcmp(main, fields->era, fields->eraLength) != 0);
}

// The pattern for a date by the formatter's year style. A year of a
// calendar counted from the Gregorian (Buddhist, ROC, Japanese) is clear
// in its pattern's digits when its Gregorian year is from 1950 to 2049,
// as CLDR's datetime conformance data has it; any other year needs its
// century, and one before its calendar's main era or a Gregorian year
// before 1000 its era too.
static uint32_t PatternFor(const DateFormat* f, const mlocDateFields* fields, int32_t gregorianYear)
{
    if (f->yearStyle == dateStandard)
    {
        return 0;
    }
    uint32_t c = f->calendar;
    bool counted = c == mloc_calendarGregorian || c == mloc_calendarBuddhist ||
                   c == mloc_calendarRoc || c == mloc_calendarJapanese;
    bool era = NeedsEra(c, fields) || (c == mloc_calendarGregorian && fields->eraYear < 1000);
    if (f->yearStyle == mloc_yearStyleWithEra || era)
    {
        return dateStyleEra;
    }
    bool clear = counted && gregorianYear >= 1950 && gregorianYear <= 2049;
    return f->yearStyle == mloc_yearStyleAuto && clear ? 0 : dateStyleFull;
}

// The Gregorian year of a day of a valid date.
static int32_t GregorianYear(int32_t day)
{
    return mlocArithmeticFromDay(mloc_calendarGregorian, day).year;
}

bool mlocDateFormatSound(const DateFormat* f)
{
    return f->stamp == dateStamp && f->calendar < dateCalendarCount && f->pattern[0] != nullptr &&
           f->pattern[1] != nullptr && f->pattern[2] != nullptr && f->range.pairs[0] != nullptr &&
           f->range.pairs[1] != nullptr && f->range.pairs[2] != nullptr &&
           f->range.fallback != nullptr && f->digitLength > 0 &&
           f->digitLength <= numberDigitSize && f->overrideDigitLength <= numberDigitSize &&
           f->minusLength <= sizeof(f->minus);
}

bool mlocReadDateValue(const DateFormat* f, const mlocDate* date, DateValue* value)
{
    mlocDate in;
    if (mlocMakeDateFromDay((mlocCalendar)f->calendar, date->day, &in) != mloc_success ||
        mlocGetDateFields(&in, &value->fields) != mloc_success)
    {
        return false;
    }
    // The Gregorian year the date's year begins in, and its own: the
    // date's year in the Gregorian calendar.
    bool gregorian = f->calendar == mloc_calendarGregorian;
    value->related =
        gregorian ? value->fields.year : GregorianYear(in.day - value->fields.dayOfYear + 1);
    value->pattern =
        PatternFor(f, &value->fields, gregorian ? value->fields.year : GregorianYear(in.day));
    return true;
}

// A formatter the library made, copied out, with a zone or without as
// asked; false for any other.
static bool ReadFormatter(const mlocDateFormatter* opaque, bool zoned, ZonedDateFormat* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    bool zone = f->zonePattern != nullptr;
    return mlocDateFormatSound(&f->date) && zone == zoned &&
           (!zoned || (mlocZoneFormatSound(&f->zone) && f->glue != nullptr));
}

typedef struct DateWriting
{
    const ZonedDateFormat* f;
    const DateValue* value;
    // The zone read at the instant, or null for a date without one.
    const ZoneAt* zone;
    // A range's end and what it writes; null for one date.
    const DateValue* end;
    const RangeChoice* range;
} DateWriting;

// The glue's slots: the date, and the zone by its pattern.
static void WriteSlot(void* context, uint32_t slot, NumberWriter* w)
{
    const DateWriting* g = context;
    const DateFormat* d = &g->f->date;
    uint32_t p = g->value->pattern;
    if (slot == 1)
    {
        mlocWriteDateFields(d, d->pattern[p], d->patternLength[p], &g->value->fields,
                            g->value->related, w);
        return;
    }
    PatternSpan none = {0, 0, 0};
    mlocWriteZonePattern(&g->f->zone, g->f->zonePattern, g->f->zonePatternLength, g->zone, &none,
                         w);
}

static void Write(const DateWriting* g, NumberWriter* w)
{
    if (g->range != nullptr)
    {
        mlocWriteRange(g->range, w);
        return;
    }
    if (g->zone != nullptr)
    {
        mlocWriteGlue(g->f->glue, g->f->glueLength, WriteSlot, (void*)g, w);
        return;
    }
    WriteSlot((void*)g, 1, w);
}

static mlocResult WriteOut(const DateWriting* g, mlocOutput* output)
{
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    Write(g, &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        Write(g, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}

mlocResult mlocDateFormatter_Format(const mlocDateFormatter* formatter, const mlocDate* date,
                                    mlocOutput* textOut)
{
    ZonedDateFormat f;
    DateValue v;
    if (!ReadFormatter(formatter, false, &f) || date == nullptr || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocReadDateValue(&f.date, date, &v))
    {
        return mloc_errorInvalid;
    }
    DateWriting g = {&f, &v, nullptr, nullptr, nullptr};
    return WriteOut(&g, textOut);
}

mlocResult mlocDateFormatter_FormatInZone(const mlocDateFormatter* formatter,
                                          const mlocTimeZone* zone, const mlocInstant* instant,
                                          mlocOutput* textOut)
{
    ZonedDateFormat f;
    DateValue v;
    ZoneAt at;
    mlocDateTime local;
    if (!ReadFormatter(formatter, true, &f) || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocReadZonedLocal(&f.zone, zone, instant, &at, &local) ||
        !mlocReadDateValue(&f.date, &local.date, &v))
    {
        return mloc_errorInvalid;
    }
    DateWriting g = {&f, &v, &at, nullptr, nullptr};
    return WriteOut(&g, textOut);
}

// A range's value by a pattern.
static void WriteRangeValue(const void* context, uint32_t value, const uint8_t* pattern,
                            uint32_t length, const PatternSpan* span, NumberWriter* w)
{
    const DateWriting* g = context;
    const DateValue* v = value == 0 ? g->value : g->end;
    mlocWriteDateFieldsIn(&g->f->date, pattern, length, &v->fields, v->related, span, w);
}

mlocResult mlocDateFormatter_FormatRange(const mlocDateFormatter* formatter, const mlocDate* start,
                                         const mlocDate* end, mlocOutput* textOut)
{
    ZonedDateFormat f;
    DateValue v[2];
    if (!ReadFormatter(formatter, false, &f) || start == nullptr || end == nullptr ||
        textOut == nullptr || (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocReadDateValue(&f.date, start, &v[0]) || !mlocReadDateValue(&f.date, end, &v[1]))
    {
        return mloc_errorInvalid;
    }
    // One pattern writes both: the year style the larger year needs.
    uint32_t p = v[0].pattern > v[1].pattern ? v[0].pattern : v[1].pattern;
    const DateRange* r = &f.date.range;
    DateWriting g = {&f, &v[0], nullptr, &v[1], nullptr};
    RangeChoice c = {
        .write = WriteRangeValue,
        .context = &g,
        .table = &r->table,
        .pair = r->pairs[p],
        .difference = mlocDateDifference(&v[0].fields, &v[1].fields),
        .own = r->own != nullptr ? (DataSpan){r->own, r->ownLength}
                                 : (DataSpan){f.date.pattern[p], f.date.patternLength[p]},
        .fallback = {r->fallback, r->fallbackLength},
    };
    g.range = &c;
    return WriteOut(&g, textOut);
}

// A range in a zone: the formatter, both dates, both instants and the zone.
typedef struct ZonedRangeWriting
{
    const ZonedDateFormat* f;
    const DateValue* dates[2];
    const ZoneAt* zones[2];
} ZonedRangeWriting;

static void WriteZonedDate(const void* context, uint32_t value, const uint8_t* pattern,
                           uint32_t length, const PatternSpan* span, NumberWriter* w)
{
    const ZonedRangeWriting* g = context;
    const DateValue* v = g->dates[value];
    mlocWriteDateFieldsIn(&g->f->date, pattern, length, &v->fields, v->related, span, w);
}

static void WriteZonedZone(const void* context, uint32_t value, const uint8_t* pattern,
                           uint32_t length, const PatternSpan* span, NumberWriter* w)
{
    const ZonedRangeWriting* g = context;
    mlocWriteZonePattern(&g->f->zone, pattern, length, g->zones[value], span, w);
}

typedef struct JoinedOut
{
    const RangeJoin* join;
    DataSpan fallback;
} JoinedOut;

static void WriteJoinedOut(const JoinedOut* g, NumberWriter* w)
{
    mlocWriteJoinedFallback(g->join, g->fallback, w);
}

static mlocResult WriteJoinedRange(const RangeJoin* join, DataSpan fallback, mlocOutput* output)
{
    JoinedOut g = {join, fallback};
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    WriteJoinedOut(&g, &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        WriteJoinedOut(&g, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}

mlocResult mlocDateFormatter_FormatRangeInZone(const mlocDateFormatter* formatter,
                                               const mlocTimeZone* zone, const mlocInstant* start,
                                               const mlocInstant* end, mlocOutput* textOut)
{
    ZonedDateFormat f;
    DateValue v[2];
    ZoneAt at[2];
    mlocDateTime local[2];
    if (!ReadFormatter(formatter, true, &f) || textOut == nullptr ||
        (textOut->buffer == nullptr && textOut->capacity != 0) ||
        !mlocReadZonedLocal(&f.zone, zone, start, &at[0], &local[0]) ||
        !mlocReadZonedLocal(&f.zone, zone, end, &at[1], &local[1]) ||
        !mlocReadDateValue(&f.date, &local[0].date, &v[0]) ||
        !mlocReadDateValue(&f.date, &local[1].date, &v[1]))
    {
        return mloc_errorInvalid;
    }
    if (local[0].date.day == local[1].date.day)
    {
        // One day: the start's date and zone, as ICU writes a range the
        // pattern shows no difference of.
        DateWriting one = {&f, &v[0], &at[0], nullptr, nullptr};
        return WriteOut(&one, textOut);
    }
    uint32_t p = v[0].pattern > v[1].pattern ? v[0].pattern : v[1].pattern;
    const DateRange* r = &f.date.range;
    ZonedRangeWriting g = {&f, {&v[0], &v[1]}, {&at[0], &at[1]}};
    RangeJoin join = {.date = {f.date.pattern[p], f.date.patternLength[p]},
                      .glue = {f.glue, f.glueLength},
                      .time = {f.zonePattern, f.zonePatternLength},
                      .writeDate = WriteZonedDate,
                      .writeTime = WriteZonedZone,
                      .context = &g};
    join.date = r->own != nullptr ? (DataSpan){r->own, r->ownLength} : join.date;
    uint32_t style = 0;
    DataSpan prefixed;
    if (mlocRangePrefix(&r->table, r->prefixes[p], mlocDateDifference(&v[0].fields, &v[1].fields),
                        &prefixed, &style))
    {
        join.date = prefixed;
        if (!mlocTablePattern(&r->table, mlocLoad16(f.timeSet + 2 * (dateTimeAtGlue + style)),
                              &join.glue))
        {
            return mloc_errorFormat;
        }
    }
    return WriteJoinedRange(&join, (DataSpan){r->fallback, r->fallbackLength}, textOut);
}
