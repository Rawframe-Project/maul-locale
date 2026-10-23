// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4X 2.3 baseline for Maul Locale's decimal and plural parts: the inputs
// of maul-locale/bench/bench_main.c's decimal and plural cases.
// Cargo dependencies: icu = "2" (2.3.1), fixed_decimal = { version =
// "0.7", features = ["ryu"] } (0.7.2); cargo build --release.
use fixed_decimal::{Decimal, FloatPrecision, SignedRoundingMode, UnsignedRoundingMode};
use icu::locale::locale;
use icu::plurals::PluralRules;
use std::hint::black_box;
use std::str::FromStr;
use std::time::Instant;

// Given a case name, times only that case (for perf stat).
fn time<F: FnMut()>(name: &str, iters: u32, mut f: F) {
    if std::env::args().nth(1).is_some_and(|only| only != name) {
        return;
    }
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
        best = best.min(ns);
    }
    println!("{name:<18} {best:10.1} ns");
}

fn main() {
    time("decimal-parse", 5_000_000, || {
        black_box(Decimal::from_str(black_box("1234567.891")).unwrap());
    });
    time("decimal-shortest", 5_000_000, || {
        black_box(Decimal::try_from_f64(black_box(1234567.891), FloatPrecision::RoundTrip).unwrap());
    });
    let value = Decimal::from_str("1234567.891").unwrap();
    time("decimal-round", 5_000_000, || {
        let mut d = black_box(value.clone());
        d.round_with_mode(-2, SignedRoundingMode::Unsigned(UnsignedRoundingMode::HalfEven));
        black_box(d);
    });
    time("plural-init", 200_000, || {
        black_box(PluralRules::try_new_cardinal(locale!("ru").into()).unwrap());
    });
    let rules = PluralRules::try_new_cardinal(locale!("ru").into()).unwrap();
    let mut n: u32 = 0;
    time("plural-select-int", 10_000_000, || {
        n = (n + 7) % 1000;
        black_box(rules.category_for(black_box(n)));
    });
    time("plural-select-dec", 10_000_000, || {
        black_box(rules.category_for(black_box(&value)));
    });
}
