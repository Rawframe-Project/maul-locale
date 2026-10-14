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
  table (two bytes a year: the twelve lengths and the first day's
  offset from the tabular civil calendar, within a day), tabular civil
  outside, which meets the table's first and last days exactly. `islamic` and `islamic-rgsa` read as Umm
  al-Qura, as in ICU4X. An observational calendar cannot be computed,
  and ICU4C's estimate is not what Saudi Arabia publishes.
- **Chinese and Dangi:** one astronomical model (Reingold and
  Dershowitz's new moons and solar terms), in integer fixed point as
  the library profile requires (mloc-0001: bit-exact, no floating
  point). It runs in the generator to write year tables for 1600 to
  2400 (`src/generated/calendar_years.c`, four bytes a year: the month
  lengths, the leap month and the new year's offset). The years where
  official rules differ come as overrides in
  `tools/calendar-overrides.txt`, each with its source; the generator
  checks that each meets its neighbours. The first are the Qing
  calendar's years 1900 to 1911, which the Hong Kong Observatory and
  the Korea Astronomy and Space Science Institute publish and Korea
  also kept. The same model, compiled in, serves days outside the
  tables, with no cache and no lock.
  - Before 1900 the dates are the model's, not the Qing court's. The
    Qing calendar reckoned in Beijing's apparent time with its own
    astronomy (Tycho's tables, then from 1742 tables after Kepler).
    Yuk Tung Liu counts over 200 months where it differs from a
    modern computation between 1645 and 1911. The one such month
    testable here, April 1906, is not explained by apparent time
    alone: the model's new moon falls at 23:54 apparent time, and the
    court began the month the next day. A table of those years from a
    source with free terms can join the overrides later. ICU4C also
    computes these years by astronomy, and ICU4X by mean motion.
  - Days and degrees are scaled by 2^32, centuries by 2^48 and sines
    by 2^62 (an eighth of a turn and a Taylor series past the scale's
    resolution); products are 128-bit and rounded to nearest. The
    book's decimals are kept exactly where they multiply a whole
    lunation count.
  - The ephemeris correction takes the moment's Gregorian year, as the
    book does (calendrical_calculations 0.2.4 approximates it, a few
    seconds apart near New Year).
  - Past 50 centuries from 2000 (the years -3000 and 7000) two fits are
    held at their values there. The first is the new moon's
    eccentricity factor: its square would scale the terms to days and
    make months of 21 to 43 days at the range's ends. Held, every
    lunation over the range is 29.2 to 29.9 days. The second is the
    ephemeris correction's year: its parabola would move universal
    time by years (2,133 days at the range's start) and walk the new
    year round the Gregorian year. Held, the new year stays between
    late October and mid-May over the whole range.
  - The months follow the book. A month begins on the day of a new
    moon in China's time (Beijing's mean time before 1929, UTC+8 after)
    or Korea's (Seoul's mean time, then UTC+8:30 and UTC+9 by the dates
    of 1908, 1912, 1954 and 1961). The eleventh month holds the winter
    solstice. In a solstice-to-solstice span of thirteen months, the
    first month without a major solar term is the leap month; this
    gives 2033's leap eleventh month, as GB/T 33661-2017 does.
  - A year is numbered by the Gregorian year whose July 1 it holds, as
    Temporal and ICU4X number it. The year has no era, and its months'
    codes are 'M01' to 'M12', a leap month taking the code of the month
    before it with 'L'. A leap month the year lacks is constrained to
    the month of its number.
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

`test_calendar` checks each calendar's fields at days across
the range as ICU4X 2.3 gives them. It also makes dates back from their
fields by era and by year, by code and by place, across 2.4 million
days. Overflow, eras and their aliases, month codes, the other fields
and every refusal have their own cases.

A run outside the repository compared every day from 1600 to 2400, and
2,001 days spread over the whole range, with ICU4X 2.3 for the ten
arithmetic calendars built first, and then for Hebrew, Japanese and
Umm al-Qura (every day from 1600 to 2400 and 3,999 days over the range, the leap
year and the year's and month's lengths included): no difference.
For Chinese and Dangi, every month start from 1600 to 2400 was
compared:
- Against binary64 Reingold (calendrical_calculations 0.2.4), every
  start agrees. Its year-based leap rule places the leap month
  differently in 4 Chinese and 4 Dangi years whose leap month follows
  the solstice; 2033 is one of them.
- Against ICU4X's observatory tables (1900 to 2100), every start agrees
  except Qing China's 1906 and Korea before 1912. With the tables and
  their Qing overrides, every day from 1900 to 2099 agrees in both
  calendars, every field included.
- Against ICU4C 79, 170 of 9,907 lines differ in each calendar. These
  are new moons near midnight that ICU4C's reduced astronomy decides
  otherwise.

`test_calendar_lunar` checks that the tables are the model's years
except the overrides, and that the days at the tables' ends fall in
the same years from either side. `test_gencalendar` checks that the
generator refuses an override that is malformed or does not meet its
neighbours.

`test_calendar_astro` checks the astronomy against a 45-digit
evaluation of the book, and the model's own bits on both product
paths.
`test_gencalendar` checks the era table's generator and its
refusals. `test_calendar_oracle` runs the comparison with ICU4X in CI
without ICU4X: every day from 1600 to 2400 (Chinese and Dangi 1900 to
2099) is formatted as the oracle program prints it and hashed, against
hashes taken from ICU4X 2.3's own output. ISO 8601 is left out, since
ICU4X gives it an era Temporal does not have.

## Consequences

Dates need no data except the Japanese eras and the astronomical and
Umm al-Qura tables, a few KB in all. A Chinese or Dangi year costs
about 0.4 milliseconds to compute; once the tables land, a date
inside 1600 to 2400 costs nanoseconds.
