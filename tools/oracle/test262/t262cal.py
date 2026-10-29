# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Runs test262's Temporal calendar tests on Maul Locale and records each
# assertion they make about a date made from fields as a row: the fields in,
# and the result test262 expects (refused, or a field's value). Its
# Temporal.ZonedDateTime tests' time zone assertions go through zone262.py.
#
# usage: python3 -I t262cal.py <libmaul-locale.so> <test262 root> <out.json> <test.js>...

import ctypes
import re
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jsmini as js  # noqa: E402
import zone262  # noqa: E402
import time262  # noqa: E402
import datetime262  # noqa: E402
import instant262  # noqa: E402
import zdt262  # noqa: E402
import dur262  # noqa: E402
import ixdtf262  # noqa: E402
import str262  # noqa: E402
from jsmini import UNDEF, NULL, Unsupported, JSThrow, JSObject, JSArray, Builtin, Tracked, un  # noqa: E402

CALENDARS = {
    "gregory": 0, "buddhist": 2, "roc": 3, "japanese": 4, "coptic": 5, "ethiopic": 6,
    "ethioaa": 7, "iso8601": 0, "indian": 8, "persian": 9, "hebrew": 10, "islamic-civil": 11,
    "islamic-tbla": 12, "islamic-umalqura": 13, "chinese": 16, "dangi": 17,
}
ALIASES = {"ethiopic-amete-alem": "ethioaa", "islamicc": "islamic-civil"}
NO_ERAS = {"chinese", "dangi", "iso8601"}
ERA_ALIASES = {"gregory": {"ad": "ce", "bc": "bce"}, "japanese": {"ad": "ce", "bc": "bce"}}


class Input(ctypes.Structure):
    _fields_ = [("era", ctypes.c_char_p), ("eraLength", ctypes.c_size_t), ("year", ctypes.c_int32),
                ("monthCode", ctypes.c_char_p), ("monthCodeLength", ctypes.c_size_t),
                ("month", ctypes.c_uint32), ("day", ctypes.c_uint32)]


class Date(ctypes.Structure):
    _fields_ = [("day", ctypes.c_int32), ("calendar", ctypes.c_uint8)]


class Fields(ctypes.Structure):
    _fields_ = [("year", ctypes.c_int32), ("eraYear", ctypes.c_int32), ("era", ctypes.c_char * 16),
                ("eraLength", ctypes.c_uint8), ("monthCode", ctypes.c_char * 5),
                ("monthCodeLength", ctypes.c_uint8), ("month", ctypes.c_uint8), ("day", ctypes.c_uint8),
                ("dayOfWeek", ctypes.c_uint8), ("daysInMonth", ctypes.c_uint8),
                ("monthsInYear", ctypes.c_uint8), ("inLeapYear", ctypes.c_uint8),
                ("dayOfYear", ctypes.c_uint16), ("daysInYear", ctypes.c_uint16)]


class Duration(ctypes.Structure):
    _fields_ = [("years", ctypes.c_int64), ("months", ctypes.c_int64), ("weeks", ctypes.c_int64),
                ("days", ctypes.c_int64)]


class Changes(ctypes.Structure):
    _fields_ = [("given", ctypes.c_uint32), ("era", ctypes.c_char_p), ("eraLength", ctypes.c_size_t),
                ("eraYear", ctypes.c_int32), ("year", ctypes.c_int32), ("monthCode", ctypes.c_char_p),
                ("monthCodeLength", ctypes.c_size_t), ("month", ctypes.c_uint32), ("day", ctypes.c_uint32)]


GIVEN = {"era": 1, "eraYear": 2, "year": 4, "month": 8, "monthCode": 16, "day": 32}


def make_changes(f):
    c = Changes()
    keep = []
    for k, bit in GIVEN.items():
        if f.get(k) is None:
            continue
        c.given |= bit
        if k in ("era", "monthCode"):
            b = f[k].encode("utf-8")
            keep.append(b)
            setattr(c, k, b)
            setattr(c, k + "Length", len(b))
        else:
            if not (-2**31 <= f[k] < 2**31) or (k in ("month", "day") and f[k] < 0):
                raise Unsupported("field beyond 32 bits")
            setattr(c, k, f[k])
    return c, keep


lib = None


UNITS = {"year": 0, "years": 0, "month": 1, "months": 1, "week": 2, "weeks": 2, "day": 3, "days": 3}
TIME_UNITS = {"hour", "hours", "minute", "minutes", "second", "seconds", "millisecond", "milliseconds",
              "microsecond", "microseconds", "nanosecond", "nanoseconds"}


MODES = {"halfExpand": 0, "halfEven": 1, "halfTrunc": 2, "halfCeil": 3, "halfFloor": 4, "ceil": 5,
         "floor": 6, "expand": 7, "trunc": 8}
NEGATED = {"ceil": "floor", "floor": "ceil", "halfCeil": "halfFloor", "halfFloor": "halfCeil"}


class Rounding(ctypes.Structure):
    _fields_ = [("largestUnit", ctypes.c_int), ("smallestUnit", ctypes.c_int),
                ("increment", ctypes.c_uint32), ("mode", ctypes.c_int)]


def until_rounded(calendar, one, two, settings):
    a = Date()
    a.day = one
    a.calendar = CALENDARS[calendar]
    b = Date()
    b.day = two
    b.calendar = CALENDARS[calendar]
    out = Duration()
    r = lib.mlocDateUntilRounded(ctypes.byref(a), ctypes.byref(b),
                                 ctypes.byref(Rounding(settings[0], settings[1], settings[2], MODES[settings[3]])),
                                 ctypes.byref(out))
    return None if r != 0 else [out.years, out.months, out.weeks, out.days]


def until_days(calendar, one, two, unit):
    a = Date()
    a.day = one
    a.calendar = CALENDARS[calendar]
    b = Date()
    b.day = two
    b.calendar = CALENDARS[calendar]
    out = Duration()
    r = lib.mlocDateUntil(ctypes.byref(a), ctypes.byref(b), unit, ctypes.byref(out))
    if r != 0:
        raise Unsupported("until refused")
    return [out.years, out.months, out.weeks, out.days]


def add_days(calendar, day, dur, overflow):
    d = Date()
    d.day = day
    d.calendar = CALENDARS[calendar]
    out = Date()
    du = Duration(*dur)
    r = lib.mlocAddToDate(ctypes.byref(d), ctypes.byref(du), 1 if overflow == "reject" else 0, ctypes.byref(out))
    return out.day if r == 0 else None


def make(calendar, era, year, month_code, month, day, overflow):
    inp = Input()
    keep = []
    if era is not None:
        b = era.encode("utf-8")
        keep.append(b)
        inp.era = b
        inp.eraLength = len(b)
    inp.year = year
    if month_code is not None:
        b = month_code.encode("utf-8")
        keep.append(b)
        inp.monthCode = b
        inp.monthCodeLength = len(b)
    inp.month = month
    inp.day = day
    out = Date()
    r = lib.mlocMakeDate(CALENDARS[calendar], ctypes.byref(inp), 1 if overflow == "reject" else 0, ctypes.byref(out))
    return out.day if r == 0 else None


def fields_of(calendar, day):
    d = Date()
    if lib.mlocMakeDateFromDay(CALENDARS[calendar], day, ctypes.byref(d)) != 0:
        raise Unsupported("day out of range")
    f = Fields()
    if lib.mlocGetDateFields(ctypes.byref(d), ctypes.byref(f)) != 0:
        raise Unsupported("fields")
    return f


def range_error(msg, origin=None):
    err = js.make_error(RANGE, msg)
    err.origin = origin
    return JSThrow(err)


def type_error(msg):
    return JSThrow(js.make_error(TYPE, msg))


RANGE = None
TYPE = None


class PlainDate:
    def __init__(self, calendar, day, origin):
        self.calendar = calendar
        self.day = day
        self.origin = origin

    def js_string(self):
        return un(str262.date_string(self))

    def js_get(self, prop):
        if prop == "calendarId":
            return self.calendar
        if prop in ("toString", "toJSON"):
            return string_method(str262.date_string, self)[prop]
        if prop == "equals":
            def equals(other=UNDEF):
                other = plain_date_from(other)
                return other.day == self.day and other.calendar == self.calendar
            return Builtin(equals)
        if prop in ("add", "subtract"):
            def arith(duration=UNDEF, options=UNDEF):
                dur = to_date_duration(duration)
                if prop == "subtract":
                    dur = [-x for x in dur]
                    dur = [0 if x == 0 else x for x in dur]
                overflow = read_overflow(options)
                key = origin_key({"calendar": self.calendar, "of": json.loads(self.origin) if self.origin else None,
                                  "add": dur, "overflow": overflow}) if self.origin else None
                day = add_days(self.calendar, self.day, dur, overflow)
                if day is None:
                    raise range_error("add refused", key)
                return PlainDate(self.calendar, day, key)
            return Builtin(arith)
        if prop in ("until", "since"):
            def difference(other=UNDEF, options=UNDEF):
                other = un(other)
                if isinstance(other, (JSObject, str)):
                    other = plain_date_from(other)
                if not isinstance(other, PlainDate):
                    raise Unsupported("difference with %r" % (other,))
                if other.calendar != self.calendar:
                    raise range_error("calendars differ")
                settings = read_units(options, prop == "since")
                key = None
                if self.origin and other.origin:
                    o = {"calendar": self.calendar, "until": [json.loads(self.origin),
                         json.loads(other.origin)], "unit": settings[0]}
                    if settings[1:] != (3, 1, "trunc"):
                        o["round"] = list(settings[1:])
                    key = origin_key(o)
                values = until_rounded(self.calendar, self.day, other.day, settings)
                if values is None:
                    raise range_error("until refused", key)
                return DurationObj(values + [0] * 6, key, prop == "since")
            return Builtin(difference)
        if prop == "with":
            def with_(item=UNDEF, options=UNDEF):
                item = un(item)
                if not isinstance(item, JSObject):
                    raise Unsupported("with of %r" % (item,))
                if item.get("calendar") is not UNDEF or item.get("timeZone") is not UNDEF:
                    raise type_error("calendar or time zone in with")
                f = read_fields(self.calendar, item)
                if all(v is None for v in f.values()):
                    raise type_error("no field")
                overflow = read_overflow(options)
                if (f.get("era") is None) != (f.get("eraYear") is None):
                    raise type_error("era without era year")
                if f.get("month") is not None and f["month"] < 1 or f.get("day") is not None and f["day"] < 1:
                    raise range_error("below 1")
                key = None
                if self.origin:
                    key = origin_key({"calendar": self.calendar, "of": json.loads(self.origin),
                                      "with": f, "overflow": overflow})
                c, keep = make_changes(f)
                d = Date()
                d.day = self.day
                d.calendar = CALENDARS[self.calendar]
                out = Date()
                if lib.mlocWithDate(ctypes.byref(d), ctypes.byref(c), 1 if overflow == "reject" else 0,
                                    ctypes.byref(out)) != 0:
                    raise range_error("with refused", key)
                return PlainDate(self.calendar, out.day, key)
            return Builtin(with_)
        if prop == "toPlainMonthDay":
            return Builtin(lambda *a: month_day_of_date(self))
        if prop == "withCalendar":
            def with_calendar(cal):
                c = calendar_id(cal)
                g = fields_of("gregory", self.day)
                return PlainDate(c, self.day, origin_key({"calendar": c, "iso": [g.year, g.month, g.day]}))
            return Builtin(with_calendar)
        f = fields_of(self.calendar, self.day)
        noera = f.eraLength == 0 or self.calendar == "iso8601"
        value = {
            "year": f.year,
            "month": f.month,
            "monthCode": f.monthCode[:f.monthCodeLength].decode(),
            "day": f.day,
            "era": UNDEF if noera else f.era[:f.eraLength].decode(),
            "eraYear": UNDEF if noera else f.eraYear,
            "inLeapYear": bool(f.inLeapYear),
            "monthsInYear": f.monthsInYear,
            "daysInMonth": f.daysInMonth,
            "daysInYear": f.daysInYear,
            "dayOfYear": f.dayOfYear,
            "dayOfWeek": f.dayOfWeek,
            "daysInWeek": 7,
        }.get(prop)
        if value is None:
            raise Unsupported("PlainDate." + prop)
        return Tracked(value, self.origin, prop)


DURATION_FIELDS = ["years", "months", "weeks", "days", "hours", "minutes", "seconds",
                   "milliseconds", "microseconds", "nanoseconds"]


def string_method(fn, *args):
    # toString with its options, and toJSON with none.
    import str262
    return {"toString": Builtin(lambda options=UNDEF: fn(*args, options)),
            "toJSON": Builtin(lambda *a: fn(*args, UNDEF))}


class DurationObj:
    def __init__(self, values, origin=None, negated=False, track=range(4)):
        self.values = [-v if negated and v else v for v in values]
        self.origin = origin
        self.negated = negated
        # The fields an assertion on which is recorded.
        self.track = track

    def js_get(self, prop):
        if prop in DURATION_FIELDS:
            v = self.values[DURATION_FIELDS.index(prop)]
            if self.origin is not None and DURATION_FIELDS.index(prop) in self.track:
                return Tracked(v, self.origin, ("-" if self.negated else "") + prop)
            return v
        if prop in ("toString", "toJSON"):
            return string_method(str262.duration_string, self.values)[prop]
        if prop == "sign":
            return next((1 if v > 0 else -1 for v in self.values if v), 0)
        if prop == "blank":
            return not any(self.values)
        if prop == "negated":
            return Builtin(lambda: DurationObj([-v if v else 0 for v in self.values]))
        return dur262.get(self, prop)


def unit_option(options, name, default):
    v = un(options.get(name)) if isinstance(options, JSObject) else UNDEF
    if v is UNDEF:
        return default
    if not isinstance(v, str):
        raise Unsupported("unit option type")
    if v in TIME_UNITS:
        raise range_error("time unit for a date")
    if v == "auto" and name == "largestUnit":
        return "auto"
    if v not in UNITS:
        raise range_error("unknown unit " + v)
    return UNITS[v]


def read_units(options, since=False):
    # GetDifferenceSettings for a date: (largest, smallest, increment, mode).
    options = un(options)
    if options is not UNDEF and not isinstance(options, JSObject):
        raise Unsupported("difference options")
    largest = unit_option(options, "largestUnit", "auto")
    increment = 1
    if isinstance(options, JSObject):
        inc = un(options.get("roundingIncrement"))
        if inc is not UNDEF:
            if isinstance(inc, bool) or not isinstance(inc, (int, float)):
                raise Unsupported("increment type")
            if isinstance(inc, float) and (math.isnan(inc) or math.isinf(inc)):
                raise range_error("increment not finite")
            increment = int(inc)
            if increment < 1 or increment > 10**9:
                raise range_error("increment out of range")
    mode = "trunc"
    if isinstance(options, JSObject):
        m = un(options.get("roundingMode"))
        if m is not UNDEF:
            if not isinstance(m, str):
                raise Unsupported("mode type")
            if m not in MODES:
                raise range_error("unknown mode")
            mode = m
    smallest = unit_option(options, "smallestUnit", 3)
    if largest == "auto":
        largest = min(smallest, 3)
    if largest > smallest:
        raise range_error("largest smaller than smallest")
    if since:
        mode = NEGATED.get(mode, mode)
    return (largest, smallest, increment, mode)


def duration_field(v):
    v = un(v)
    if v is UNDEF:
        return None
    if isinstance(v, str):
        v = js.tonum(v)
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        raise Unsupported("duration field type")
    if isinstance(v, float) and (math.isnan(v) or math.isinf(v) or not v.is_integer()):
        raise range_error("duration field not integral")
    return int(v)


def check_duration(values):
    if any(v > 0 for v in values) and any(v < 0 for v in values):
        raise range_error("duration of mixed signs")
    if any(abs(v) >= 2**32 for v in values[:3]):
        raise range_error("duration years, months or weeks too large")
    ns = values[3] * 86400 * 10**9 + values[4] * 3600 * 10**9 + values[5] * 60 * 10**9 + \
        values[6] * 10**9 + values[7] * 10**6 + values[8] * 10**3 + values[9]
    if abs(ns) >= 2**53 * 10**9:
        raise range_error("duration too long")


def parse_duration(text):
    # ParseTemporalDurationString, through mlocParseDuration.
    return ixdtf262.duration(text)


class DurationClass:
    def is_instance(self, v):
        return isinstance(un(v), DurationObj)

    def construct(self, args):
        values = []
        for i in range(10):
            v = duration_field(args[i]) if i < len(args) else None
            values.append(v or 0)
        check_duration(values)
        return DurationObj(values)

    def js_get(self, prop):
        if prop == "from":
            def from_(item):
                item = un(item)
                if isinstance(item, DurationObj):
                    return DurationObj(list(item.values))
                if isinstance(item, str):
                    values = parse_duration(item)
                elif isinstance(item, JSObject):
                    given = [duration_field(item.get(n)) for n in DURATION_FIELDS]
                    if all(v is None for v in given):
                        raise type_error("no duration field")
                    values = [v or 0 for v in given]
                else:
                    raise Unsupported("Duration.from")
                check_duration(values)
                return DurationObj(values)
            return Builtin(from_)
        if prop == "compare":
            return Builtin(lambda a=UNDEF, b=UNDEF, o=UNDEF: dur262.compare(a, b, o))
        raise Unsupported("Duration." + prop)


def to_date_duration(item):
    values = to_duration_values(item)
    time_ns = values[4] * 3600 * 10**9 + values[5] * 60 * 10**9 + values[6] * 10**9 + \
        values[7] * 10**6 + values[8] * 10**3 + values[9]
    extra = abs(time_ns) // (86400 * 10**9) * (1 if time_ns >= 0 else -1)
    return [values[0], values[1], values[2], values[3] + extra]


def to_duration_values(item):
    # ToTemporalDuration's ten fields.
    item = un(item)
    if isinstance(item, DurationObj):
        values = list(item.values)
    elif isinstance(item, JSObject):
        values = []
        seen = False
        for name in DURATION_FIELDS:
            v = duration_field(item.get(name))
            seen = seen or v is not None
            values.append(v or 0)
        if not seen:
            raise type_error("no duration field")
        check_duration(values)
    elif isinstance(item, str):
        values = parse_duration(item)
        check_duration(values)
    else:
        raise Unsupported("duration from %r" % (item,))
    return values


def to_integer(v, name):
    v = un(v)
    if v is UNDEF:
        return None
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        if isinstance(v, str):
            v = js.tonum(v)
            if isinstance(v, float) and (math.isnan(v) or math.isinf(v)):
                raise range_error(name + " not a number")
            return int(v)
        raise Unsupported("non-number for " + name)
    if isinstance(v, float) and (math.isnan(v) or math.isinf(v)):
        raise range_error(name + " not finite")
    return int(v)


def to_string_field(v, name):
    v = un(v)
    if v is UNDEF:
        return None
    if not isinstance(v, str):
        raise Unsupported("non-string for " + name)
    return v


def read_overflow(options):
    options = un(options)
    if options is UNDEF:
        return "constrain"
    if not isinstance(options, JSObject):
        raise Unsupported("options")
    o = un(options.get("overflow"))
    if o is UNDEF:
        return "constrain"
    if o not in ("constrain", "reject"):
        raise range_error("overflow")
    return o


def calendar_id(v):
    # ToTemporalCalendarIdentifier: a Temporal object's calendar, or a
    # calendar string (ParseTemporalCalendarString, then
    # CanonicalizeCalendar); absent, the ISO calendar.
    v = un(v)
    if v is UNDEF:
        return "iso8601"
    if isinstance(v, (PlainDate, PlainMonthDay, PlainYearMonth, datetime262.PlainDateTime,
                      zone262.ZonedDateTime)):
        return v.calendar
    if not isinstance(v, str):
        raise type_error("calendar not a string")
    return ixdtf262.calendar_string(v)


def constructor_calendar(v):
    # A constructor's calendar: an id alone, CanonicalizeCalendar's.
    v = un(v)
    if v is UNDEF:
        return "iso8601"
    if not isinstance(v, str):
        raise type_error("calendar not a string")
    return ixdtf262.canonical_calendar(v)


def read_fields(cal, item):
    f = {"day": to_integer(item.get("day"), "day")}
    if cal not in NO_ERAS:
        f["era"] = to_string_field(item.get("era"), "era")
        f["eraYear"] = to_integer(item.get("eraYear"), "eraYear")
    f["month"] = to_integer(item.get("month"), "month")
    f["monthCode"] = to_string_field(item.get("monthCode"), "monthCode")
    f["year"] = to_integer(item.get("year"), "year")
    return f


class PlainMonthDay:
    def __init__(self, calendar, day, origin):
        self.calendar = calendar
        self.day = day
        self.origin = origin

    def iso(self):
        g = fields_of("gregory", self.day)
        return g.year, g.month, g.day

    def js_string(self):
        return un(str262.date_string(self, UNDEF, "md"))

    def js_get(self, prop):
        if prop == "calendarId":
            return self.calendar
        if prop in ("toString", "toJSON"):
            return {"toString": Builtin(lambda o=UNDEF: str262.date_string(self, o, "md")),
                    "toJSON": Builtin(lambda *a: str262.date_string(self, UNDEF, "md"))}[prop]
        if prop == "equals":
            def equals(other=UNDEF):
                other = plain_month_day_from(other)
                return other.day == self.day and other.calendar == self.calendar
            return Builtin(equals)
        f = fields_of(self.calendar, self.day)
        if prop == "monthCode":
            return Tracked(f.monthCode[:f.monthCodeLength].decode(), self.origin, "monthCode")
        if prop == "day":
            return Tracked(f.day, self.origin, "day")
        raise Unsupported("PlainMonthDay." + prop)


def month_day_of_date(date, options=UNDEF):
    # ToTemporalMonthDay of a PlainDate: its month code and day.
    f = fields_of(date.calendar, date.day)
    fields = {"day": f.day, "era": None, "eraYear": None, "month": None,
              "monthCode": f.monthCode[:f.monthCodeLength].decode(), "year": None}
    if date.calendar in NO_ERAS:
        del fields["era"], fields["eraYear"]
    overflow = read_overflow(options)
    key = origin_key({"calendar": date.calendar, "monthday": fields, "overflow": overflow})
    c, keep = make_changes(fields)
    out = Date()
    if lib.mlocMakeMonthDay(CALENDARS[date.calendar], ctypes.byref(c), 1 if overflow == "reject" else 0,
                            ctypes.byref(out)) != 0:
        raise range_error("month-day refused", key)
    return PlainMonthDay(date.calendar, out.day, key)


def plain_month_day_from(item=UNDEF, options=UNDEF):
    item = un(item)
    if isinstance(item, PlainMonthDay):
        read_overflow(options)
        return PlainMonthDay(item.calendar, item.day, item.origin)
    if isinstance(item, str):
        # ToTemporalMonthDay of a string: in the ISO calendar the month and
        # day in the reference year 1972, in another the month-day of the
        # date given, constrained.
        r = ixdtf262.parse(item, ixdtf262.MONTH_DAY)
        cal = ixdtf262.canonical_calendar(r.calendar)
        read_overflow(options)
        options = JSObject({"overflow": "constrain"})
        item = iso_construct([1972 if cal == "iso8601" else r.year, r.month, r.day, cal])
    if isinstance(item, PlainDate):
        return month_day_of_date(item, options)
    if not isinstance(item, JSObject):
        raise Unsupported("month-day from %r" % (item,))
    cal = calendar_id(item.get("calendar"))
    f = read_fields(cal, item)
    overflow = read_overflow(options)
    if f["day"] is None or (f["month"] is None and f["monthCode"] is None):
        raise type_error("missing fields")
    if (f.get("era") is None) != (f.get("eraYear") is None):
        raise type_error("era and era year")
    if f["month"] is not None and f["year"] is None and f.get("era") is None and cal != "iso8601":
        raise type_error("month without year")
    if (f["month"] is not None and f["month"] < 1) or f["day"] < 1:
        raise range_error("below 1")
    if cal == "iso8601" and f["monthCode"] is None and f["year"] is None:
        f["monthCode"] = "M%02d" % f["month"] if f["month"] <= 99 else "M99"
        f["month"] = None
    key = origin_key({"calendar": cal, "monthday": f, "overflow": overflow})
    c, keep = make_changes(f)
    out = Date()
    if lib.mlocMakeMonthDay(CALENDARS[cal], ctypes.byref(c), 1 if overflow == "reject" else 0,
                            ctypes.byref(out)) != 0:
        raise range_error("month-day refused", key)
    return PlainMonthDay(cal, out.day, key)


class PlainMonthDayClass:
    def is_instance(self, v):
        return isinstance(un(v), PlainMonthDay)

    def js_get(self, prop):
        if prop == "from":
            return Builtin(plain_month_day_from)
        raise Unsupported("PlainMonthDay." + prop)

    def construct(self, args):
        raise Unsupported("new PlainMonthDay")


class PlainYearMonth:
    # A year-month: the first day of its month.
    def __init__(self, calendar, day, origin):
        self.calendar = calendar
        self.day = day
        self.origin = origin

    def as_date(self):
        return PlainDate(self.calendar, self.day, self.origin)

    def js_string(self):
        return un(str262.date_string(self, UNDEF, "ym"))

    def js_get(self, prop):
        if prop == "calendarId":
            return self.calendar
        if prop in ("toString", "toJSON"):
            return {"toString": Builtin(lambda o=UNDEF: str262.date_string(self, o, "ym")),
                    "toJSON": Builtin(lambda *a: str262.date_string(self, UNDEF, "ym"))}[prop]
        if prop in ("year", "month", "monthCode", "era", "eraYear", "daysInMonth", "daysInYear",
                    "monthsInYear", "inLeapYear"):
            return self.as_date().js_get(prop)
        if prop in ("add", "subtract"):
            def arith(duration=UNDEF, options=UNDEF):
                # AddDurationToYearMonth: the duration, the overflow, then
                # weeks, days and time refused.
                values = to_duration_values(duration)
                dur = values[:4]
                if prop == "subtract":
                    dur = [-x if x else 0 for x in dur]
                overflow = read_overflow(options)
                if any(values[2:]):
                    raise range_error("weeks, days or time for a year-month")
                key = origin_key({"calendar": self.calendar, "of": json.loads(self.origin),
                                  "add": dur, "overflow": overflow}) if self.origin else None
                day = add_days(self.calendar, self.day, dur, overflow)
                if day is None:
                    raise range_error("add refused", key)
                if fields_of(self.calendar, day).day != 1:
                    raise Unsupported("year-month added off its first day")
                return PlainYearMonth(self.calendar, day, key)
            return Builtin(arith)
        if prop in ("until", "since"):
            def difference(other=UNDEF, options=UNDEF):
                other = un(other)
                if isinstance(other, (JSObject, str)):
                    other = plain_year_month_from(other)
                if not isinstance(other, PlainYearMonth):
                    raise Unsupported("difference with %r" % (other,))
                if other.calendar != self.calendar:
                    raise range_error("calendars differ")
                settings = read_ym_units(options, prop == "since")
                key = None
                if self.origin and other.origin:
                    key = origin_key({"calendar": self.calendar, "until": [json.loads(self.origin),
                                      json.loads(other.origin)], "unit": settings[0],
                                      "round": list(settings[1:])})
                values = until_rounded(self.calendar, self.day, other.day, settings)
                if values is None:
                    raise range_error("until refused", key)
                return DurationObj(values + [0] * 6, key, prop == "since")
            return Builtin(difference)
        if prop in ("with", "toPlainDate"):
            def with_(item=UNDEF, options=UNDEF):
                item = un(item)
                if not isinstance(item, JSObject):
                    raise Unsupported("year-month with of %r" % (item,))
                f = read_fields(self.calendar, item)
                if prop == "with":
                    if item.get("calendar") is not UNDEF or item.get("timeZone") is not UNDEF:
                        raise type_error("calendar or time zone in with")
                    f["day"] = None
                    if all(v is None for v in f.values()):
                        raise type_error("no field")
                    overflow = read_overflow(options)
                else:
                    f = {"day": f["day"]}
                    if f["day"] is None:
                        raise type_error("no day")
                    overflow = "constrain"
                if (f.get("era") is None) != (f.get("eraYear") is None):
                    raise type_error("era without era year")
                if f.get("month") is not None and f["month"] < 1 or f.get("day") is not None and f["day"] < 1:
                    raise range_error("below 1")
                key = origin_key({"calendar": self.calendar, "of": json.loads(self.origin),
                                  "with": f, "overflow": overflow}) if self.origin else None
                c, keep = make_changes(f)
                d = Date()
                d.day = self.day
                d.calendar = CALENDARS[self.calendar]
                out = Date()
                if lib.mlocWithDate(ctypes.byref(d), ctypes.byref(c), 1 if overflow == "reject" else 0,
                                    ctypes.byref(out)) != 0:
                    raise range_error("with refused", key)
                if prop == "toPlainDate":
                    return PlainDate(self.calendar, out.day, key)
                return PlainYearMonth(self.calendar, out.day, key)
            return Builtin(with_)
        if prop == "equals":
            def equals(other=UNDEF):
                other = plain_year_month_from(other)
                return other.day == self.day and other.calendar == self.calendar
            return Builtin(equals)
        raise Unsupported("PlainYearMonth." + prop)


def read_ym_units(options, since):
    # GetDifferenceSettings for a year-month: weeks and days refused,
    # smallest month by default, largest year.
    options = un(options)
    for name in ("largestUnit", "smallestUnit"):
        v = un(options.get(name)) if isinstance(options, JSObject) else UNDEF
        if v in ("week", "weeks", "day", "days"):
            raise range_error("week or day for a year-month")
    if isinstance(options, JSObject):
        o = JSObject(dict(options.props))
    else:
        o = JSObject()
    if un(o.get("smallestUnit")) is UNDEF:
        o.set("smallestUnit", "month")
    largest = un(o.get("largestUnit"))
    if largest in (UNDEF, "auto"):
        o.set("largestUnit", "year")
    return read_units(o, since)


# ISOYearMonthWithinLimits, as (year, month).
YEAR_MONTH_LIMITS = ((-271821, 4), (275760, 9))


def year_month_of_string(text, options):
    # ToTemporalYearMonth of a string, to the fields it goes on with: the
    # ISO year and month, or in another calendar the year and month code
    # of the date given, constrained.
    r = ixdtf262.parse(text, ixdtf262.YEAR_MONTH)
    cal = ixdtf262.canonical_calendar(r.calendar)
    read_overflow(options)
    if not YEAR_MONTH_LIMITS[0] <= (r.year, r.month) <= YEAR_MONTH_LIMITS[1]:
        raise range_error("year-month out of range")
    if cal == "iso8601":
        return JSObject({"year": r.year, "month": r.month, "calendar": cal})
    if (r.year, r.month, r.day) < (-271821, 4, 19):
        raise Unsupported("a year-month's date before the dates")
    f = fields_of(cal, iso_construct([r.year, r.month, r.day, cal]).day)
    return JSObject({"year": f.year, "monthCode": f.monthCode[:f.monthCodeLength].decode(),
                     "calendar": cal})


def plain_year_month_from(item=UNDEF, options=UNDEF):
    item = un(item)
    if isinstance(item, PlainYearMonth):
        read_overflow(options)
        return PlainYearMonth(item.calendar, item.day, item.origin)
    if isinstance(item, str):
        item = year_month_of_string(item, options)
        options = UNDEF
    if not isinstance(item, JSObject):
        raise Unsupported("year-month from %r" % (item,))
    cal = calendar_id(item.get("calendar"))
    f = read_fields(cal, item)
    f["day"] = 1
    overflow = read_overflow(options)
    if f["month"] is None and f["monthCode"] is None:
        raise type_error("month")
    if (f.get("era") is None) != (f.get("eraYear") is None):
        raise type_error("era and era year")
    if f["year"] is None and f.get("era") is None:
        raise type_error("year")
    if f["month"] is not None and f["month"] < 1:
        raise range_error("month below 1")
    origin = {"calendar": cal, "era": f.get("era"), "eraYear": f.get("eraYear"), "year": f["year"],
              "monthCode": f["monthCode"], "month": f["month"] or 0, "day": 1, "overflow": overflow}
    key = origin_key(origin)
    for k in ("eraYear", "year"):
        if origin[k] is not None and not -2**31 <= origin[k] < 2**31:
            raise Unsupported("year beyond 32 bits")
    if f.get("era") is not None:
        d = make(cal, f["era"], f["eraYear"], f["monthCode"], f["month"] or 0, 1, overflow)
        if d is not None and f["year"] is not None and fields_of(cal, d).year != f["year"]:
            d = None
    else:
        d = make(cal, None, f["year"], f["monthCode"], f["month"] or 0, 1, overflow)
    if d is None:
        raise range_error("refused", key)
    return PlainYearMonth(cal, d, key)


class PlainYearMonthClass:
    def is_instance(self, v):
        return isinstance(un(v), PlainYearMonth)

    def js_get(self, prop):
        if prop == "from":
            return Builtin(plain_year_month_from)
        raise Unsupported("PlainYearMonth." + prop)

    def construct(self, args):
        args = [un(a) for a in args] + [UNDEF] * 4
        y, m = to_integer(args[0], "iso"), to_integer(args[1], "iso")
        cal = constructor_calendar(args[2])
        ref = 1 if args[3] is UNDEF else to_integer(args[3], "iso")
        if None in (y, m):
            raise Unsupported("year-month args")
        day = make("gregory", None, y, None, m, ref, "reject") if m >= 1 and ref >= 1 and -2**31 <= y < 2**31 else None
        if day is None:
            raise Unsupported("year-month reference outside the dates")
        if fields_of(cal, day).day != 1:
            raise Unsupported("reference day not the month's first")
        return PlainYearMonth(cal, day, origin_key({"calendar": cal, "iso": [y, m, ref]}))


def plain_date_from(item=UNDEF, options=UNDEF):
    item = un(item)
    if isinstance(item, PlainDate):
        read_overflow(options)
        return PlainDate(item.calendar, item.day, item.origin)
    if isinstance(item, str):
        return iso_from_string(item, options)
    if not isinstance(item, JSObject):
        raise Unsupported("from %r" % (item,))
    cal = calendar_id(item.get("calendar"))
    day = to_integer(item.get("day"), "day")
    era = eray = None
    if cal not in NO_ERAS:
        era = to_string_field(item.get("era"), "era")
        eray = to_integer(item.get("eraYear"), "eraYear")
    month = to_integer(item.get("month"), "month")
    code = to_string_field(item.get("monthCode"), "monthCode")
    year = to_integer(item.get("year"), "year")
    if day is None:
        raise type_error("day")
    if month is None and code is None:
        raise type_error("month")
    if (era is None) != (eray is None):
        raise type_error("era and eraYear")
    if year is None and era is None:
        raise type_error("year")
    overflow = read_overflow(options)
    if month is not None and month < 1:
        raise range_error("month below 1")
    if day < 1:
        raise range_error("day below 1")
    origin = {"calendar": cal, "era": era, "eraYear": eray, "year": year, "monthCode": code,
              "month": month or 0, "day": day, "overflow": overflow}
    for k in ("eraYear", "year"):
        if origin[k] is not None and not -2**31 <= origin[k] < 2**31:
            raise Unsupported("year beyond 32 bits")
    if not (origin["month"] < 2**32 and day < 2**32):
        raise Unsupported("month or day beyond 32 bits")
    key = origin_key(origin)
    if era is not None:
        d = make(cal, era, eray, code, month or 0, day, overflow)
        if d is not None and year is not None and fields_of(cal, d).year != year:
            d = None
    else:
        d = make(cal, None, year, code, month or 0, day, overflow)
    if d is None:
        raise range_error("refused", key)
    return PlainDate(cal, d, key)


def iso_from_string(text, options):
    # ToTemporalDate of a string: the date of a date-time string, in its
    # calendar.
    r = ixdtf262.parse(text, ixdtf262.DATE_TIME)
    cal = ixdtf262.canonical_calendar(r.calendar)
    read_overflow(options)
    return iso_construct([r.year, r.month, r.day, cal])


def iso_construct(args):
    args = [un(a) for a in args] + [UNDEF] * 4
    y, m, d = (to_integer(args[i], "iso") for i in range(3))
    cal = constructor_calendar(args[3])
    if None in (y, m, d):
        raise Unsupported("iso args")
    day = make("gregory", None, y, None, m, d, "reject") if m >= 1 and d >= 1 and -2**31 <= y < 2**31 else None
    if day is None:
        raise range_error("iso date")
    return PlainDate(cal, day, origin_key({"calendar": cal, "iso": [y, m, d]}))


def origin_key(o):
    return json.dumps(o, sort_keys=True)


class PlainDateClass:
    def construct(self, args):
        return iso_construct(args)

    def is_instance(self, v):
        return isinstance(un(v), PlainDate)

    def js_get(self, prop):
        if prop == "from":
            return Builtin(plain_date_from)
        raise Unsupported("PlainDate." + prop)


class Unknown:
    def __init__(self, name):
        self.name = name

    def js_get(self, prop):
        raise Unsupported(self.name + "." + prop)

    def construct(self, args):
        raise Unsupported("new " + self.name)

    def js_typeof(self):
        raise Unsupported("typeof " + self.name)


class TemporalNS:
    def js_get(self, prop):
        if prop == "PlainDate":
            return PLAINDATE
        if prop == "Duration":
            return DURATIONCLASS
        if prop == "PlainMonthDay":
            return PLAINMONTHDAY
        if prop == "PlainYearMonth":
            return PLAINYEARMONTH
        if prop == "ZonedDateTime":
            return zone262.ZONEDDATETIME
        if prop == "PlainTime":
            return time262.PLAINTIME
        if prop == "PlainDateTime":
            return datetime262.PLAINDATETIME
        if prop == "Instant":
            return instant262.INSTANT
        return Unknown("Temporal." + prop)


PLAINDATE = PlainDateClass()
DURATIONCLASS = DurationClass()
PLAINMONTHDAY = PlainMonthDayClass()
PLAINYEARMONTH = PlainYearMonthClass()


class Recorder:
    def __init__(self):
        self.rows = {}
        self.failures = []
        self.file = ""

    def expect(self, key, prop, value):
        prop = prop.lstrip("-")
        row = self.rows.setdefault(key, {"expect": {}, "files": []})
        if self.file not in row["files"]:
            row["files"].append(self.file)
        old = row["expect"].get(prop, value)
        if old != value:
            self.failures.append({"file": self.file, "origin": key, "prop": prop,
                                  "conflict": [old, value]})
        row["expect"][prop] = value

    def fail(self, msg, key=None, prop=None, got=None, want=None):
        self.failures.append({"file": self.file, "message": msg, "origin": key, "prop": prop,
                              "got": jsonable(got), "want": jsonable(want)})


def jsonable(v):
    v = un(v)
    if v is UNDEF:
        return "undefined"
    if isinstance(v, (bool, int, float, str)) or v is None:
        return v
    return js.jsstr(v) if not isinstance(v, JSObject) else "[object]"


def canonical_era(cal, era):
    era = un(era)
    if era is UNDEF:
        return ""
    return ERA_ALIASES.get(cal, {}).get(era, era)


def expected_value(origin_key_, prop, v):
    v = un(v)
    if prop.lstrip("-") in DURATION_FIELDS:
        if isinstance(v, float) and v.is_integer():
            v = int(v)
        if not isinstance(v, int) or isinstance(v, bool):
            raise Unsupported("expected duration value %r" % (v,))
        return -v if prop.startswith("-") and v else v
    origin = json.loads(origin_key_)
    if "str" in origin:
        return str262.expected(prop, v)
    if "zone" in origin:
        return zone262.expected(prop, v)
    if "time" in origin:
        return time262.expected(prop, v)
    if "inst" in origin:
        return instant262.expected(prop, v)
    if "zdt" in origin:
        return zdt262.expected(prop, v)
    if "dur" in origin:
        return dur262.expected(prop, v)
    cal = origin["calendar"]
    if prop == "era":
        return canonical_era(cal, v)
    if prop == "eraYear" and v is UNDEF:
        return None
    if isinstance(v, bool):
        return int(v)
    if isinstance(v, (int, str)):
        return v
    if isinstance(v, float) and v.is_integer():
        return int(v)
    raise Unsupported("expected value %r" % (v,))


def make_assert(rec, interp_ref):
    def check(actual, expected, msg=""):
        ok = js.same_value(actual, expected)
        if isinstance(actual, Tracked) and actual.prop.startswith("!"):
            # !x === true checks x false.
            if not isinstance(un(expected), bool):
                raise Unsupported("negated value against a non-boolean")
            actual = Tracked(not actual.value, actual.origin, actual.prop[1:])
            expected = not un(expected)
        if isinstance(actual, Tracked) and actual.origin is not None:
            want = expected_value(actual.origin, actual.prop, expected)
            got = expected_value(actual.origin, actual.prop, actual)
            if actual.prop == "era":
                ok = got == want
            rec.expect(actual.origin, actual.prop, want)
            if not ok:
                rec.fail(js.jsstr(msg), actual.origin, actual.prop, got, want)
        elif not ok:
            rec.fail("untracked: " + js.jsstr(msg), got=actual, want=expected)
        return UNDEF

    def not_same(actual, expected, msg=""):
        if js.same_value(actual, expected):
            rec.fail("notSameValue: " + js.jsstr(msg))
        return UNDEF

    def throws(cls, fn, msg=""):
        LAST_MADE[0] = None
        try:
            interp_ref[0].call(fn, [])
        except JSThrow as t:
            err = t.value
            got_cls = getattr(err, "error_class", None)
            origin = getattr(err, "origin", None)
            if got_cls is not cls:
                rec.fail("threw %s, expected %s: %s" % (getattr(got_cls, "name", "?"), cls.name, js.jsstr(msg)), origin)
            elif origin is not None:
                rec.expect(origin, "refused", 1)
            return UNDEF
        last = LAST_MADE[0]
        if last is not None and cls is RANGE:
            rec.expect(last, "refused", 1)
            rec.fail("did not throw: " + js.jsstr(msg), last, "refused", 0, 1)
        else:
            rec.fail("did not throw: " + js.jsstr(msg))
        return UNDEF

    def plain(cond, msg=""):
        if isinstance(cond, Tracked) and cond.origin is not None and isinstance(cond.value, bool):
            negated = cond.prop.startswith("!")
            prop = cond.prop.lstrip("!")
            rec.expect(cond.origin, prop, 0 if negated else 1)
            if not cond.value:
                rec.fail("assert: " + js.jsstr(msg), cond.origin, prop, int(cond.value != negated), int(not negated))
            return UNDEF
        if not js.truthy(cond):
            rec.fail("assert: " + js.jsstr(msg))
        return UNDEF

    def compare_array(a, b, msg=""):
        a, b = un(a), un(b)
        if len(a.items) != len(b.items) or not all(js.same_value(x, y) for x, y in zip(a.items, b.items)):
            rec.fail("compareArray: " + js.jsstr(msg))
        return UNDEF

    a = Builtin(plain, "assert")
    a.props = {"sameValue": Builtin(check), "notSameValue": Builtin(not_same),
               "throws": Builtin(throws), "compareArray": Builtin(compare_array)}
    return a, check


LAST_MADE = [None]


class AssertObj:
    def __init__(self, fn, props):
        self.fn = fn
        self.props = props

    def call(self, args):
        return self.fn.call(args)

    def js_get(self, prop):
        if prop in self.props:
            return self.props[prop]
        raise Unsupported("assert." + prop)


def make_helpers(check):
    def assert_plain_date(date, year, month, month_code, day, desc="", era=UNDEF, era_year=UNDEF):
        date = un(date)
        if not isinstance(date, PlainDate):
            raise Unsupported("assertPlainDate of %r" % (date,))
        for prop, want in (("era", era), ("eraYear", era_year), ("year", year), ("month", month),
                           ("monthCode", month_code), ("day", day)):
            check(date.js_get(prop), want, js.jsstr(desc) + " " + prop)
        return UNDEF

    def assert_duration(duration, *args):
        duration = un(duration)
        if not isinstance(duration, DurationObj):
            raise Unsupported("assertDuration of %r" % (duration,))
        args = list(args) + [UNDEF] * (11 - len(args))
        desc = args[10] if args[10] is not UNDEF else ""
        for i, name in enumerate(DURATION_FIELDS):
            check(duration.js_get(name), args[i], js.jsstr(desc) + " " + name)
        return UNDEF

    def assert_durations_equal(actual, expected, desc=""):
        expected = un(expected)
        if not isinstance(expected, DurationObj):
            raise Unsupported("assertDurationsEqual expected")
        return assert_duration(actual, *[un(expected.js_get(n)) for n in DURATION_FIELDS], desc)

    def assert_plain_month_day(md, month_code, day, desc="", ref_year=1972):
        md = un(md)
        if not isinstance(md, PlainMonthDay):
            raise Unsupported("assertPlainMonthDay of %r" % (md,))
        check(md.js_get("monthCode"), month_code, js.jsstr(desc) + " monthCode")
        check(md.js_get("day"), day, js.jsstr(desc) + " day")
        check(Tracked(md.iso()[0], md.origin, "referenceYear"), ref_year, js.jsstr(desc) + " reference year")
        return UNDEF

    iso_months = JSArray([JSObject({"month": m, "monthCode": "M%02d" % m,
                                    "daysInMonth": [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31][m - 1]})
                          for m in range(1, 13)])

    def assert_plain_year_month(ym, year, month, month_code, desc="", era=UNDEF, era_year=UNDEF, ref_day=1):
        ym = un(ym)
        if not isinstance(ym, PlainYearMonth):
            raise Unsupported("assertPlainYearMonth of %r" % (ym,))
        d = ym.as_date()
        for prop, want in (("era", era), ("eraYear", era_year), ("year", year), ("month", month),
                           ("monthCode", month_code)):
            check(d.js_get(prop), want, js.jsstr(desc) + " " + prop)
        if un(ref_day) is not NULL:
            check(Tracked(fields_of("gregory", ym.day).day, ym.origin, "referenceDay"), ref_day,
                  js.jsstr(desc) + " reference day")
        return UNDEF

    def assert_plain_time(time, *args):
        time = un(time)
        if not isinstance(time, time262.PlainTime):
            raise Unsupported("assertPlainTime of %r" % (time,))
        args = list(args) + [UNDEF] * (7 - len(args))
        desc = args[6] if args[6] is not UNDEF else ""
        for i, name in enumerate(time262.TIME_FIELDS):
            check(time.js_get(name), args[i], js.jsstr(desc) + " " + name)
        return UNDEF

    def assert_plain_date_time(dt, year, month, month_code, day, *rest):
        dt = un(dt)
        if not isinstance(dt, datetime262.PlainDateTime):
            raise Unsupported("assertPlainDateTime of %r" % (dt,))
        rest = list(rest) + [UNDEF] * (9 - len(rest))
        desc = js.jsstr(rest[6]) if rest[6] is not UNDEF else ""
        for prop, want in (("era", rest[7]), ("eraYear", rest[8]), ("year", year), ("month", month),
                           ("monthCode", month_code), ("day", day)):
            check(dt.js_get(prop), want, desc + " " + prop)
        for i, name in enumerate(time262.TIME_FIELDS):
            check(dt.js_get(name), rest[i], desc + " " + name)
        return UNDEF

    def assert_zoned_equal(actual, expected, desc=""):
        actual, expected = un(actual), un(expected)
        if not isinstance(actual, zone262.ZonedDateTime) or not isinstance(expected, zone262.ZonedDateTime):
            raise Unsupported("assertZonedDateTimesEqual of %r" % (actual,))
        check(actual.js_get("epochNanoseconds"), js.BigInt(expected.ns), js.jsstr(desc) + " epochNanoseconds")
        check(actual.js_get("timeZoneId"), un(expected.js_get("timeZoneId")), js.jsstr(desc) + " zone")
        check(actual.js_get("calendarId"), expected.calendar, js.jsstr(desc) + " calendar")
        return UNDEF

    def assert_instants_equal(actual, expected, desc=""):
        actual, expected = un(actual), un(expected)
        if not isinstance(actual, instant262.Instant) or not isinstance(expected, instant262.Instant):
            raise Unsupported("assertInstantsEqual of %r" % (actual,))
        check(actual.js_get("epochNanoseconds"), js.BigInt(expected.ns), js.jsstr(desc) + " epochNanoseconds")
        return UNDEF

    class Helpers:
        def js_get(self, prop):
            if prop == "assertZonedDateTimesEqual":
                return Builtin(assert_zoned_equal)
            if prop == "assertInstantsEqual":
                return Builtin(assert_instants_equal)
            if prop == "assertPlainDateTime":
                return Builtin(assert_plain_date_time)
            if prop == "assertPlainTime":
                return Builtin(assert_plain_time)
            if prop == "assertPlainYearMonth":
                return Builtin(assert_plain_year_month)
            if prop == "ISOMonths":
                return iso_months
            if prop == "assertPlainMonthDay":
                return Builtin(assert_plain_month_day)
            if prop == "assertDuration":
                return Builtin(assert_duration)
            if prop == "assertDurationsEqual":
                return Builtin(assert_durations_equal)
            if prop == "assertPlainDate":
                return Builtin(assert_plain_date)
            raise Unsupported("TemporalHelpers." + prop)
    return Helpers()


def main():
    global lib, RANGE, TYPE
    lib = ctypes.CDLL(sys.argv[1])
    calendars = {c: c for c in CALENDARS}
    calendars.update(ALIASES)
    zone262.setup(lib, range_error, type_error, LAST_MADE, calendars)
    time262.setup(lib, range_error, type_error, LAST_MADE, sys.modules[__name__])
    datetime262.setup(lib, sys.modules[__name__])
    instant262.setup(lib, sys.modules[__name__])
    zdt262.setup(lib, sys.modules[__name__])
    str262.setup(lib, sys.modules[__name__])
    dur262.setup(lib, sys.modules[__name__])
    ixdtf262.setup(lib, sys.modules[__name__])
    root, out = sys.argv[2], sys.argv[3]
    rec = Recorder()
    summary = []
    for path in sys.argv[4:]:
        rel = os.path.relpath(path, root)
        rec.file = rel
        interp_ref = [None]
        g = js.standard_globals(interp_ref)
        RANGE, TYPE = g["RangeError"], g["TypeError"]
        a, check = make_assert(rec, interp_ref)
        g["assert"] = AssertObj(a, a.props)
        g["Temporal"] = TemporalNS()
        g["Intl"] = zone262.INTL
        g["TemporalHelpers"] = make_helpers(check)
        interp = js.Interp(g)
        interp_ref[0] = interp
        before_rows = sum(len(r["expect"]) for r in rec.rows.values())
        before_fail = len(rec.failures)
        status = "ok"
        try:
            interp.run(open(path, encoding="utf-8").read())
        except Unsupported as u:
            status = "unsupported: %s" % u
        except JSThrow as t:
            status = "uncaught: %s" % js.jsstr(un(t.value).get("message") if isinstance(un(t.value), JSObject) else t.value)
        except (RecursionError, IndexError, KeyError, ValueError, TypeError, AttributeError) as e:
            status = "evaluator error: %s %s" % (type(e).__name__, e)
        after_rows = sum(len(r["expect"]) for r in rec.rows.values())
        summary.append({"file": rel, "status": status, "checks": after_rows - before_rows,
                        "failures": len(rec.failures) - before_fail})
    with open(out, "w") as f:
        json.dump({"rows": rec.rows, "failures": rec.failures, "files": summary}, f, indent=1, sort_keys=True)


# Tracks the last date made, for an assert.throws whose function did not
# throw.
_orig_from = plain_date_from


def plain_date_from(item=UNDEF, options=UNDEF):  # noqa: F811
    LAST_MADE[0] = None
    d = _orig_from(item, options)
    LAST_MADE[0] = d.origin
    return d


PLAINDATE_FROM = plain_date_from

if __name__ == "__main__":
    main()
