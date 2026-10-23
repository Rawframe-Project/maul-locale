# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Temporal.PlainDateTime for t262cal.py, on Maul Locale's date-time
# functions: the constructor and from (property bags), the date and time
# fields, add and subtract, until and since, round, equals and compare.
# Options are read here, as a host reads them. Each value an assertion can
# check carries its origin, a JSON object with keys "dt" and "calendar":
#   {"dt": "make", "iso": [y, m, d], "t": [h, mi, s, ms, us, ns]}
#   {"dt": "add", "at": [day, h, mi, s, ns], "d": [y, mo, w, d, s, ns],
#    "overflow": o}
#   {"dt": "until"|"since", "one": [...], "two": [...],
#    "r": [largest, smallest, increment, mode]}
#   {"dt": "round", "at": [...], "r": [unit, increment, mode]}
# with props of a date (year, month, monthCode, day, era, eraYear), of a
# time (hour to nanosecond), of a duration (years to nanoseconds), or
# "refused". A day is mlocDate's count.

import ctypes
import json

from jsmini import UNDEF, Unsupported, JSObject, Builtin, Tracked, un
import jsmini as js
import time262 as tm

DATE_FIELDS = ["year", "month", "monthCode", "day", "era", "eraYear"]


class Date(ctypes.Structure):
    _fields_ = [("day", ctypes.c_int32), ("calendar", ctypes.c_uint8),
                ("reserved", ctypes.c_uint8 * 3)]


class DateTime(ctypes.Structure):
    _fields_ = [("date", Date), ("time", tm.Time)]


class DateDuration(ctypes.Structure):
    _fields_ = [(n, ctypes.c_int64) for n in ("years", "months", "weeks", "days")]


class DateTimeDuration(ctypes.Structure):
    _fields_ = [("date", DateDuration), ("time", tm.TimeDuration)]


class DateTimeRounding(ctypes.Structure):
    _fields_ = [("largest", ctypes.c_int), ("smallest", ctypes.c_int),
                ("increment", ctypes.c_uint32), ("mode", ctypes.c_int)]


UNITS = {"year": 0, "month": 1, "week": 2, "day": 3, "hour": 4, "minute": 5, "second": 6,
         "millisecond": 7, "microsecond": 8, "nanosecond": 9}


class Ctx:
    lib = None
    host = None


def setup(lib, host):
    Ctx.lib, Ctx.host = lib, host


def key(o):
    return json.dumps(o, sort_keys=True)


class PlainDateTime:
    def __init__(self, calendar, value, origin):
        self.calendar = calendar
        self.value = value
        self.origin = origin

    def at(self):
        return [self.value.date.day] + tm.parts(self.value.time)

    def js_string(self):
        import str262
        return un(str262.date_time_string(self.calendar, self.value))

    def js_get(self, prop):
        h = Ctx.host
        if prop in ("toString", "toJSON"):
            import str262
            return {"toString": Builtin(lambda o=UNDEF: str262.date_time_string(self.calendar,
                                                                                self.value, o)),
                    "toJSON": Builtin(lambda *a: str262.date_time_string(self.calendar,
                                                                         self.value))}[prop]
        if prop in DATE_FIELDS:
            date = h.PlainDate(self.calendar, self.value.date.day, self.origin)
            return date.js_get(prop)
        if prop in tm.TIME_FIELDS:
            v = tm.PlainTime(self.value.time, None).fields()[tm.TIME_FIELDS.index(prop)]
            return Tracked(v, self.origin, prop) if self.origin is not None else v
        if prop in ("dayOfWeek", "dayOfYear", "daysInMonth", "daysInYear", "monthsInYear",
                    "inLeapYear", "daysInWeek"):
            return h.PlainDate(self.calendar, self.value.date.day, None).js_get(prop)
        if prop == "calendarId":
            return self.calendar
        if prop == "add":
            return Builtin(lambda d=UNDEF, o=UNDEF: self.add(d, o, 1))
        if prop == "subtract":
            return Builtin(lambda d=UNDEF, o=UNDEF: self.add(d, o, -1))
        if prop == "until":
            return Builtin(lambda o=UNDEF, opts=UNDEF: self.until(o, opts, False))
        if prop == "since":
            return Builtin(lambda o=UNDEF, opts=UNDEF: self.until(o, opts, True))
        if prop == "round":
            return Builtin(lambda opts=UNDEF: self.round(opts))
        if prop == "equals":
            def equals(other=UNDEF):
                other = to_date_time(other)
                return self.at() == other.at() and self.calendar == other.calendar
            return Builtin(equals)
        if prop == "toPlainTime":
            return Builtin(lambda: tm.PlainTime(self.value.time, None))
        raise Unsupported("PlainDateTime." + prop)

    def add(self, item, options, sign):
        values = tm.duration_values(item)
        if sign < 0:
            values = [-v for v in values]
        overflow = Ctx.host.read_overflow(options)
        t = tm.time_duration(values[4:])
        d = DateTimeDuration(DateDuration(*values[:4]), t)
        origin = key({"dt": "add", "calendar": self.calendar, "at": self.at(),
                      "d": values[:4] + [t.seconds, t.nanoseconds], "overflow": overflow})
        out = DateTime()
        Ctx.host.LAST_MADE[0] = origin
        if Ctx.lib.mlocAddToDateTime(ctypes.byref(self.value), ctypes.byref(d),
                                     1 if overflow == "reject" else 0, ctypes.byref(out)) != 0:
            raise Ctx.host.range_error("add refused", origin)
        return PlainDateTime(self.calendar, out, origin)

    def until(self, other, options, since):
        h = Ctx.host
        other = to_date_time(other)
        if other.calendar != self.calendar:
            raise h.range_error("calendars differ")
        largest, smallest, increment, mode = settings(options)
        operation = "since" if since else "until"
        origin = key({"dt": operation, "calendar": self.calendar, "one": self.at(),
                      "two": other.at(), "r": [largest, smallest, increment, mode]})
        r = DateTimeRounding(largest, smallest, increment,
                             h.MODES[h.NEGATED.get(mode, mode) if since else mode])
        d = DateTimeDuration()
        h.LAST_MADE[0] = origin
        if Ctx.lib.mlocDateTimeUntil(ctypes.byref(self.value), ctypes.byref(other.value),
                                     ctypes.byref(r), ctypes.byref(d)) != 0:
            raise h.range_error("until refused", origin)
        # TemporalDurationFromInternal: the time part balanced to the
        # largest unit, days for a date unit and added to the date's.
        f = tm.DurationFields()
        unit = max(largest, 3) - 3
        if Ctx.lib.mlocBalanceTimeDuration(ctypes.byref(d.time), unit, ctypes.byref(f)) != 0:
            raise Unsupported("balance past int64")
        values = [d.date.years, d.date.months, d.date.weeks, d.date.days + f.days, f.hours,
                  f.minutes, f.seconds, f.milliseconds, f.microseconds, f.nanoseconds]
        # Temporal keeps each field as a float64.
        values = [int(float(v)) for v in values]
        return h.DurationObj(values, origin, since, track=range(10))

    def round(self, options):
        h = Ctx.host
        options = un(options)
        if options is UNDEF:
            raise h.type_error("round needs options")
        if isinstance(options, str):
            options = JSObject({"smallestUnit": options})
        if not isinstance(options, JSObject):
            raise Unsupported("round options")
        increment = tm.read_increment(options)
        mode = tm.read_mode(options, "halfExpand")
        unit = tm.read_unit(options, "smallestUnit", None)
        if unit is None:
            raise h.range_error("smallestUnit required")
        origin = key({"dt": "round", "calendar": self.calendar, "at": self.at(),
                      "r": [unit, increment, mode]})
        out = DateTime()
        h.LAST_MADE[0] = origin
        if Ctx.lib.mlocRoundDateTime(ctypes.byref(self.value), unit, increment, h.MODES[mode],
                                     ctypes.byref(out)) != 0:
            raise h.range_error("round refused", origin)
        return PlainDateTime(self.calendar, out, origin)


def settings(options):
    # GetDifferenceSettings for date-times: largest, increment, mode,
    # smallest; the largest's default is day or the smallest if larger.
    h = Ctx.host
    options = un(options)
    if options is UNDEF or isinstance(options, (js.JSFunction, Builtin)):
        options = JSObject({})
    if not isinstance(options, JSObject):
        raise Unsupported("difference options")
    largest = read_unit(options, "largestUnit", "auto")
    increment = tm.read_increment(options)
    mode = tm.read_mode(options, "trunc")
    smallest = read_unit(options, "smallestUnit", 9)
    if largest == "auto":
        largest = min(smallest, 3)
    if largest > smallest:
        raise h.range_error("largest smaller than smallest")
    return largest, smallest, increment, mode


def read_unit(options, name, default):
    v = un(options.get(name))
    if v is UNDEF:
        return default
    if not isinstance(v, str):
        raise Unsupported("unit option type")
    if v == "auto" and name == "largestUnit":
        return "auto"
    singular = v[:-1] if v.endswith("s") else v
    if singular not in UNITS:
        raise Ctx.host.range_error("unit " + v)
    return UNITS[singular]


def make(calendar, iso, time_values):
    h = Ctx.host
    origin = key({"dt": "make", "calendar": calendar, "iso": iso, "t": time_values})
    h.LAST_MADE[0] = origin
    y, m, d = iso
    day = h.make("gregory", None, y, None, m, d, "reject") \
        if m >= 1 and d >= 1 and -2**31 <= y < 2**31 else None
    t = tm.Time()
    values = [max(min(v, 2**63 - 1), -2**63) for v in time_values]
    if day is None or Ctx.lib.mlocMakeTime(ctypes.byref(tm.TimeInput(*values)), 1,
                                           ctypes.byref(t)) != 0:
        raise h.range_error("date-time refused", origin)
    out = DateTime()
    date = Date(day, h.CALENDARS[calendar])
    if Ctx.lib.mlocMakeDateTime(ctypes.byref(date), ctypes.byref(t), ctypes.byref(out)) != 0:
        raise h.range_error("date-time refused", origin)
    return PlainDateTime(calendar, out, origin)


def to_date_time(item, options=UNDEF):
    # ToTemporalDateTime of a date-time or a property bag. A bag's date
    # comes through the calendar's fields (t262cal's PlainDate.from), its
    # time through mlocMakeTime; the result is not tracked, its date and
    # time being what those make.
    h = Ctx.host
    item = un(item)
    if isinstance(item, PlainDateTime):
        if options is not UNDEF:
            h.read_overflow(options)
        return PlainDateTime(item.calendar, item.value, item.origin)
    if isinstance(item, str):
        # ToTemporalDateTime of a string: its date and time, midnight when
        # it gives none, in its calendar.
        import ixdtf262
        r = ixdtf262.parse(item, ixdtf262.DATE_TIME)
        calendar = ixdtf262.canonical_calendar(r.calendar)
        h.read_overflow(options)
        return make(calendar, [r.year, r.month, r.day], list(r.time or (0,) * 6))
    if not isinstance(item, JSObject):
        raise Unsupported("PlainDateTime from %r" % (item,))
    values = [h.to_integer(item.get(n), n) or 0 for n in tm.TIME_FIELDS]
    date = h.plain_date_from(item, options)
    overflow = h.read_overflow(options)
    values = [max(min(v, 2**63 - 1), -2**63) for v in values]
    t = tm.Time()
    if Ctx.lib.mlocMakeTime(ctypes.byref(tm.TimeInput(*values)), 1 if overflow == "reject" else 0,
                            ctypes.byref(t)) != 0:
        raise h.range_error("time refused")
    out = DateTime()
    if Ctx.lib.mlocMakeDateTime(ctypes.byref(Date(date.day, h.CALENDARS[date.calendar])),
                                ctypes.byref(t), ctypes.byref(out)) != 0:
        raise h.range_error("date-time out of limits")
    return PlainDateTime(date.calendar, out, None)


class PlainDateTimeClass:
    def is_instance(self, v):
        return isinstance(un(v), PlainDateTime)

    def construct(self, args):
        h = Ctx.host
        args = list(args) + [UNDEF] * 10
        iso = [h.to_integer(args[i], "iso") for i in range(3)]
        if None in iso:
            raise Unsupported("PlainDateTime without a date")
        values = [h.to_integer(args[3 + i], "time") or 0 for i in range(6)]
        calendar = h.constructor_calendar(args[9])
        return make(calendar, iso, values)

    def js_get(self, prop):
        if prop == "from":
            return Builtin(lambda item=UNDEF, options=UNDEF: to_date_time(item, options))
        if prop == "compare":
            def compare(a=UNDEF, b=UNDEF):
                x, y = to_date_time(a).at(), to_date_time(b).at()
                return (x > y) - (x < y)
            return Builtin(compare)
        raise Unsupported("PlainDateTime." + prop)


PLAINDATETIME = PlainDateTimeClass()
