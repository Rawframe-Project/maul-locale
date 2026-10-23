// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4X 2.3 baseline for Maul Locale's locale part: the inputs of
// maul-locale/bench/bench_main.c. ICU4X has no language matcher.
use icu::calendar::week::WeekInformation;
use icu::locale::fallback::{LocaleFallbackConfig, LocaleFallbacker};
use icu::locale::{locale, Locale, LocaleCanonicalizer, LocaleExpander};
use icu_provider::DataLocale;
use std::hint::black_box;
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
    time("parse", 2_000_000, || {
        black_box(Locale::try_from_str(black_box("zh-Hant-TW-u-nu-hanidec")).unwrap());
    });
    let canonicalizer = LocaleCanonicalizer::new_extended();
    time("canonicalize", 2_000_000, || {
        let mut l = Locale::try_from_str(black_box("iw-IL")).unwrap();
        black_box(canonicalizer.canonicalize(&mut l));
        black_box(l);
    });
    time("canonicalize-hard", 1_000_000, || {
        let mut l = Locale::try_from_str(black_box("sh-SU-u-ms-imperial")).unwrap();
        black_box(canonicalizer.canonicalize(&mut l));
        black_box(l);
    });
    let expander = LocaleExpander::new_extended();
    let zh_tw = locale!("zh-TW").id;
    time("maximize", 5_000_000, || {
        let mut l = black_box(zh_tw.clone());
        black_box(expander.maximize(&mut l));
        black_box(l);
    });
    let zh_hant_tw = locale!("zh-Hant-TW").id;
    time("minimize", 2_000_000, || {
        let mut l = black_box(zh_hant_tw.clone());
        black_box(expander.minimize(&mut l));
        black_box(l);
    });
    let fallbacker = LocaleFallbacker::new().for_config(LocaleFallbackConfig::default());
    let es_mx = DataLocale::from(locale!("es-MX"));
    time("parent", 2_000_000, || {
        let mut it = fallbacker.fallback_for(black_box(es_mx.clone()));
        it.step();
        black_box(it.get());
    });
    let en_gb = locale!("en-GB");
    time("preferences", 2_000_000, || {
        black_box(WeekInformation::try_new(black_box((&en_gb).into())).unwrap());
    });
}
