// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the date, time and date-time formatters and the
// relative day with a time. The input picks a formatter and its options
// (in range or not), a locale (one of a few with calendar, hour cycle
// and region keywords, or the input's own text), two dates, two times,
// two instants, a zone and a day amount; the formatter is made over the
// compiled-in data and the dates, spell-out, zone names and relative
// data files, and writes each value, range and zoned value it takes. A
// call answers only as its documentation says, measures what it then
// writes in full, never writes past a short buffer, writes valid UTF-8,
// reports its parts inside the text in order of start, and writes the
// same text again.

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"
#include "maul-locale/decimal.h"
#include "maul-locale/locale.h"
#include "maul-locale/relative.h"
#include "maul-locale/zone.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static bool OpenFile(const char* path, uint8_t* bytes, size_t capacity, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    size_t size = fread(bytes, 1, capacity, file);
    fclose(file);
    return size < capacity && mlocOpenData(bytes, size, out) == mloc_success;
}

// The data files are read once into buffers with room to spare (8 MiB
// for zone-names.mloc's 2.9 MB, 4 MiB for the others, each at most
// 1.3 MB); a file that outgrows its buffer stops the run at the first
// input.
static const mlocDataSet* Data(void)
{
    static mlocData s_sources[5];
    alignas(8) static uint8_t s_dates[4 << 20];
    alignas(8) static uint8_t s_spellout[4 << 20];
    alignas(8) static uint8_t s_zones[8 << 20];
    alignas(8) static uint8_t s_relative[4 << 20];
    static mlocDataSet s_set;
    if (s_set.count == 0)
    {
        mlocOpenBuiltinData(&s_sources[0]);
        Require(OpenFile(MLOC_DATES_FILE, s_dates, sizeof(s_dates), &s_sources[1]) &&
                OpenFile(MLOC_SPELLOUT_FILE, s_spellout, sizeof(s_spellout), &s_sources[2]) &&
                OpenFile(MLOC_ZONE_NAMES_FILE, s_zones, sizeof(s_zones), &s_sources[3]) &&
                OpenFile(MLOC_RELATIVE_FILE, s_relative, sizeof(s_relative), &s_sources[4]));
        s_set = (mlocDataSet){s_sources, 5};
    }
    return &s_set;
}

static bool ValidUtf8(const uint8_t* text, size_t length)
{
    for (size_t at = 0; at < length;)
    {
        uint8_t b = text[at];
        size_t n = b < 0x80                ? 1
                   : b >= 0xC2 && b < 0xE0 ? 2
                   : b >= 0xE0 && b < 0xF0 ? 3
                   : b >= 0xF0 && b < 0xF5 ? 4
                                           : 0;
        if (n == 0 || at + n > length)
        {
            return false;
        }
        uint32_t cp = n == 1 ? b : (uint32_t)(b & (0xFF >> (n + 1)));
        for (size_t k = 1; k < n; k++)
        {
            if ((text[at + k] & 0xC0) != 0x80)
            {
                return false;
            }
            cp = cp << 6 | (text[at + k] & 0x3Fu);
        }
        static const uint32_t s_least[5] = {0, 0, 0x80, 0x800, 0x10000};
        if (cp < s_least[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        {
            return false;
        }
        at += n;
    }
    return true;
}

// The input read field by field, zeros past its end.
typedef struct Reader
{
    const uint8_t* data;
    size_t size;
    size_t at;
} Reader;

static uint64_t Take(Reader* r, int bytes)
{
    uint64_t value = 0;
    for (int k = 0; k < bytes; k++)
    {
        value |= (uint64_t)(r->at < r->size ? r->data[r->at] : 0) << (8 * k);
        r->at++;
    }
    return value;
}

typedef struct Values
{
    mlocDate dates[2];
    mlocTime times[2];
    mlocInstant instants[2];
    mlocTimeZone zone;
    mlocDecimal days;
} Values;

typedef struct Formatters
{
    mlocDateFormatter date;
    mlocTimeFormatter time;
    mlocDateTimeFormatter dateTime;
    mlocRelativeFormatter relative;
} Formatters;

typedef enum Call
{
    callDate,
    callDateInZone,
    callDateRange,
    callDateRangeInZone,
    callTime,
    callTimeInZone,
    callTimeRange,
    callTimeRangeInZone,
    callDateTime,
    callDateTimeInZone,
    callDateTimeRange,
    callDateTimeRangeInZone,
    callDayTime,
} Call;

static mlocResult Run(const Formatters* f, const Values* v, Call call, mlocOutput* output)
{
    mlocDateTime a = {v->dates[0], v->times[0]};
    mlocDateTime b = {v->dates[1], v->times[1]};
    const mlocTimeZone* zone = &v->zone;
    switch (call)
    {
    case callDate:
        return mlocDateFormatter_Format(&f->date, &v->dates[0], output);
    case callDateInZone:
        return mlocDateFormatter_FormatInZone(&f->date, zone, &v->instants[0], output);
    case callDateRange:
        return mlocDateFormatter_FormatRange(&f->date, &v->dates[0], &v->dates[1], output);
    case callDateRangeInZone:
        return mlocDateFormatter_FormatRangeInZone(&f->date, zone, &v->instants[0], &v->instants[1],
                                                   output);
    case callTime:
        return mlocTimeFormatter_Format(&f->time, &v->times[0], output);
    case callTimeInZone:
        return mlocTimeFormatter_FormatInZone(&f->time, zone, &v->instants[0], output);
    case callTimeRange:
        return mlocTimeFormatter_FormatRange(&f->time, &v->times[0], &v->times[1], output);
    case callTimeRangeInZone:
        return mlocTimeFormatter_FormatRangeInZone(&f->time, zone, &v->instants[0], &v->instants[1],
                                                   output);
    case callDateTime:
        return mlocDateTimeFormatter_Format(&f->dateTime, &a, output);
    case callDateTimeInZone:
        return mlocDateTimeFormatter_FormatInZone(&f->dateTime, zone, &v->instants[0], output);
    case callDateTimeRange:
        return mlocDateTimeFormatter_FormatRange(&f->dateTime, &a, &b, output);
    case callDateTimeRangeInZone:
        return mlocDateTimeFormatter_FormatRangeInZone(&f->dateTime, zone, &v->instants[0],
                                                       &v->instants[1], output);
    default:
        return mlocRelativeFormatter_FormatDayTime(&f->relative, &f->time, &v->days, &v->times[0],
                                                   output);
    }
}

typedef struct Parts
{
    size_t length;
    size_t lastStart;
    size_t count;
} Parts;

static void Part(void* context, mlocPart part, size_t start, size_t length)
{
    (void)part;
    Parts* p = context;
    Require(start <= p->length && length <= p->length - start && start >= p->lastStart);
    p->lastStart = start;
    p->count++;
}

// Measures, writes in full, again with parts, and into a buffer one byte
// short, with guard bytes after each.
static void Check(const Formatters* f, const Values* v, Call call)
{
    mlocOutput probe = {0};
    mlocResult measured = Run(f, v, call, &probe);
    Require(measured == mloc_errorCapacity || measured == mloc_errorInvalid ||
            measured == mloc_errorNotFound || measured == mloc_errorLimit ||
            measured == mloc_errorFormat);
    if (measured != mloc_errorCapacity)
    {
        return;
    }
    size_t length = probe.length;
    Require(length > 0 && length < 8192);
    static char s_text[8192 + 16];
    static char s_again[8192 + 16];
    memset(s_text, 0xA5, length + 16);
    mlocOutput out = {.buffer = s_text, .capacity = length};
    Require(Run(f, v, call, &out) == mloc_success && out.length == length);
    Require(ValidUtf8((const uint8_t*)s_text, length));
    for (size_t k = 0; k < 16; k++)
    {
        Require((uint8_t)s_text[length + k] == 0xA5);
    }
    Parts parts = {.length = length};
    mlocOutput again = {.buffer = s_again, .capacity = length, .part = Part, .context = &parts};
    Require(Run(f, v, call, &again) == mloc_success && again.length == length &&
            memcmp(s_again, s_text, length) == 0);
    memset(s_again, 0xA5, length + 16);
    mlocOutput shortOut = {.buffer = s_again, .capacity = length - 1};
    Require(Run(f, v, call, &shortOut) == mloc_errorCapacity && shortOut.length == length);
    for (size_t k = length - 1; k < length + 16; k++)
    {
        Require((uint8_t)s_again[k] == 0xA5);
    }
}

static void ReadValues(const mlocDataSet* set, Reader* r, Values* v)
{
    memset(v, 0, sizeof(*v));
    for (int i = 0; i < 2; i++)
    {
        // A day, time or zone the library refuses to make is left zeroed,
        // and the calls get it as they would any other value.
        int32_t day = (int32_t)(uint32_t)Take(r, 4);
        if (mlocMakeDateFromDay(mloc_calendarGregorian, day, &v->dates[i]) != mloc_success)
        {
            v->dates[i] = (mlocDate){0};
        }
        mlocTimeInput in = {.hour = (int64_t)Take(r, 1),
                            .minute = (int64_t)Take(r, 1),
                            .second = (int64_t)Take(r, 1),
                            .nanosecond = (int64_t)Take(r, 4)};
        if (mlocMakeTime(&in, mloc_overflowConstrain, &v->times[i]) != mloc_success)
        {
            v->times[i] = (mlocTime){0};
        }
        v->instants[i] = (mlocInstant){(int64_t)Take(r, 6) - (INT64_C(1) << 47),
                                       (int32_t)(Take(r, 4) % 1000000000)};
    }
    size_t zones = mlocGetTimeZoneCount(set);
    size_t which = (size_t)Take(r, 2);
    if (zones == 0 || mlocGetTimeZone(set, which % zones, &v->zone) != mloc_success)
    {
        v->zone = (mlocTimeZone){0};
    }
    Require(mlocMakeDecimal((int8_t)Take(r, 1), &v->days) == mloc_success);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static const char* const s_locales[] = {"en",
                                            "en-GB",
                                            "ja-u-ca-japanese",
                                            "ar-EG",
                                            "zh-Hant-TW",
                                            "he-u-ca-hebrew",
                                            "fa-u-ca-persian",
                                            "th-u-ca-buddhist",
                                            "ko-u-hc-h23",
                                            "de-u-rg-uszzzz",
                                            "zh-u-ca-chinese",
                                            "hi-IN-u-nu-deva",
                                            "am-u-ca-ethiopic",
                                            "en-u-hc-c12",
                                            "und",
                                            "sr-Latn"};
    const mlocDataSet* set = Data();
    Reader r = {data, size, 0};
    uint8_t which = (uint8_t)Take(&r, 1);
    uint8_t o[8];
    for (int k = 0; k < 8; k++)
    {
        o[k] = (uint8_t)Take(&r, 1);
    }
    Values v;
    ReadValues(set, &r, &v);
    // The locale: one of the list, or the rest of the input's text.
    mlocLocale locale;
    const char* id = s_locales[which % 16];
    size_t idLength = strlen(id);
    if ((which & 0x10) != 0 && r.at < size)
    {
        id = (const char*)data + r.at;
        idLength = size - r.at;
    }
    if (mlocCanonicalizeLocale(set, id, idLength, &locale, nullptr).status != mloc_success)
    {
        return 0;
    }
    // Options from their bytes: the low values in range, the high ones
    // past it.
    mlocDateOptions date = {.fields = (mlocDateFieldSet)(o[0] % 12),
                            .length = (mlocDateLength)(o[1] % 4),
                            .yearStyle = (mlocYearStyle)(o[1] / 4 % 4),
                            .style = (mlocDateStyle)(o[2] % 6),
                            .zone = (mlocZoneStyle)(o[2] / 6 % 10)};
    mlocTimeOptions time = {.hourCycle = (mlocHourCycle)(o[3] % 8),
                            .precision = (mlocTimePrecision)(o[4] % 14),
                            .style = (mlocTimeStyle)(o[5] % 6),
                            .zone = (mlocZoneStyle)(o[5] / 6 % 10)};
    mlocDateTimeOptions dateTime = {
        .date = date, .time = time, .glue = (mlocDateTimeGlue)(o[6] % 3)};
    mlocRelativeOptions relative = {.width = (mlocRelativeWidth)(o[7] % 4),
                                    .numeric = (mlocRelativeNumeric)(o[7] / 4 % 3)};
    Formatters f;
    memset(&f, 0, sizeof(f));
    mlocResult made[4] = {
        mlocInitDateFormatter(set, &locale, &date, &f.date),
        mlocInitTimeFormatter(set, &locale, &time, &f.time),
        mlocInitDateTimeFormatter(set, &locale, &dateTime, &f.dateTime),
        mlocInitRelativeFormatter(set, &locale, &relative, &f.relative),
    };
    for (int k = 0; k < 4; k++)
    {
        Require(made[k] >= 0 || made[k] == mloc_errorInvalid || made[k] == mloc_errorNotFound ||
                made[k] == mloc_errorFormat);
    }
    if (made[0] >= 0)
    {
        for (int c = callDate; c <= callDateRangeInZone; c++)
        {
            Check(&f, &v, (Call)c);
        }
    }
    if (made[1] >= 0)
    {
        for (int c = callTime; c <= callTimeRangeInZone; c++)
        {
            Check(&f, &v, (Call)c);
        }
    }
    if (made[2] >= 0)
    {
        for (int c = callDateTime; c <= callDateTimeRangeInZone; c++)
        {
            Check(&f, &v, (Call)c);
        }
    }
    if (made[1] >= 0 && made[3] >= 0)
    {
        Check(&f, &v, callDayTime);
    }
    return 0;
}
