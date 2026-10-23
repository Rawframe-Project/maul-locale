# Dates and times

Dates, times, time zones and durations are values with arithmetic, as ECMA-262's Temporal defines them, and are formatted in a locale as CLDR, ICU and ECMA-402 write them. The headers are `maul-locale/calendar.h`, `zone.h`, `ixdtf.h`, `datetime.h`, `relative.h` and `duration.h`. The library never reads the clock or the system's time zone: the program passes the instant and the zone.

## Calendars and dates

An `mlocDate` is a day and the calendar it is read in, so the same day can be read in any of CLDR's 18 calendars: Gregorian, ISO 8601, Buddhist, ROC, Japanese, Coptic, Ethiopic (two eras), Indian, Persian, Hebrew, five Islamic calendars, Chinese and Dangi. Two dates are the same day when their days are equal.

`mlocMakeDate` makes a date from an era and year, a month by code or by number, and a day; a value past the calendar's is clamped or refused, as Temporal's overflow option says. Month codes name months across years with leap months: `M05L` is the Chinese leap fifth month, and in Hebrew `M05L` is Adar I. `mlocGetDateFields` gives the fields of a date in its calendar.

```c
mlocDateInput input = {.year = 2026, .month = 10, .day = 15};
mlocDate date;
mlocDateFields fields;
if (mlocMakeDate(mloc_calendarGregorian, &input, mloc_overflowReject, &date) == mloc_success &&
    mlocMakeDateFromDay(mloc_calendarHebrew, date.day, &date) == mloc_success &&
    mlocGetDateFields(&date, &fields) == mloc_success)
{
    // fields: year 5787, the second month (Cheshvan), day 4
}
```

`mlocAddToDate`, `mlocDateUntil` and `mlocWithDate` add durations, take differences and change fields in the date's own calendar, as Temporal's `PlainDate` does: adding a month to January 31 gives the end of February.

## Time zones and instants

An `mlocInstant` is a point in time, seconds and nanoseconds since 1970 in universal time. An `mlocTimeZone` is found by its IANA identifier in any letter case ("America/New_York"), by a fixed offset ("+05:30"), by its BCP 47 id (`mlocFindTimeZoneBcp47`, "usnyc") or by its Windows name (`mlocFindTimeZoneWindows`). Its rules come from the IANA release compiled into the data; a [zones file](data.md#a-newer-time-zone-release) brings a newer one.

`mlocGetDateTimeFor` gives the local date-time of a zone at an instant. The other way, a local date-time can name no instant (a gap when clocks move forward) or two (a fold when they move back); `mlocGetInstantFor` chooses as the disambiguation says, and `mlocGetPossibleInstants` lists them.

```c
mlocTimeZone zone;
mlocDateTime local;
if (mlocFindTimeZone(&set, "Europe/Istanbul", 15, &zone) == mloc_success &&
    mlocGetDateTimeFor(&zone, &instant, mloc_calendarIso8601, &local) == mloc_success)
{
    // 2026-10-15T13:03:00Z: local is 2026-10-15T16:03 (UTC+3)
}
```

`mlocAddToZonedDateTime`, `mlocZonedDateTimeUntil` and `mlocRoundZonedDateTime` do arithmetic on a zoned date-time as Temporal's `ZonedDateTime` does: a day added across a change of clocks keeps the wall-clock time, and an hour added does not. Plain times, date-times, instants and durations have their own arithmetic, rounding and totals in `zone.h`.

## Temporal strings

`maul-locale/ixdtf.h` reads and writes the strings of ISO 8601 and RFC 9557 as Temporal does: `2026-10-15T16:03:00+03:00[Europe/Istanbul][u-ca=hebrew]`, durations such as `P1Y2M3DT4H`, offsets and zone and calendar strings. `mlocParseTemporalString` takes the goals a call accepts (a date, a date-time, an instant, a zoned date-time) and reports the fields, the offset and the spans of the annotations, refusing what the grammar does not allow.

## Formatting dates and times

With `dates.mloc` in the set, `mlocInitDateFormatter` makes a formatter of a locale for what a date should show: a field set (year, month and day; month and day with the weekday; a month of a year) at a length (long, medium, short), as ICU4X's semantic skeletons do, or one of the locale's four standard formats. The calendar is the locale's (its `-u-ca` key, else its region's), and the date is read in it. `mlocInitTimeFormatter` does the same for a time of day with an hour cycle and a precision, `mlocInitDateTimeFormatter` joins both with the calendar's glue ("October 15, 2026 at 4:03 PM").

A formatter made with a zone style writes the zone too, from `zone-names.mloc`: a specific name ("Pacific Daylight Time", "PDT"), a generic one ("Pacific Time"), a location ("Los Angeles Time"), a city or an offset ("GMT-7"). The `InZone` functions take a zone and an instant and write the zone's local date or time.

```c
mlocDateTimeOptions options = {
    .date = {.fields = mloc_dateFieldSetYearMonthDay, .length = mloc_dateLengthMedium},
    .time = {.precision = mloc_timePrecisionMinute, .zone = mloc_zoneStyleSpecificShort},
};
mlocDateTimeFormatter formatter;
char text[96];
mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
if (mlocInitDateTimeFormatter(&formatter, &set, &locale, &options) >= 0 &&
    mlocFormatDateTimeInZone(&formatter, &zone, &instant, &output) == mloc_success)
{
    // en-US, America/Los_Angeles, 2026-10-15T23:03:00Z: "Oct 15, 2026, 4:03 PM PDT"
}
```

The year shows its century or era where it would be unclear. Ranges (`mlocFormatDateRange`, `mlocFormatTimeRange`, `mlocFormatDateTimeRange` and their `InZone` forms) write the shared fields once, as ICU's `DateIntervalFormat` does: "Oct 15 – 18, 2026", "4:03 – 5:30 PM". A part function receives the year, month, day, hour, day period, zone and the other parts with their ranges.

## Relative time

With `relative.mloc` in the set, `mlocFormatRelative` writes an amount of a unit relative to now, as ECMA-402's `Intl.RelativeTimeFormat` does: "in 3 days", "2 hr. ago", and with the numeric option set to auto "tomorrow", "last year", "next Monday". The caller computes the amount; the library does not know what now is.

```c
mlocRelativeOptions options = {.numeric = mloc_relativeNumericAuto};
mlocRelativeFormatter formatter;
char text[64];
mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
if (mlocInitRelativeFormatter(&formatter, &set, &locale, &options) >= 0 &&
    mlocFormatRelative(&formatter, &amount, mloc_relativeDay, &output) == mloc_success)
{
    // en, 1: "tomorrow"; 3: "in 3 days"; -1: "yesterday"
}
```

`mlocFormatRelativeDayTime` joins a relative day with a time of day: "tomorrow at 3:00 PM".

## Durations

With `units.mloc` in the set, `mlocFormatDuration` writes a duration's fields as ECMA-402's `Intl.DurationFormat` does: long ("1 hour, 2 minutes, 3 seconds"), short, narrow or digital ("1:02:03.456"), each field shown always or only when it is not zero. The fields are the ten of a Temporal duration; `mlocGetDurationValues` gives them from a duration the arithmetic computed.
