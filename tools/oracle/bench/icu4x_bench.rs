// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

use icu::calendar::Date;
use icu::collator::{options::CollatorOptions, Collator};
use icu::datetime::{fieldsets::YMD, DateTimeFormatter};
use icu::decimal::input::Decimal;
use icu::decimal::DecimalFormatter;
use icu::locale::{locale, LanguageIdentifier, Locale, LocaleExpander};
use icu::plurals::PluralRules;
use std::hint::black_box;
use std::time::Instant;
use writeable::Writeable;

fn time<F: FnMut()>(name: &str, iters: u32, mut f: F) {
    for _ in 0..iters / 10 {
        f();
    }
    let start = Instant::now();
    for _ in 0..iters {
        f();
    }
    let ns = start.elapsed().as_nanos() as f64 / iters as f64;
    println!("{name:<28} {ns:10.1} ns");
}

fn main() {
    let words = [
        "Äpfel", "apfel", "Zebra", "über", "Ufer", "Straße", "Strasse", "Öl",
    ];

    time("plurals construct ru", 20_000, || {
        black_box(PluralRules::try_new_cardinal(locale!("ru").into()).unwrap());
    });
    let pr = PluralRules::try_new_cardinal(locale!("ru").into()).unwrap();
    let mut n: u32 = 0;
    time("plurals select ru int", 10_000_000, || {
        n = (n + 7) % 1000;
        black_box(pr.category_for(black_box(n)));
    });
    let dec_val: Decimal = "1234567.891".parse().unwrap();
    time("plurals select ru decimal", 10_000_000, || {
        black_box(pr.category_for(black_box(&dec_val)));
    });

    time("decimal construct de", 20_000, || {
        black_box(DecimalFormatter::try_new(locale!("de").into(), Default::default()).unwrap());
    });
    let df = DecimalFormatter::try_new(locale!("de").into(), Default::default()).unwrap();
    let mut out = String::with_capacity(64);
    time("decimal format de", 5_000_000, || {
        out.clear();
        df.format(black_box(&dec_val)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");

    time("date construct en YMD medium", 20_000, || {
        black_box(DateTimeFormatter::try_new(locale!("en").into(), YMD::medium()).unwrap());
    });
    let dtf = DateTimeFormatter::try_new(locale!("en").into(), YMD::medium()).unwrap();
    let date = Date::try_new_iso(2026, 10, 12).unwrap();
    time("date format en YMD medium", 5_000_000, || {
        out.clear();
        dtf.format(black_box(&date)).write_to(&mut out).unwrap();
        black_box(&out);
    });
    println!("  -> {out}");

    time("collator construct de", 20_000, || {
        black_box(Collator::try_new(locale!("de").into(), CollatorOptions::default()).unwrap());
    });
    let col = Collator::try_new(locale!("de").into(), CollatorOptions::default()).unwrap();
    let mut i = 0usize;
    time("collator compare de", 10_000_000, || {
        i = (i + 3) % 64;
        black_box(col.compare(black_box(words[i / 8]), black_box(words[i % 8])));
    });

    time("locale parse", 5_000_000, || {
        black_box("zh-Hant-TW-u-nu-hanidec".parse::<Locale>().unwrap());
    });
    let lx = LocaleExpander::new_common();
    let base: LanguageIdentifier = "zh-TW".parse().unwrap();
    time("locale maximize zh-TW", 5_000_000, || {
        let mut l = base.clone();
        lx.maximize(black_box(&mut l));
        black_box(l);
    });
}
