# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Temporal.Instant for t262cal.py, on Maul Locale's instant functions:
# the constructor, fromEpochMilliseconds and fromEpochNanoseconds,
# epochMilliseconds and epochNanoseconds, add and subtract, until and
# since, round, equals and compare. Options are read here, as a host
# reads them. Each value an assertion can check carries its origin, a
# JSON object with key "inst" (instants as [seconds, nanoseconds] past
# them, floored):
#   {"inst": "add", "at": [s, n], "d": [seconds, nanoseconds]}  prop "ns"
#   {"inst": "until"|"since", "one": [...], "two": [...],
#    "r": [largest, smallest, increment, mode]}  props hours..nanoseconds
#   {"inst": "round", "at": [...], "r": [unit, increment, mode]}  prop "ns"
# or "refused". Units are mlocTimeUnit's (1 hour to 6 nanosecond).

import ctypes
import json

from jsmini import UNDEF, Unsupported, JSObject, Builtin, Tracked, un, BigInt
import jsmini as js
import time262 as tm
import zone262 as zn

NS = 10**9
LIMIT = 8_640_000_000_000_000_000_000


class Ctx:
    lib = None
    host = None


def setup(lib, host):
    Ctx.lib, Ctx.host = lib, host


def key(o):
    return json.dumps(o, sort_keys=True)


def at(ns):
    return list(divmod(ns, NS))


class Instant:
    def __init__(self, ns, origin=None):
        self.ns = ns
        self.origin = origin

    def c(self):
        s, n = divmod(self.ns, NS)
        return zn.Instant(s, n)

    def js_string(self):
        import str262
        return un(str262.instant_string(self.ns))

    def js_get(self, prop):
        h = Ctx.host
        if prop in ("toString", "toJSON"):
            import str262
            return {"toString": Builtin(lambda o=UNDEF: str262.instant_string(self.ns, o)),
                    "toJSON": Builtin(lambda *a: str262.instant_string(self.ns))}[prop]
        if prop == "epochNanoseconds":
            v = BigInt(self.ns)
            return Tracked(v, self.origin, "ns") if self.origin is not None else v
        if prop == "epochMilliseconds":
            return self.ns // 10**6
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
            return Builtin(lambda o=UNDEF: to_instant(o).ns == self.ns)
        raise Unsupported("Instant." + prop)

    def add(self, item, sign):
        h = Ctx.host
        values = tm.duration_values(item)
        if any(values[:4]):
            raise h.range_error("date units for an instant")
        d = tm.time_duration([sign * v for v in values[4:]])
        origin = key({"inst": "add", "at": at(self.ns), "d": [d.seconds, d.nanoseconds]})
        out = zn.Instant()
        h.LAST_MADE[0] = origin
        if Ctx.lib.mlocAddToInstant(ctypes.byref(self.c()), ctypes.byref(d), ctypes.byref(out)) != 0:
            raise h.range_error("add refused", origin)
        return Instant(out.seconds * NS + out.nanoseconds, origin)

    def until(self, other, options, since):
        h = Ctx.host
        other = to_instant(other)
        largest, smallest, increment, mode = settings(options)
        operation = "since" if since else "until"
        origin = key({"inst": operation, "one": at(self.ns), "two": at(other.ns),
                      "r": [largest, smallest, increment, mode]})
        r = tm.TimeRounding(largest, smallest, increment,
                            h.MODES[h.NEGATED.get(mode, mode) if since else mode])
        d = tm.TimeDuration()
        h.LAST_MADE[0] = origin
        if Ctx.lib.mlocInstantUntil(ctypes.byref(self.c()), ctypes.byref(other.c()),
                                    ctypes.byref(r), ctypes.byref(d)) != 0:
            raise h.range_error("until refused", origin)
        f = tm.DurationFields()
        if Ctx.lib.mlocBalanceTimeDuration(ctypes.byref(d), largest, ctypes.byref(f)) == 0:
            values = [f.hours, f.minutes, f.seconds, f.milliseconds, f.microseconds,
                      f.nanoseconds]
        else:
            # Past int64_t: the host forms the largest field from the
            # duration (microseconds or nanoseconds).
            total = d.seconds * NS + d.nanoseconds
            sign = -1 if total < 0 else 1
            if largest == 6:
                values = [0, 0, 0, 0, 0, total]
            else:
                values = [0, 0, 0, 0, sign * (abs(total) // 1000), sign * (abs(total) % 1000)]
        values = [int(float(v)) for v in [0, 0, 0, 0] + values]
        return h.DurationObj(values, origin, since, track=range(4, 10))

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
        if unit == 0:
            raise h.range_error("day for an instant")
        origin = key({"inst": "round", "at": at(self.ns), "r": [unit, increment, mode]})
        out = zn.Instant()
        h.LAST_MADE[0] = origin
        if Ctx.lib.mlocRoundInstant(ctypes.byref(self.c()), unit, increment, h.MODES[mode],
                                    ctypes.byref(out)) != 0:
            raise h.range_error("round refused", origin)
        return Instant(out.seconds * NS + out.nanoseconds, origin)


def settings(options):
    h = Ctx.host
    options = un(options)
    if options is UNDEF or isinstance(options, (js.JSFunction, Builtin)):
        options = JSObject({})
    if not isinstance(options, JSObject):
        raise Unsupported("difference options")
    largest = tm.read_unit(options, "largestUnit", "auto")
    increment = tm.read_increment(options)
    mode = tm.read_mode(options, "trunc")
    smallest = tm.read_unit(options, "smallestUnit", 6)
    if largest == 0 or smallest == 0:
        raise h.range_error("day for an instant")
    if largest == "auto":
        largest = min(smallest, 3)
    if largest > smallest:
        raise h.range_error("largest smaller than smallest")
    return largest, smallest, increment, mode


def checked(ns):
    if not isinstance(ns, int) or abs(ns) > LIMIT:
        raise Ctx.host.range_error("instant out of range")
    return Instant(ns)


def to_instant(item):
    item = un(item)
    if isinstance(item, Instant):
        return item
    if isinstance(item, zn.ZonedDateTime):
        return Instant(item.ns)
    if isinstance(item, str):
        # ToTemporalInstant of a string: its date-time less its offset.
        import ixdtf262
        r = ixdtf262.parse(item, ixdtf262.INSTANT)
        return Instant(ixdtf262.exact_nanoseconds(r, 0 if r.utc else r.offset))
    raise Unsupported("Instant from %r" % (item,))


class InstantClass:
    def is_instance(self, v):
        return isinstance(un(v), Instant)

    def construct(self, args):
        args = list(args) + [UNDEF]
        v = un(args[0])
        if not isinstance(v, BigInt):
            raise Unsupported("Instant of a non-BigInt")
        return checked(v.v)

    def js_get(self, prop):
        h = Ctx.host
        if prop == "fromEpochNanoseconds":
            def from_ns(v=UNDEF):
                v = un(v)
                if not isinstance(v, BigInt):
                    raise Unsupported("fromEpochNanoseconds of a non-BigInt")
                return checked(v.v)
            return Builtin(from_ns)
        if prop == "fromEpochMilliseconds":
            def from_ms(v=UNDEF):
                v = un(v)
                if isinstance(v, float) and v.is_integer():
                    v = int(v)
                if not isinstance(v, int) or isinstance(v, bool):
                    raise Unsupported("fromEpochMilliseconds of %r" % (v,))
                return checked(v * 10**6)
            return Builtin(from_ms)
        if prop == "compare":
            def compare(a=UNDEF, b=UNDEF):
                x, y = to_instant(a).ns, to_instant(b).ns
                return (x > y) - (x < y)
            return Builtin(compare)
        raise Unsupported("Instant." + prop)


def expected(prop, v):
    v = un(v)
    if prop == "ns":
        if not isinstance(v, BigInt):
            raise Unsupported("expected instant %r" % (v,))
        return str(v.v)
    if isinstance(v, float) and v.is_integer():
        v = int(v)
    if not isinstance(v, int) or isinstance(v, bool):
        raise Unsupported("expected value %r" % (v,))
    return v


INSTANT = InstantClass()
