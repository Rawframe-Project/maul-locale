// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A negative value of every signed enumeration a caller gives the
// library (mlocResult's errors are negative, so only its name is
// checked): each parameter, each field of an options or argument struct,
// the duration options' arrays and the genders mlocGetListGender reads.
// Each call is made once with a valid value, which must succeed, then
// with -1 and INT32_MIN, which must be refused with mloc_errorInvalid:
// a validator that compares a value as signed would let them through.

#include "test_harness.h"

#include "maul-locale/calendar.h"
#include "maul-locale/collate.h"
#include "maul-locale/currency.h"
#include "maul-locale/datetime.h"
#include "maul-locale/decimal.h"
#include "maul-locale/duration.h"
#include "maul-locale/grammar.h"
#include "maul-locale/list.h"
#include "maul-locale/message.h"
#include "maul-locale/names.h"
#include "maul-locale/number.h"
#include "maul-locale/plural.h"
#include "maul-locale/relative.h"
#include "maul-locale/search.h"
#include "maul-locale/segment.h"
#include "maul-locale/spellout.h"
#include "maul-locale/unit.h"
#include "maul-locale/zone.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char* const s_files[] = {
    "collation.mloc", "currency-names.mloc", "dates.mloc", "names.mloc",      "relative.mloc",
    "segments.mloc",  "spellout.mloc",       "units.mloc", "zone-names.mloc",
};
enum
{
    fileCount = sizeof(s_files) / sizeof(s_files[0]),
};

static mlocData s_sources[fileCount + 1];
static mlocDataSet s_data;
static mlocLocale s_en;
static mlocLocale s_tr;
static mlocLocale s_ko;
static mlocTimeZone s_zone;
static mlocDate s_date;
static mlocTime s_time;
static mlocDateTime s_dateTime;
static mlocDecimal s_one;
static mlocTimeDuration s_hour;
static mlocCollator s_collator;
static mlocNumberFormatter s_number;
static mlocCurrencyFormatter s_currency;
static mlocRelativeFormatter s_relative;
static mlocPluralRules s_plural;
static mlocInflector s_turkish;
static mlocInflector s_korean;
static uint8_t s_bound[2048];
static const mlocInstant s_epoch = {0, 0};
static const mlocDateInput s_day = {.year = 2026, .month = 10, .day = 31};
static char s_text[512];

static mlocOutput Text(void)
{
    return (mlocOutput){.buffer = s_text, .capacity = sizeof(s_text)};
}

// A call made with an enumeration's value.
typedef mlocResult (*Call)(int32_t value);

typedef struct Parameter
{
    const char* name;
    Call call;
    int32_t valid;
} Parameter;

static mlocResult MakeDateCalendar(int32_t v)
{
    mlocDate date;
    return mlocMakeDate((mlocCalendar)v, &s_day, mloc_overflowReject, &date);
}

static mlocResult MakeDateOverflow(int32_t v)
{
    mlocDate date;
    return mlocMakeDate(mloc_calendarGregorian, &s_day, (mlocOverflow)v, &date);
}

static mlocResult MakeDateFromDayCalendar(int32_t v)
{
    mlocDate date;
    return mlocMakeDateFromDay((mlocCalendar)v, 739000, &date);
}

static mlocResult AddToDateOverflow(int32_t v)
{
    mlocDateDuration days = {.days = 1};
    mlocDate date;
    return mlocAddToDate(&s_date, &days, (mlocOverflow)v, &date);
}

static mlocResult DateUntilUnit(int32_t v)
{
    mlocDateDuration duration;
    return mlocDateUntil(&s_date, &s_date, (mlocDateUnit)v, &duration);
}

static mlocResult WithDateOverflow(int32_t v)
{
    mlocDateChanges changes = {.given = mloc_givenDay, .day = 1};
    mlocDate date;
    return mlocWithDate(&s_date, &changes, (mlocOverflow)v, &date);
}

static const mlocDateChanges s_monthDay = {.given = mloc_givenMonthCode | mloc_givenDay,
                                           .monthCode = "M10",
                                           .monthCodeLength = 3,
                                           .day = 31};

static mlocResult MakeMonthDayCalendar(int32_t v)
{
    mlocDate date;
    return mlocMakeMonthDay((mlocCalendar)v, &s_monthDay, mloc_overflowReject, &date);
}

static mlocResult MakeMonthDayOverflow(int32_t v)
{
    mlocDate date;
    return mlocMakeMonthDay(mloc_calendarGregorian, &s_monthDay, (mlocOverflow)v, &date);
}

static mlocResult CurrencyParseMode(int32_t v)
{
    mlocDecimal decimal;
    char code[3];
    size_t end;
    return mlocCurrencyFormatter_Parse(&s_currency, "$1.00", 5, (mlocParseMode)v, 0, &decimal, code,
                                       &end);
}

static mlocResult CurrencySymbolDisplay(int32_t v)
{
    mlocOutput output = Text();
    return mlocGetCurrencySymbol(&s_data, &s_en, "USD", 3, (mlocCurrencyDisplay)v, &output);
}

static mlocResult CurrencyDigitsUsage(int32_t v)
{
    uint8_t digits;
    uint16_t increment;
    return mlocGetCurrencyDigits(&s_data, "USD", 3, (mlocCurrencyUsage)v, &digits, &increment);
}

static mlocResult ZoneFormatterStyle(int32_t v)
{
    mlocZoneFormatter formatter;
    return mlocInitZoneFormatter(&s_data, &s_en, (mlocZoneStyle)v, &formatter);
}

static mlocResult RoundDecimalMode(int32_t v)
{
    mlocDecimal decimal = s_one;
    return mlocRoundDecimal(&decimal, 0, 1, (mlocRoundingMode)v);
}

static mlocResult NameType(int32_t v)
{
    mlocOutput output = Text();
    return mlocGetName(&s_data, &s_en, (mlocNameType)v, "fr", 2, nullptr, &output);
}

static mlocResult NumberParseMode(int32_t v)
{
    mlocDecimal decimal;
    size_t end;
    return mlocNumberFormatter_Parse(&s_number, "1", 1, (mlocParseMode)v, &decimal, &end);
}

static mlocResult PluralType(int32_t v)
{
    mlocPluralRules rules;
    return mlocInitPluralRules(&s_data, &s_en, (mlocPluralType)v, &rules);
}

static mlocResult PluralRangeStart(int32_t v)
{
    mlocPluralCategory category;
    return mlocPluralRules_SelectRange(&s_plural, (mlocPluralCategory)v, mloc_pluralOther,
                                       &category);
}

static mlocResult PluralRangeEnd(int32_t v)
{
    mlocPluralCategory category;
    return mlocPluralRules_SelectRange(&s_plural, mloc_pluralOne, (mlocPluralCategory)v, &category);
}

static mlocResult RelativeUnit(int32_t v)
{
    mlocOutput output = Text();
    return mlocRelativeFormatter_Format(&s_relative, &s_one, (mlocRelativeUnit)v, &output);
}

static mlocResult SegmentKind(int32_t v)
{
    mlocSegmenter segmenter;
    return mlocInitSegmenter(&s_data, &s_en, (mlocSegmentKind)v, "a b", 3, false, &segmenter);
}

static mlocResult UnitNameWidth(int32_t v)
{
    mlocOutput output = Text();
    return mlocGetUnitDisplayName(&s_data, &s_en, "meter", 5, (mlocUnitWidth)v, &output);
}

static mlocResult TransitionDirection(int32_t v)
{
    mlocInstant transition;
    return mlocGetTimeZoneTransition(&s_zone, &s_epoch, (mlocTransitionDirection)v, &transition);
}

static mlocResult MakeTimeOverflow(int32_t v)
{
    mlocTimeInput noon = {.hour = 12};
    mlocTime time;
    return mlocMakeTime(&noon, (mlocOverflow)v, &time);
}

static mlocResult BalanceUnit(int32_t v)
{
    mlocTimeDurationFields fields;
    return mlocBalanceTimeDuration(&s_hour, (mlocTimeUnit)v, &fields);
}

static mlocResult RoundTimeDurationUnit(int32_t v)
{
    mlocTimeDuration duration;
    return mlocRoundTimeDuration(&s_hour, (mlocTimeUnit)v, 1, mloc_roundHalfExpand, &duration);
}

static mlocResult RoundTimeDurationMode(int32_t v)
{
    mlocTimeDuration duration;
    return mlocRoundTimeDuration(&s_hour, mloc_timeUnitHour, 1, (mlocRoundingMode)v, &duration);
}

static mlocResult TotalTimeDurationUnit(int32_t v)
{
    double total;
    return mlocTotalTimeDuration(&s_hour, (mlocTimeUnit)v, &total);
}

static mlocResult RoundTimeUnit(int32_t v)
{
    mlocTime time;
    int64_t days;
    return mlocRoundTime(&s_time, (mlocTimeUnit)v, 1, mloc_roundHalfExpand, &time, &days);
}

static mlocResult RoundTimeMode(int32_t v)
{
    mlocTime time;
    int64_t days;
    return mlocRoundTime(&s_time, mloc_timeUnitHour, 1, (mlocRoundingMode)v, &time, &days);
}

static mlocResult DurationValuesUnit(int32_t v)
{
    mlocDateTimeDuration duration = {.time = s_hour};
    mlocDurationValues values;
    return mlocGetDurationValues(&duration, (mlocUnit)v, &values);
}

static mlocResult AddToDateTimeOverflow(int32_t v)
{
    mlocDateTimeDuration duration = {.date = {.days = 1}};
    mlocDateTime dateTime;
    return mlocAddToDateTime(&s_dateTime, &duration, (mlocOverflow)v, &dateTime);
}

static mlocResult RoundDateTimeUnit(int32_t v)
{
    mlocDateTime dateTime;
    return mlocRoundDateTime(&s_dateTime, (mlocTimeUnit)v, 1, mloc_roundHalfExpand, &dateTime);
}

static mlocResult RoundDateTimeMode(int32_t v)
{
    mlocDateTime dateTime;
    return mlocRoundDateTime(&s_dateTime, mloc_timeUnitHour, 1, (mlocRoundingMode)v, &dateTime);
}

static mlocResult DateTimeTotalUnit(int32_t v)
{
    double total;
    return mlocDateTimeTotal(&s_dateTime, &s_dateTime, (mlocUnit)v, &total);
}

static mlocResult RoundInstantUnit(int32_t v)
{
    mlocInstant instant;
    return mlocRoundInstant(&s_epoch, (mlocTimeUnit)v, 1, mloc_roundHalfExpand, &instant);
}

static mlocResult RoundInstantMode(int32_t v)
{
    mlocInstant instant;
    return mlocRoundInstant(&s_epoch, mloc_timeUnitHour, 1, (mlocRoundingMode)v, &instant);
}

static mlocResult InstantForDisambiguation(int32_t v)
{
    mlocInstant instant;
    return mlocGetInstantFor(&s_zone, &s_dateTime, (mlocDisambiguation)v, &instant);
}

static mlocResult DateTimeForCalendar(int32_t v)
{
    mlocDateTime dateTime;
    return mlocGetDateTimeFor(&s_zone, &s_epoch, (mlocCalendar)v, &dateTime);
}

static mlocResult InstantForOffsetOption(int32_t v)
{
    mlocInstant instant;
    return mlocGetInstantForOffset(&s_zone, &s_dateTime, 0, (mlocOffsetOption)v,
                                   mloc_disambiguationCompatible, false, &instant);
}

static mlocResult InstantForOffsetDisambiguation(int32_t v)
{
    mlocInstant instant;
    return mlocGetInstantForOffset(&s_zone, &s_dateTime, 0, mloc_offsetIgnore,
                                   (mlocDisambiguation)v, false, &instant);
}

static mlocResult AddToZonedCalendar(int32_t v)
{
    mlocDateTimeDuration duration = {.date = {.days = 1}};
    mlocInstant instant;
    return mlocAddToZonedDateTime(&s_zone, (mlocCalendar)v, &s_epoch, &duration,
                                  mloc_overflowReject, &instant);
}

static mlocResult AddToZonedOverflow(int32_t v)
{
    mlocDateTimeDuration duration = {.date = {.days = 1}};
    mlocInstant instant;
    return mlocAddToZonedDateTime(&s_zone, mloc_calendarGregorian, &s_epoch, &duration,
                                  (mlocOverflow)v, &instant);
}

static mlocResult ZonedUntilCalendar(int32_t v)
{
    mlocDateTimeRounding rounding = {mloc_unitDay, mloc_unitNanosecond, 1, mloc_roundTrunc};
    mlocDateTimeDuration duration;
    return mlocZonedDateTimeUntil(&s_zone, (mlocCalendar)v, &s_epoch, &s_epoch, &rounding,
                                  &duration);
}

static mlocResult ZonedTotalCalendar(int32_t v)
{
    double total;
    return mlocZonedDateTimeTotal(&s_zone, (mlocCalendar)v, &s_epoch, &s_epoch, mloc_unitDay,
                                  &total);
}

static mlocResult ZonedTotalUnit(int32_t v)
{
    double total;
    return mlocZonedDateTimeTotal(&s_zone, mloc_calendarGregorian, &s_epoch, &s_epoch, (mlocUnit)v,
                                  &total);
}

static mlocResult RoundZonedUnit(int32_t v)
{
    mlocInstant instant;
    return mlocRoundZonedDateTime(&s_zone, &s_epoch, (mlocTimeUnit)v, 1, mloc_roundHalfExpand,
                                  &instant);
}

static mlocResult RoundZonedMode(int32_t v)
{
    mlocInstant instant;
    return mlocRoundZonedDateTime(&s_zone, &s_epoch, mloc_timeUnitHour, 1, (mlocRoundingMode)v,
                                  &instant);
}

static mlocResult ListGender(int32_t v)
{
    mlocPersonGender genders[2] = {mloc_personMale, (mlocPersonGender)v};
    mlocPersonGender gender;
    return mlocGetListGender(&s_data, &s_en, genders, 2, &gender);
}

static mlocResult MessageValueKind(int32_t v)
{
    mlocMessageArgument argument = {"x", 1, {(mlocMessageValueKind)v, 0, {.integer = 1}}};
    mlocOutput output = Text();
    return mlocFormatMessage(s_bound, &argument, 1, &output, nullptr);
}

#define PARAMETER(call, valid) {#call, call, valid}

static const Parameter s_parameters[] = {
    PARAMETER(MakeDateCalendar, mloc_calendarGregorian),
    PARAMETER(MakeDateOverflow, mloc_overflowReject),
    PARAMETER(MakeDateFromDayCalendar, mloc_calendarGregorian),
    PARAMETER(AddToDateOverflow, mloc_overflowReject),
    PARAMETER(DateUntilUnit, mloc_dateUnitDay),
    PARAMETER(WithDateOverflow, mloc_overflowReject),
    PARAMETER(MakeMonthDayCalendar, mloc_calendarGregorian),
    PARAMETER(MakeMonthDayOverflow, mloc_overflowReject),
    PARAMETER(CurrencyParseMode, mloc_parseLenient),
    PARAMETER(CurrencySymbolDisplay, 0),
    PARAMETER(CurrencyDigitsUsage, mloc_currencyUsageStandard),
    PARAMETER(ZoneFormatterStyle, mloc_zoneStyleSpecificLong),
    PARAMETER(RoundDecimalMode, mloc_roundHalfEven),
    PARAMETER(NameType, mloc_nameLanguage),
    PARAMETER(NumberParseMode, mloc_parseLenient),
    PARAMETER(PluralType, mloc_pluralCardinal),
    PARAMETER(PluralRangeStart, mloc_pluralOne),
    PARAMETER(PluralRangeEnd, mloc_pluralOther),
    PARAMETER(RelativeUnit, mloc_relativeDay),
    PARAMETER(SegmentKind, mloc_segmentWord),
    PARAMETER(UnitNameWidth, mloc_unitWidthLong),
    PARAMETER(TransitionDirection, mloc_transitionNext),
    PARAMETER(MakeTimeOverflow, mloc_overflowReject),
    PARAMETER(BalanceUnit, mloc_timeUnitHour),
    PARAMETER(RoundTimeDurationUnit, mloc_timeUnitHour),
    PARAMETER(RoundTimeDurationMode, mloc_roundHalfExpand),
    PARAMETER(TotalTimeDurationUnit, mloc_timeUnitHour),
    PARAMETER(RoundTimeUnit, mloc_timeUnitHour),
    PARAMETER(RoundTimeMode, mloc_roundHalfExpand),
    PARAMETER(DurationValuesUnit, mloc_unitHour),
    PARAMETER(AddToDateTimeOverflow, mloc_overflowReject),
    PARAMETER(RoundDateTimeUnit, mloc_timeUnitHour),
    PARAMETER(RoundDateTimeMode, mloc_roundHalfExpand),
    PARAMETER(DateTimeTotalUnit, mloc_unitDay),
    PARAMETER(RoundInstantUnit, mloc_timeUnitHour),
    PARAMETER(RoundInstantMode, mloc_roundHalfExpand),
    PARAMETER(InstantForDisambiguation, mloc_disambiguationCompatible),
    PARAMETER(DateTimeForCalendar, mloc_calendarGregorian),
    PARAMETER(InstantForOffsetOption, mloc_offsetIgnore),
    PARAMETER(InstantForOffsetDisambiguation, mloc_disambiguationCompatible),
    PARAMETER(AddToZonedCalendar, mloc_calendarGregorian),
    PARAMETER(AddToZonedOverflow, mloc_overflowReject),
    PARAMETER(ZonedUntilCalendar, mloc_calendarGregorian),
    PARAMETER(ZonedTotalCalendar, mloc_calendarGregorian),
    PARAMETER(ZonedTotalUnit, mloc_unitDay),
    PARAMETER(RoundZonedUnit, mloc_timeUnitHour),
    PARAMETER(RoundZonedMode, mloc_roundHalfExpand),
    PARAMETER(ListGender, mloc_personMale),
    PARAMETER(MessageValueKind, mloc_messageValueInteger),
};

// A call made with options; a field is checked by writing its value over
// a valid set of them.
typedef mlocResult (*Make)(const void* options);

typedef struct Field
{
    const char* name;
    Make make;
    const void* valid;
    size_t size;
    size_t offset;
} Field;

static mlocResult DateUntilRounded(const void* options)
{
    mlocDateDuration duration;
    return mlocDateUntilRounded(&s_date, &s_date, options, &duration);
}

static mlocResult Collator(const void* options)
{
    mlocCollator collator;
    return mlocInitCollator(&s_data, &s_en, options, &collator);
}

static mlocResult CurrencyFormatter(const void* options)
{
    mlocCurrencyFormatter formatter;
    return mlocInitCurrencyFormatter(&s_data, &s_en, "USD", 3, options, &formatter);
}

static mlocResult DateFormatter(const void* options)
{
    mlocDateFormatter formatter;
    return mlocInitDateFormatter(&s_data, &s_en, options, &formatter);
}

static mlocResult TimeFormatter(const void* options)
{
    mlocTimeFormatter formatter;
    return mlocInitTimeFormatter(&s_data, &s_en, options, &formatter);
}

static mlocResult DateTimeFormatter(const void* options)
{
    mlocDateTimeFormatter formatter;
    return mlocInitDateTimeFormatter(&s_data, &s_en, options, &formatter);
}

static mlocResult DurationFormatter(const void* options)
{
    mlocDurationFormatter formatter;
    return mlocInitDurationFormatter(&s_data, &s_en, options, &formatter);
}

static mlocResult TurkishInflection(const void* request)
{
    mlocOutput output = Text();
    return mlocInflector_InflectText(&s_turkish, "kitap", 5, request, &output);
}

static mlocResult KoreanInflection(const void* request)
{
    mlocOutput output = Text();
    return mlocInflector_InflectText(&s_korean, "\xEC\xB1\x85", 3, request, &output);
}

static mlocResult ListFormatter(const void* options)
{
    mlocListFormatter formatter;
    return mlocInitListFormatter(&s_data, &s_en, options, &formatter);
}

static mlocResult MessageFormatter(const void* options)
{
    mlocMessageFormatter formatter;
    return mlocInitMessageFormatter(&s_data, &s_en, options, &formatter);
}

static mlocResult Name(const void* options)
{
    mlocOutput output = Text();
    return mlocGetName(&s_data, &s_en, mloc_nameRegion, "FR", 2, options, &output);
}

static mlocResult LocaleName(const void* options)
{
    mlocOutput output = Text();
    return mlocGetLocaleName(&s_data, &s_en, &s_tr, options, &output);
}

static mlocResult NumberFormatter(const void* options)
{
    mlocNumberFormatter formatter;
    return mlocInitNumberFormatter(&s_data, &s_en, options, &formatter);
}

static mlocResult RelativeFormatter(const void* options)
{
    mlocRelativeFormatter formatter;
    return mlocInitRelativeFormatter(&s_data, &s_en, options, &formatter);
}

static mlocResult Searcher(const void* options)
{
    mlocSearcher searcher;
    return mlocInitSearcher(&s_collator, "a", 1, options, &searcher);
}

static mlocResult SpelloutFormatter(const void* options)
{
    mlocSpelloutFormatter formatter;
    return mlocInitSpelloutFormatter(&s_data, &s_en, "%spellout-cardinal", 18, options, &formatter);
}

static mlocResult UnitFormatter(const void* options)
{
    mlocUnitFormatter formatter;
    return mlocInitUnitFormatter(&s_data, &s_en, "meter", 5, options, &formatter);
}

static mlocResult TimeUntil(const void* rounding)
{
    mlocTimeDurationFields fields;
    return mlocTimeUntil(&s_time, &s_time, rounding, &fields);
}

static mlocResult InstantUntil(const void* rounding)
{
    mlocTimeDuration duration;
    return mlocInstantUntil(&s_epoch, &s_epoch, rounding, &duration);
}

static mlocResult DateTimeUntil(const void* rounding)
{
    mlocDateTimeDuration duration;
    return mlocDateTimeUntil(&s_dateTime, &s_dateTime, rounding, &duration);
}

static mlocResult ZonedUntil(const void* rounding)
{
    mlocDateTimeDuration duration;
    return mlocZonedDateTimeUntil(&s_zone, mloc_calendarGregorian, &s_epoch, &s_epoch, rounding,
                                  &duration);
}

static const mlocDateRounding s_dateRounding = {mloc_dateUnitYear, mloc_dateUnitDay, 1,
                                                mloc_roundTrunc};
static const mlocCollatorOptions s_collatorOptions = {0};
static const mlocCurrencyOptions s_currencyOptions = {0};
static const mlocDateOptions s_dateOptions = {0};
static const mlocTimeOptions s_timeOptions = {0};
static const mlocDateTimeOptions s_dateTimeOptions = {0};
static const mlocDurationOptions s_durationOptions = {0};
static const mlocInflection s_inflection = {0};
static const mlocListOptions s_listOptions = {0};
static const mlocMessageOptions s_messageOptions = {0};
static const mlocNameOptions s_nameOptions = {0};
static const mlocLocaleNameOptions s_localeNameOptions = {0};
static const mlocNumberOptions s_numberOptions = {0};
static const mlocRelativeOptions s_relativeOptions = {0};
static const mlocSearchOptions s_searchOptions = {0};
static const mlocSpelloutOptions s_spelloutOptions = {0};
static const mlocUnitOptions s_unitOptions = {0};
static const mlocTimeRounding s_timeRounding = {mloc_timeUnitHour, mloc_timeUnitNanosecond, 1,
                                                mloc_roundTrunc};
static const mlocDateTimeRounding s_dateTimeRounding = {mloc_unitDay, mloc_unitNanosecond, 1,
                                                        mloc_roundTrunc};

#define FIELD(make, valid, type, member)                                                           \
    {#type "." #member " (" #make ")", make, &valid, sizeof(type), offsetof(type, member)}

static const Field s_fields[] = {
    FIELD(DateUntilRounded, s_dateRounding, mlocDateRounding, largestUnit),
    FIELD(DateUntilRounded, s_dateRounding, mlocDateRounding, smallestUnit),
    FIELD(DateUntilRounded, s_dateRounding, mlocDateRounding, mode),
    FIELD(Collator, s_collatorOptions, mlocCollatorOptions, strength),
    FIELD(Collator, s_collatorOptions, mlocCollatorOptions, alternate),
    FIELD(Collator, s_collatorOptions, mlocCollatorOptions, maxVariable),
    FIELD(Collator, s_collatorOptions, mlocCollatorOptions, caseFirst),
    FIELD(Collator, s_collatorOptions, mlocCollatorOptions, caseLevel),
    FIELD(Collator, s_collatorOptions, mlocCollatorOptions, numeric),
    FIELD(Collator, s_collatorOptions, mlocCollatorOptions, backwards),
    FIELD(CurrencyFormatter, s_currencyOptions, mlocCurrencyOptions, number.notation),
    FIELD(CurrencyFormatter, s_currencyOptions, mlocCurrencyOptions, number.style),
    FIELD(CurrencyFormatter, s_currencyOptions, mlocCurrencyOptions, number.grouping),
    FIELD(CurrencyFormatter, s_currencyOptions, mlocCurrencyOptions, number.signDisplay),
    FIELD(CurrencyFormatter, s_currencyOptions, mlocCurrencyOptions, number.precision.kind),
    FIELD(CurrencyFormatter, s_currencyOptions, mlocCurrencyOptions, number.precision.mode),
    FIELD(CurrencyFormatter, s_currencyOptions, mlocCurrencyOptions,
          number.precision.trailingZeros),
    FIELD(CurrencyFormatter, s_currencyOptions, mlocCurrencyOptions, display),
    FIELD(CurrencyFormatter, s_currencyOptions, mlocCurrencyOptions, usage),
    FIELD(CurrencyFormatter, s_currencyOptions, mlocCurrencyOptions, sign),
    FIELD(DateFormatter, s_dateOptions, mlocDateOptions, fields),
    FIELD(DateFormatter, s_dateOptions, mlocDateOptions, length),
    FIELD(DateFormatter, s_dateOptions, mlocDateOptions, yearStyle),
    FIELD(DateFormatter, s_dateOptions, mlocDateOptions, style),
    FIELD(DateFormatter, s_dateOptions, mlocDateOptions, zone),
    FIELD(TimeFormatter, s_timeOptions, mlocTimeOptions, hourCycle),
    FIELD(TimeFormatter, s_timeOptions, mlocTimeOptions, precision),
    FIELD(TimeFormatter, s_timeOptions, mlocTimeOptions, style),
    FIELD(TimeFormatter, s_timeOptions, mlocTimeOptions, zone),
    FIELD(DateTimeFormatter, s_dateTimeOptions, mlocDateTimeOptions, date.fields),
    FIELD(DateTimeFormatter, s_dateTimeOptions, mlocDateTimeOptions, date.length),
    FIELD(DateTimeFormatter, s_dateTimeOptions, mlocDateTimeOptions, date.yearStyle),
    FIELD(DateTimeFormatter, s_dateTimeOptions, mlocDateTimeOptions, date.style),
    FIELD(DateTimeFormatter, s_dateTimeOptions, mlocDateTimeOptions, date.zone),
    FIELD(DateTimeFormatter, s_dateTimeOptions, mlocDateTimeOptions, time.hourCycle),
    FIELD(DateTimeFormatter, s_dateTimeOptions, mlocDateTimeOptions, time.precision),
    FIELD(DateTimeFormatter, s_dateTimeOptions, mlocDateTimeOptions, time.style),
    FIELD(DateTimeFormatter, s_dateTimeOptions, mlocDateTimeOptions, time.zone),
    FIELD(DateTimeFormatter, s_dateTimeOptions, mlocDateTimeOptions, glue),
    FIELD(DurationFormatter, s_durationOptions, mlocDurationOptions, style),
    FIELD(DurationFormatter, s_durationOptions, mlocDurationOptions, fields[0]),
    FIELD(DurationFormatter, s_durationOptions, mlocDurationOptions,
          fields[mloc_durationFieldCount - 1]),
    FIELD(DurationFormatter, s_durationOptions, mlocDurationOptions, displays[0]),
    FIELD(DurationFormatter, s_durationOptions, mlocDurationOptions,
          displays[mloc_durationFieldCount - 1]),
    FIELD(TurkishInflection, s_inflection, mlocInflection, grammaticalCase),
    FIELD(TurkishInflection, s_inflection, mlocInflection, reading),
    FIELD(KoreanInflection, s_inflection, mlocInflection, particle),
    FIELD(KoreanInflection, s_inflection, mlocInflection, reading),
    FIELD(ListFormatter, s_listOptions, mlocListOptions, type),
    FIELD(ListFormatter, s_listOptions, mlocListOptions, width),
    FIELD(MessageFormatter, s_messageOptions, mlocMessageOptions, isolation),
    FIELD(Name, s_nameOptions, mlocNameOptions, form),
    FIELD(Name, s_nameOptions, mlocNameOptions, fallback),
    FIELD(Name, s_nameOptions, mlocNameOptions, capitalization),
    FIELD(LocaleName, s_localeNameOptions, mlocLocaleNameOptions, languageDisplay),
    FIELD(LocaleName, s_localeNameOptions, mlocLocaleNameOptions, form),
    FIELD(LocaleName, s_localeNameOptions, mlocLocaleNameOptions, capitalization),
    FIELD(NumberFormatter, s_numberOptions, mlocNumberOptions, notation),
    FIELD(NumberFormatter, s_numberOptions, mlocNumberOptions, style),
    FIELD(NumberFormatter, s_numberOptions, mlocNumberOptions, grouping),
    FIELD(NumberFormatter, s_numberOptions, mlocNumberOptions, signDisplay),
    FIELD(NumberFormatter, s_numberOptions, mlocNumberOptions, precision.kind),
    FIELD(NumberFormatter, s_numberOptions, mlocNumberOptions, precision.mode),
    FIELD(NumberFormatter, s_numberOptions, mlocNumberOptions, precision.trailingZeros),
    FIELD(RelativeFormatter, s_relativeOptions, mlocRelativeOptions, number.notation),
    FIELD(RelativeFormatter, s_relativeOptions, mlocRelativeOptions, number.style),
    FIELD(RelativeFormatter, s_relativeOptions, mlocRelativeOptions, number.grouping),
    FIELD(RelativeFormatter, s_relativeOptions, mlocRelativeOptions, number.signDisplay),
    FIELD(RelativeFormatter, s_relativeOptions, mlocRelativeOptions, number.precision.kind),
    FIELD(RelativeFormatter, s_relativeOptions, mlocRelativeOptions, number.precision.mode),
    FIELD(RelativeFormatter, s_relativeOptions, mlocRelativeOptions,
          number.precision.trailingZeros),
    FIELD(RelativeFormatter, s_relativeOptions, mlocRelativeOptions, width),
    FIELD(RelativeFormatter, s_relativeOptions, mlocRelativeOptions, numeric),
    FIELD(RelativeFormatter, s_relativeOptions, mlocRelativeOptions, capitalization),
    FIELD(Searcher, s_searchOptions, mlocSearchOptions, comparison),
    FIELD(SpelloutFormatter, s_spelloutOptions, mlocSpelloutOptions, capitalization),
    FIELD(UnitFormatter, s_unitOptions, mlocUnitOptions, number.notation),
    FIELD(UnitFormatter, s_unitOptions, mlocUnitOptions, number.style),
    FIELD(UnitFormatter, s_unitOptions, mlocUnitOptions, number.grouping),
    FIELD(UnitFormatter, s_unitOptions, mlocUnitOptions, number.signDisplay),
    FIELD(UnitFormatter, s_unitOptions, mlocUnitOptions, number.precision.kind),
    FIELD(UnitFormatter, s_unitOptions, mlocUnitOptions, number.precision.mode),
    FIELD(UnitFormatter, s_unitOptions, mlocUnitOptions, number.precision.trailingZeros),
    FIELD(UnitFormatter, s_unitOptions, mlocUnitOptions, width),
    FIELD(UnitFormatter, s_unitOptions, mlocUnitOptions, coordinate),
    FIELD(UnitFormatter, s_unitOptions, mlocUnitOptions, grammaticalCase),
    FIELD(TimeUntil, s_timeRounding, mlocTimeRounding, largestUnit),
    FIELD(TimeUntil, s_timeRounding, mlocTimeRounding, smallestUnit),
    FIELD(TimeUntil, s_timeRounding, mlocTimeRounding, mode),
    FIELD(InstantUntil, s_timeRounding, mlocTimeRounding, largestUnit),
    FIELD(InstantUntil, s_timeRounding, mlocTimeRounding, smallestUnit),
    FIELD(InstantUntil, s_timeRounding, mlocTimeRounding, mode),
    FIELD(DateTimeUntil, s_dateTimeRounding, mlocDateTimeRounding, largestUnit),
    FIELD(DateTimeUntil, s_dateTimeRounding, mlocDateTimeRounding, smallestUnit),
    FIELD(DateTimeUntil, s_dateTimeRounding, mlocDateTimeRounding, mode),
    FIELD(ZonedUntil, s_dateTimeRounding, mlocDateTimeRounding, largestUnit),
    FIELD(ZonedUntil, s_dateTimeRounding, mlocDateTimeRounding, smallestUnit),
    FIELD(ZonedUntil, s_dateTimeRounding, mlocDateTimeRounding, mode),
};

static const int32_t s_negatives[] = {-1, INT32_MIN};

static void TestParameters(void)
{
    for (size_t i = 0; i < sizeof(s_parameters) / sizeof(s_parameters[0]); i++)
    {
        const Parameter* p = &s_parameters[i];
        bool refused = p->call(p->valid) == mloc_success;
        for (size_t n = 0; n < sizeof(s_negatives) / sizeof(s_negatives[0]); n++)
        {
            refused = refused && p->call(s_negatives[n]) == mloc_errorInvalid;
        }
        if (!refused)
        {
            printf("  %s\n", p->name);
        }
        CHECK(refused, "a parameter takes its valid value and refuses a negative one");
    }
}

static void TestFields(void)
{
    // Room for the largest options struct, aligned as any of them.
    union
    {
        max_align_t align;
        unsigned char bytes[256];
    } options;
    for (size_t i = 0; i < sizeof(s_fields) / sizeof(s_fields[0]); i++)
    {
        const Field* f = &s_fields[i];
        CHECK(f->size <= sizeof(options.bytes) && f->offset + sizeof(int32_t) <= f->size,
              "the options fit the room");
        memcpy(options.bytes, f->valid, f->size);
        bool refused = f->make(options.bytes) == mloc_success;
        for (size_t n = 0; n < sizeof(s_negatives) / sizeof(s_negatives[0]); n++)
        {
            memcpy(options.bytes + f->offset, &s_negatives[n], sizeof(int32_t));
            refused = refused && f->make(options.bytes) == mloc_errorInvalid;
        }
        if (!refused)
        {
            printf("  %s\n", f->name);
        }
        CHECK(refused, "a field takes its valid value and refuses a negative one");
    }
}

// Errors are negative results: a negative value no result has is named
// as unknown.
static void TestResultName(void)
{
    const char* unknown = mlocGetResultName((mlocResult)99);
    CHECK(strcmp(mlocGetResultName(mloc_errorInvalid), "mloc_errorInvalid") == 0 &&
              strcmp(mlocGetResultName((mlocResult)-1000), unknown) == 0 &&
              strcmp(mlocGetResultName((mlocResult)INT32_MIN), unknown) == 0,
          "a negative value no result has is named as unknown");
}

static bool OpenData(void)
{
    // Room for every file at once.
    alignas(8) static uint8_t s_bytes[24 << 20];
    size_t used = 0;
    mlocOpenBuiltinData(&s_sources[0]);
    for (size_t i = 0; i < fileCount; i++)
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", MLOC_GENERATED_DIR, s_files[i]);
        FILE* file = fopen(path, "rb");
        size_t size = file == nullptr ? 0 : fread(s_bytes + used, 1, sizeof(s_bytes) - used, file);
        if (file != nullptr)
        {
            fclose(file);
        }
        if (size == 0 || used + size == sizeof(s_bytes) ||
            mlocOpenData(s_bytes + used, size, &s_sources[i + 1]) != mloc_success)
        {
            fprintf(stderr, "test_enum_refusals: cannot open %s\n", path);
            return false;
        }
        used += (size + 7) & ~(size_t)7;
    }
    s_data = (mlocDataSet){s_sources, fileCount + 1};
    return true;
}

static bool Bind(void)
{
    // The bound message reads its formatter while it is used.
    static mlocMessageFormatter formatter;
    static uint8_t compiled[256];
    size_t compiledSize = 0;
    size_t boundSize = 0;
    mlocMessageError error;
    return mlocCompileMessage("{$x}", 4, compiled, sizeof(compiled), &compiledSize, &error)
                   .status == mloc_success &&
           mlocInitMessageFormatter(&s_data, &s_en, nullptr, &formatter) == mloc_success &&
           mlocMessageFormatter_Bind(&formatter, compiled, compiledSize, s_bound, sizeof(s_bound),
                                     &boundSize) == mloc_success;
}

int main(void)
{
    if (!OpenData())
    {
        return 1;
    }
    mlocTimeInput noon = {.hour = 12};
    mlocTimeDurationFields hour = {.hours = 1};
    CHECK(mlocParseLocale("en", 2, &s_en).status == mloc_success &&
              mlocParseLocale("tr", 2, &s_tr).status == mloc_success &&
              mlocParseLocale("ko", 2, &s_ko).status == mloc_success &&
              mlocFindTimeZone(&s_data, "Europe/Paris", 12, &s_zone) == mloc_success &&
              mlocMakeDate(mloc_calendarGregorian, &s_day, mloc_overflowReject, &s_date) ==
                  mloc_success &&
              mlocMakeTime(&noon, mloc_overflowReject, &s_time) == mloc_success &&
              mlocMakeDateTime(&s_date, &s_time, &s_dateTime) == mloc_success &&
              mlocParseDecimal("1", 1, &s_one).status == mloc_success &&
              mlocMakeTimeDuration(&hour, &s_hour) == mloc_success &&
              mlocInitCollator(&s_data, &s_en, nullptr, &s_collator) == mloc_success &&
              mlocInitNumberFormatter(&s_data, &s_en, nullptr, &s_number) == mloc_success &&
              mlocInitCurrencyFormatter(&s_data, &s_en, "USD", 3, nullptr, &s_currency) ==
                  mloc_success &&
              mlocInitRelativeFormatter(&s_data, &s_en, nullptr, &s_relative) == mloc_success &&
              mlocInitPluralRules(&s_data, &s_en, mloc_pluralCardinal, &s_plural) == mloc_success &&
              mlocInitInflector(&s_data, &s_tr, &s_turkish) == mloc_success &&
              mlocInitInflector(&s_data, &s_ko, &s_korean) == mloc_success && Bind(),
          "data, objects and a bound message to call with");
    TestParameters();
    TestFields();
    TestResultName();
    return s_failures == 0 ? 0 : 1;
}
