# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Temporal.PlainTime for t262cal.py, on Maul Locale's time functions:
# the constructor and from (property bags), the fields, add and subtract,
# until and since, round, equals and compare. Options are read here, as a
# host reads them; the library makes, adds, differs and rounds. Each value
# an assertion can check carries its origin, a JSON object with key
# "time":
#   {"time": "make", "in": [h, m, s, ms, us, ns], "overflow": o}
#   {"time": "add", "t": [h, m, s, ns], "d": [seconds, nanoseconds]}
#   {"time": "until"|"since", "one": [...], "two": [...],
#    "r": [largest, smallest, increment, mode]}
#   {"time": "round", "t": [...], "r": [unit, increment, mode]}
# with props hour to nanosecond of a time, hours to nanoseconds of a
# duration, or "refused".

import ctypes
import json
import math

import jsmini as js
from jsmini import UNDEF, Unsupported, JSObject, Builtin, Tracked, un

UNITS = {"day": 0, "hour": 1, "minute": 2, "second": 3, "millisecond": 4, "microsecond": 5,
         "nanosecond": 6}
TIME_FIELDS = ["hour", "minute", "second", "millisecond", "microsecond", "nanosecond"]


class Time(ctypes.Structure):
    _fields_ = [("hour", ctypes.c_uint8), ("minute", ctypes.c_uint8), ("second", ctypes.c_uint8),
                ("reserved", ctypes.c_uint8), ("nanosecond", ctypes.c_uint32)]


class TimeInput(ctypes.Structure):
    _fields_ = [(n, ctypes.c_int64) for n in TIME_FIELDS]


class TimeDuration(ctypes.Structure):
    _fields_ = [("seconds", ctypes.c_int64), ("nanoseconds", ctypes.c_int32)]


class DurationFields(ctypes.Structure):
    _fields_ = [(n, ctypes.c_int64) for n in ("days", "hours", "minutes", "seconds",
                                               "milliseconds", "microseconds", "nanoseconds")]


class TimeRounding(ctypes.Structure):
    _fields_ = [("largest", ctypes.c_int), ("smallest", ctypes.c_int),
                ("increment", ctypes.c_uint32), ("mode", ctypes.c_int)]


class Ctx:
    lib = None
    range_error = None
    type_error = None
    last_made = None
    host = None


def setup(lib, range_error, type_error, last_made, host):
    # host: t262cal's module, for its Duration objects and option readers.
    Ctx.lib, Ctx.range_error, Ctx.type_error, Ctx.last_made, Ctx.host = \
        lib, range_error, type_error, last_made, host


def key(o):
    return json.dumps(o, sort_keys=True)


def parts(t):
    return [t.hour, t.minute, t.second, t.nanosecond]


class PlainTime:
    def __init__(self, t, origin):
        self.t = t
        self.origin = origin

    def fields(self):
        n = self.t.nanosecond
        return [self.t.hour, self.t.minute, self.t.second, n // 10**6, n // 1000 % 1000, n % 1000]

    def js_string(self):
        import str262
        return un(str262.time_string(self.t))

    def js_get(self, prop):
        if prop in TIME_FIELDS:
            v = self.fields()[TIME_FIELDS.index(prop)]
            return Tracked(v, self.origin, prop) if self.origin is not None else v
        if prop == "add":
            return Builtin(lambda d=UNDEF, *r: self.add(d, 1))
        if prop == "subtract":
            return Builtin(lambda d=UNDEF, *r: self.add(d, -1))
        if prop == "until":
            return Builtin(lambda o=UNDEF, opts=UNDEF: self.until(o, opts, False))
        if prop == "since":
            return Builtin(lambda o=UNDEF, opts=UNDEF: self.until(o, opts, True))
        if prop == "round":
            return Builtin(lambda opts=UNDEF: self.round(opts))
        if prop == "equals":
            return Builtin(lambda o=UNDEF: parts(self.t) == parts(to_time(o).t))
        if prop in ("toString", "toJSON"):
            import str262
            return {"toString": Builtin(lambda o=UNDEF: str262.time_string(self.t, o)),
                    "toJSON": Builtin(lambda *a: str262.time_string(self.t))}[prop]
        raise Unsupported("PlainTime." + prop)

    def add(self, item, sign):
        values = duration_values(item)
        d = time_duration([sign * v for v in values[4:]])
        out = Time()
        if Ctx.lib.mlocAddToTime(ctypes.byref(self.t), ctypes.byref(d), ctypes.byref(out),
                                 None) != 0:
            raise Unsupported("add refused")
        origin = key({"time": "add", "t": parts(self.t), "d": [d.seconds, d.nanoseconds]})
        return PlainTime(out, origin)

    def until(self, other, options, since):
        other = to_time(other)
        largest, smallest, increment, mode = difference_settings(options)
        operation = "since" if since else "until"
        origin = key({"time": operation, "one": parts(self.t), "two": parts(other.t),
                      "r": [largest, smallest, increment, mode]})
        r = TimeRounding(largest, smallest, increment,
                         Ctx.host.MODES[Ctx.host.NEGATED.get(mode, mode) if since else mode])
        f = DurationFields()
        Ctx.last_made[0] = origin
        if Ctx.lib.mlocTimeUntil(ctypes.byref(self.t), ctypes.byref(other.t), ctypes.byref(r),
                                 ctypes.byref(f)) != 0:
            raise Ctx.range_error("time until refused", origin)
        values = [0, 0, 0, 0, f.hours, f.minutes, f.seconds, f.milliseconds, f.microseconds,
                  f.nanoseconds]
        # Temporal keeps each field as a float64.
        values = [int(float(v)) for v in values]
        return Ctx.host.DurationObj(values, origin, since, track=range(4, 10))

    def round(self, options):
        options = un(options)
        if options is UNDEF:
            raise Ctx.type_error("round needs options")
        if isinstance(options, str):
            options = JSObject({"smallestUnit": options})
        if not isinstance(options, JSObject):
            raise Unsupported("round options")
        increment = read_increment(options)
        mode = read_mode(options, "halfExpand")
        unit = read_unit(options, "smallestUnit", None)
        if unit is None:
            raise Ctx.range_error("smallestUnit required")
        if unit == 0:
            raise Ctx.range_error("day for a time")
        origin = key({"time": "round", "t": parts(self.t), "r": [unit, increment, mode]})
        out = Time()
        Ctx.last_made[0] = origin
        if Ctx.lib.mlocRoundTime(ctypes.byref(self.t), unit, increment, Ctx.host.MODES[mode],
                                 ctypes.byref(out), None) != 0:
            raise Ctx.range_error("round refused", origin)
        return PlainTime(out, origin)


def time_duration(values):
    # A time duration of hours to nanoseconds: mlocMakeTimeDuration when
    # each field fits int64_t, else folded here, as zone.h leaves such a
    # field to the host (ctypes would wrap it silently).
    d = TimeDuration()
    if all(-2**63 <= v < 2**63 for v in values):
        f = DurationFields(0, *values)
        if Ctx.lib.mlocMakeTimeDuration(ctypes.byref(f), ctypes.byref(d)) != 0:
            raise Ctx.range_error("time duration too long")
        return d
    units = [3600 * 10**9, 60 * 10**9, 10**9, 10**6, 1000, 1]
    total = sum(v * u for v, u in zip(values, units))
    if abs(total) >= 2**53 * 10**9:
        raise Ctx.range_error("time duration too long")
    sign = -1 if total < 0 else 1
    d.seconds = sign * (abs(total) // 10**9)
    d.nanoseconds = sign * (abs(total) % 10**9)
    return d


def unsupported(what):
    raise Unsupported(what)


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
        raise Ctx.range_error("unit " + v)
    return UNITS[singular]


def read_increment(options):
    inc = un(options.get("roundingIncrement"))
    if inc is UNDEF:
        return 1
    if isinstance(inc, bool) or not isinstance(inc, (int, float)):
        raise Unsupported("increment type")
    if isinstance(inc, float) and (math.isnan(inc) or math.isinf(inc)):
        raise Ctx.range_error("increment not finite")
    inc = int(inc)
    if inc < 1 or inc > 10**9:
        raise Ctx.range_error("increment out of range")
    return inc


def read_mode(options, default):
    m = un(options.get("roundingMode"))
    if m is UNDEF:
        return default
    if not isinstance(m, str):
        raise Unsupported("mode type")
    if m not in Ctx.host.MODES:
        raise Ctx.range_error("mode " + m)
    return m


def difference_settings(options):
    # GetDifferenceSettings for times, in its order of reads: largest,
    # increment, mode, smallest.
    options = un(options)
    if options is UNDEF or isinstance(options, (js.JSFunction, Builtin)):
        options = JSObject({})
    if options is js.NULL or isinstance(options, (bool, int, float, str)):
        raise Ctx.type_error("options not an object")
    if not isinstance(options, JSObject):
        raise Unsupported("difference options")
    largest = read_unit(options, "largestUnit", "auto")
    increment = read_increment(options)
    mode = read_mode(options, "trunc")
    smallest = read_unit(options, "smallestUnit", 6)
    if largest == 0 or smallest == 0:
        raise Ctx.range_error("day for a time")
    if largest == "auto":
        largest = min(smallest, 1)
    if largest > smallest:
        raise Ctx.range_error("largest smaller than smallest")
    return largest, smallest, increment, mode


def duration_values(item):
    item = un(item)
    if isinstance(item, Ctx.host.DurationObj):
        return list(item.values)
    if isinstance(item, str):
        values = Ctx.host.parse_duration(item)
    elif isinstance(item, JSObject):
        values = []
        seen = False
        for name in Ctx.host.DURATION_FIELDS:
            v = Ctx.host.duration_field(item.get(name))
            seen = seen or v is not None
            values.append(v or 0)
        if not seen:
            raise Ctx.type_error("no duration field")
    else:
        raise Unsupported("duration from %r" % (item,))
    Ctx.host.check_duration(values)
    return values


def make(values, overflow):
    # A field past int64_t is out of range either way; clamped, it stays
    # so.
    values = [max(min(v, 2**63 - 1), -2**63) for v in values]
    origin = key({"time": "make", "in": values, "overflow": overflow})
    inp = TimeInput(*values)
    out = Time()
    Ctx.last_made[0] = origin
    if Ctx.lib.mlocMakeTime(ctypes.byref(inp), 1 if overflow == "reject" else 0,
                            ctypes.byref(out)) != 0:
        raise Ctx.range_error("time refused", origin)
    return PlainTime(out, origin)


def to_time(item, options=UNDEF):
    item = un(item)
    if isinstance(item, PlainTime):
        Ctx.host.read_overflow(options)
        return PlainTime(item.t, None)
    if isinstance(item, JSObject):
        values = []
        seen = False
        for name in TIME_FIELDS:
            v = Ctx.host.to_integer(item.get(name), name)
            seen = seen or v is not None
            values.append(v or 0)
        if not seen:
            raise Ctx.type_error("no time field")
        overflow = Ctx.host.read_overflow(options)
        return make(values, overflow)
    if isinstance(item, str):
        # ParseTemporalTimeString, then the options.
        import ixdtf262
        r = ixdtf262.parse(item, ixdtf262.TIME)
        Ctx.host.read_overflow(options)
        return make(list(r.time), "reject")
    raise Unsupported("PlainTime from %r" % (item,))


class PlainTimeClass:
    def is_instance(self, v):
        return isinstance(un(v), PlainTime)

    def construct(self, args):
        values = []
        for i, name in enumerate(TIME_FIELDS):
            v = Ctx.host.to_integer(args[i], name) if i < len(args) else None
            values.append(v or 0)
        return make(values, "reject")

    def js_get(self, prop):
        if prop == "from":
            return Builtin(lambda item=UNDEF, options=UNDEF: to_time(item, options))
        if prop == "compare":
            def compare(a=UNDEF, b=UNDEF):
                x, y = parts(to_time(a).t), parts(to_time(b).t)
                return (x > y) - (x < y)
            return Builtin(compare)
        raise Unsupported("PlainTime." + prop)


def expected(prop, v):
    v = un(v)
    if isinstance(v, float) and v.is_integer():
        v = int(v)
    if not isinstance(v, int) or isinstance(v, bool):
        raise Unsupported("expected time value %r" % (v,))
    return v


PLAINTIME = PlainTimeClass()
