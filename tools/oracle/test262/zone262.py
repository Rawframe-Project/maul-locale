# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Temporal.ZonedDateTime's time zone parts for t262cal.py, on Maul Locale's
# zone functions: the constructor, timeZoneId, offsetNanoseconds,
# epochNanoseconds, equals, getTimeZoneTransition, withTimeZone, add and
# subtract of time units, and Intl.supportedValuesOf("timeZone"). Each
# value an assertion can check carries its origin, a JSON object with key
# "zone":
#   {"zone": "find", "id": given}                 refused (RangeError)
#   {"zone": "id", "id": given}                   prop "id"
#   {"zone": "offset", "id": given, "ns": "..."}  prop "offset"
#   {"zone": "next"|"previous", "id", "ns"}       prop "at": "..." or None
#   {"zone": "equal", "one": id, "two": id}       prop "equal": 0 or 1

import ctypes
import json

import jsmini as js
from jsmini import UNDEF, NULL, Unsupported, JSObject, JSArray, Builtin, Tracked, un, BigInt

LIMIT = 8_640_000_000_000_000_000_000
NS = 1_000_000_000
TIME_UNITS = {"hours": 3600 * NS, "minutes": 60 * NS, "seconds": NS, "milliseconds": 1_000_000,
              "microseconds": 1000, "nanoseconds": 1}
DATE_UNITS = ("years", "months", "weeks", "days")


class Instant(ctypes.Structure):
    _fields_ = [("seconds", ctypes.c_int64), ("nanoseconds", ctypes.c_int32)]


class TimeZone(ctypes.Structure):
    _fields_ = [("zones", ctypes.c_void_p), ("zonesSize", ctypes.c_uint32),
                ("identifier", ctypes.c_uint16), ("offsetMinutes", ctypes.c_int16)]


class Data(ctypes.Structure):
    _fields_ = [("opaque", ctypes.c_uint64 * 4)]


class DataSet(ctypes.Structure):
    _fields_ = [("sources", ctypes.POINTER(Data)), ("count", ctypes.c_size_t)]


class Output(ctypes.Structure):
    _fields_ = [("buffer", ctypes.c_void_p), ("capacity", ctypes.c_size_t),
                ("length", ctypes.c_size_t), ("part", ctypes.c_void_p), ("context", ctypes.c_void_p)]


class Ctx:
    lib = None
    data = None
    builtin = None
    range_error = None
    type_error = None
    last_made = None


def setup(lib, range_error, type_error, last_made, calendars):
    # calendars: each calendar id the reader knows, lower case, to its
    # canonical id.
    Ctx.calendars = calendars
    Ctx.lib = lib
    Ctx.builtin = Data()
    lib.mlocOpenBuiltinData(ctypes.byref(Ctx.builtin))
    Ctx.data = DataSet(ctypes.pointer(Ctx.builtin), 1)
    Ctx.range_error = range_error
    Ctx.type_error = type_error
    Ctx.last_made = last_made


def key(o):
    return json.dumps(o, sort_keys=True)


def instant(ns):
    s, n = divmod(ns, NS)
    return Instant(s, n)


def find(given):
    z = TimeZone()
    raw = given.encode("utf-8")
    if Ctx.lib.mlocFindTimeZone(ctypes.byref(Ctx.data), raw, len(raw), ctypes.byref(z)) != 0:
        return None
    return z


def write(fn, zone):
    buf = ctypes.create_string_buffer(256)
    out = Output(ctypes.cast(buf, ctypes.c_void_p), 256, 0, None, None)
    if fn(ctypes.byref(zone), ctypes.byref(out)) != 0:
        raise Unsupported("format time zone id")
    return buf.raw[:out.length].decode("utf-8")


def to_zone(given, constructor):
    # ToTemporalTimeZoneIdentifier (the constructor's ParseTimeZoneIdentifier
    # when constructor is set), then the zone found: the identifier, a
    # name or a normalized offset, and the zone.
    import ixdtf262
    if not isinstance(given, str):
        raise Ctx.type_error("time zone not a string")
    name = ixdtf262.time_zone_string(given, identifier_only=constructor)
    z = find(name)
    origin = key({"zone": "find", "id": name})
    if z is None:
        raise Ctx.range_error("time zone " + given, origin)
    Ctx.last_made[0] = origin
    return name, z


def to_ns(v):
    v = un(v)
    if not isinstance(v, BigInt):
        raise Ctx.type_error("epoch nanoseconds not a BigInt")
    if abs(v.v) > LIMIT:
        raise Ctx.range_error("epoch nanoseconds out of range")
    return v.v


class ZonedDateTime:
    def __init__(self, ns, given, zone, origin=None, calendar="iso8601"):
        self.calendar = calendar
        self.ns = ns
        self.given = given
        self.zone = zone
        # The transition this one was found as, if it was.
        self.origin = origin

    def offset_seconds(self):
        out = ctypes.c_int32()
        if Ctx.lib.mlocGetTimeZoneOffset(ctypes.byref(self.zone), ctypes.byref(instant(self.ns)),
                                         ctypes.byref(out)) != 0:
            raise Unsupported("offset")
        return out.value

    def js_string(self):
        import str262
        return un(str262.zoned_string(self))

    def js_get(self, prop):
        if prop == "timeZoneId":
            return Tracked(write(Ctx.lib.mlocFormatTimeZoneId, self.zone),
                           key({"zone": "id", "id": self.given}), "id")
        if prop == "epochNanoseconds":
            v = BigInt(self.ns)
            if self.origin is None:
                return v
            return Tracked(v, self.origin, "ns" if '"zdt"' in self.origin else "at")
        if prop == "epochMilliseconds":
            return self.ns // 1_000_000
        if prop == "offsetNanoseconds":
            return Tracked(self.offset_seconds() * NS,
                           key({"zone": "offset", "id": self.given, "ns": str(self.ns)}), "offset")
        if prop == "calendarId":
            return self.calendar
        if prop == "equals":
            return Builtin(self.equals)
        if prop == "getTimeZoneTransition":
            return Builtin(self.transition)
        if prop == "withTimeZone":
            return Builtin(lambda given=UNDEF: ZonedDateTime(self.ns, *to_zone(un(given), False),
                                                             calendar=self.calendar))
        import zdt262
        return zdt262.get(self, prop)

    def equals(self, other=UNDEF):
        other = un(other)
        if not isinstance(other, ZonedDateTime):
            raise Unsupported("equals with a non-ZonedDateTime")
        if other.ns != self.ns or other.calendar != self.calendar:
            return False
        equal = ctypes.c_bool()
        if Ctx.lib.mlocTimeZonesEqual(ctypes.byref(self.zone), ctypes.byref(other.zone),
                                      ctypes.byref(equal)) != 0:
            raise Unsupported("time zones equal")
        return Tracked(bool(equal.value), key({"zone": "equal", "one": self.given, "two": other.given}),
                       "equal")

    def transition(self, arg=UNDEF):
        arg = un(arg)
        if arg is UNDEF:
            raise Ctx.type_error("direction required")
        if isinstance(arg, (js.JSFunction, Builtin)):
            raise Ctx.range_error("direction required")
        if isinstance(arg, JSObject):
            arg = un(arg.get("direction"))
            if arg is UNDEF:
                raise Ctx.range_error("direction required")
            if not isinstance(arg, str):
                raise Unsupported("direction coerced to a string")
        elif not isinstance(arg, str):
            raise Ctx.type_error("direction not a string or an object")
        if arg not in ("next", "previous"):
            raise Ctx.range_error("direction " + arg)
        origin = key({"zone": arg, "id": self.given, "ns": str(self.ns)})
        out = Instant()
        r = Ctx.lib.mlocGetTimeZoneTransition(ctypes.byref(self.zone), ctypes.byref(instant(self.ns)),
                                              0 if arg == "next" else 1, ctypes.byref(out))
        if r != 0:
            return Tracked(NULL, origin, "at")
        found = ZonedDateTime(out.seconds * NS + out.nanoseconds, self.given, self.zone, origin)
        return Tracked(found, origin, "at")


class ZonedDateTimeClass:
    def is_instance(self, v):
        return isinstance(un(v), ZonedDateTime)

    def construct(self, args):
        args = list(args) + [UNDEF] * 3
        ns = to_ns(args[0])
        given = un(args[1])
        calendar = un(args[2])
        name, zone = to_zone(given, True)
        import ixdtf262
        if calendar is UNDEF:
            calendar = "iso8601"
        elif not isinstance(calendar, str):
            raise Ctx.type_error("calendar not a string")
        return ZonedDateTime(ns, name, zone, calendar=ixdtf262.canonical_calendar(calendar))

    def js_get(self, prop):
        import zdt262
        if prop == "from":
            return Builtin(lambda item=UNDEF, options=UNDEF: zdt262.to_zoned(item, options))
        if prop == "compare":
            def compare(a=UNDEF, b=UNDEF):
                x, y = zdt262.to_zoned(a).ns, zdt262.to_zoned(b).ns
                return (x > y) - (x < y)
            return Builtin(compare)
        raise Unsupported("ZonedDateTime." + prop)


def supported_values_of(what=UNDEF):
    if un(what) != "timeZone":
        raise Unsupported("Intl.supportedValuesOf " + js.jsstr(what))
    names = []
    for i in range(Ctx.lib.mlocGetTimeZoneCount(ctypes.byref(Ctx.data))):
        z = TimeZone()
        if Ctx.lib.mlocGetTimeZone(ctypes.byref(Ctx.data), i, ctypes.byref(z)) != 0:
            raise Unsupported("time zone list")
        names.append(write(Ctx.lib.mlocFormatTimeZoneId, z))
    return JSArray(names)


class IntlNS:
    def js_get(self, prop):
        if prop == "supportedValuesOf":
            return Builtin(supported_values_of)
        raise Unsupported("Intl." + prop)


def expected(prop, v):
    # The value an assertion on a zone's value expects, as a row keeps it.
    v = un(v)
    if prop == "id":
        if not isinstance(v, str):
            raise Unsupported("expected id %r" % (v,))
        return v
    if prop == "offset":
        if isinstance(v, float) and v.is_integer():
            v = int(v)
        if not isinstance(v, int) or isinstance(v, bool):
            raise Unsupported("expected offset %r" % (v,))
        return v
    if prop == "at":
        if v is NULL:
            return None
        if isinstance(v, BigInt):
            return str(v.v)
        if isinstance(v, ZonedDateTime):
            return str(v.ns)
        raise Unsupported("expected transition %r" % (v,))
    if prop == "equal":
        if not isinstance(v, bool):
            raise Unsupported("expected equality %r" % (v,))
        return int(v)
    raise Unsupported("zone prop " + prop)


ZONEDDATETIME = ZonedDateTimeClass()
INTL = IntlNS()
