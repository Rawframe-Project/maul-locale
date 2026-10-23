# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Temporal.Duration's arithmetic for t262cal.py, on Maul Locale's
# functions: round and total (with no starting point, a plain date or
# date-time, or a zoned date-time as relativeTo), add and subtract, and
# compare. Options are read here, as a host reads them, and a duration's
# internal record (its date part and a time duration, the days folded
# into the time where ECMA-262 counts them as 24 hours) is formed here.
# Each value an assertion can check carries its origin, a JSON object
# with key "dur" (a record as [years, months, weeks, days, seconds,
# nanoseconds]; units mlocUnit's; a plain start as its calendar and day,
# a zoned one as its zone, calendar and instant [seconds, nanoseconds]):
#   {"dur": "round", "rel": ..., "d": record, "r": [largest, smallest,
#    increment, mode]}                            props years..nanoseconds
#   {"dur": "total", "rel": ..., "d": record, "unit": u}       prop "total"
#   {"dur": "add", "one": [s, ns], "two": [s, ns], "largest": u}
#                                                 props years..nanoseconds
#   {"dur": "compare", "rel": ..., "one": record, "two": record}  prop "c"
# with "rel" ["none"], ["plain", calendar, day] or ["zoned", zone,
# calendar, [s, ns]]; or "refused".

import ctypes
import json

from jsmini import UNDEF, Unsupported, JSObject, Builtin, Tracked, un
import jsmini as js
import time262 as tm
import datetime262 as dt
import zone262 as zn

NS = 10**9


class DurationValues(ctypes.Structure):
    _fields_ = [(n, ctypes.c_double) for n in (
        "years", "months", "weeks", "days", "hours", "minutes", "seconds", "milliseconds",
        "microseconds", "nanoseconds")]


class Ctx:
    lib = None
    host = None


def setup(lib, host):
    Ctx.lib, Ctx.host = lib, host


def key(o):
    return json.dumps(o, sort_keys=True)


def default_largest(values):
    return next((i for i, v in enumerate(values) if v), 9)


def internal(values):
    # ToInternalDurationRecord, by mlocMakeDuration from the float64
    # fields.
    out = dt.DateTimeDuration()
    if Ctx.lib.mlocMakeDuration(ctypes.byref(DurationValues(*[float(v) for v in values])),
                                ctypes.byref(out)) != 0:
        raise Ctx.host.range_error("duration out of range")
    return [out.date.years, out.date.months, out.date.weeks, out.date.days], out.time


def with_24_hour_days(values):
    # ToInternalDurationRecordWith24HourDays: the days into the time.
    date, t = internal(values)
    days = tm.TimeDuration(date[3] * 86400, 0)
    out = tm.TimeDuration()
    if Ctx.lib.mlocAddTimeDurations(ctypes.byref(t), ctypes.byref(days), ctypes.byref(out)) != 0:
        raise Ctx.host.range_error("duration out of range")
    return date[:3] + [0], out


def record(date, t):
    return list(date) + [t.seconds, t.nanoseconds]


def from_internal(date, t, largest, origin=None):
    # TemporalDurationFromInternal, by mlocGetDurationValues: the fields
    # as float64 values, refused past Temporal's limits.
    d = dt.DateTimeDuration(dt.DateDuration(*date), t)
    out = DurationValues()
    if Ctx.lib.mlocGetDurationValues(ctypes.byref(d), largest, ctypes.byref(out)) != 0:
        raise Ctx.host.range_error("duration fields out of range", origin)
    return [int(getattr(out, n)) for n, _ in DurationValues._fields_]


def round_time_part(values, time_unit, increment, mode):
    # Duration.prototype.toString's rounding: the time part rounded by
    # mlocRoundTimeDuration, balanced up to the larger of the duration's
    # largest unit and seconds.
    date, t = internal(values)
    out = tm.TimeDuration()
    if Ctx.lib.mlocRoundTimeDuration(ctypes.byref(t), time_unit, increment, mode,
                                     ctypes.byref(out)) != 0:
        raise Ctx.host.range_error("rounded duration out of range")
    return from_internal(date, out, min(default_largest(values), 6))


def relative_to(options):
    # GetTemporalRelativeToOption: a zoned date-time, a plain date or
    # date-time (its date), or a property bag of either.
    h = Ctx.host
    v = un(options.get("relativeTo"))
    if v is UNDEF:
        return None
    if isinstance(v, zn.ZonedDateTime):
        return v
    if isinstance(v, h.PlainDate):
        return v
    if isinstance(v, dt.PlainDateTime):
        return h.PlainDate(v.calendar, v.value.date.day, None)
    if isinstance(v, JSObject):
        if un(v.get("timeZone")) is not UNDEF:
            import zdt262
            return zdt262.from_bag(v, UNDEF, "reject")
        return h.plain_date_from(v)
    if isinstance(v, str):
        # A zoned date-time when the string has a time zone annotation,
        # else its date; the offset rejected when it does not match.
        import ixdtf262
        import zdt262
        r = ixdtf262.parse(v, ixdtf262.DATE_TIME_ZONED | ixdtf262.DATE_TIME)
        if r.time_zone is None:
            return h.iso_construct([r.year, r.month, r.day, ixdtf262.canonical_calendar(r.calendar)])
        name, zone = zn.to_zone(r.time_zone, False)
        calendar = ixdtf262.canonical_calendar(r.calendar)
        return zdt262.zoned_of_record(r, name, zone, calendar, "compatible", "reject",
                                      0 if r.offset_has_seconds else 1)
    raise Unsupported("relativeTo %r" % (v,))


def rel_key(rel):
    if rel is None:
        return ["none"]
    if isinstance(rel, zn.ZonedDateTime):
        return ["zoned", rel.given, rel.calendar, list(divmod(rel.ns, NS))]
    return ["plain", rel.calendar, rel.day]


def midnight(rel):
    h = Ctx.host
    out = dt.DateTime()
    date = dt.Date(rel.day, h.CALENDARS[rel.calendar])
    if Ctx.lib.mlocMakeDateTime(ctypes.byref(date), ctypes.byref(tm.Time()),
                                ctypes.byref(out)) != 0:
        raise Unsupported("midnight")
    return out


def plain_target(rel, date, t, origin):
    # The plain date-time the duration reaches from the start's midnight,
    # the days constrained (AddDurationToDateTime).
    h = Ctx.host
    start = midnight(rel)
    out = dt.DateTime()
    d = dt.DateTimeDuration(dt.DateDuration(*date), t)
    h.LAST_MADE[0] = origin
    if Ctx.lib.mlocAddToDateTime(ctypes.byref(start), ctypes.byref(d), 0, ctypes.byref(out)) != 0:
        raise h.range_error("target refused", origin)
    return start, out


def zoned_target(rel, date, t, origin):
    h = Ctx.host
    out = zn.Instant()
    d = dt.DateTimeDuration(dt.DateDuration(*date), t)
    h.LAST_MADE[0] = origin
    if Ctx.lib.mlocAddToZonedDateTime(ctypes.byref(rel.zone), h.CALENDARS[rel.calendar],
                                      ctypes.byref(zn.instant(rel.ns)), ctypes.byref(d), 0,
                                      ctypes.byref(out)) != 0:
        raise h.range_error("target refused", origin)
    return out


def options_object(options, name):
    options = un(options)
    if options is UNDEF:
        raise Ctx.host.type_error(name + " needs options")
    if isinstance(options, str):
        return JSObject({"smallestUnit" if name == "round" else "unit": options})
    if not isinstance(options, JSObject):
        raise Unsupported(name + " options")
    return options


def round_(values, options):
    h = Ctx.host
    options = options_object(options, "round")
    largest = dt.read_unit(options, "largestUnit", None)
    rel = relative_to(options)
    increment = tm.read_increment(options)
    mode = tm.read_mode(options, "halfExpand")
    smallest = dt.read_unit(options, "smallestUnit", None)
    smallest_present = smallest is not None
    if smallest == "auto":
        raise h.range_error("smallestUnit auto")
    if smallest is None:
        smallest = 9
    existing = default_largest(values)
    default = min(existing, smallest)
    largest_present = largest is not None
    if largest is None or largest == "auto":
        largest = default
    if not smallest_present and not largest_present:
        raise h.range_error("no unit given")
    if largest > smallest:
        raise h.range_error("largest smaller than smallest")
    maximum = {4: 24, 5: 60, 6: 60, 7: 1000, 8: 1000, 9: 1000}.get(smallest)
    if maximum is not None and (increment >= maximum or maximum % increment):
        raise h.range_error("increment")
    if increment > 1 and largest != smallest and smallest <= 3:
        raise h.range_error("increment of a date unit")
    r = [largest, smallest, increment, mode]
    if isinstance(rel, zn.ZonedDateTime):
        date, t = internal(values)
        origin = key({"dur": "round", "rel": rel_key(rel), "d": record(date, t), "r": r})
        target = zoned_target(rel, date, t, origin)
        rounding = dt.DateTimeRounding(largest, smallest, increment, h.MODES[mode])
        out = dt.DateTimeDuration()
        if Ctx.lib.mlocZonedDateTimeUntil(ctypes.byref(rel.zone), h.CALENDARS[rel.calendar],
                                          ctypes.byref(zn.instant(rel.ns)), ctypes.byref(target),
                                          ctypes.byref(rounding), ctypes.byref(out)) != 0:
            raise h.range_error("round refused", origin)
        result = from_internal([out.date.years, out.date.months, out.date.weeks, out.date.days],
                               out.time, max(largest, 4), origin)
        return h.DurationObj(result, origin, track=range(10))
    if rel is not None:
        date, t = with_24_hour_days(values)
        origin = key({"dur": "round", "rel": rel_key(rel), "d": record(date, t), "r": r})
        start, target = plain_target(rel, date, t, origin)
        rounding = dt.DateTimeRounding(largest, smallest, increment, h.MODES[mode])
        out = dt.DateTimeDuration()
        if Ctx.lib.mlocDateTimeUntil(ctypes.byref(start), ctypes.byref(target),
                                     ctypes.byref(rounding), ctypes.byref(out)) != 0:
            raise h.range_error("round refused", origin)
        result = from_internal([out.date.years, out.date.months, out.date.weeks, out.date.days],
                               out.time, largest, origin)
        return h.DurationObj(result, origin, track=range(10))
    if existing < 3 or largest < 3:
        raise h.range_error("calendar units need relativeTo")
    date, t = with_24_hour_days(values)
    origin = key({"dur": "round", "rel": ["none"], "d": record(date, t), "r": r})
    out = tm.TimeDuration()
    h.LAST_MADE[0] = origin
    if Ctx.lib.mlocRoundTimeDuration(ctypes.byref(t), smallest - 3, increment, h.MODES[mode],
                                     ctypes.byref(out)) != 0:
        raise h.range_error("round refused", origin)
    return h.DurationObj(from_internal([0, 0, 0, 0], out, largest, origin), origin,
                         track=range(10))


def total(values, options):
    h = Ctx.host
    options = options_object(options, "total")
    rel = relative_to(options)
    unit = dt.read_unit(options, "unit", None)
    if unit is None:
        raise h.range_error("unit required")
    if unit == "auto":
        raise h.range_error("unit auto")
    result = ctypes.c_double()
    if isinstance(rel, zn.ZonedDateTime):
        date, t = internal(values)
        origin = key({"dur": "total", "rel": rel_key(rel), "d": record(date, t), "unit": unit})
        target = zoned_target(rel, date, t, origin)
        if Ctx.lib.mlocZonedDateTimeTotal(ctypes.byref(rel.zone), h.CALENDARS[rel.calendar],
                                          ctypes.byref(zn.instant(rel.ns)), ctypes.byref(target),
                                          unit, ctypes.byref(result)) != 0:
            raise h.range_error("total refused", origin)
    elif rel is not None:
        date, t = with_24_hour_days(values)
        origin = key({"dur": "total", "rel": rel_key(rel), "d": record(date, t), "unit": unit})
        start, target = plain_target(rel, date, t, origin)
        if Ctx.lib.mlocDateTimeTotal(ctypes.byref(start), ctypes.byref(target), unit,
                                     ctypes.byref(result)) != 0:
            raise h.range_error("total refused", origin)
    else:
        if default_largest(values) < 3 or unit < 3:
            raise h.range_error("calendar units need relativeTo")
        date, t = with_24_hour_days(values)
        origin = key({"dur": "total", "rel": ["none"], "d": record(date, t), "unit": unit})
        h.LAST_MADE[0] = origin
        if Ctx.lib.mlocTotalTimeDuration(ctypes.byref(t), unit - 3, ctypes.byref(result)) != 0:
            raise h.range_error("total refused", origin)
    v = result.value
    return Tracked(int(v) if v.is_integer() and abs(v) < 2**53 else v, origin, "total")


def add(values, other, sign):
    h = Ctx.host
    two = tm.duration_values(other)
    if sign < 0:
        two = [-v for v in two]
    largest = min(default_largest(values), default_largest(two))
    if largest < 3:
        raise h.range_error("calendar units")
    _, t1 = with_24_hour_days(values)
    _, t2 = with_24_hour_days(two)
    origin = key({"dur": "add", "one": [t1.seconds, t1.nanoseconds],
                  "two": [t2.seconds, t2.nanoseconds], "largest": largest})
    out = tm.TimeDuration()
    h.LAST_MADE[0] = origin
    if Ctx.lib.mlocAddTimeDurations(ctypes.byref(t1), ctypes.byref(t2), ctypes.byref(out)) != 0:
        raise h.range_error("add refused", origin)
    return h.DurationObj(from_internal([0, 0, 0, 0], out, largest, origin), origin,
                         track=range(10))


def compare(one, two, options):
    h = Ctx.host
    a, b = tm.duration_values(one), tm.duration_values(two)
    options = un(options)
    if options is not UNDEF and not isinstance(options, JSObject):
        raise Unsupported("compare options")
    rel = relative_to(options) if options is not UNDEF else None
    if a == b:
        return 0
    la, lb = default_largest(a), default_largest(b)
    if isinstance(rel, zn.ZonedDateTime) and (la <= 3 or lb <= 3):
        d1, d2 = internal(a), internal(b)
        origin = key({"dur": "compare", "rel": rel_key(rel), "one": record(*d1),
                      "two": record(*d2)})
        x = zoned_target(rel, d1[0], d1[1], origin)
        y = zoned_target(rel, d2[0], d2[1], origin)
        c = (x.seconds, x.nanoseconds)
        e = (y.seconds, y.nanoseconds)
        return Tracked((c > e) - (c < e), origin, "c")
    if la < 3 or lb < 3:
        if rel is None or isinstance(rel, zn.ZonedDateTime):
            raise h.range_error("calendar units need a plain relativeTo")
        d1, d2 = internal(a), internal(b)
        origin = key({"dur": "compare", "rel": rel_key(rel), "one": record(*d1),
                      "two": record(*d2)})
        totals = []
        for date, t in (d1, d2):
            days = date[3]
            if any(date[:3]):
                h.LAST_MADE[0] = origin
                later = h.add_days(rel.calendar, rel.day, date[:3] + [0], "constrain")
                if later is None:
                    raise h.range_error("date refused", origin)
                days += later - rel.day
            totals.append(t.seconds * NS + t.nanoseconds + days * 86400 * NS)
        for v in totals:
            if abs(v) >= 2**53 * NS:
                raise h.range_error("too long", origin)
        return Tracked((totals[0] > totals[1]) - (totals[0] < totals[1]), origin, "c")
    x = a[3] * 86400 * NS + tm_ns(a)
    y = b[3] * 86400 * NS + tm_ns(b)
    return (x > y) - (x < y)


def tm_ns(values):
    units = [3600 * NS, 60 * NS, NS, 10**6, 1000, 1]
    return sum(v * u for v, u in zip(values[4:], units))


def get(duration, prop):
    values = list(duration.values)
    if prop == "round":
        return Builtin(lambda opts=UNDEF: round_(values, opts))
    if prop == "total":
        return Builtin(lambda opts=UNDEF: total(values, opts))
    if prop == "add":
        return Builtin(lambda other=UNDEF, *rest: add(values, other, 1))
    if prop == "subtract":
        return Builtin(lambda other=UNDEF, *rest: add(values, other, -1))
    raise Unsupported("Duration." + prop)


def expected(prop, v):
    v = un(v)
    if prop == "total":
        if isinstance(v, bool) or not isinstance(v, (int, float)):
            raise Unsupported("expected total %r" % (v,))
        return repr(float(v))
    if prop == "c":
        if isinstance(v, float) and v.is_integer():
            v = int(v)
        return v
    raise Unsupported("dur prop " + prop)
