// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4X baseline for Maul Locale: the datetime part's work,
// the same inputs as maul-locale's bench/bench_dates.c, all in en. Best of
// five runs after a warm-up of a tenth, nanoseconds per call. Built with
// icu 2.3, icu_datetime 2.3 with its "unstable" feature (the range
// formatter), icu_experimental 0.6, fixed_decimal 0.7, writeable 0.6;
// cargo build --release, opt-level 3.
use fixed_decimal::Decimal;
use icu::calendar::{Date, Iso};
use icu::datetime::fieldsets::{self, zone};
use icu::datetime::input::{DateTime, Time, ZonedDateTime};
use icu::datetime::options::TimePrecision;
use icu_datetime::range::DateRangeFormatter;
use icu::datetime::DateTimeFormatter;
use icu::locale::locale;
use icu::time::zone::IanaParser;
use icu_experimental::duration::options::DurationFormatterOptions;
use icu_experimental::duration::{Duration, DurationFormatter, DurationSign, ValidatedDurationFormatterOptions};
use icu_experimental::relativetime::{RelativeTimeFormatter, RelativeTimeFormatterOptions};
use std::hint::black_box;
use std::time::Instant;
use writeable::Writeable;

fn time<F: FnMut()>(name: &str, iters: u32, mut f: F) {
    for _ in 0..iters / 10 {
        f();
    }
    let mut best = f64::MAX;
    for _ in 0..5 {
        let start = Instant::now();
        for _ in 0..iters {
            f();
        }
        let ns = start.elapsed().as_nanos() as f64 / iters as f64;
        if ns < best {
            best = ns;
        }
    }
    println!("{name:<18} {best:10.1} ns");
}

fn main() {
    let mut out = String::with_capacity(128);
    let date = Date::try_new_iso(2026, 10, 17).unwrap();
    let clock = Time::try_new(15, 4, 5, 0).unwrap();
    let both = DateTime { date, time: clock };
    let zoned = ZonedDateTime::try_strict_from_str(
        "2026-10-17T15:04:05-04:00[America/New_York]",
        Iso,
        IanaParser::new(),
    )
    .unwrap();
    let start = Date::try_new_iso(2026, 10, 5).unwrap();
    let end = Date::try_new_iso(2026, 10, 8).unwrap();

    time("date-init", 200_000, || {
        black_box(DateTimeFormatter::try_new(locale!("en").into(), fieldsets::YMD::medium()).unwrap());
    });
    let ymd = DateTimeFormatter::try_new(locale!("en").into(), fieldsets::YMD::medium()).unwrap();
    time("date-format", 2_000_000, || {
        out.clear();
        ymd.format(black_box(&date)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
    let hm = DateTimeFormatter::try_new(locale!("en").into(), fieldsets::T::hm()).unwrap();
    time("time-format", 2_000_000, || {
        out.clear();
        hm.format(black_box(&clock)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
    let ymdt = fieldsets::YMDT::medium().with_time_precision(TimePrecision::Minute);
    time("datetime-init", 100_000, || {
        black_box(DateTimeFormatter::try_new(locale!("en").into(), ymdt).unwrap());
    });
    let dt = DateTimeFormatter::try_new(locale!("en").into(), ymdt).unwrap();
    time("datetime-format", 2_000_000, || {
        out.clear();
        dt.format(black_box(&both)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
    let z = DateTimeFormatter::try_new(locale!("en").into(), ymdt.with_zone(zone::SpecificShort)).unwrap();
    time("zoned-format", 1_000_000, || {
        out.clear();
        z.format(black_box(&zoned)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
    let range = DateRangeFormatter::try_new(locale!("en").into(), fieldsets::YMD::medium()).unwrap();
    time("date-range", 1_000_000, || {
        out.clear();
        range.format(black_box(&start), black_box(&end)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
    time("relative-init", 200_000, || {
        black_box(RelativeTimeFormatter::try_new_long_day(locale!("en").into(), RelativeTimeFormatterOptions::default()).unwrap());
    });
    let rel = RelativeTimeFormatter::try_new_long_day(locale!("en").into(), RelativeTimeFormatterOptions::default()).unwrap();
    let three: Decimal = "3".parse().unwrap();
    time("relative-format", 2_000_000, || {
        out.clear();
        rel.format(black_box(three.clone())).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
    let options = ValidatedDurationFormatterOptions::validate(DurationFormatterOptions::default()).unwrap();
    time("duration-init", 100_000, || {
        black_box(DurationFormatter::try_new(locale!("en").into(), options).unwrap());
    });
    let df = DurationFormatter::try_new(locale!("en").into(), options).unwrap();
    let hms = Duration { sign: DurationSign::Positive, hours: 1, minutes: 2, seconds: 3, ..Duration::new() };
    time("duration-format", 1_000_000, || {
        out.clear();
        df.format(black_box(&hms)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
}
