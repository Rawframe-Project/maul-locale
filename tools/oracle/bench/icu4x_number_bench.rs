// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4X baseline for Maul Locale: the number part's work, the
// same inputs as maul-locale's bench/bench_main.c. Best of five runs after
// a warm-up of a tenth, nanoseconds per call.
use fixed_decimal::Decimal;
use icu::decimal::{CompactDecimalFormatter, DecimalFormatter};
use icu::locale::locale;
use icu::locale::preferences::extensions::unicode::keywords::currency;
use icu_experimental::dimension::currency::formatter::CurrencyFormatter;
use icu_experimental::dimension::percent::formatter::PercentFormatter;
use icu_experimental::dimension::units::categorized_formatter::CategorizedFormatter;
use icu_experimental::dimension::units::options::{UnitsFormatterOptions, Width};
use icu_experimental::measure::category::Length;
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
    let value: Decimal = "1234567.891".parse().unwrap();
    let million: Decimal = "1234567".parse().unwrap();
    // ICU4X's percent formatter writes the value as it is: 25.6 for 25.6%.
    let ratio: Decimal = "25.6".parse().unwrap();
    let money: Decimal = "1234567.89".parse().unwrap();
    let length: Decimal = "1234.5".parse().unwrap();
    let mut out = String::with_capacity(128);

    time("number-init", 500_000, || {
        black_box(DecimalFormatter::try_new(locale!("de").into(), Default::default()).unwrap());
    });
    let de = DecimalFormatter::try_new(locale!("de").into(), Default::default()).unwrap();
    time("number-format", 2_000_000, || {
        out.clear();
        de.format(black_box(&value)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
    let compact = CompactDecimalFormatter::try_new_short(locale!("en").into(), Default::default()).unwrap();
    time("compact-format", 2_000_000, || {
        out.clear();
        compact.format(black_box(&million)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
    let percent = PercentFormatter::try_new(locale!("en").into(), Default::default()).unwrap();
    time("percent-format", 2_000_000, || {
        out.clear();
        percent.format(black_box(&ratio)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
    time("currency-init", 200_000, || {
        black_box(CurrencyFormatter::try_new_symbol(locale!("en").into(), currency!("USD"), Default::default()).unwrap());
    });
    let usd = CurrencyFormatter::try_new_symbol(locale!("en").into(), currency!("USD"), Default::default()).unwrap();
    time("currency-format", 2_000_000, || {
        out.clear();
        usd.format_fixed_decimal(black_box(&money)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
    let long = UnitsFormatterOptions::from(Width::Long);
    time("unit-init", 200_000, || {
        black_box(CategorizedFormatter::<Length>::try_new_core(locale!("en").into(), Length::meter(), long).unwrap());
    });
    let meter = CategorizedFormatter::<Length>::try_new_core(locale!("en").into(), Length::meter(), long).unwrap();
    time("unit-format", 2_000_000, || {
        out.clear();
        meter.format_fixed_decimal(black_box(&length)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");
}
