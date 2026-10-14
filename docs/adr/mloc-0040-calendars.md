# mloc-0040. Calendars: Temporal's values, computed in code, astronomical years as tables

Status: Accepted

## Context

CLDR names 18 calendar systems. Most are arithmetic. Hebrew follows
fixed rules of the molad. Umm al-Qura follows a published table.
Chinese and Dangi follow astronomy: new moons and solar terms.

ICU4C computes the Chinese calendar's astronomy at run time, behind
caches and a mutex, with a reduced model that misses the observatories'
calendar in 9 Chinese and 4 Dangi months between 1900 and 2100.
ICU4X 2.3 ships tables checked against the Purple Mountain and Hong
Kong observatories for 1900 to 2100, and a mean-motion estimate
outside them that differs from the astronomical calendar in about a
third of months. For every arithmetic calendar the two ICUs agree, and
they share Persian's 33-year rule with the same 78 corrections. They
differ on `islamic`: ICU4C estimates it astronomically, while ICU4X
reads it as Umm al-Qura.

Temporal, the model JavaScript adopts, makes dates values in a
calendar, with era codes and month codes (`M05L` for a leap month), and
an explicit choice when a field overflows.

## Decision

- **Values:** a date is a day and a calendar (`mlocDate`, 8 bytes). Day
  1 is 0001-01-01 in the proleptic Gregorian calendar. The days run
  over Temporal's range, -271821-04-19 to +275760-09-13. Fields are
  computed from the day:
  - the year counted on across eras, and the era and the year in it,
    by Temporal's era codes (`ce`, `bce`, `be`, `roc`, `broc`, `am`,
    `aa`, `shaka`, `ap`, `ah`, `bh`, and Japan's eras);
  - the month code and the month's place in the year, and the day;
  - the weekday, the day of the year, and the year's and month's
    lengths.
- **Making a date:** from an era and a year in it, or a year; a month
  by code or by place (both must agree when both are given); and a
  day. A month or day past the calendar's is clamped or refused, as
  Temporal's `overflow` option says. A malformed code, an era of
  another calendar, or a leap month in a calendar without one is
  refused either way.
- **Arithmetic calendars in code** (after Reingold and Dershowitz):
  Gregorian, ISO 8601, Buddhist, ROC, Japanese, Coptic, both Ethiopic
  eras, Indian, Persian, Hebrew, and the tabular Islamic calendars.
- **Umm al-Qura:** KACST's month lengths for 1300 to 1600 AH as a
  table, tabular outside. `islamic` and `islamic-rgsa` read as Umm
  al-Qura, as in ICU4X. An observational calendar cannot be computed,
  and ICU4C's estimate is not what Saudi Arabia publishes.
- **Chinese and Dangi:** one astronomical model (Reingold and
  Dershowitz's new moons and solar terms), in integer fixed point as
  the library profile requires (mloc-0001: bit-exact, no floating
  point). It runs in the generator to write year tables for 1600 to
  2400. The years where official rules differ come as overrides, each
  with its source. The same model, compiled in, serves days outside
  the tables, with no cache and no lock.
- **Japanese eras** come from CLDR's data: the generator writes them as
  a compiled table (`src/generated/calendar_eras.c`), each era's code,
  its first Gregorian year and its first day. Nothing reads the
  environment for a tentative era; a new era comes with a new CLDR.
  Days before January 1 of 1873 (Meiji 6), when Japan took the
  Gregorian calendar, read in `ce` and `bce`, as Temporal's era table
  and ICU4X have it; ICU4C 79 starts Meiji on its CLDR date, 1868, a
  listed departure. On input an era's years count on past its end
  (Heisei 32 is 2020), and `ce`, `bce`, `ad` and `bc` are taken too.

## Proof

`test_calendar` checks each built calendar's fields at days across
the range as ICU4X 2.3 gives them. It also makes dates back from their
fields by era and by year, by code and by place, across 2.4 million
days. Overflow, eras and their aliases, month codes, the other fields
and every refusal have their own cases.

A run outside the repository compared every day from 1600 to 2400, and
2,001 days spread over the whole range, with ICU4X 2.3 for the ten
arithmetic calendars built first, and then for Hebrew and Japanese
(every day from 1600 to 2400 and 3,999 days over the range, the leap
year and the year's and month's lengths included): no difference.
`test_gencalendar` checks the era table's generator and its
refusals. The differential test
against ICU4C 79 and ICU4X joins CI with the other calendars.

## Consequences

Dates need no data except the Japanese eras and the astronomical and
Umm al-Qura tables, a few KB in all. A Chinese or Dangi date outside
1600 to 2400 costs about 100 microseconds, against nanoseconds inside.
Calendars land one slice at a time; until a calendar's slice, its
functions refuse with `mloc_errorNotFound`.
