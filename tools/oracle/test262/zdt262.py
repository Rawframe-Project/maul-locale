# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Temporal.ZonedDateTime's arithmetic for t262cal.py, on Maul Locale's
# zoned functions: add and subtract, until and since, round, startOfDay,
# hoursInDay, from and with of property bags (the offset weighed against
# the zone), withPlainTime, withCalendar, the local fields and the
# conversions. zone262.ZonedDateTime hands it each property it does not
# know. Options are read here, as a host reads them. Each value an
# assertion can check carries its origin, a JSON object with key "zdt"
# (instants as [seconds, nanoseconds], floored; a date-time as [day, h,
# mi, s, ns], day in mlocDate's count; units mlocUnit's, a round's unit
# mlocTimeUnit's):
#   {"zdt": "add", "id", "calendar", "at", "d": [y, mo, w, d, s, ns],
#    "overflow"}                                                  prop "ns"
#   {"zdt": "until"|"since", "id", "calendar", "one", "two",
#    "r": [largest, smallest, increment, mode]}   props years..nanoseconds
#   {"zdt": "round", "id", "at", "r": [unit, increment, mode]}    prop "ns"
#   {"zdt": "start", "id", "day"}                                 prop "ns"
#   {"zdt": "offset", "id", "dt", "offset", "option", "dis", "minutes"}
#                                                                 prop "ns"
# or "refused".

import ctypes
import json

from jsmini import UNDEF, Unsupported, JSObject, Builtin, Tracked, un, BigInt
import jsmini as js
import time262 as tm
import datetime262 as dt
import zone262 as zn

NS = 10**9
OPTIONS = {"use": 0, "ignore": 1, "prefer": 2, "reject": 3}
DISAMBIGUATIONS = {"compatible": 0, "earlier": 1, "later": 2, "reject": 3}


class Ctx:
    lib = None
    host = None


def setup(lib, host):
    Ctx.lib, Ctx.host = lib, host


def key(o):
    return json.dumps(o, sort_keys=True)


def at(ns):
    return list(divmod(ns, NS))


def local(z):
    out = dt.DateTime()
    if Ctx.lib.mlocGetDateTimeFor(ctypes.byref(z.zone), ctypes.byref(zn.instant(z.ns)),
                                  Ctx.host.CALENDARS[z.calendar], ctypes.byref(out)) != 0:
        raise Unsupported("local date-time")
    return out


def dt_parts(v):
    return [v.date.day] + [v.time.hour, v.time.minute, v.time.second, v.time.nanosecond]


def made(z, ns, origin):
    return zn.ZonedDateTime(ns, z.given, z.zone, origin, calendar=z.calendar)


def get(z, prop):
    h = Ctx.host
    if prop in dt.DATE_FIELDS or prop in tm.TIME_FIELDS or prop in (
            "dayOfWeek", "dayOfYear", "daysInMonth", "daysInYear", "monthsInYear", "inLeapYear",
            "daysInWeek"):
        return dt.PlainDateTime(z.calendar, local(z), None).js_get(prop)
    if prop == "offset":
        import str262
        return str262.offset_string(z)
    if prop in ("toString", "toJSON"):
        import str262
        return {"toString": Builtin(lambda o=UNDEF: str262.zoned_string(z, o)),
                "toJSON": Builtin(lambda *a: str262.zoned_string(z))}[prop]
    if prop == "add":
        return Builtin(lambda d=UNDEF, o=UNDEF: add(z, d, o, 1))
    if prop == "subtract":
        return Builtin(lambda d=UNDEF, o=UNDEF: add(z, d, o, -1))
    if prop == "until":
        return Builtin(lambda o=UNDEF, opts=UNDEF: until(z, o, opts, False))
    if prop == "since":
        return Builtin(lambda o=UNDEF, opts=UNDEF: until(z, o, opts, True))
    if prop == "round":
        return Builtin(lambda opts=UNDEF: round_(z, opts))
    if prop == "startOfDay":
        return Builtin(lambda: start_of_day(z, local(z).date.day))
    if prop == "hoursInDay":
        day = local(z).date.day
        first, next_ = start_of_day(z, day), start_of_day(z, day + 1)
        hours = (next_.ns - first.ns) / (3600 * NS)
        return int(hours) if hours.is_integer() else hours
    if prop == "toInstant":
        import instant262
        return Builtin(lambda: instant262.Instant(z.ns))
    if prop == "toPlainDateTime":
        return Builtin(lambda: dt.PlainDateTime(z.calendar, local(z), None))
    if prop == "toPlainDate":
        return Builtin(lambda: h.PlainDate(z.calendar, local(z).date.day, None))
    if prop == "toPlainTime":
        return Builtin(lambda: tm.PlainTime(local(z).time, None))
    if prop == "withCalendar":
        def with_calendar(c=UNDEF):
            if un(c) is UNDEF:
                raise h.type_error("calendar required")
            return zn.ZonedDateTime(z.ns, z.given, z.zone, calendar=h.calendar_id(c))
        return Builtin(with_calendar)
    if prop == "withPlainTime":
        return Builtin(lambda t=UNDEF: with_plain_time(z, t))
    if prop == "with":
        return Builtin(lambda bag=UNDEF, opts=UNDEF: with_(z, bag, opts))
    raise Unsupported("ZonedDateTime." + prop)


def add(z, item, options, sign):
    h = Ctx.host
    values = tm.duration_values(item)
    if sign < 0:
        values = [-v for v in values]
    overflow = h.read_overflow(options)
    t = tm.time_duration(values[4:])
    d = dt.DateTimeDuration(dt.DateDuration(*values[:4]), t)
    origin = key({"zdt": "add", "id": z.given, "calendar": z.calendar, "at": at(z.ns),
                  "d": values[:4] + [t.seconds, t.nanoseconds], "overflow": overflow})
    out = zn.Instant()
    h.LAST_MADE[0] = origin
    if Ctx.lib.mlocAddToZonedDateTime(ctypes.byref(z.zone), h.CALENDARS[z.calendar],
                                      ctypes.byref(zn.instant(z.ns)), ctypes.byref(d),
                                      1 if overflow == "reject" else 0, ctypes.byref(out)) != 0:
        raise h.range_error("add refused", origin)
    return made(z, out.seconds * NS + out.nanoseconds, origin)


def settings(options):
    # GetDifferenceSettings for zoned date-times: the largest unit's
    # default is hour or the smallest if larger.
    h = Ctx.host
    options = un(options)
    if options is UNDEF or isinstance(options, (js.JSFunction, Builtin)):
        options = JSObject({})
    if not isinstance(options, JSObject):
        raise Unsupported("difference options")
    largest = dt.read_unit(options, "largestUnit", "auto")
    increment = tm.read_increment(options)
    mode = tm.read_mode(options, "trunc")
    smallest = dt.read_unit(options, "smallestUnit", 9)
    if largest == "auto":
        largest = min(smallest, 4)
    if largest > smallest:
        raise h.range_error("largest smaller than smallest")
    return largest, smallest, increment, mode


def until(z, other, options, since):
    h = Ctx.host
    other = to_zoned(other)
    if other.calendar != z.calendar:
        raise h.range_error("calendars differ")
    largest, smallest, increment, mode = settings(options)
    if largest < 4:
        equal = ctypes.c_bool()
        if Ctx.lib.mlocTimeZonesEqual(ctypes.byref(z.zone), ctypes.byref(other.zone),
                                      ctypes.byref(equal)) != 0:
            raise Unsupported("time zones equal")
        if not equal.value:
            raise h.range_error("time zones differ for a date unit")
        if z.ns == other.ns:
            # DifferenceTemporalZonedDateTime: equal instants, before any
            # rounding.
            return h.DurationObj([0] * 10)
    operation = "since" if since else "until"
    origin = key({"zdt": operation, "id": z.given, "calendar": z.calendar, "one": at(z.ns),
                  "two": at(other.ns), "r": [largest, smallest, increment, mode]})
    r = dt.DateTimeRounding(largest, smallest, increment,
                            h.MODES[h.NEGATED.get(mode, mode) if since else mode])
    d = dt.DateTimeDuration()
    h.LAST_MADE[0] = origin
    if Ctx.lib.mlocZonedDateTimeUntil(ctypes.byref(z.zone), h.CALENDARS[z.calendar],
                                      ctypes.byref(zn.instant(z.ns)),
                                      ctypes.byref(zn.instant(other.ns)), ctypes.byref(r),
                                      ctypes.byref(d)) != 0:
        raise h.range_error("until refused", origin)
    # TemporalDurationFromInternal: the time part balanced to the largest
    # unit when it is a time unit, else to hours.
    unit = largest - 3 if largest >= 4 else 1
    f = tm.DurationFields()
    if Ctx.lib.mlocBalanceTimeDuration(ctypes.byref(d.time), unit, ctypes.byref(f)) == 0:
        time_values = [f.hours, f.minutes, f.seconds, f.milliseconds, f.microseconds,
                       f.nanoseconds]
    else:
        total = d.time.seconds * NS + d.time.nanoseconds
        sign = -1 if total < 0 else 1
        if unit == 6:
            time_values = [0, 0, 0, 0, 0, total]
        elif unit == 5:
            time_values = [0, 0, 0, 0, sign * (abs(total) // 1000), sign * (abs(total) % 1000)]
        else:
            raise Unsupported("balance past int64")
    values = [d.date.years, d.date.months, d.date.weeks, d.date.days] + time_values
    values = [int(float(v)) for v in values]
    return h.DurationObj(values, origin, since, track=range(10))


def round_(z, options):
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
    origin = key({"zdt": "round", "id": z.given, "at": at(z.ns), "r": [unit, increment, mode]})
    out = zn.Instant()
    h.LAST_MADE[0] = origin
    if Ctx.lib.mlocRoundZonedDateTime(ctypes.byref(z.zone), ctypes.byref(zn.instant(z.ns)), unit,
                                      increment, h.MODES[mode], ctypes.byref(out)) != 0:
        raise h.range_error("round refused", origin)
    return made(z, out.seconds * NS + out.nanoseconds, origin)


def start_of_day(z, day):
    h = Ctx.host
    origin = key({"zdt": "start", "id": z.given, "day": day})
    out = zn.Instant()
    h.LAST_MADE[0] = origin
    date = dt.Date(day, h.CALENDARS["gregory"])
    if not (-2**31 <= day < 2**31) or Ctx.lib.mlocGetStartOfDay(
            ctypes.byref(z.zone), ctypes.byref(date), ctypes.byref(out)) != 0:
        raise h.range_error("start of day refused", origin)
    return made(z, out.seconds * NS + out.nanoseconds, origin)


def parse_offset(text):
    # ParseDateTimeUTCOffset, in nanoseconds, through mlocParseUTCOffset.
    raw = text.encode("utf-8", "surrogatepass")
    ns = ctypes.c_int64()
    if Ctx.lib.mlocParseUTCOffset(raw, len(raw), ctypes.byref(ns), None) != 0:
        import ixdtf262
        raise ixdtf262.refused("offset", text)
    return ns.value


def interpret(z_given, zone, value, offset, option, disambiguation, minutes):
    h = Ctx.host
    origin = key({"zdt": "offset", "id": z_given, "dt": dt_parts(value), "offset": offset,
                  "option": option, "dis": disambiguation, "minutes": minutes})
    out = zn.Instant()
    h.LAST_MADE[0] = origin
    if Ctx.lib.mlocGetInstantForOffset(ctypes.byref(zone), ctypes.byref(value),
                                       ctypes.c_int64(offset), OPTIONS[option],
                                       DISAMBIGUATIONS[disambiguation], bool(minutes),
                                       ctypes.byref(out)) != 0:
        raise h.range_error("offset interpretation refused", origin)
    return out.seconds * NS + out.nanoseconds, origin


def read_choice(options, name, choices, default):
    options = un(options)
    if options is UNDEF:
        return default
    if not isinstance(options, JSObject):
        raise Unsupported("options type")
    v = un(options.get(name))
    if v is UNDEF:
        return default
    if not isinstance(v, str):
        raise Unsupported(name + " option type")
    if v not in choices:
        raise Ctx.host.range_error(name + " " + v)
    return v


def bag_date_time(item, options, calendar):
    # A bag's date through the calendar's fields and its time through
    # mlocMakeTime, as datetime262.to_date_time reads them.
    bag = JSObject(dict(item.props)) if hasattr(item, "props") else item
    if calendar is not None and un(item.get("calendar")) is UNDEF:
        bag.set("calendar", calendar) if hasattr(bag, "set") else None
    return dt.to_date_time(bag, options)


def from_bag(item, options, default_offset_option):
    # ToTemporalZonedDateTime's order: the calendar, the offset's syntax,
    # the time zone required and found, the options, then the fields.
    h = Ctx.host
    if un(item.get("calendar")) is not UNDEF:
        h.calendar_id(item.get("calendar"))
    offset_text = un(item.get("offset"))
    if offset_text is not UNDEF and not isinstance(offset_text, str):
        raise Unsupported("offset of a non-string")
    offset = parse_offset(offset_text) if offset_text is not UNDEF else None
    given = un(item.get("timeZone"))
    if given is UNDEF:
        raise h.type_error("timeZone required")
    given, zone = zn.to_zone(given, False)
    disambiguation = read_choice(options, "disambiguation", DISAMBIGUATIONS, "compatible")
    option = read_choice(options, "offset", OPTIONS, default_offset_option)
    local_dt = dt.to_date_time(item, options)
    if offset is None:
        option = "ignore"
    ns, origin = interpret(given, zone, local_dt.value, offset or 0, option, disambiguation, 0)
    return zn.ZonedDateTime(ns, given, zone, origin, calendar=local_dt.calendar)


def with_(z, bag, options):
    raise Unsupported("ZonedDateTime.with")


def with_plain_time(z, item):
    h = Ctx.host
    item = un(item)
    day = local(z).date.day
    if item is UNDEF:
        return start_of_day(z, day)
    t = tm.to_time(item)
    # The date-time made as mlocMakeDateTime makes it, refused past
    # Temporal's date-times.
    value = dt.DateTime()
    if Ctx.lib.mlocMakeDateTime(ctypes.byref(local(z).date), ctypes.byref(t.t),
                                ctypes.byref(value)) != 0:
        raise h.range_error("date-time refused")
    ns, origin = interpret(z.given, z.zone, value, 0, "ignore", "compatible", 0)
    return made(z, ns, origin)


def zoned_of_record(r, name, zone, calendar, disambiguation, option, minutes):
    # InterpretISODateTimeOffset of a parsed zoned string: the start of
    # its day when it gives no time; Z exact, an offset by the option, or
    # none the wall-clock time; the offset matched to the minute when
    # minutes is set.
    import ixdtf262
    h = Ctx.host
    z = zn.ZonedDateTime(0, name, zone, calendar=calendar)
    if r.time is None:
        day = h.make("gregory", None, r.year, None, r.month, r.day, "reject")
        if day is None:
            raise h.range_error("date refused")
        return start_of_day(z, day)
    if r.utc or (r.offset is not None and option == "use"):
        return made(z, ixdtf262.exact_nanoseconds(r, 0 if r.utc else r.offset), None)
    if r.offset is None:
        option = "ignore"
    value = dt.make(calendar, [r.year, r.month, r.day], list(r.time)).value
    ns, origin = interpret(name, zone, value, r.offset or 0, option, disambiguation, minutes)
    return made(z, ns, origin)


def from_string(text, options):
    # ToTemporalZonedDateTime of a string: the zone, calendar and offset's
    # precision, then the options.
    import ixdtf262
    r = ixdtf262.parse(text, ixdtf262.DATE_TIME_ZONED)
    name, zone = zn.to_zone(r.time_zone, False)
    calendar = ixdtf262.canonical_calendar(r.calendar)
    disambiguation = read_choice(options, "disambiguation", DISAMBIGUATIONS, "compatible")
    option = read_choice(options, "offset", OPTIONS, "reject")
    Ctx.host.read_overflow(options)
    return zoned_of_record(r, name, zone, calendar, disambiguation, option,
                           0 if r.offset_has_seconds else 1)


def to_zoned(item, options=UNDEF):
    item = un(item)
    if isinstance(item, zn.ZonedDateTime):
        if options is not UNDEF:
            read_choice(options, "disambiguation", DISAMBIGUATIONS, "compatible")
            read_choice(options, "offset", OPTIONS, "reject")
            Ctx.host.read_overflow(options)
        return zn.ZonedDateTime(item.ns, item.given, item.zone, item.origin,
                                calendar=item.calendar)
    if isinstance(item, JSObject):
        return from_bag(item, options, "reject")
    if isinstance(item, str):
        return from_string(item, options)
    raise Unsupported("ZonedDateTime from %r" % (item,))


def expected(prop, v):
    v = un(v)
    if prop == "ns":
        if isinstance(v, BigInt):
            return str(v.v)
        if isinstance(v, zn.ZonedDateTime):
            return str(v.ns)
        raise Unsupported("expected instant %r" % (v,))
    if isinstance(v, float) and v.is_integer():
        v = int(v)
    if not isinstance(v, int) or isinstance(v, bool):
        raise Unsupported("expected value %r" % (v,))
    return v
