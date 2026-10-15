#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks the library's arithmetic of zoned date-times (record mloc-0041)
# against this program's own reading of ECMA-262 over Python's zoneinfo
# reading zic's fat TZif files for the same tzdb source: AddZonedDateTime,
# DifferenceZonedDateTimeWithRounding (DifferenceZonedDateTime,
# RoundRelativeDuration with ComputeNudgeWindow, NudgeToCalendarUnit,
# NudgeToZonedTime and BubbleRelativeDuration),
# DifferenceZonedDateTimeWithTotal, Temporal.ZonedDateTime's round and
# InterpretISODateTimeOffset, in the ISO calendar, written from
# the specification's steps with exact integers and fractions. Around
# every change of offset from 1900 to 2040 in every zone, conform_zoned is
# asked about differences spanning the change with each kind of rounding
# and their totals of each unit (the float64 nearest the exact total),
# durations added across it, instants near it rounded, and local times in
# its gap or fold found with either offset.
#
# usage: check_zoned.py <conform_zoned> <tzdb source directory>

import datetime
import os
import struct
import subprocess
import sys
import tempfile
from fractions import Fraction

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_local  # noqa: E402
from check_zones import FILES  # noqa: E402

FIRST, END = 1900, 2040
NS = 10**9
DAY = 86400 * NS
EPOCH_DAY = datetime.date(1970, 1, 1).toordinal()
YEAR, MONTH, WEEK, DAY_UNIT, HOUR, NANOSECOND = 0, 1, 2, 3, 4, 9
UNIT_LENGTH = {3: DAY, 4: 3600 * NS, 5: 60 * NS, 6: NS, 7: 10**6, 8: 1000, 9: 1}
MAX_TIME = 2**53 * NS - 1
# mlocRoundingMode's order; GetUnsignedRoundingMode for positive and
# negative values.
MODES = ["halfExpand", "halfEven", "halfTrunc", "halfCeil", "halfFloor", "ceil", "floor",
         "expand", "trunc"]
UNSIGNED = {"ceil": ("infinity", "zero"), "floor": ("zero", "infinity"),
            "expand": ("infinity", "infinity"), "trunc": ("zero", "zero"),
            "halfCeil": ("half-infinity", "half-zero"), "halfFloor": ("half-zero", "half-infinity"),
            "halfExpand": ("half-infinity", "half-infinity"), "halfTrunc": ("half-zero", "half-zero"),
            "halfEven": ("half-even", "half-even")}
OPTIONS = ["use", "ignore", "prefer", "reject"]
DISAMBIGUATIONS = ["compatible", "earlier", "later", "reject"]


class Refused(Exception):
    pass


def sign_of(v):
    return (v > 0) - (v < 0)


# Rounding.

def apply_unsigned(x, r1, r2, mode):
    if x == r1:
        return r1
    if mode == "zero":
        return r1
    if mode == "infinity":
        return r2
    d1, d2 = x - r1, r2 - x
    if d1 < d2:
        return r1
    if d2 < d1:
        return r2
    if mode == "half-zero":
        return r1
    if mode == "half-infinity":
        return r2
    return r1 if (r1 / (r2 - r1)) % 2 == 0 else r2


def round_to_increment(x, increment, mode):
    quotient = Fraction(x, increment)
    negative = quotient < 0
    if negative:
        quotient = -quotient
    r1 = quotient.numerator // quotient.denominator
    r2 = r1 + 1
    rounded = apply_unsigned(quotient, r1, r2, UNSIGNED[mode][1 if negative else 0])
    return (-rounded if negative else rounded) * increment


def round_time_duration(d, increment, mode):
    rounded = round_to_increment(d, increment, mode)
    if abs(rounded) > MAX_TIME:
        raise Refused("time duration too long")
    return rounded


# The ISO calendar.

def days_in_month(y, m):
    if m == 2:
        return 29 if y % 4 == 0 and (y % 100 != 0 or y % 400 == 0) else 28
    return 30 if m in (4, 6, 9, 11) else 31


def balance_year_month(y, m):
    return y + (m - 1) // 12, (m - 1) % 12 + 1


def regulate(y, m, d, overflow):
    if overflow == "constrain":
        m = min(max(m, 1), 12)
        return (y, m, min(max(d, 1), days_in_month(y, m)))
    if not (1 <= m <= 12 and 1 <= d <= days_in_month(y, m)):
        raise Refused("date rejected")
    return (y, m, d)


def ordinal(date):
    return datetime.date(*date).toordinal()


def add_days(date, n):
    d = datetime.date.fromordinal(ordinal(date) + n)
    return (d.year, d.month, d.day)


def date_add(date, duration, overflow):
    years, months, weeks, days = duration
    y, m = balance_year_month(date[0] + years, date[1] + months)
    return add_days(regulate(y, m, date[2], overflow), days + 7 * weeks)


def compare_surpasses(sign, y, m, d, target):
    if y != target[0]:
        return sign * (y - target[0]) > 0
    if m != target[1]:
        return sign * (m - target[1]) > 0
    if d != target[2]:
        return sign * (d - target[2]) > 0
    return False


def surpasses(sign, base, target, years, months, weeks, days):
    y0 = base[0] + years
    if compare_surpasses(sign, y0, base[1], base[2], target):
        return True
    if months == 0 and weeks == 0 and days == 0:
        return False
    y, m = balance_year_month(y0, base[1] + months)
    if compare_surpasses(sign, y, m, base[2], target):
        return True
    if weeks == 0 and days == 0:
        return False
    balanced = add_days(regulate(y, m, base[2], "constrain"), 7 * weeks + days)
    return compare_surpasses(sign, *balanced, target)


def date_until(one, two, largest):
    sign = -sign_of(ordinal(one) - ordinal(two))
    if sign == 0:
        return (0, 0, 0, 0)
    years = months = weeks = days = 0
    if largest == YEAR:
        candidate = sign
        while not surpasses(sign, one, two, candidate, 0, 0, 0):
            years, candidate = candidate, candidate + sign
    if largest in (YEAR, MONTH):
        candidate = sign
        while not surpasses(sign, one, two, years, candidate, 0, 0):
            months, candidate = candidate, candidate + sign
    if largest == WEEK:
        candidate = sign
        while not surpasses(sign, one, two, years, months, candidate, 0):
            weeks, candidate = candidate, candidate + sign
    # The days left, as the steps count them one by one.
    reached = add_days(regulate(*balance_year_month(one[0] + years, one[1] + months), one[2],
                                "constrain"), 7 * weeks)
    days = ordinal(two) - ordinal(reached)
    return (years, months, weeks, days)


# The zone.

class Zone:
    def __init__(self, path, name):
        self.z = check_local.Zone(path, name)

    def offset(self, ns):
        return self.z.offset_at(ns // NS) * NS

    def date_time_for(self, ns):
        local = ns + self.offset(ns)
        d = datetime.date.fromordinal(EPOCH_DAY + local // DAY)
        return (d.year, d.month, d.day), local % DAY

    def local_seconds(self, date, time):
        return (ordinal(date) - EPOCH_DAY) * 86400 + time // NS

    def possible(self, date, time):
        return [p * NS + time % NS for p in self.z.possible(self.local_seconds(date, time))]

    def epoch_for(self, date, time, disambiguation="compatible"):
        p = self.z.instant_for(self.local_seconds(date, time), disambiguation)
        if p is None:
            raise Refused("disambiguation rejected")
        return p * NS + time % NS

    def start_of_day(self, date):
        return self.z.start_of_day(self.local_seconds(date, 0)) * NS


# ECMA-262's operations.

def add_zoned(zone, ns, duration, overflow):
    date_part, time_part = duration[:4], duration[4]
    if all(v == 0 for v in date_part):
        return ns + time_part
    date, time = zone.date_time_for(ns)
    added = date_add(date, date_part, overflow)
    return zone.epoch_for(added, time) + time_part


def difference_zoned(zone, ns1, ns2, largest):
    if ns1 == ns2:
        return (0, 0, 0, 0), 0
    start_date, start_time = zone.date_time_for(ns1)
    end_date, end_time = zone.date_time_for(ns2)
    if start_date == end_date:
        return (0, 0, 0, 0), ns2 - ns1
    sign = 1 if ns2 - ns1 < 0 else -1
    max_correction = 2 if sign == -1 else 1
    correction = 0
    time_duration = end_time - start_time
    if sign_of(time_duration) == sign:
        correction += 1
    success = False
    while correction <= max_correction and not success:
        intermediate_date = add_days(end_date, correction * sign)
        intermediate_ns = zone.epoch_for(intermediate_date, start_time)
        time_duration = ns2 - intermediate_ns
        if sign != sign_of(time_duration):
            success = True
        correction += 1
    assert success
    return date_until(start_date, intermediate_date, min(largest, DAY_UNIT)), time_duration


def duration_sign(date, time):
    for v in date:
        if v:
            return sign_of(v)
    return sign_of(time)


def nudge_window(sign, date, time, origin_ns, dt, zone, increment, unit, shift):
    years, months, weeks, days = date
    if unit == YEAR:
        r1 = round_to_increment(years, increment, "trunc") + (increment * sign if shift else 0)
        r2 = r1 + increment * sign
        start, end = (r1, 0, 0, 0), (r2, 0, 0, 0)
    elif unit == MONTH:
        r1 = round_to_increment(months, increment, "trunc") + (increment * sign if shift else 0)
        r2 = r1 + increment * sign
        start, end = (years, r1, 0, 0), (years, r2, 0, 0)
    elif unit == WEEK:
        weeks_start = date_add(dt[0], (years, months, 0, 0), "constrain")
        weeks_end = add_days(weeks_start, days)
        until = date_until(weeks_start, weeks_end, WEEK)
        r1 = round_to_increment(weeks + until[2], increment, "trunc")
        r2 = r1 + increment * sign
        start, end = (years, months, r1, 0), (years, months, r2, 0)
    else:
        r1 = round_to_increment(days, increment, "trunc")
        r2 = r1 + increment * sign
        start, end = (years, months, weeks, r1), (years, months, weeks, r2)
    if all(v == 0 for v in start):
        start_ns = origin_ns
    else:
        start_ns = zone.epoch_for(date_add(dt[0], start, "constrain"), dt[1])
    end_ns = zone.epoch_for(date_add(dt[0], end, "constrain"), dt[1])
    return r1, r2, start_ns, end_ns, start, end


def nudge_to_calendar_unit(sign, date, time, origin_ns, dest_ns, dt, zone, increment, unit, mode):
    expanded = False
    w = nudge_window(sign, date, time, origin_ns, dt, zone, increment, unit, False)
    low, high = (w[2], w[3]) if sign == 1 else (w[3], w[2])
    if not low <= dest_ns <= high:
        w = nudge_window(sign, date, time, origin_ns, dt, zone, increment, unit, True)
        expanded = True
    r1, r2, start_ns, end_ns, start, end = w
    progress = Fraction(dest_ns - start_ns, end_ns - start_ns)
    total = r1 + progress * increment * sign
    unsigned = UNSIGNED[mode][1 if sign < 0 else 0]
    if progress == 1:
        rounded = abs(r2)
    else:
        rounded = apply_unsigned(abs(total), abs(r1), abs(r2), unsigned)
    if rounded == abs(r2):
        return end, 0, end_ns, True
    return start, 0, start_ns, expanded


def nudge_to_zoned_time(sign, date, time, dt, zone, increment, unit, mode):
    start = date_add(dt[0], date, "constrain")
    end = add_days(start, sign)
    start_ns = zone.epoch_for(start, dt[1])
    end_ns = zone.epoch_for(end, dt[1])
    day_span = end_ns - start_ns
    assert sign_of(day_span) == sign
    step = increment * UNIT_LENGTH[unit]
    rounded = round_time_duration(time, step, mode)
    beyond = rounded - day_span
    if sign_of(beyond) != -sign:
        rounded = round_time_duration(beyond, step, mode)
        return (date[0], date[1], date[2], date[3] + sign), rounded, end_ns + rounded, True
    return date, rounded, start_ns + rounded, False


def bubble(sign, date, time, nudged_ns, dt, zone, largest, smallest):
    if smallest == largest:
        return date, time
    unit = smallest - 1
    while unit >= largest:
        if unit != WEEK or largest == WEEK:
            if unit == YEAR:
                end = (date[0] + sign, 0, 0, 0)
            elif unit == MONTH:
                end = (date[0], date[1] + sign, 0, 0)
            else:
                end = (date[0], date[1], date[2] + sign, 0)
            end_ns = zone.epoch_for(date_add(dt[0], end, "constrain"), dt[1])
            if sign_of(nudged_ns - end_ns) != -sign:
                date, time = end, 0
            else:
                break
        unit -= 1
    return date, time


def round_relative(date, time, origin_ns, dest_ns, dt, zone, largest, increment, smallest, mode):
    sign = -1 if duration_sign(date, time) < 0 else 1
    if smallest <= DAY_UNIT:
        date, time, nudged, expanded = nudge_to_calendar_unit(
            sign, date, time, origin_ns, dest_ns, dt, zone, increment, smallest, mode)
    else:
        date, time, nudged, expanded = nudge_to_zoned_time(sign, date, time, dt, zone, increment,
                                                           smallest, mode)
    if expanded and smallest != WEEK:
        date, time = bubble(sign, date, time, nudged, dt, zone, largest, min(smallest, DAY_UNIT))
    return date, time


def until(zone, ns1, ns2, largest, smallest, increment, mode):
    if largest >= HOUR:
        return (0, 0, 0, 0), round_time_duration(ns2 - ns1, increment * UNIT_LENGTH[smallest], mode)
    date, time = difference_zoned(zone, ns1, ns2, largest)
    if smallest == NANOSECOND and increment == 1:
        return date, time
    return round_relative(date, time, ns1, ns2, zone.date_time_for(ns1), zone, largest, increment,
                          smallest, mode)


def total(zone, ns1, ns2, unit):
    # DifferenceZonedDateTimeWithTotal.
    if unit >= HOUR:
        return Fraction(ns2 - ns1, UNIT_LENGTH[unit])
    date, time = difference_zoned(zone, ns1, ns2, unit)
    sign = -1 if duration_sign(date, time) < 0 else 1
    dt = zone.date_time_for(ns1)
    w = nudge_window(sign, date, time, ns1, dt, zone, 1, unit, False)
    low, high = (w[2], w[3]) if sign == 1 else (w[3], w[2])
    if not low <= ns2 <= high:
        w = nudge_window(sign, date, time, ns1, dt, zone, 1, unit, True)
    r1, _, start_ns, end_ns, _, _ = w
    return r1 + Fraction(ns2 - start_ns, end_ns - start_ns) * sign


def interpret_offset(zone, date, time, offset, option, disambiguation):
    if option == "ignore":
        return zone.epoch_for(date, time, disambiguation)
    utc = (ordinal(date) - EPOCH_DAY) * DAY + time
    if option == "use":
        return utc - offset
    for candidate in zone.possible(date, time):
        if utc - candidate == offset:
            return candidate
    if option == "reject":
        raise Refused("offset rejected")
    return zone.epoch_for(date, time, disambiguation)


def round_zoned(zone, ns, unit, increment, mode):
    date, time = zone.date_time_for(ns)
    if unit == 0:
        start_ns = zone.start_of_day(date)
        end_ns = zone.start_of_day(add_days(date, 1))
        ns = min(ns, end_ns - 1)
        return start_ns + round_time_duration(ns - start_ns, end_ns - start_ns, mode)
    rounded = round_to_increment(time, increment * UNIT_LENGTH[unit + 3], mode)
    date = add_days(date, rounded // DAY)
    return interpret_offset(zone, date, rounded % DAY, zone.offset(ns), "prefer", "compatible")


# Questions and answers.

def instant_text(ns):
    return "%d %d" % divmod(ns, NS)


def time_text(t):
    s = abs(t) // NS * sign_of(t)
    return "%d %d" % (s, t - s * NS)


def answer(fn):
    try:
        return fn()
    except Refused:
        return "-"


def questions(zone, t, before, after):
    a = (t - 3 * 3600 - 17 * 60) * NS + 123456789
    b = (t + 2 * 3600 + 13 * 60) * NS + 987654321
    c = (t - 86400 + 1800) * NS
    d = (t + 86400 - 3600) * NS + 500000000
    at = t * NS
    out = []
    for one, two, largest, smallest, increment, mode in (
            (a, b, 3, 9, 1, "trunc"), (c, d, 1, 4, 1, "halfExpand"), (d, c, 3, 3, 1, "halfEven"),
            (c, d, 3, 5, 30, "ceil"), (a, at + 30 * DAY, 1, 3, 1, "halfExpand"),
            (at + 40 * DAY + 7 * 3600 * NS, a, 0, 2, 1, "floor"), (b, c, 2, 4, 2, "halfFloor"),
            (a, b, 4, 6, 15, "halfTrunc")):
        def fn(one=one, two=two, largest=largest, smallest=smallest, increment=increment,
               mode=mode):
            date, time = until(zone, one, two, largest, smallest, increment, mode)
            return "%d %d %d %d %s" % (*date, time_text(time))
        out.append(("until %s %s %d %d %d %d" % (instant_text(one), instant_text(two), largest,
                                                  smallest, increment, MODES.index(mode)), fn))
    # Near a whole day and a whole month from half a day before the
    # change, where rounding reaches the next day or month and bubbles.
    one = at - 12 * 3600 * NS
    date, time = zone.date_time_for(one)
    next_day = zone.epoch_for(add_days(date, 1), time)
    month_before = zone.epoch_for(date_add(date, (0, -1, 0, 0), "constrain"), time)
    spans = []
    for delta in (-1800, -1, 1, 1800):
        two = next_day + delta * NS
        spans += [(one, two, 3, 4, 1, "halfExpand"), (one, two, 1, 5, 30, "halfEven"),
                  (two, one, 3, 4, 1, "halfCeil")]
    for delta in (-1200, 1200):
        two = one + delta * NS
        spans += [(month_before, two, 0, 4, 1, "halfExpand"), (month_before, two, 0, 3, 1, "halfExpand"),
                  (two, month_before, 0, 4, 1, "halfExpand"), (two, month_before, 1, 3, 1, "expand")]
    for one, two, largest, smallest, increment, mode in spans:
        def fn(one=one, two=two, largest=largest, smallest=smallest, increment=increment,
               mode=mode):
            date, time = until(zone, one, two, largest, smallest, increment, mode)
            return "%d %d %d %d %s" % (*date, time_text(time))
        out.append(("until %s %s %d %d %d %d" % (instant_text(one), instant_text(two), largest,
                                                  smallest, increment, MODES.index(mode)), fn))
    # Totals of each unit over spans across the change.
    for one, two in ((c, d), (d, c), (a, at + 30 * DAY), (month_before, one + 1200 * NS),
                     (one + 1200 * NS, month_before)):
        for unit in (0, 1, 2, 3, 4, 9):
            def fn(one=one, two=two, unit=unit):
                return struct.pack(">d", float(total(zone, one, two, unit))).hex()
            out.append(("total %s %s %d" % (instant_text(one), instant_text(two), unit), fn))
    for start, duration, overflow in ((c, (0, 0, 0, 1, 0), 0), (c, (0, 1, 0, 0, 25 * 3600 * NS), 0),
                                      (a, (0, 0, 0, -1, 0), 0), (at - 1, (0, 0, 1, 0, 0), 1),
                                      (b, (0, -1, 0, -1, -3600 * NS), 1)):
        def fn(start=start, duration=duration, overflow=overflow):
            return instant_text(add_zoned(zone, start, duration,
                                          "reject" if overflow else "constrain"))
        out.append(("add %s %d %d %d %d %s %d" % (instant_text(start), *duration[:4],
                                                  time_text(duration[4]), overflow), fn))
    for start, unit, increment, mode in ((a, 0, 1, "halfExpand"), (b, 0, 1, "halfEven"),
                                         (at, 0, 1, "floor"), (d, 0, 1, "ceil"),
                                         (at + 1800 * NS, 1, 1, "halfEven"),
                                         (at - 1, 2, 15, "ceil"), (c, 1, 6, "trunc"),
                                         (b, 3, 30, "halfExpand")):
        def fn(start=start, unit=unit, increment=increment, mode=mode):
            return instant_text(round_zoned(zone, start, unit, increment, mode))
        out.append(("round %s %d %d %d" % (instant_text(start), unit, increment,
                                           MODES.index(mode)), fn))
    for local in (t + before - 1800, t + after + 600, t + min(before, after) + 1):
        date = datetime.date.fromordinal(EPOCH_DAY + local // 86400)
        iso, time = (date.year, date.month, date.day), local % 86400 * NS
        for offset in (before * NS, after * NS):
            for option, disambiguation in (("prefer", "compatible"), ("reject", "compatible"),
                                           ("use", "compatible"), ("prefer", "later"),
                                           ("ignore", "earlier")):
                def fn(iso=iso, time=time, offset=offset, option=option,
                       disambiguation=disambiguation):
                    return instant_text(interpret_offset(zone, iso, time, offset, option,
                                                         disambiguation))
                out.append(("offset %d 0 %d %d %d" % (local, offset, OPTIONS.index(option),
                                                      DISAMBIGUATIONS.index(disambiguation)), fn))
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: check_zoned.py <conform_zoned> <tzdb source directory>")
    conform, source = sys.argv[1], sys.argv[2]
    check_local.FIRST, check_local.END = FIRST, END
    failures = 0
    with tempfile.TemporaryDirectory() as compiled:
        subprocess.run(["zic", "-b", "fat", "-d", compiled] + FILES, cwd=source, check=True)
        names = sorted(subprocess.run(["sh", "-c", "awk '$1 == \"Zone\" { print $2 }' " +
                                       " ".join(FILES)], cwd=source, capture_output=True,
                                      text=True, check=True).stdout.split())
        lines = []
        wants = []
        for name in names:
            path = os.path.join(compiled, name)
            zone = Zone(path, name)
            lines.append(f"zone {name}")
            for t, before, after in check_local.changes(path):
                for question, fn in questions(zone, t, before, after):
                    lines.append(question)
                    wants.append((name, question, answer(fn)))
        got = subprocess.run([conform], input="\n".join(lines) + "\n", capture_output=True,
                             text=True, check=True).stdout.splitlines()
        if len(got) != len(wants):
            sys.exit(f"conform_zoned answered {len(got)} of {len(wants)} questions")
        for (name, question, want), have in zip(wants, got):
            if have != want:
                failures += 1
                if failures <= 20:
                    print(f"{name} {question}: ECMA-262 {want!r}, library {have!r}")
    print(f"{len(wants)} questions over {len(names)} zones from {FIRST} to {END}")
    if failures:
        sys.exit(f"{failures} answers differ from ECMA-262")


if __name__ == "__main__":
    main()
