# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Temporal's toString and toJSON for t262cal.py, on Maul Locale's writers
# (maul-locale/ixdtf.h): the options each reads (calendarName,
# fractionalSecondDigits, offset, roundingMode, smallestUnit, timeZone,
# timeZoneName), the rounding before writing through the library's own
# rounding, and the string, tracked as a row: the writer, its value and
# options, and the text test262 expects.

import ctypes
import json
import math

import jsmini as js
from jsmini import UNDEF, Unsupported, JSObject, Tracked, un

NS = 10**9
SHOW_CALENDAR = {"auto": 0, "always": 1, "never": 2, "critical": 3}
SHOW_TIME_ZONE = {"auto": 0, "never": 1, "critical": 2}
AUTO, MINUTE = -1, -2
# mlocTimeUnit: a day, then an hour to a nanosecond.
TIME_UNITS = {"day": 0, "hour": 1, "minute": 2, "second": 3, "millisecond": 4, "microsecond": 5,
              "nanosecond": 6}
MODES = {"halfExpand": 0, "halfEven": 1, "halfTrunc": 2, "halfCeil": 3, "halfFloor": 4, "ceil": 5,
         "floor": 6, "expand": 7, "trunc": 8}


class Options(ctypes.Structure):
    _fields_ = [("calendar", ctypes.c_char_p), ("calendarLength", ctypes.c_size_t),
                ("secondsPrecision", ctypes.c_uint8), ("fractionDigits", ctypes.c_uint8),
                ("showCalendar", ctypes.c_uint8), ("showTimeZone", ctypes.c_uint8),
                ("hideOffset", ctypes.c_bool)]


class Output(ctypes.Structure):
    _fields_ = [("buffer", ctypes.c_void_p), ("capacity", ctypes.c_size_t),
                ("length", ctypes.c_size_t), ("part", ctypes.c_void_p), ("context", ctypes.c_void_p)]


class Ctx:
    lib = None
    host = None


def setup(lib, host):
    Ctx.lib, Ctx.host = lib, host


def key(o):
    return json.dumps(o, sort_keys=True)


def write(fn, *args):
    buf = ctypes.create_string_buffer(512)
    out = Output(ctypes.cast(buf, ctypes.c_void_p), 512, 0, None, None)
    r = fn(*args, ctypes.byref(out))
    if r != 0:
        raise Unsupported("writer refused (%d)" % r)
    return buf.raw[:out.length].decode("utf-8")


def options_struct(calendar="iso8601", precision=AUTO, show_calendar="auto", show_time_zone="auto",
                   hide_offset=False):
    raw = calendar.encode("ascii")
    seconds = 0 if precision == AUTO else 1 if precision == MINUTE else 2
    o = Options(raw, len(raw), seconds, max(precision, 0), SHOW_CALENDAR[show_calendar],
                SHOW_TIME_ZONE[show_time_zone], hide_offset)
    o.precision = precision
    return o


def tracked(text, kind, value, opts):
    o = [opts.calendar.decode("ascii"), opts.precision, opts.showCalendar, opts.showTimeZone,
         int(opts.hideOffset)]
    return Tracked(text, key({"str": kind, "v": value, "o": o}), "text")


def expected(prop, v):
    v = un(v)
    if prop != "text" or not isinstance(v, str):
        raise Unsupported("expected string %r" % (v,))
    return v


# Options.

def options_object(options):
    options = un(options)
    if options is UNDEF:
        return None
    if not isinstance(options, JSObject):
        if options is js.NULL or isinstance(options, (bool, int, float, str)):
            raise Ctx.host.type_error("options not an object")
        raise Unsupported("options type")
    return options


def string_option(options, name, choices, default):
    if options is None:
        return default
    v = un(options.get(name))
    if v is UNDEF:
        return default
    if not isinstance(v, str):
        raise Unsupported(name + " option type")
    if v not in choices:
        raise Ctx.host.range_error("%s %s" % (name, v))
    return v


def digits_option(options):
    # GetTemporalFractionalSecondDigitsOption.
    if options is None:
        return AUTO
    v = un(options.get("fractionalSecondDigits"))
    if v is UNDEF:
        return AUTO
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        if isinstance(v, str):
            if v != "auto":
                raise Ctx.host.range_error("fractionalSecondDigits " + v)
            return AUTO
        raise Unsupported("fractionalSecondDigits type")
    if isinstance(v, float) and (math.isnan(v) or math.isinf(v)):
        raise Ctx.host.range_error("fractionalSecondDigits not finite")
    n = math.floor(v)
    if n < 0 or n > 9:
        raise Ctx.host.range_error("fractionalSecondDigits out of range")
    return int(n)


SINGULAR = {u + "s": u for u in TIME_UNITS}
SINGULAR.update({u: u for u in TIME_UNITS})
DATE_UNITS = {"year", "years", "month", "months", "week", "weeks"}


def smallest_option(options):
    if options is None:
        return None
    v = un(options.get("smallestUnit"))
    if v is UNDEF:
        return None
    if not isinstance(v, str):
        raise Unsupported("smallestUnit type")
    if v in DATE_UNITS or v not in SINGULAR or SINGULAR[v] in ("day", "hour"):
        raise Ctx.host.range_error("smallestUnit " + v)
    return SINGULAR[v]


def precision_record(smallest, digits):
    # ToSecondsStringPrecisionRecord: (precision, unit, increment).
    fixed = {"minute": (MINUTE, "minute", 1), "second": (0, "second", 1),
             "millisecond": (3, "millisecond", 1), "microsecond": (6, "microsecond", 1),
             "nanosecond": (9, "nanosecond", 1)}
    if smallest is not None:
        return fixed[smallest]
    if digits == AUTO:
        return AUTO, "nanosecond", 1
    if digits == 0:
        return 0, "second", 1
    unit = ("millisecond", "microsecond", "nanosecond")[(digits - 1) // 3]
    return digits, unit, 10 ** (2 - (digits - 1) % 3)


def read_common(options, calendar_name=False, offset=False, time_zone_name=False, time_zone=False,
                seconds=True):
    # The options in the order each toString reads them, alphabetical; a
    # date's reads calendarName alone.
    o = options_object(options)
    show = string_option(o, "calendarName", SHOW_CALENDAR, "auto") if calendar_name else "auto"
    if not seconds:
        return {"show": show}
    digits = digits_option(o)
    hide = string_option(o, "offset", ("auto", "never"), "auto") == "never" if offset else False
    mode = string_option(o, "roundingMode", MODES, "trunc")
    smallest = smallest_option(o)
    zone = None
    if time_zone and o is not None:
        given = un(o.get("timeZone"))
        if given is not UNDEF:
            import zone262
            zone = zone262.to_zone(given, False)
    tz_show = string_option(o, "timeZoneName", SHOW_TIME_ZONE, "auto") if time_zone_name else "auto"
    precision, unit, increment = precision_record(smallest, digits)
    return {"show": show, "hide": hide, "mode": mode, "precision": precision, "unit": unit,
            "increment": increment, "zone": zone, "tz_show": tz_show}


# The toString of each type.

def date_string(date, options=UNDEF, partial=None):
    # TemporalDateToString, TemporalYearMonthToString or
    # TemporalMonthDayToString, of a date with day and calendar.
    r = read_common(options, calendar_name=True, seconds=False)
    import datetime262 as dt
    opts = options_struct(date.calendar, AUTO, r["show"])
    d = dt.Date(date.day, Ctx.host.CALENDARS[date.calendar])
    fn = {None: Ctx.lib.mlocFormatTemporalDate, "ym": Ctx.lib.mlocFormatYearMonth,
          "md": Ctx.lib.mlocFormatMonthDay}[partial]
    text = write(fn, ctypes.byref(d), ctypes.byref(opts))
    return tracked(text, partial or "date", [date.day], opts)


def time_string(t, options=UNDEF):
    r = read_common(options)
    import time262 as tm
    rounded = tm.Time()
    days = ctypes.c_int64()
    if Ctx.lib.mlocRoundTime(ctypes.byref(t), TIME_UNITS[r["unit"]], r["increment"], MODES[r["mode"]],
                             ctypes.byref(rounded), ctypes.byref(days)) != 0:
        raise Unsupported("round time")
    opts = options_struct(precision=r["precision"])
    text = write(Ctx.lib.mlocFormatTemporalTime, ctypes.byref(rounded), ctypes.byref(opts))
    return tracked(text, "time", time_value(rounded), opts)


def time_value(t):
    return [t.hour, t.minute, t.second, t.nanosecond]


def date_time_string(calendar, value, options=UNDEF):
    r = read_common(options, calendar_name=True)
    import datetime262 as dt
    rounded = dt.DateTime()
    if Ctx.lib.mlocRoundDateTime(ctypes.byref(value), TIME_UNITS[r["unit"]], r["increment"],
                                 MODES[r["mode"]], ctypes.byref(rounded)) != 0:
        raise Ctx.host.range_error("rounded date-time out of range")
    opts = options_struct(calendar, r["precision"], r["show"])
    text = write(Ctx.lib.mlocFormatTemporalDateTime, ctypes.byref(rounded), ctypes.byref(opts))
    return tracked(text, "dt", [rounded.date.day] + time_value(rounded.time), opts)


def rounded_instant(ns, r):
    import zone262 as zn
    out = zn.Instant()
    if Ctx.lib.mlocRoundInstant(ctypes.byref(zn.instant(ns)), TIME_UNITS[r["unit"]], r["increment"],
                                MODES[r["mode"]], ctypes.byref(out)) != 0:
        raise Ctx.host.range_error("rounded instant out of range")
    return out


def instant_string(ns, options=UNDEF):
    r = read_common(options, time_zone=True)
    out = rounded_instant(ns, r)
    opts = options_struct(precision=r["precision"])
    zone = r["zone"]
    text = write(Ctx.lib.mlocFormatInstant, ctypes.byref(out),
                 ctypes.byref(zone[1]) if zone else None, ctypes.byref(opts))
    return tracked(text, "inst", [out.seconds, out.nanoseconds, zone[0] if zone else None], opts)


def zoned_string(z, options=UNDEF):
    r = read_common(options, calendar_name=True, offset=True, time_zone_name=True)
    out = rounded_instant(z.ns, r)
    opts = options_struct(z.calendar, r["precision"], r["show"], r["tz_show"], r["hide"])
    text = write(Ctx.lib.mlocFormatZonedDateTime, ctypes.byref(z.zone), ctypes.byref(out),
                 ctypes.byref(opts))
    return tracked(text, "zdt", [z.given, out.seconds, out.nanoseconds], opts)


def offset_string(z):
    seconds = z.offset_seconds()
    text = write(Ctx.lib.mlocFormatUTCOffset, ctypes.c_int64(seconds * NS))
    return Tracked(text, key({"str": "offset", "v": [seconds * NS], "o": []}), "text")


def duration_string(values, options=UNDEF):
    # Duration.prototype.toString: the time part rounded, balanced up to
    # the larger of the duration's largest unit and seconds.
    r = read_common(options)
    if r["precision"] == MINUTE:
        raise Ctx.host.range_error("smallestUnit minute for a duration")
    import dur262
    import ixdtf262
    v = list(values)
    if r["unit"] != "nanosecond" or r["increment"] != 1:
        v = dur262.round_time_part(v, TIME_UNITS[r["unit"]], r["increment"], MODES[r["mode"]])
    dv = ixdtf262.DurationValues(*[float(x) for x in v])
    opts = options_struct(precision=r["precision"])
    text = write(Ctx.lib.mlocFormatTemporalDuration, ctypes.byref(dv), ctypes.byref(opts))
    return tracked(text, "dur", [str(x) for x in v], opts)
