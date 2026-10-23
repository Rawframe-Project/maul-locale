// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Prints, for every day in a range, a calendar's fields as ICU4X 2.3
// gives them: "day year era eraYear monthCode dayOfMonth inLeapYear
// daysInYear daysInMonth monthsInYear dayOfYear dayOfWeek", era "-" for
// none. maul-locale's
// test_calendar_oracle hashes these lines (FNV-1a, 64 bits) over
// 1600 to 2400 (days 584023 to 876582; Chinese and Dangi 693596 to
// 766644): icu4x_calendar_fields <calendar> <first> <last> [step].
// Cargo.toml: icu_calendar = "=2.3.0", calendrical_calculations =
// "=0.2.4".
use icu_calendar::cal::*;
use icu_calendar::{AsCalendar, Date, Iso};
use calendrical_calculations::rata_die::RataDie;

fn line<C: AsCalendar>(rd: i64, d: Date<C>) {
    let y = d.year();
    let (era, ey) = match y.era() { Some(e) => (e.era.to_string(), e.year), None => ("-".to_string(), y.extended_year()) };
    println!("{} {} {} {} {} {} {} {} {} {} {} {}", rd, y.extended_year(), era, ey, d.month().standard_code.0, d.day_of_month().0,
             d.is_in_leap_year() as u8, d.days_in_year(), d.days_in_month(), d.months_in_year(), d.day_of_year().0,
             d.day_of_week() as u8);
}
fn main() {
    let a: Vec<String> = std::env::args().collect();
    let cal = a[1].as_str();
    let from: i64 = a[2].parse().unwrap();
    let to: i64 = a[3].parse().unwrap();
    let step: i64 = a.get(4).map(|s| s.parse().unwrap()).unwrap_or(1);
    let mut rd = from;
    while rd <= to {
        let iso = Date::<Iso>::from_rata_die(RataDie::new(rd), Iso);
        match cal {
            "gregory" => line(rd, iso.to_calendar(Gregorian)),
            "iso8601" => line(rd, iso),
            "buddhist" => line(rd, iso.to_calendar(Buddhist)),
            "roc" => line(rd, iso.to_calendar(Roc)),
            "coptic" => line(rd, iso.to_calendar(Coptic)),
            "ethiopic" => line(rd, iso.to_calendar(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteMihret))),
            "ethioaa" => line(rd, iso.to_calendar(Ethiopian::new_with_era_style(EthiopianEraStyle::AmeteAlem))),
            "indian" => line(rd, iso.to_calendar(Indian)),
            "persian" => line(rd, iso.to_calendar(Persian)),
            "islamic-civil" => line(rd, iso.to_calendar(Hijri::new_tabular(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Friday))),
            "hebrew" => line(rd, iso.to_calendar(Hebrew)),
            "japanese" => line(rd, iso.to_calendar(Japanese::new())),
            "islamic-umalqura" => line(rd, iso.to_calendar(Hijri::new_umm_al_qura())),
            "chinese" => line(rd, iso.to_calendar(ChineseTraditional::new())),
            "dangi" => line(rd, iso.to_calendar(KoreanTraditional::new())),
            "islamic-tbla" => line(rd, iso.to_calendar(Hijri::new_tabular(HijriTabularLeapYears::TypeII, HijriTabularEpoch::Thursday))),
            _ => panic!("calendar"),
        }
        rd += step;
    }
}
