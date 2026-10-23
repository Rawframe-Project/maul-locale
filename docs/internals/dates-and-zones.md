# Dates and zones

Calendars, time zones, Temporal's values and strings, date and time formatting, relative time and durations. Sources: `src/calendar*.c`, `src/zone*.c`, `src/*_arith.c`, `src/ixdtf_*.c`, `src/date*.c`, `src/time*.c`, `src/relative*.c`, `src/duration*.c`.

## Values

Dates, times, date-times, instants and zoned date-times are separate immutable value types with Temporal's semantics, because that is the model JavaScript adopted and the most completely specified one: era and month codes, an explicit overflow choice, and an explicit choice in a gap or fold (compatible, earlier, later, refused). Leap seconds are not represented. A date is 8 bytes, a day and a calendar; its fields are computed from the day, so two dates are the same day when their days are equal, whatever their calendars. Days run over Temporal's whole range.

The arithmetic follows ECMA-262's abstract operations step by step, and its functions name the operation they implement. Durations keep Temporal's float64 fields, but every computation reads them as exact integers and sums them in 128 bits, so results are the specification's exact values; a returned `double` is the nearest to the exact value, assembled by integer arithmetic. test262's Temporal tests, converted to tables by `tools/oracle/test262/`, are replayed by the `*_test262` suites.

## Calendars

- **Arithmetic calendars in code,** after Reingold and Dershowitz's *Calendrical Calculations*: Gregorian, ISO 8601, Buddhist, ROC, Japanese, Coptic, Ethiopic, Indian, Persian (its 33-year rule with the 78 published corrections), Hebrew and the tabular Islamic calendars.
- **Umm al-Qura** from KACST's published month lengths for 1300 to 1600 AH, two bytes a year, and the tabular civil calendar outside, which meets the table's ends exactly. `islamic` and `islamic-rgsa` read as Umm al-Qura, as in ICU4X: an observational calendar cannot be computed, and ICU4C's astronomical estimate is not what Saudi Arabia publishes.
- **Chinese and Dangi** from one astronomical model (new moons and solar terms) in integer fixed point, since the library uses no floating point. The generator runs it to write year tables for 1600 to 2400 (`src/generated/calendar_years.c`, four bytes a year); years where official rules differ come from `tools/calendar-overrides.txt`, each with its source (the Qing calendar's 1900 to 1911). The same model, compiled in, serves days outside the tables, with no cache and no lock. ICU4C computes the astronomy at run time behind caches and a mutex, with a reduced model that misses the observatories' calendar in a few months between 1900 and 2100. Before 1900 the dates are the model's, not the Qing court's own reckoning.

## Time zones

The generator compiles the tzdb source itself, as zic compiles it, and never reads TZif files or the system's zones. Links resolve to their Zones' data (the default build, as ICU and browsers have it). The source is read in its rearguard form, where daylight saving is never negative, because formatting picks standard or daylight names by it; the generator checks that the rearguard form gives the main form's offsets for every zone.

The zones unit holds, per zone, its transitions as `int32_t` seconds (with `int64_t` for the few outside), a one-byte index per transition into shared offset pairs, and a closing rule of two yearly changes that runs to the end of Temporal's range. Finding an offset is a binary search, then the rule for later years. CI checks every zone's offsets against zic and zdump from 1800 to 2500 (`tools/check_zones.py`), the possible instants of local times around every change against Python's `zoneinfo` (`check_local.py`), and zoned arithmetic against a reading of ECMA-262 over `zoneinfo` (`check_zoned.py`), for the pinned release and for a zones file of an older one.

BCP 47 ids and Windows names come from CLDR's `timezone.xml` and `windowsZones.xml`, compiled in; `tools/check_zone_codes.py` checks every mapping.

## Formatting

The API takes meaning, never pattern letters: a semantic skeleton (UTS #35's field sets with a length, year style, hour cycle, precision and zone style) or one of the four standard lengths. The generator runs ICU's `DateTimePatternGenerator` matching for every locale, calendar and field set and stores the patterns, compiled, in `dates.mloc`; the library only selects. ICU runs the matching each time a formatter is made; ICU4X, like this library, does it while building its data.

Where ICU4C's data build and CLDR's inheritance disagree, the generator follows CLDR (UTS #35: root's aliases are resolved from the locale asked for). So a non-Gregorian calendar has its own `atTime` glue through root's alias where ICU takes the Gregorian one, the Chinese calendars take the locale's Gregorian time formats, and a pattern is read whole where ICU's reader keeps 50 tokens. Each such case is a listed difference in the skeleton and interval checks against ICU4C. CLDR's own datetime test data expects the standard glue for date-time composites where UTS #35, ICU4C and ICU4X use `atTime`; the check asks for the standard glue there.

Interval patterns (date, time and date-time ranges) are resolved by the generator as ICU4C's `DateIntervalFormat` resolves them, per greatest differing field. Zone names follow ICU4C's `TimeZoneFormat` fallbacks (specific, generic, location, offset) from `zone-names.mloc`, with metazones by instant; the location form reads region names from `names.mloc`.

## Relative time and durations

Relative time is ECMA-402's `Intl.RelativeTimeFormat` over CLDR's fields, in `relative.mloc`. A width's slot is taken from the first key along the chain that files it, else the next wider width's, as root's aliases send them; a plural category's pattern comes from the first width that has it, as ICU's data gives it, where CLDR's count fallback alone would reach another width's "other" first. Filing only where this lookup would differ took the file from 2.8 MB to 0.6 MB. The caller gives the amount: the library does not know what now is.

Durations follow ECMA-402's `Intl.DurationFormat` (`PartitionDurationFormatPattern`): unit-style fields with the unit names of `units.mloc` in their plural category, numeric fields as `FormatNumericUnits` chooses them, the fraction truncated, never rounded, from the exact sum of the sub-second fields.
