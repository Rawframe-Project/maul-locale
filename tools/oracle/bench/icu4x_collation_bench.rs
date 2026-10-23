// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4X 2.3 baseline for Maul Locale: collation's work, the
// same inputs as icu4c_collation_bench.cpp and maul-locale's
// bench/bench_collation.c, nanoseconds per call, the best of five runs
// after a warm-up of a tenth. ICU4X has no alphabetic index and no
// search, so those cases are not here. Built as a cargo project whose
// main.rs is this file, with icu = "2" and --release.
use icu::collator::{options::CollatorOptions, Collator};
use icu::locale::locale;
use std::hint::black_box;
use std::time::Instant;

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
        best = best.min(ns);
    }
    println!("{name:<26} {best:>10.1} ns");
}

const DE: [&str; 8] = ["Äpfel", "apfel", "Zebra", "über", "Ufer", "Straße", "Strasse", "Öl"];
const FR: [&str; 8] = ["côte", "coté", "côté", "cote", "Cote", "élève", "élevé", "Élise"];
const ZH: [&str; 8] = ["北京", "上海", "广州", "深圳", "中国", "人民", "阿姨", "一个"];
const KO: [&str; 8] = ["가나", "한국", "서울", "부산", "김치", "사람", "학교", "가족"];
const PREFIX: [&str; 8] = [
    "Rechnungen/2026/Oktober/Kunde-A-Äpfel",
    "Rechnungen/2026/Oktober/Kunde-A-apfel",
    "Rechnungen/2026/Oktober/Kunde-A-Zebra",
    "Rechnungen/2026/Oktober/Kunde-A-über",
    "Rechnungen/2026/Oktober/Kunde-A-Ufer",
    "Rechnungen/2026/Oktober/Kunde-A-Straße",
    "Rechnungen/2026/Oktober/Kunde-A-Strasse",
    "Rechnungen/2026/Oktober/Kunde-A-Öl",
];

fn compare(name: &str, locale: icu::locale::Locale, words: &[&str; 8]) {
    let col = Collator::try_new(locale.into(), CollatorOptions::default()).unwrap();
    let mut i = 0usize;
    time(name, 5_000_000, || {
        i = (i + 3) % 64;
        black_box(col.compare(black_box(words[i / 8]), black_box(words[i % 8])));
    });
}

fn main() {
    time("collator construct de", 20_000, || {
        black_box(Collator::try_new(locale!("de").into(), CollatorOptions::default()).unwrap());
    });
    time("collator construct zh", 20_000, || {
        black_box(Collator::try_new(locale!("zh").into(), CollatorOptions::default()).unwrap());
    });
    compare("compare de", locale!("de"), &DE);
    compare("compare fr", locale!("fr"), &FR);
    compare("compare zh", locale!("zh"), &ZH);
    compare("compare ko", locale!("ko"), &KO);
    compare("compare long prefix de", locale!("de"), &PREFIX);
    let col = Collator::try_new(locale!("de").into(), CollatorOptions::default()).unwrap();
    let utf16: Vec<Vec<u16>> = DE.iter().map(|w| w.encode_utf16().collect()).collect();
    let mut i = 0usize;
    time("compare de UTF-16", 5_000_000, || {
        i = (i + 3) % 64;
        black_box(col.compare_utf16(black_box(&utf16[i / 8]), black_box(&utf16[i % 8])));
    });
    let mut key: Vec<u8> = Vec::with_capacity(256);
    time("sort key de", 5_000_000, || {
        i = (i + 1) % 8;
        key.clear();
        let _ = col.write_sort_key_to(black_box(DE[i]), &mut key);
        black_box(key.len());
    });
}
