# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Temporal's strings for t262cal.py, on Maul Locale's parsers
# (maul-locale/ixdtf.h): the ISO Date-Time Parse Record of a string for a
# set of goals, durations, time zone strings and calendar strings, each
# refused as ECMA-262 refuses them (a RangeError). The record's time zone
# and calendar come back as text for the host to look up, as the library
# leaves them.

import ctypes
import json

from jsmini import Unsupported

DATE_TIME_ZONED, DATE_TIME, INSTANT, TIME, MONTH_DAY, YEAR_MONTH = 1, 2, 4, 8, 16, 32


class Span(ctypes.Structure):
    _fields_ = [("start", ctypes.c_size_t), ("length", ctypes.c_size_t)]


class Time(ctypes.Structure):
    _fields_ = [("hour", ctypes.c_uint8), ("minute", ctypes.c_uint8), ("second", ctypes.c_uint8),
                ("nanosecond", ctypes.c_uint32)]


class Parse(ctypes.Structure):
    _fields_ = [("format", ctypes.c_uint32), ("year", ctypes.c_int32), ("month", ctypes.c_uint8),
                ("day", ctypes.c_uint8), ("hasYear", ctypes.c_bool), ("hasTime", ctypes.c_bool),
                ("time", Time), ("utc", ctypes.c_bool), ("hasOffset", ctypes.c_bool),
                ("offsetHasSeconds", ctypes.c_bool), ("offsetNanoseconds", ctypes.c_int64),
                ("timeZone", Span), ("calendar", Span)]


class ZoneParse(ctypes.Structure):
    _fields_ = [("name", Span), ("utc", ctypes.c_bool), ("hasOffset", ctypes.c_bool),
                ("offsetMinutes", ctypes.c_int16)]


class DurationValues(ctypes.Structure):
    _fields_ = [(n, ctypes.c_double) for n in (
        "years", "months", "weeks", "days", "hours", "minutes", "seconds", "milliseconds",
        "microseconds", "nanoseconds")]


class Ctx:
    lib = None
    host = None


def setup(lib, host):
    Ctx.lib, Ctx.host = lib, host


def refused(kind, text, formats=None):
    # The RangeError of a string a parser refuses, with the row it makes:
    # the parser, the goals for a date-time, and the text.
    o = {"ixdtf": kind, "text": text}
    if formats is not None:
        o["formats"] = formats
    return Ctx.host.range_error("%s string refused: %r" % (kind, text), json.dumps(o, sort_keys=True))


def text_of(raw, span):
    return raw[span.start:span.start + span.length].decode("utf-8")


class Record:
    # ParseISODateTime's record, in Python values.
    def __init__(self, raw, p):
        self.format = p.format
        self.year = p.year if p.hasYear else None
        self.month = p.month
        self.day = p.day
        t = p.time
        self.time = (t.hour, t.minute, t.second, t.nanosecond // 10**6, t.nanosecond // 1000 % 1000,
                     t.nanosecond % 1000) if p.hasTime else None
        self.utc = p.utc
        self.offset = p.offsetNanoseconds if p.hasOffset else None
        self.offset_has_seconds = p.offsetHasSeconds
        self.time_zone = text_of(raw, p.timeZone) if p.timeZone.length else None
        self.calendar = text_of(raw, p.calendar) if p.calendar.length else None


def parse(text, formats):
    raw = text.encode("utf-8", "surrogatepass")
    p = Parse()
    if Ctx.lib.mlocParseTemporalString(raw, len(raw), formats, ctypes.byref(p)) != 0:
        raise refused("dt", text, formats)
    return Record(raw, p)


def canonical_calendar(text):
    # CanonicalizeCalendar of a string's calendar, the ISO one by default.
    h = Ctx.host
    if text is None:
        return "iso8601"
    c = "".join(ch.lower() if "A" <= ch <= "Z" else ch for ch in text)
    c = h.ALIASES.get(c, c)
    if c not in h.CALENDARS:
        raise h.range_error("calendar " + text)
    return c


def calendar_string(text):
    # ParseTemporalCalendarString, then CanonicalizeCalendar.
    raw = text.encode("utf-8", "surrogatepass")
    span = Span()
    if Ctx.lib.mlocParseCalendarString(raw, len(raw), ctypes.byref(span)) != 0:
        raise refused("calendar", text)
    return canonical_calendar(text_of(raw, span) if span.length else None)


def duration(text):
    raw = text.encode("utf-8", "surrogatepass")
    v = DurationValues()
    if Ctx.lib.mlocParseDuration(raw, len(raw), ctypes.byref(v)) != 0:
        raise refused("duration", text)
    return [int(getattr(v, n)) for n, _ in DurationValues._fields_]


def offset_identifier(minutes):
    sign = "-" if minutes < 0 else "+"
    return "%s%02d:%02d" % (sign, abs(minutes) // 60, abs(minutes) % 60)


def time_zone_string(text, identifier_only=False):
    # ParseTemporalTimeZoneString (or ParseTimeZoneIdentifier alone): the
    # identifier to look up, a name or a normalized offset.
    raw = text.encode("utf-8", "surrogatepass")
    z = ZoneParse()
    if Ctx.lib.mlocParseTimeZoneString(raw, len(raw), ctypes.byref(z)) != 0:
        raise refused("zone", text)
    if z.name.length:
        name = text_of(raw, z.name)
        if identifier_only and name != text:
            raise Ctx.host.range_error("not a time zone identifier: %r" % text)
        return name
    if z.utc:
        if identifier_only:
            raise Ctx.host.range_error("not a time zone identifier: %r" % text)
        return "UTC"
    if identifier_only and not _is_offset_identifier(text):
        raise Ctx.host.range_error("not a time zone identifier: %r" % text)
    return offset_identifier(z.offsetMinutes)


def _is_offset_identifier(text):
    import re
    return re.fullmatch(r"[+-]([01]\d|2[0-3])(:?[0-5]\d)?", text) is not None


NS = 10**9
LIMIT = 8_640_000_000_000_000_000_000


def epoch_days(y, m, d):
    # Days from 1970-01-01 to an ISO date, proleptic.
    y -= m <= 2
    era = y // 400
    yoe = y - era * 400
    doy = (153 * (m + (-3 if m > 2 else 9)) + 2) // 5 + d - 1
    return era * 146097 + yoe * 365 + yoe // 4 - yoe // 100 + doy - 719468


def exact_nanoseconds(r, offset):
    # InterpretISODateTimeOffset's exact path: BalanceISODateTime of the
    # date-time less the offset, CheckISODaysRange, then the epoch
    # nanoseconds checked.
    h, mi, s, ms, us, ns = r.time or (0,) * 6
    total = (epoch_days(r.year, r.month, r.day) * 86400 + h * 3600 + mi * 60 + s) * NS + \
        ms * 10**6 + us * 1000 + ns - offset
    if abs(total // (86400 * NS)) > 10**8 or abs(total) > LIMIT:
        raise Ctx.host.range_error("instant out of range")
    return total


def unsupported(what):
    raise Unsupported(what)
