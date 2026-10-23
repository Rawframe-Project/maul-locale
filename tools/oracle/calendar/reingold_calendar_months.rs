// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Prints every month start of the Chinese or Dangi calendar between two
// Gregorian years by Reingold and Dershowitz's full astronomy
// (calendrical_calculations 0.2.4), with no precomputed table.
use calendrical_calculations::chinese_based::{chinese_based_date_from_fixed, Chinese, Dangi};
use calendrical_calculations::gregorian::{fixed_from_gregorian, gregorian_from_fixed};
fn main() {
    let a: Vec<String> = std::env::args().collect();
    let dangi = a[1] == "dangi";
    let from: i32 = a[2].parse().unwrap();
    let to: i32 = a[3].parse().unwrap();
    let mut d = fixed_from_gregorian(from, 1, 1);
    let end = fixed_from_gregorian(to + 1, 1, 1);
    let t = std::time::Instant::now();
    let mut n = 0u64;
    while d < end {
        let r = if dangi { chinese_based_date_from_fixed::<Dangi>(d) } else { chinese_based_date_from_fixed::<Chinese>(d) };
        n += 1;
        if r.day == 1 {
            let m = r.month;
            let code = match r.leap_month.map(|l| l.get()) {
                Some(l) if m == l => format!("M{:02}L", m - 1),
                Some(l) if m > l => format!("M{:02}", m - 1),
                _ => format!("M{:02}", m),
            };
            let (y, mo, da) = gregorian_from_fixed(d).unwrap();
            println!("{:04}-{:02}-{:02} {}", y, mo, da, code);
        }
        d = d + 1;
    }
    eprintln!("{} days, {:.0} ns a day", n, t.elapsed().as_nanos() as f64 / n as f64);
}
