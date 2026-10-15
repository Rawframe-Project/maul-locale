#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks the library's parsers of Temporal's strings (record mloc-0041)
# against this program's own reading of ECMA-262's grammar
# (temporal/grammar.emu): each goal as regular expressions over its
# basic and extended forms, with backtracking where the library reads
# ahead once; the early errors, ParseISODateTime's annotation rules,
# ParseTemporalDurationString's exact fractions and CreateTemporalDuration's
# limits, ParseDateTimeUTCOffset, ParseTemporalTimeZoneString and
# ParseTemporalCalendarString in code. The strings are every string
# literal of test262's Temporal tests given as an argument, strings
# assembled from the grammar's pieces and near misses, and each of those
# with a byte dropped, doubled or changed. The writers are checked the
# same way against this program's own reading of ECMA-262's formatting
# operations (TemporalDateToString to TemporalDurationToString,
# FormatUTCOffsetNanoseconds), over dates and instants across Temporal's
# range, times, offset zones, every precision and show option, and
# durations with exact sub-second sums past 2^64; each string written is
# also read back through the library's parser.
#
# usage: check_ixdtf.py <conform_ixdtf> [test262 directory]

import os
import random
import re
import subprocess
import sys
from fractions import Fraction

YEAR = r"(?P<year>\d{4}|[+-]\d{6})"
MONTH = r"(?P<month>0[1-9]|1[0-2])"
DAY = r"(?P<day>0[1-9]|[12]\d|3[01])"
HOUR = r"[01]\d|2[0-3]"
MINUTE = r"[0-5]\d"
FRACTION = r"[.,]\d{1,9}"
ZONE_NAME = r"[A-Za-z._][A-Za-z._0-9+\-]*(?:/[A-Za-z._][A-Za-z._0-9+\-]*)*"
ZONE_OFFSET = r"[+-](?:%s)(?::?(?:%s))?" % (HOUR, MINUTE)
ZONE_ANNOTATION = r"(?:\[!?(?P<tz>%s|%s)\])" % (ZONE_OFFSET, ZONE_NAME)
ANNOTATIONS = r"(?P<anns>(?:\[!?[a-z_][a-z_0-9\-]*=[A-Za-z0-9]+(?:-[A-Za-z0-9]+)*\])*)"
GOALS = [1, 2, 4, 8, 16, 32]


def date(extended):
    sep = "-" if extended else ""
    return YEAR + sep + MONTH + sep + DAY


def time(extended):
    sep = ":" if extended else ""
    return (r"(?P<hour>%s)(?:%s(?P<minute>%s)(?:%s(?P<second>%s|60)(?P<frac>%s)?)?)?"
            % (HOUR, sep, MINUTE, sep, MINUTE, FRACTION))


def offset(extended, sub_minute):
    sep = ":" if extended else ""
    seconds = r"(?:%s(?P<osecond>%s)(?P<ofrac>%s)?)?" % (sep, MINUTE, FRACTION) if sub_minute else ""
    return (r"(?P<offset>(?P<osign>[+-])(?P<ohour>%s)(?:%s(?P<ominute>%s)%s)?)"
            % (HOUR, sep, MINUTE, seconds))


def suffix(zoned):
    return ZONE_ANNOTATION + ("" if zoned else "?") + ANNOTATIONS


def date_time_patterns(zoned, time_required, z):
    out = []
    for e1 in (True, False):
        for e2 in (True, False):
            for e3 in (True, False):
                utc = r"(?P<z>[Zz])|" if z else ""
                part = r"[Tt ]%s(?:%s%s)?" % (time(e2), utc, offset(e3, True))
                body = date(e1) + ("(?:%s)" % part if time_required else "(?:%s)?" % part)
                out.append(re.compile(body + suffix(zoned)))
    return out


def instant_patterns():
    out = []
    for e1 in (True, False):
        for e2 in (True, False):
            for e3 in (True, False):
                body = date(e1) + r"[Tt ]%s(?:(?P<z>[Zz])|%s)" % (time(e2), offset(e3, True))
                out.append(re.compile(body + suffix(False)))
    return out


def time_patterns(designated):
    out = []
    for e2 in (True, False):
        for e3 in (True, False):
            body = r"(?P<to>%s(?:%s)?)" % (time(e2), offset(e3, True))
            out.append(re.compile(("[Tt]" if designated else "") + body + suffix(False)))
    return out


MONTH_DAY = re.compile(r"(?:--)?" + MONTH + "-?" + DAY + suffix(False))
YEAR_MONTH = re.compile(YEAR + "-?" + MONTH + suffix(False))
MONTH_DAY_ALONE = re.compile(r"(?:--)?" + MONTH + "-?" + DAY)
YEAR_MONTH_ALONE = re.compile(YEAR + "-?" + MONTH)
PATTERNS = {
    "zoned": date_time_patterns(True, False, True),
    "plain": date_time_patterns(False, False, False),
    "timed": date_time_patterns(False, True, False),
    "instant": instant_patterns(),
    "time-t": time_patterns(True),
    "time": time_patterns(False),
}


class Refused(Exception):
    pass


def leap(y):
    return y % 4 == 0 and (y % 100 != 0 or y % 400 == 0)


def valid_month_day(m, d):
    return not (d == 31 and m in (2, 4, 6, 9, 11)) and not (m == 2 and d == 30)


def early_errors(g):
    # IsValidDate, IsValidMonthDay and the year -000000.
    groups = g.groupdict()
    if groups.get("year") == "-000000":
        return False
    if groups.get("month") and groups.get("day"):
        m, d = int(groups["month"]), int(groups["day"])
        if not valid_month_day(m, d):
            return False
        if groups.get("year") and m == 2 and d == 29 and not leap(int(groups["year"])):
            return False
    return True


def match_any(patterns, text, extra=lambda g: True):
    for p in patterns:
        g = p.fullmatch(text)
        if g and early_errors(g) and extra(g):
            return g
    return None


def goal_match(goal, text):
    # (match, matched without a date-time) for one goal.
    if goal == 1:
        return match_any(PATTERNS["zoned"], text), False
    if goal == 2:
        return match_any(PATTERNS["plain"], text), False
    if goal == 4:
        return match_any(PATTERNS["instant"], text), False
    if goal == 8:
        g = match_any(PATTERNS["time-t"], text)
        if g is None:
            g = match_any(PATTERNS["time"], text, lambda g: not (
                MONTH_DAY_ALONE.fullmatch(g.group("to")) and early_errors(
                    MONTH_DAY_ALONE.fullmatch(g.group("to"))) or
                YEAR_MONTH_ALONE.fullmatch(g.group("to")) and early_errors(
                    YEAR_MONTH_ALONE.fullmatch(g.group("to")))))
        if g is None:
            g = match_any(PATTERNS["timed"], text)
        return g, False
    alone = MONTH_DAY if goal == 16 else YEAR_MONTH
    g = alone.fullmatch(text)
    if g and early_errors(g):
        return g, True
    return match_any(PATTERNS["plain"], text), False


def fraction_ns(text):
    return int((text[1:] + "000000000")[:9]) if text else 0


def offset_ns(g):
    ns = ((int(g.group("ohour")) * 60 + int(g.group("ominute") or 0)) * 60 +
          int((g.groupdict().get("osecond") or 0))) * 10**9 + fraction_ns(
              g.groupdict().get("ofrac") or "")
    return -ns if g.group("osign") == "-" else ns


def annotations(text):
    calendar, critical_calendar, refused = None, False, False
    for critical, key, value in re.findall(r"\[(!?)([a-z_][a-z_0-9\-]*)=([A-Za-z0-9\-]+)\]", text):
        if key == "u-ca":
            if calendar is None:
                calendar, critical_calendar = value, critical == "!"
            elif critical == "!" or critical_calendar:
                refused = True
        elif critical == "!":
            refused = True
    return calendar, refused


def parse_date_time(text, formats):
    for goal in GOALS:
        if not formats & goal:
            continue
        g, alone = goal_match(goal, text)
        if g is None:
            continue
        groups = g.groupdict()
        calendar, refused = annotations(groups.get("anns") or "")
        if refused:
            raise Refused()
        if alone and calendar is not None and calendar.lower() != "iso8601":
            raise Refused()
        year = None if goal == 16 and alone else int(groups.get("year") or 0)
        month = int(groups.get("month") or 1)
        day = int(groups.get("day") or 1)
        t = "_"
        if groups.get("hour"):
            second = int(groups.get("second") or 0)
            t = "%d:%d:%d.%d" % (int(groups["hour"]), int(groups.get("minute") or 0),
                                 59 if second == 60 else second,
                                 fraction_ns(groups.get("frac") or ""))
        utc = 1 if groups.get("z") else 0
        off = "_"
        if not utc and groups.get("offset"):
            off = "%d/%d" % (offset_ns(g), 1 if groups.get("osecond") else 0)
        return "%d %s %d %d %s %d %s %s %s" % (
            goal, "_" if year is None else year, month, day, t, utc, off,
            groups.get("tz") or "_", calendar or "_")
    raise Refused()


OFFSETS = [re.compile(offset(e, True)) for e in (True, False)]


def parse_offset(text):
    for p in OFFSETS:
        g = p.fullmatch(text)
        if g:
            return offset_ns(g), 1 if g.group("osecond") else 0
    raise Refused()


ZONE_ID = re.compile(r"(?P<off>%s)|(?P<name>%s)" % (ZONE_OFFSET, ZONE_NAME))


def zone_identifier(text):
    g = ZONE_ID.fullmatch(text)
    if g is None:
        raise Refused()
    if g.group("name"):
        return "%s 0 _" % text
    sign = -1 if text[0] == "-" else 1
    digits = text[1:].replace(":", "")
    return "_ 0 %d" % (sign * (int(digits[:2]) * 60 + int(digits[2:4] or 0)))


def parse_zone(text):
    if ZONE_ID.fullmatch(text):
        return zone_identifier(text)
    record = parse_date_time(text, 63).split()
    if record[7] != "_":
        return zone_identifier(record[7])
    if record[5] == "1":
        return "_ 1 _"
    if record[6] != "_":
        ns, seconds = map(int, record[6].split("/"))
        if seconds:
            raise Refused()
        return "_ 0 %d" % (ns // (60 * 10**9) if ns >= 0 else -((-ns) // (60 * 10**9)))
    raise Refused()


def parse_calendar(text):
    try:
        record = parse_date_time(text, 63).split()
        return record[8]
    except Refused:
        pass
    if re.fullmatch(r"[A-Za-z0-9]+(?:-[A-Za-z0-9]+)*", text):
        return text
    raise Refused()


DURATION = re.compile(
    r"(?P<sign>[+-])?[Pp](?:(?P<y>\d+)[Yy])?(?:(?P<mo>\d+)[Mm])?(?:(?P<w>\d+)[Ww])?"
    r"(?:(?P<d>\d+)[Dd])?(?P<t>[Tt](?:(?P<h>\d+)(?P<hf>[.,]\d{1,9})?[Hh])?"
    r"(?:(?P<mi>\d+)(?P<mif>[.,]\d{1,9})?[Mm])?(?:(?P<s>\d+)(?P<sf>[.,]\d{1,9})?[Ss])?)?")


def parse_duration(text):
    g = DURATION.fullmatch(text)
    if g is None:
        raise Refused()
    v = g.groupdict()
    date_parts = [v[k] for k in ("y", "mo", "w", "d")]
    time_parts = [v[k] for k in ("h", "mi", "s")]
    if not any(date_parts) and not any(time_parts):
        raise Refused()
    if v["t"] and not any(time_parts):
        raise Refused()
    # A fraction only on the last time part given.
    if (v["hf"] and (v["mi"] or v["s"])) or (v["mif"] and v["s"]):
        raise Refused()
    whole = [int(x or 0) for x in date_parts]
    hours = int(v["h"] or 0)
    if v["hf"]:
        minutes = Fraction(int(v["hf"][1:]), 10**(len(v["hf"]) - 1)) * 60
    else:
        minutes = Fraction(int(v["mi"] or 0))
    if v["mif"]:
        seconds = Fraction(int(v["mif"][1:]), 10**(len(v["mif"]) - 1)) * 60
    elif v["s"]:
        seconds = Fraction(int(v["s"]))
    else:
        seconds = (minutes - int(minutes)) * 60
    if v["sf"]:
        ms = Fraction(int(v["sf"][1:]), 10**(len(v["sf"]) - 1)) * 1000
    else:
        ms = (seconds - int(seconds)) * 1000
    us = (ms - int(ms)) * 1000
    ns = (us - int(us)) * 1000
    fields = whole + [hours] + [int(x) for x in (minutes, seconds, ms, us, ns)]
    factor = -1 if v["sign"] == "-" else 1
    fields = [f * factor for f in fields]
    # CreateTemporalDuration's IsValidDuration.
    if any(abs(f) >= 2**32 for f in fields[:3]):
        raise Refused()
    normalized = (fields[3] * 86400 + fields[4] * 3600 + fields[5] * 60 + fields[6]) * 10**9 + \
        int(float(fields[7])) * 10**6 + int(float(fields[8])) * 10**3 + int(float(fields[9]))
    if abs(normalized) >= 2**53 * 10**9:
        raise Refused()
    return " ".join(format(float(f) + 0.0, ".17g") for f in fields)


def answer(kind, text, formats=63):
    try:
        if kind == "dt":
            return parse_date_time(text, formats)
        if kind == "offset":
            return "%d %d" % parse_offset(text)
        if kind == "zone":
            return parse_zone(text)
        if kind == "calendar":
            return parse_calendar(text)
        return parse_duration(text)
    except Refused:
        return "-"


PIECES = {
    "year": ["2020", "1976", "0000", "9999", "+002020", "-000001", "-271821", "+275760",
             "-000000", "202", "20200", "+20200"],
    "month": ["01", "02", "12", "13", "00", "1"],
    "day": ["01", "28", "29", "30", "31", "32", "00"],
    "sep": ["T", "t", " ", "", "TT"],
    "time": ["12", "12:34", "1234", "12:34:56", "123456", "12:34:56.7", "12:34:56,123456789",
             "12:34:56.1234567890", "23:59:60", "24:00", "12:3", "12:34:5", "1234:56", "12:3456",
             "00", "12:60"],
    "offset": ["", "Z", "z", "+01", "+01:00", "+0100", "-00:00", "+01:00:00", "+010000",
               "+01:00:00.5", "+0100:00", "+01:0000", "+24:00", "-05:30", "+01:00:00.1234567891",
               "+1", "−02:00"],
    "zone": ["", "[UTC]", "[!UTC]", "[Europe/Vienna]", "[+01:00]", "[+0100]", "[+01]",
             "[+01:00:00]", "[America/Argentina/Buenos_Aires]", "[Etc/GMT+5]", "[.x]", "[-x]",
             "[]", "[a/]", "[/a]", "[a//b]", "[America/Mazatlan]", "[zz/z_z]"],
    "ann": ["", "[u-ca=hebrew]", "[!u-ca=hebrew]", "[u-ca=iso8601]", "[foo=bar]", "[!foo=bar]",
            "[u-ca=hebrew][u-ca=gregory]", "[u-ca=hebrew][!u-ca=gregory]",
            "[!u-ca=hebrew][u-ca=gregory]", "[U-CA=hebrew]", "[u-ca=]", "[u-ca=a-b-c]",
            "[u-ca=a--b]", "[_k=v]", "[k-1=v]", "[1k=v]", "[u-ca=ISO8601]", "[z=yz]",
            "[u-ca=zz-z9]"],
}


def generated(rng):
    out = set()
    for _ in range(40000):
        y, m, d = (rng.choice(PIECES[k]) for k in ("year", "month", "day"))
        ext = rng.random() < 0.6
        date = y + ("-" if ext else "") + m + ("-" if ext and rng.random() < 0.95 else "") + d
        kind = rng.random()
        if kind < 0.15:
            date = (rng.choice(["", "--"]) + m + rng.choice(["-", ""]) + d) if rng.random() < 0.5 \
                else (y + rng.choice(["-", ""]) + m)
            text = date
        elif kind < 0.3:
            text = rng.choice(["", "T", "t"]) + rng.choice(PIECES["time"]) + rng.choice(PIECES["offset"])
        elif kind < 0.45:
            text = date
        else:
            text = date + rng.choice(PIECES["sep"]) + rng.choice(PIECES["time"]) + rng.choice(
                PIECES["offset"])
        text += rng.choice(PIECES["zone"]) + rng.choice(PIECES["ann"]) + (
            rng.choice(PIECES["ann"]) if rng.random() < 0.2 else "")
        out.add(text)
    durations = set()
    units = ["Y", "M", "W", "D"]
    for _ in range(20000):
        text = rng.choice(["", "", "-", "+"]) + rng.choice(["P", "p"])
        for u in units:
            if rng.random() < 0.3:
                text += str(rng.choice([0, 1, 5, 12, 4294967295, 4294967296, 10**20])) + rng.choice(
                    [u, u.lower()])
        if rng.random() < 0.7:
            text += rng.choice(["T", "t"])
            for u in ["H", "M", "S"]:
                if rng.random() < 0.5:
                    text += str(rng.choice([0, 1, 46, 66, 71, 2501999792983, 9007199254740991,
                                            10**25]))
                    if rng.random() < 0.3:
                        text += rng.choice([".", ","]) + str(rng.randrange(1, 10**rng.randint(1, 10)))
                    text += rng.choice([u, u.lower()])
        if rng.random() < 0.05:
            text = text.replace("T", "", 1)
        durations.add(text)
    return out, durations


def mutated(rng, texts, count):
    out = set()
    texts = sorted(texts)
    for _ in range(count):
        t = rng.choice(texts)
        if not t:
            continue
        i = rng.randrange(len(t))
        op = rng.random()
        if op < 0.33:
            t = t[:i] + t[i + 1:]
        elif op < 0.66:
            t = t[:i] + t[i] + t[i:]
        else:
            t = t[:i] + rng.choice("0123456789-:+TZ[]=!/.,PYMWDHS ") + t[i + 1:]
        out.add(t)
    return out


def test262_strings(root):
    out = set()
    for dirpath, _, files in os.walk(os.path.join(root, "test")):
        if "Temporal" not in dirpath:
            continue
        for name in files:
            if not name.endswith(".js"):
                continue
            with open(os.path.join(dirpath, name), encoding="utf-8") as f:
                source = f.read()
            for m in re.finditer(r"\"([^\"\\\n]{0,100})\"|'([^'\\\n]{0,100})'", source):
                s = m.group(1) if m.group(1) is not None else m.group(2)
                if re.search(r"\d", s) or s[:2].upper() in ("P", "-P", "+P"):
                    out.add(s)
    return out


# The writers, as ECMA-262's formatting operations write.

MIN_DAY, MAX_DAY = -99280838, 100719162
SHOW_CALENDAR = ["auto", "always", "never", "critical"]
SHOW_TIME_ZONE = ["zone-auto", "zone-never", "zone-critical"]


def iso_date(day):
    # mlocDate's day 1 is 0001-01-01; days from 1970 by Hinnant's
    # civil_from_days.
    z = day - 719163 + 719468
    era = z // 146097
    doe = z - era * 146097
    yoe = (doe - doe // 1460 + doe // 36524 - doe // 146096) // 365
    doy = doe - (365 * yoe + yoe // 4 - yoe // 100)
    mp = (5 * doy + 2) // 153
    d = doy - (153 * mp + 2) // 5 + 1
    m = mp + 3 if mp < 10 else mp - 9
    return yoe + era * 400 + (m <= 2), m, d


def pad_year(y):
    return "%04d" % y if 0 <= y <= 9999 else ("+" if y > 0 else "-") + "%06d" % abs(y)


def calendar_annotation(cal, show):
    if show == "never" or (show == "auto" and cal == "iso8601"):
        return ""
    return "[%su-ca=%s]" % ("!" if show == "critical" else "", cal)


def fraction(ns, precision):
    if precision == -1:
        return ("." + "%09d" % ns).rstrip("0") if ns else ""
    return "." + ("%09d" % ns)[:precision] if precision > 0 else ""


def time_string(h, m, s, ns, precision):
    if precision == -2:
        return "%02d:%02d" % (h, m)
    return "%02d:%02d:%02d" % (h, m, s) + fraction(ns, precision)


def offset_minutes_string(minutes):
    return ("-" if minutes < 0 else "+") + "%02d:%02d" % divmod(abs(minutes), 60)


def utc_offset_string(ns):
    a = abs(ns)
    s, sub = divmod(a, 10**9)
    precision = -2 if s % 60 == 0 and sub == 0 else -1
    return ("-" if ns < 0 else "+") + time_string(s // 3600, s // 60 % 60, s % 60, sub, precision)


def date_time_of(seconds, nanoseconds):
    day, rest = divmod(seconds, 86400)
    y, m, d = iso_date(day + 719163)
    return "%s-%02d-%02dT" % (pad_year(y), m, d), rest


def write_answer(q):
    kind, v = q[0], q[1:]
    if kind in ("wdate", "wym", "wmd"):
        day, cal, precision, show = v
        y, m, d = iso_date(day)
        full = show in ("always", "critical") or cal != "iso8601"
        if kind == "wdate":
            text = "%s-%02d-%02d" % (pad_year(y), m, d)
        elif kind == "wym":
            text = "%s-%02d" % (pad_year(y), m) + ("-%02d" % d if full else "")
        else:
            text = ("%s-" % pad_year(y) if full else "") + "%02d-%02d" % (m, d)
        return text + (calendar_annotation(cal, show) if kind == "wdate" or full else "")
    if kind == "wtime":
        return time_string(*v)
    if kind == "wdt":
        day, h, m, s, ns, cal, precision, show = v
        y, mo, d = iso_date(day)
        return "%s-%02d-%02dT" % (pad_year(y), mo, d) + time_string(h, m, s, ns, precision) + \
            calendar_annotation(cal, show)
    if kind in ("winst", "wzdt"):
        if kind == "winst":
            seconds, ns, zone, precision = v
            cal, show, tz, hide = "iso8601", "never", "zone-never", zone is None
        else:
            zone, seconds, ns, cal, precision, show, tz, hide = v
        if abs(seconds * 10**9 + ns) > 8640000000000 * 10**9:
            return "-"
        date, rest = date_time_of(seconds + (zone or 0) * 60, ns)
        text = date + time_string(rest // 3600, rest // 60 % 60, rest % 60, ns, precision)
        if kind == "winst" and zone is None:
            return text + "Z"
        if not hide:
            text += offset_minutes_string(zone)
        if tz != "zone-never":
            text += "[%s%s]" % ("!" if tz == "zone-critical" else "", offset_minutes_string(zone))
        return text + calendar_annotation(cal, show)
    if kind == "wdur":
        fields, precision = v[:10], v[10]
        values = [Fraction(f) for f in fields]
        signs = {(f > 0) - (f < 0) for f in values} - {0}
        if len(signs) > 1 or precision == -2 or any(f.denominator != 1 for f in values) or \
                any(abs(f) >= 2**32 for f in values[:3]):
            return "-"
        a = [abs(int(f)) for f in values]
        total = ((a[3] * 24 + a[4]) * 60 + a[5]) * 60 * 10**9 + a[6] * 10**9 + a[7] * 10**6 + \
            a[8] * 10**3 + a[9]
        if total >= 2**53 * 10**9:
            return "-"
        date = "".join("%d%s" % (n, c) for n, c in zip(a[:4], "YMWD") if n)
        time = "".join("%d%s" % (n, c) for n, c in zip(a[4:6], "HM") if n)
        sub = a[6] * 10**9 + a[7] * 10**6 + a[8] * 10**3 + a[9]
        if sub or not any(a[:6]) or precision != -1:
            time += "%d%sS" % (sub // 10**9, fraction(sub % 10**9, precision))
        return ("-" if signs == {-1} else "") + "P" + date + ("T" + time if time else "")
    if kind == "woffset":
        return utc_offset_string(v[0]) if abs(v[0]) < 86400 * 10**9 else "-"
    raise ValueError(kind)


def truncated(ns, precision):
    if precision == -2:
        return 0
    if precision == -1:
        return ns
    return ns // 10 ** (9 - precision) * 10 ** (9 - precision)


def read_back_question(q, text):
    # The parse that reads a written string back, and what it must give.
    kind, v = q[0], q[1:]
    hexed = "x" + text.encode("utf-8").hex()
    if kind == "wdur":
        fields, precision = v[:10], v[10]
        a = [abs(int(f)) for f in fields]
        sign = -1 if any(f < 0 for f in fields) else 1
        sub = a[6] * 10**9 + a[7] * 10**6 + a[8] * 10**3 + a[9]
        frac = truncated(sub % 10**9, precision)
        want = a[:6] + [sub // 10**9, frac // 10**6, frac // 1000 % 1000, frac % 1000]
        return "duration " + hexed, [sign * n if n else 0 for n in want]
    if kind == "woffset":
        return "offset " + hexed, [v[0]]
    if kind in ("wdate", "wym", "wmd"):
        y, m, d = iso_date(v[0])
        return "dt 63 " + hexed, [y, m, d, None, None]
    if kind == "wtime":
        h, m, s, ns, precision = v
        return "dt 8 " + hexed, [None, None, None, (h, m, 0 if precision == -2 else s,
                                                    truncated(ns, precision)), None]
    if kind == "wdt":
        day, h, m, s, ns, cal, precision, show = v
        y, mo, d = iso_date(day)
        return "dt 2 " + hexed, [y, mo, d, (h, m, 0 if precision == -2 else s,
                                            truncated(ns, precision)), None]
    if kind == "winst":
        seconds, ns, zone, precision = v
    else:
        zone, seconds, ns, cal, precision, show, tz, hide = v
    local = seconds + (zone or 0) * 60
    day, rest = divmod(local, 86400)
    y, mo, d = iso_date(day + 719163)
    t = (rest // 3600, rest // 60 % 60, 0 if precision == -2 else rest % 60,
         truncated(ns, precision))
    offset = None if kind == "wzdt" and hide else (zone or 0) * 60 * 10**9
    return "dt 63 " + hexed, [y, mo, d, t, offset]


def read_back(pairs):
    # Reads each written string back through the library's parser; returns
    # the count of strings that do not give their values.
    asked = [read_back_question(q, text) for q, text in pairs]
    got = subprocess.run([sys.argv[1]], input="\n".join(a for a, _ in asked) + "\n",
                         capture_output=True, text=True, check=True).stdout.splitlines()
    failures = 0
    for (q, text), (question, want), have in zip(pairs, asked, got):
        tokens = have.split()
        if question.startswith("duration") or question.startswith("offset"):
            ok = tokens != ["-"] and [int(float(x)) for x in tokens[:len(want)]] == want
        else:
            ok = tokens != ["-"] and len(tokens) == 9
            if ok:
                year, month, day, time, offset = want
                ok = (year is None or (tokens[1], tokens[2], tokens[3]) ==
                      (str(year), str(month), str(day)) or q[0] in ("wym", "wmd"))
                if time is not None:
                    ok = ok and tokens[4] == "%d:%d:%d.%d" % time
                if offset is not None:
                    ok = ok and (tokens[6] == "%d/0" % offset or (offset == 0 and tokens[5] == "1"))
        if not ok:
            failures += 1
            if failures <= 30:
                print("read back %r (%s): %r" % (text, write_question(q), have))
    return failures


def write_question(q):
    parts = [q[0]]
    for x in q[1:]:
        if x is None:
            parts.append("-")
        elif isinstance(x, str):
            parts.append(str(SHOW_CALENDAR.index(x)) if x in SHOW_CALENDAR else
                         str(SHOW_TIME_ZONE.index(x)) if x in SHOW_TIME_ZONE else x)
        elif isinstance(x, float):
            parts.append(repr(x))
        else:
            parts.append(str(x))
    return " ".join(parts)


def write_questions(rng):
    out = []
    calendars = ["iso8601", "gregory", "hebrew", "islamic-umalqura", "japanese"]
    days = [MIN_DAY, MAX_DAY, 1, 0, 719163, 3652059, 3652060] + \
        [rng.randint(MIN_DAY, MAX_DAY) for _ in range(3000)]
    precisions = [-1, -2] + list(range(10))
    for day in days:
        cal = rng.choice(calendars)
        show = rng.choice(SHOW_CALENDAR)
        for kind in ("wdate", "wym", "wmd"):
            out.append((kind, day, cal, -1, show))
        t = (rng.randrange(24), rng.randrange(60), rng.randrange(60),
             rng.choice([0, 1, 500000000, 999999999, rng.randrange(10**9)]))
        out.append(("wdt", day) + t + (cal, rng.choice(precisions), show))
    for _ in range(3000):
        t = (rng.randrange(24), rng.randrange(60), rng.randrange(60),
             rng.choice([0, 10, 120000000, rng.randrange(10**9)]))
        out.append(("wtime",) + t + (rng.choice(precisions),))
    limit = 8640000000000
    for _ in range(6000):
        seconds = rng.choice([limit, -limit, limit + 1, 0, -1, rng.randint(-limit, limit)])
        ns = rng.choice([0, 1, 999999999, rng.randrange(10**9)])
        zone = rng.choice([None, 0, 330, -44, 1439, -1439, rng.randint(-1439, 1439)])
        out.append(("winst", seconds, ns, zone, rng.choice(precisions)))
        if zone is not None:
            out.append(("wzdt", zone, seconds, ns, rng.choice(calendars), rng.choice(precisions),
                        rng.choice(SHOW_CALENDAR), rng.choice(SHOW_TIME_ZONE),
                        rng.randrange(2)))
    for _ in range(6000):
        sign = rng.choice([1, -1])
        fields = []
        for i in range(10):
            if rng.random() < 0.5:
                fields.append(0.0)
                continue
            top = [2**32, 2**32, 2**32, 10**11, 10**12, 10**14, 2**53, 2**63, 2**73, 2**83][i]
            fields.append(float(sign * rng.choice([1, rng.randrange(1, 1000), rng.randrange(top)])))
        if rng.random() < 0.05:
            fields[rng.randrange(10)] *= -1
        out.append(("wdur",) + tuple(fields) + (rng.choice([-1, -2] + list(range(10))),))
    for precision in [-1, -2] + list(range(10)):
        for fields in ([0.0] * 10, [1.0] + [0.0] * 9, [0.0] * 5 + [-5.0] + [0.0] * 4,
                       [0.0] * 3 + [2.0] + [0.0] * 6, [0.0] * 9 + [1.0]):
            out.append(("wdur",) + tuple(fields) + (precision,))
    for _ in range(2000):
        out.append(("woffset", rng.choice([0, -1, 86400 * 10**9, rng.randint(-86399 * 10**9,
                                                                             86399 * 10**9),
                                          rng.randint(-1439, 1439) * 60 * 10**9])))
    return out


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit("usage: check_ixdtf.py <conform_ixdtf> [test262 directory]")
    rng = random.Random(9557)
    texts, durations = generated(rng)
    if len(sys.argv) == 3:
        literals = test262_strings(sys.argv[2])
        texts |= literals
        durations |= {s for s in literals if "P" in s.upper()}
    texts |= mutated(rng, texts, 40000)
    durations |= mutated(rng, durations, 20000)
    questions = []
    for t in sorted(texts):
        for formats in GOALS + [63, 3, 48]:
            questions.append(("dt", t, formats))
        questions.append(("zone", t, 0))
        questions.append(("calendar", t, 0))
    offsets = {t for t in texts if len(t) <= 20} | set(PIECES["offset"])
    for t in sorted(offsets):
        questions.append(("offset", t, 0))
    for t in sorted(durations):
        questions.append(("duration", t, 0))
    lines = []
    for kind, t, formats in questions:
        hexed = "x" + t.encode("utf-8").hex()
        lines.append("dt %d %s" % (formats, hexed) if kind == "dt" else "%s %s" % (kind, hexed))
    writes = write_questions(rng)
    lines += [write_question(q) for q in writes]
    got = subprocess.run([sys.argv[1]], input="\n".join(lines) + "\n", capture_output=True,
                         text=True, check=True).stdout.splitlines()
    if len(got) != len(questions) + len(writes):
        sys.exit("conform_ixdtf answered %d of %d questions" % (len(got), len(lines)))
    failures = 0
    for (kind, t, formats), have in zip(questions, got):
        want = answer(kind, t, formats)
        if have != want:
            failures += 1
            if failures <= 30:
                print("%s %r %s: ECMA-262 %r, library %r" % (kind, t, formats or "", want, have))
    # Each string written, then read back by the library's parser.
    readback = []
    for q, have in zip(writes, got[len(questions):]):
        want = write_answer(q)
        if have != want:
            failures += 1
            if failures <= 30:
                print("%s: ECMA-262 %r, library %r" % (write_question(q), want, have))
        elif have != "-":
            readback.append((q, have))
    failures += read_back(readback)
    print("%d questions over %d strings and %d durations, %d strings written" % (
        len(questions), len(texts), len(durations), len(writes)))
    if failures:
        sys.exit("%d answers differ from ECMA-262" % failures)


if __name__ == "__main__":
    main()
