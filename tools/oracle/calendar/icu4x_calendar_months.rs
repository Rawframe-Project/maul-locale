// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Prints every month start of the Chinese or Korean traditional calendar
// between two Gregorian years, as ICU4X 2.3 computes it.
use icu_calendar::cal::{ChineseTraditional, KoreanTraditional};
use icu_calendar::{Date, Iso};
fn main() {
    let a: Vec<String> = std::env::args().collect();
    let korean = a[1] == "dangi";
    let from: i32 = a[2].parse().unwrap();
    let to: i32 = a[3].parse().unwrap();
    let mut d = Date::try_new_iso(from, 1, 1).unwrap();
    let end = Date::try_new_iso(to + 1, 1, 1).unwrap();
    while d < end {
        let (code, day) = if korean {
            let c = d.to_calendar(KoreanTraditional::new());
            (c.month().standard_code.0.to_string(), c.day_of_month().0)
        } else {
            let c = d.to_calendar(ChineseTraditional::new());
            (c.month().standard_code.0.to_string(), c.day_of_month().0)
        };
        if day == 1 {
            println!("{:04}-{:02}-{:02} {}", d.extended_year(), d.month().ordinal, d.day_of_month().0, code);
        }
        d = Date::<Iso>::from_rata_die(d.to_rata_die() + 1, Iso);
    }
}
