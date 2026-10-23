// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// One case run N times, for instruction counts (perf stat at N and 0).
use fixed_decimal::Decimal;
use icu::decimal::DecimalFormatter;
use icu::locale::locale;
use icu_experimental::dimension::percent::formatter::PercentFormatter;
use icu_experimental::dimension::units::categorized_formatter::CategorizedFormatter;
use icu_experimental::dimension::units::options::{UnitsFormatterOptions, Width};
use icu_experimental::measure::category::Length;
use std::hint::black_box;
use writeable::Writeable;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let case = args[1].as_str();
    let n: u32 = args[2].parse().unwrap();
    let value: Decimal = "1234567.891".parse().unwrap();
    let ratio: Decimal = "25.6".parse().unwrap();
    let length: Decimal = "1234.5".parse().unwrap();
    let mut out = String::with_capacity(128);
    let long = UnitsFormatterOptions::from(Width::Long);
    let de = DecimalFormatter::try_new(locale!("de").into(), Default::default()).unwrap();
    let percent = PercentFormatter::try_new(locale!("en").into(), Default::default()).unwrap();
    let meter = CategorizedFormatter::<Length>::try_new_core(locale!("en").into(), Length::meter(), long).unwrap();
    for _ in 0..n {
        out.clear();
        match case {
            "number-init" => { black_box(DecimalFormatter::try_new(locale!("de").into(), Default::default()).unwrap()); }
            "number-format" => { de.format(black_box(&value)).write_to(&mut out).unwrap(); }
            "percent-format" => { percent.format(black_box(&ratio)).write_to(&mut out).unwrap(); }
            "unit-init" => { black_box(CategorizedFormatter::<Length>::try_new_core(locale!("en").into(), Length::meter(), long).unwrap()); }
            "unit-format" => { meter.format_fixed_decimal(black_box(&length)).write_to(&mut out).unwrap(); }
            _ => panic!("case"),
        }
        black_box(&out);
    }
    println!("{out}");
}
