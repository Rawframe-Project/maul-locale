// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) Ecma International; the test262 authors
//
// Generated from test262 at d53ebc8b1931d9f1541e43d59650776e4b766248 (github.com/tc39/test262); do not edit.
// The license is in LICENSES/BSD-3-Clause-test262.txt.
//
// Every string test262's Temporal tests expect a parser to refuse, one row
// each: the parser ('dt' with the goals as mlocTemporalFormat bits,
// 'offset', 'zone', 'calendar' or 'duration'), x and the string's bytes in
// hexadecimal, then '= refused'.
//
// 428 rows, 428 checks, from 324 files:
// test/built-ins/Temporal/Duration/compare/argument-duration-out-of-range.js
// test/built-ins/Temporal/Duration/compare/relativeto-argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/Duration/compare/relativeto-argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/Duration/compare/relativeto-no-fractional-minutes-hours.js
// test/built-ins/Temporal/Duration/compare/relativeto-propertybag-invalid-offset-string.js
// test/built-ins/Temporal/Duration/compare/relativeto-propertybag-timezone-string-datetime.js
// test/built-ins/Temporal/Duration/compare/relativeto-propertybag-timezone-string-leap-second.js
// test/built-ins/Temporal/Duration/compare/relativeto-propertybag-timezone-string-year-zero.js
// test/built-ins/Temporal/Duration/compare/relativeto-propertybag-timezone-wrong-type.js
// test/built-ins/Temporal/Duration/compare/relativeto-string-invalid.js
// test/built-ins/Temporal/Duration/compare/relativeto-sub-minute-offset.js
// test/built-ins/Temporal/Duration/compare/year-zero.js
// test/built-ins/Temporal/Duration/from/argument-duration-out-of-range.js
// test/built-ins/Temporal/Duration/from/argument-string-fractional-with-zero-subparts.js
// test/built-ins/Temporal/Duration/from/argument-string-invalid.js
// test/built-ins/Temporal/Duration/prototype/add/argument-duration-out-of-range.js
// test/built-ins/Temporal/Duration/prototype/add/argument-string.js
// test/built-ins/Temporal/Duration/prototype/round/relativeto-argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/Duration/prototype/round/relativeto-argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/Duration/prototype/round/relativeto-no-fractional-minutes-hours.js
// test/built-ins/Temporal/Duration/prototype/round/relativeto-propertybag-invalid-offset-string.js
// test/built-ins/Temporal/Duration/prototype/round/relativeto-propertybag-timezone-string-datetime.js
// test/built-ins/Temporal/Duration/prototype/round/relativeto-propertybag-timezone-string-leap-second.js
// test/built-ins/Temporal/Duration/prototype/round/relativeto-propertybag-timezone-string-year-zero.js
// test/built-ins/Temporal/Duration/prototype/round/relativeto-propertybag-timezone-wrong-type.js
// test/built-ins/Temporal/Duration/prototype/round/relativeto-string-invalid.js
// test/built-ins/Temporal/Duration/prototype/round/throws-on-wrong-offset-for-zoned-date-time-relative-to.js
// test/built-ins/Temporal/Duration/prototype/round/year-zero.js
// test/built-ins/Temporal/Duration/prototype/subtract/argument-duration-out-of-range.js
// test/built-ins/Temporal/Duration/prototype/subtract/argument-string.js
// test/built-ins/Temporal/Duration/prototype/total/relativeto-argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/Duration/prototype/total/relativeto-argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/Duration/prototype/total/relativeto-no-fractional-minutes-hours.js
// test/built-ins/Temporal/Duration/prototype/total/relativeto-propertybag-invalid-offset-string.js
// test/built-ins/Temporal/Duration/prototype/total/relativeto-propertybag-timezone-string-datetime.js
// test/built-ins/Temporal/Duration/prototype/total/relativeto-propertybag-timezone-string-leap-second.js
// test/built-ins/Temporal/Duration/prototype/total/relativeto-propertybag-timezone-string-year-zero.js
// test/built-ins/Temporal/Duration/prototype/total/relativeto-propertybag-timezone-wrong-type.js
// test/built-ins/Temporal/Duration/prototype/total/relativeto-string-invalid.js
// test/built-ins/Temporal/Duration/prototype/total/relativeto-sub-minute-offset.js
// test/built-ins/Temporal/Duration/prototype/total/throws-on-wrong-offset-for-zoneddatetime-relativeto.js
// test/built-ins/Temporal/Duration/prototype/total/year-zero.js
// test/built-ins/Temporal/Instant/compare/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/Instant/compare/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/Instant/compare/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/Instant/compare/argument-string-invalid.js
// test/built-ins/Temporal/Instant/compare/argument-string-minus-sign.js
// test/built-ins/Temporal/Instant/compare/argument-string-multiple-calendar.js
// test/built-ins/Temporal/Instant/compare/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/Instant/compare/argument-string-too-many-decimals.js
// test/built-ins/Temporal/Instant/compare/instant-string.js
// test/built-ins/Temporal/Instant/compare/no-fractional-minutes-hours.js
// test/built-ins/Temporal/Instant/compare/year-zero.js
// test/built-ins/Temporal/Instant/prototype/add/argument-duration-out-of-range.js
// test/built-ins/Temporal/Instant/prototype/equals/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/Instant/prototype/equals/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/Instant/prototype/equals/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/Instant/prototype/equals/argument-string-invalid.js
// test/built-ins/Temporal/Instant/prototype/equals/argument-string-minus-sign.js
// test/built-ins/Temporal/Instant/prototype/equals/argument-string-multiple-calendar.js
// test/built-ins/Temporal/Instant/prototype/equals/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/Instant/prototype/equals/argument-string-too-many-decimals.js
// test/built-ins/Temporal/Instant/prototype/equals/instant-string-sub-minute-offset.js
// test/built-ins/Temporal/Instant/prototype/equals/instant-string.js
// test/built-ins/Temporal/Instant/prototype/equals/no-fractional-minutes-hours.js
// test/built-ins/Temporal/Instant/prototype/equals/year-zero.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-invalid.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-minus-sign.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-multiple-calendar.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-too-many-decimals.js
// test/built-ins/Temporal/Instant/prototype/since/instant-string-sub-minute-offset.js
// test/built-ins/Temporal/Instant/prototype/since/instant-string.js
// test/built-ins/Temporal/Instant/prototype/since/no-fractional-minutes-hours.js
// test/built-ins/Temporal/Instant/prototype/since/year-zero.js
// test/built-ins/Temporal/Instant/prototype/subtract/argument-duration-out-of-range.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-invalid.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-minus-sign.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-multiple-calendar.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-too-many-decimals.js
// test/built-ins/Temporal/Instant/prototype/until/instant-string-sub-minute-offset.js
// test/built-ins/Temporal/Instant/prototype/until/instant-string.js
// test/built-ins/Temporal/Instant/prototype/until/no-fractional-minutes-hours.js
// test/built-ins/Temporal/Instant/prototype/until/year-zero.js
// test/built-ins/Temporal/PlainDate/prototype/since/argument-propertybag-calendar-case-insensitive.js
// test/built-ins/Temporal/PlainDate/prototype/until/argument-propertybag-calendar-case-insensitive.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-propertybag-calendar-case-insensitive.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-propertybag-calendar-year-zero.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-string-invalid.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-string-minus-sign.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-string-multiple-calendar.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-string-too-many-decimals.js
// test/built-ins/Temporal/PlainDateTime/compare/argument-string-with-utc-designator.js
// test/built-ins/Temporal/PlainDateTime/compare/no-fractional-minutes-hours.js
// test/built-ins/Temporal/PlainDateTime/compare/year-zero.js
// test/built-ins/Temporal/PlainDateTime/prototype/add/argument-duration-out-of-range.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-propertybag-calendar-case-insensitive.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-propertybag-calendar-year-zero.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-string-minus-sign.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-string-multiple-calendar.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-string-too-many-decimals.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/argument-string-with-utc-designator.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/no-fractional-minutes-hours.js
// test/built-ins/Temporal/PlainDateTime/prototype/equals/year-zero.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-propertybag-calendar-case-insensitive.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-propertybag-calendar-year-zero.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-string-minus-sign.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-string-multiple-calendar.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-string-too-many-decimals.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/argument-string-with-utc-designator.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/no-fractional-minutes-hours.js
// test/built-ins/Temporal/PlainDateTime/prototype/since/year-zero.js
// test/built-ins/Temporal/PlainDateTime/prototype/subtract/argument-duration-out-of-range.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-propertybag-calendar-case-insensitive.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-propertybag-calendar-year-zero.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-string-minus-sign.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-string-multiple-calendar.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-string-too-many-decimals.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/argument-string-with-utc-designator.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/no-fractional-minutes-hours.js
// test/built-ins/Temporal/PlainDateTime/prototype/until/year-zero.js
// test/built-ins/Temporal/PlainTime/compare/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/PlainTime/compare/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/PlainTime/compare/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/PlainTime/compare/argument-string-minus-sign.js
// test/built-ins/Temporal/PlainTime/compare/argument-string-multiple-calendar.js
// test/built-ins/Temporal/PlainTime/compare/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/PlainTime/compare/argument-string-no-implicit-midnight.js
// test/built-ins/Temporal/PlainTime/compare/argument-string-too-many-decimals.js
// test/built-ins/Temporal/PlainTime/compare/argument-string-with-utc-designator.js
// test/built-ins/Temporal/PlainTime/compare/no-fractional-minutes-hours.js
// test/built-ins/Temporal/PlainTime/compare/year-zero.js
// test/built-ins/Temporal/PlainTime/from/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/PlainTime/from/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/PlainTime/from/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/PlainTime/from/argument-string-invalid.js
// test/built-ins/Temporal/PlainTime/from/argument-string-minus-sign.js
// test/built-ins/Temporal/PlainTime/from/argument-string-multiple-calendar.js
// test/built-ins/Temporal/PlainTime/from/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/PlainTime/from/argument-string-no-implicit-midnight.js
// test/built-ins/Temporal/PlainTime/from/argument-string-too-many-decimals.js
// test/built-ins/Temporal/PlainTime/from/argument-string-trailing-junk.js
// test/built-ins/Temporal/PlainTime/from/argument-string-with-utc-designator.js
// test/built-ins/Temporal/PlainTime/from/no-fractional-minutes-hours.js
// test/built-ins/Temporal/PlainTime/from/year-zero.js
// test/built-ins/Temporal/PlainTime/prototype/add/argument-duration-out-of-range.js
// test/built-ins/Temporal/PlainTime/prototype/equals/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/PlainTime/prototype/equals/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/PlainTime/prototype/equals/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/PlainTime/prototype/equals/argument-string-minus-sign.js
// test/built-ins/Temporal/PlainTime/prototype/equals/argument-string-multiple-calendar.js
// test/built-ins/Temporal/PlainTime/prototype/equals/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/PlainTime/prototype/equals/argument-string-no-implicit-midnight.js
// test/built-ins/Temporal/PlainTime/prototype/equals/argument-string-too-many-decimals.js
// test/built-ins/Temporal/PlainTime/prototype/equals/argument-string-with-utc-designator.js
// test/built-ins/Temporal/PlainTime/prototype/equals/year-zero.js
// test/built-ins/Temporal/PlainTime/prototype/since/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/PlainTime/prototype/since/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/PlainTime/prototype/since/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/PlainTime/prototype/since/argument-string-minus-sign.js
// test/built-ins/Temporal/PlainTime/prototype/since/argument-string-multiple-calendar.js
// test/built-ins/Temporal/PlainTime/prototype/since/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/PlainTime/prototype/since/argument-string-no-implicit-midnight.js
// test/built-ins/Temporal/PlainTime/prototype/since/argument-string-too-many-decimals.js
// test/built-ins/Temporal/PlainTime/prototype/since/argument-string-with-utc-designator.js
// test/built-ins/Temporal/PlainTime/prototype/since/year-zero.js
// test/built-ins/Temporal/PlainTime/prototype/subtract/argument-duration-out-of-range.js
// test/built-ins/Temporal/PlainTime/prototype/until/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/PlainTime/prototype/until/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/PlainTime/prototype/until/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/PlainTime/prototype/until/argument-string-minus-sign.js
// test/built-ins/Temporal/PlainTime/prototype/until/argument-string-multiple-calendar.js
// test/built-ins/Temporal/PlainTime/prototype/until/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/PlainTime/prototype/until/argument-string-no-implicit-midnight.js
// test/built-ins/Temporal/PlainTime/prototype/until/argument-string-too-many-decimals.js
// test/built-ins/Temporal/PlainTime/prototype/until/argument-string-with-utc-designator.js
// test/built-ins/Temporal/PlainTime/prototype/until/year-zero.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-propertybag-calendar-case-insensitive.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-propertybag-calendar-year-zero.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-propertybag-invalid-offset-string.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-propertybag-timezone-string-datetime.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-propertybag-timezone-string-leap-second.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-propertybag-timezone-string-sub-minute-offset.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-propertybag-timezone-string-year-zero.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-propertybag-timezone-wrong-type.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-string-minus-sign.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-string-multiple-calendar.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-string-sub-minute-offset.js
// test/built-ins/Temporal/ZonedDateTime/compare/argument-string-too-many-decimals.js
// test/built-ins/Temporal/ZonedDateTime/compare/leap-second.js
// test/built-ins/Temporal/ZonedDateTime/compare/no-fractional-minutes-hours.js
// test/built-ins/Temporal/ZonedDateTime/compare/year-zero.js
// test/built-ins/Temporal/ZonedDateTime/compare/zoneddatetime-string-multiple-offsets.js
// test/built-ins/Temporal/ZonedDateTime/compare/zoneddatetime-string.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-propertybag-calendar-case-insensitive.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-propertybag-calendar-year-zero.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-propertybag-timezone-string-datetime.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-propertybag-timezone-string-leap-second.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-propertybag-timezone-string-sub-minute-offset.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-propertybag-timezone-string-year-zero.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-propertybag-timezone-wrong-type.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-invalid.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-minus-sign.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-multiple-calendar.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-no-junk-at-end.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-sub-minute-offset.js
// test/built-ins/Temporal/ZonedDateTime/from/argument-string-too-many-decimals.js
// test/built-ins/Temporal/ZonedDateTime/from/constrain-has-no-effect-on-invalid-iso-string.js
// test/built-ins/Temporal/ZonedDateTime/from/leap-second.js
// test/built-ins/Temporal/ZonedDateTime/from/no-fractional-minutes-hours.js
// test/built-ins/Temporal/ZonedDateTime/from/offset-string-invalid.js
// test/built-ins/Temporal/ZonedDateTime/from/year-zero.js
// test/built-ins/Temporal/ZonedDateTime/from/zoneddatetime-string-multiple-offsets.js
// test/built-ins/Temporal/ZonedDateTime/from/zoneddatetime-string.js
// test/built-ins/Temporal/ZonedDateTime/prototype/add/argument-duration-out-of-range.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-propertybag-calendar-case-insensitive.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-propertybag-calendar-year-zero.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-propertybag-invalid-offset-string.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-propertybag-timezone-string-datetime.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-propertybag-timezone-string-leap-second.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-propertybag-timezone-string-sub-minute-offset.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-propertybag-timezone-string-year-zero.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-propertybag-timezone-wrong-type.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-string-minus-sign.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-string-multiple-calendar.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-string-sub-minute-offset.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/argument-string-too-many-decimals.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/leap-second.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/no-fractional-minutes-hours.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/year-zero.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/zoneddatetime-string-multiple-offsets.js
// test/built-ins/Temporal/ZonedDateTime/prototype/since/zoneddatetime-string.js
// test/built-ins/Temporal/ZonedDateTime/prototype/subtract/argument-duration-out-of-range.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-propertybag-calendar-case-insensitive.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-propertybag-calendar-invalid-iso-string.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-propertybag-calendar-year-zero.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-propertybag-invalid-offset-string.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-propertybag-timezone-string-datetime.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-propertybag-timezone-string-leap-second.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-propertybag-timezone-string-sub-minute-offset.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-propertybag-timezone-string-year-zero.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-propertybag-timezone-wrong-type.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-string-calendar-invalid-iso-string.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-string-minus-sign.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-string-multiple-calendar.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-string-sub-minute-offset.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/argument-string-too-many-decimals.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/leap-second.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/no-fractional-minutes-hours.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/year-zero.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/zoneddatetime-string-multiple-offsets.js
// test/built-ins/Temporal/ZonedDateTime/prototype/until/zoneddatetime-string.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withCalendar/calendar-case-insensitive.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withCalendar/calendar-invalid-iso-string.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withPlainTime/argument-string-calendar-annotation-invalid-key.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withPlainTime/argument-string-critical-unknown-annotation.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withPlainTime/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withPlainTime/argument-string-minus-sign.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withPlainTime/argument-string-multiple-calendar.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withPlainTime/argument-string-multiple-time-zone.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withPlainTime/argument-string-no-implicit-midnight.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withPlainTime/argument-string-too-many-decimals.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withPlainTime/argument-string-with-utc-designator.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withPlainTime/year-zero.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withTimeZone/timezone-string-datetime.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withTimeZone/timezone-string-leap-second.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withTimeZone/timezone-string-sub-minute-offset.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withTimeZone/timezone-string-year-zero.js
// test/built-ins/Temporal/ZonedDateTime/prototype/withTimeZone/timezone-wrong-type.js
// test/built-ins/Temporal/ZonedDateTime/timezone-wrong-type.js
// test/intl402/Temporal/ZonedDateTime/timezone-ids-basic.js

"dt 1 x = refused",
"dt 1 x2d3030303030302d31302d3331 = refused",
"dt 1 x2d3030303030302d31302d33315431373a3435 = refused",
"dt 1 x2d3030303030302d31302d33315431373a34352b30303a30305b5554435d = refused",
"dt 1 x2d3030303030302d31302d33315431373a34352b30313a3030 = refused",
"dt 1 x2d3030303030302d31302d33315431373a34355a = refused",
"dt 1 x2d303030303030302d30312d30315430303a30322b30303a30305b5554435d = refused",
"dt 1 x2d303030303030302d30312d30315430303a30323a30302e3030303030303030302b30303a30305b5554435d = refused",
"dt 1 x2d303030303030302d30312d30315430303a30325a5b5554435d = refused",
"dt 1 x313937302d30312d30315430302b30303a30303a30302e303030303030303030305a5b5554435d = refused",
"dt 1 x313937302d30312d30315430303a3030 = refused",
"dt 1 x313937302d30312d30315430303a30302b30313a3030 = refused",
"dt 1 x313937302d30312d30315430303a30303a30302e313233343536373839305a5b5554435d = refused",
"dt 1 x313937302d30312d30315430303a30303a30302e313233343536373839315a5b5554435d = refused",
"dt 1 x313937302d30312d30315430303a30305a = refused",
"dt 1 x313937302d30312d30315430303a30305b215554435d5b5554435d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b215554435d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b21666f6f3d6261725d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b21666f6f3d6261725d5b752d63613d69736f383630315d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b21752d63613d69736f383630315d5b752d63613d69736f383630315d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b464f4f3d6261725d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b552d43413d69736f383630315d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b5554435d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b666f6f3d6261725d5b5554435d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b752d43413d69736f383630315d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b752d63613d69736f383630315d5b21666f6f3d6261725d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b752d63613d69736f383630315d5b5554435d = refused",
"dt 1 x313937302d30312d30315430303a30305b5554435d5b752d63613d69736f383630315d5b666f6f3d6261725d5b21752d63613d69736f383630315d = refused",
"dt 1 x313937302d30312d30315430303a30305b666f6f3d6261725d5b215f666f6f2d626172303d446f6e742d49676e6f72652d546869732d39393939393939393939395d = refused",
"dt 1 x313937302d30312d30315430313a33353a33302b30313a33353a30302e3030303030303030305b2b30313a33353a30302e3030303030303030305d = refused",
"dt 1 x313937362d31312d31385431353a32333a33302e31322d30323a30305be2889230323a30305d = refused",
"dt 1 x313937362d31312d31385431353a32333a33302e3132333435363738392d30383a30305b2d30383a30305d6a756e6b = refused",
"dt 1 x313937362d31312d31385431353a32333a33302e3132e2889230323a3030 = refused",
"dt 1 x313937362d31312d31385431353a32333a33302e3132e2889230323a30305be2889230323a30305d = refused",
"dt 1 x313939372d31322d30345b752d63613d313131312d31312d31315d = refused",
"dt 1 x313939372d31322d30345b752d63613d31313131313131315d = refused",
"dt 1 x313939372d31322d30345b752d63613d6e6f746163616c5d = refused",
"dt 1 x323030302d30352d30325431323a33343a35362b32333a35395b2b32333a35393a36305d = refused",
"dt 1 x323032302d31332d33345432343a36305b2d30383a30305d = refused",
"dt 1 x323032312d30382d31395431373a33303a34352e3132333435363738392d31323a31323a35392e395b2d31323a31323a35392e395d = refused",
"dt 1 x323032322d30392d31352b30303a30305b5554435d = refused",
"dt 1 x323032322d30392d31352d30323a33305b416d65726963612f53745f4a6f686e735d = refused",
"dt 1 x323032322d30392d31355a5b4575726f70652f5669656e6e615d = refused",
"dt 1 x323032322d30392d31355a5b5554435d = refused",
"dt 1 x323032352d30312d303154303030303a30302b30303a30305b5554435d = refused",
"dt 1 x323032352d30312d30315430303a303030302b30303a30305b5554435d = refused",
"dt 1 x323032352d30312d30315430303a30303a30302b303030303a30305b5554435d = refused",
"dt 1 x323032352d30312d30315430303a30303a30302b30303a303030305b5554435d = refused",
"dt 1 x323032352d303130315430303a30303a30302b30303a30305b5554435d = refused",
"dt 1 x323032352d30342d30335430353a30372e313233 = refused",
"dt 1 x323032352d30342d30335431322e35 = refused",
"dt 1 x3230323530312d30315430303a30303a30302b30303a30305b5554435d = refused",
"dt 1 xe288923030393939392d31312d31385431353a32333a33302e31325b5554435d = refused",
"dt 2 x = refused",
"dt 2 x2d3030303030302d31302d3331 = refused",
"dt 2 x2d3030303030302d31302d33315431373a3435 = refused",
"dt 2 x2d3030303030302d31302d33315431373a34352b30303a30305b5554435d = refused",
"dt 2 x2d3030303030302d31302d33315431373a34352b30313a3030 = refused",
"dt 2 x2d3030303030302d31302d33315431373a34355a = refused",
"dt 2 x2d3030303030302d31322d3037 = refused",
"dt 2 x2d3030303030302d31322d30375430333a32343a3330 = refused",
"dt 2 x2d3030303030302d31322d30375430333a32343a33302b30303a30305b5554435d = refused",
"dt 2 x2d3030303030302d31322d30375430333a32343a33302b30313a3030 = refused",
"dt 2 x313937302d30312d30315430302b30303a30303a30302e31323334353637383930 = refused",
"dt 2 x313937302d30312d30315430302b30303a30303a30302e31323334353637383931 = refused",
"dt 2 x313937302d30312d30315430303a30303a30302e31323334353637383930 = refused",
"dt 2 x313937302d30312d30315430303a30303a30302e31323334353637383931 = refused",
"dt 2 x313937302d30312d30315430303a30305b215554435d5b5554435d = refused",
"dt 2 x313937302d30312d30315430303a30305b21666f6f3d6261725d = refused",
"dt 2 x313937302d30312d30315430303a30305b21752d63613d69736f383630315d5b752d63613d69736f383630315d = refused",
"dt 2 x313937302d30312d30315430303a30305b464f4f3d6261725d = refused",
"dt 2 x313937302d30312d30315430303a30305b552d43413d69736f383630315d = refused",
"dt 2 x313937302d30312d30315430303a30305b5554435d5b215554435d = refused",
"dt 2 x313937302d30312d30315430303a30305b5554435d5b21666f6f3d6261725d = refused",
"dt 2 x313937302d30312d30315430303a30305b5554435d5b21666f6f3d6261725d5b752d63613d69736f383630315d = refused",
"dt 2 x313937302d30312d30315430303a30305b5554435d5b5554435d = refused",
"dt 2 x313937302d30312d30315430303a30305b5554435d5b666f6f3d6261725d5b5554435d = refused",
"dt 2 x313937302d30312d30315430303a30305b5554435d5b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 2 x313937302d30312d30315430303a30305b5554435d5b752d63613d69736f383630315d5b5554435d = refused",
"dt 2 x313937302d30312d30315430303a30305b666f6f3d6261725d5b215f666f6f2d626172303d446f6e742d49676e6f72652d546869732d39393939393939393939395d = refused",
"dt 2 x313937302d30312d30315430303a30305b752d43413d69736f383630315d = refused",
"dt 2 x313937302d30312d30315430303a30305b752d63613d69736f383630315d5b21666f6f3d6261725d = refused",
"dt 2 x313937302d30312d30315430303a30305b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 2 x313937302d30312d30315430303a30305b752d63613d69736f383630315d5b666f6f3d6261725d5b21752d63613d69736f383630315d = refused",
"dt 2 x313937302d30312d30315b21752d63613d69736f383630315d5b752d63613d69736f383630315d = refused",
"dt 2 x313937302d30312d30315b5554435d5b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 2 x313937302d30312d30315b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 2 x313937302d30312d30315b752d63613d69736f383630315d5b666f6f3d6261725d5b21752d63613d69736f383630315d = refused",
"dt 2 x313937362d31312d31385431353a32333a33302e3132e2889230323a3030 = refused",
"dt 2 x323031392d31302d30315430393a30303a30305a = refused",
"dt 2 x323031392d31302d30315430393a30303a30305a5b5554435d = refused",
"dt 2 x323032322d30392d31352b30303a3030 = refused",
"dt 2 x323032322d30392d31352b30303a30305b5554435d = refused",
"dt 2 x323032322d30392d31352d30323a3330 = refused",
"dt 2 x323032322d30392d31352d30323a33305b416d65726963612f53745f4a6f686e735d = refused",
"dt 2 x323032322d30392d31355a = refused",
"dt 2 x323032322d30392d31355a5b4575726f70652f5669656e6e615d = refused",
"dt 2 x323032322d30392d31355a5b5554435d = refused",
"dt 2 x323032352d30342d30335430353a30372e313233 = refused",
"dt 2 x323032352d30342d30335431322e35 = refused",
"dt 2 x696e76616c69642069736f3836303120737472696e67 = refused",
"dt 2 xe288923030393939392d31312d31385431353a32333a33302e3132 = refused",
"dt 3 x = refused",
"dt 3 x2d3030303030302d31312d3031 = refused",
"dt 3 x2d3030303030302d31312d30345430303a3030 = refused",
"dt 3 x31353a33303a34352e313233343536 = refused",
"dt 3 x313937302d30312d30315430303a30302d30303a34343a35395b2d30303a34343a35395d = refused",
"dt 3 x313937312d30312d30315430303a30302b30323a30305b2d30303a34343a33305d = refused",
"dt 3 x323031392d31312d30315430303a30305a = refused",
"dt 3 x323032352d30312d30315430303a30303a30302b30303a30303030 = refused",
"dt 3 x323032352d30342d30335430353a30372e313233 = refused",
"dt 3 x323032352d30342d30335431322e35 = refused",
"dt 3 x503159543148 = refused",
"dt 3 x555443 = refused",
"dt 3 x62616420737472696e67 = refused",
"dt 3 x69736f38363031 = refused",
"dt 4 x = refused",
"dt 4 x2b303030323032302d30312d30315430303a30305a = refused",
"dt 4 x2b3030323032302d3031 = refused",
"dt 4 x2d3030303030302d30332d33305430303a34355a = refused",
"dt 4 x2d3030303030302d30332d33305430313a34352b30313a3030 = refused",
"dt 4 x2d3030303030302d30332d33305430313a34353a30302b30303a30305b5554435d = refused",
"dt 4 x2d50313259 = refused",
"dt 4 x30312d3031 = refused",
"dt 4 x30323032302d30312d30315430303a30305a = refused",
"dt 4 x313937302d30312d30315430302b30303a30303a30302e31323334353637383930 = refused",
"dt 4 x313937302d30312d30315430302b30303a30303a30302e31323334353637383931 = refused",
"dt 4 x313937302d30312d30315430303a3030 = refused",
"dt 4 x313937302d30312d30315430303a30303a30302e313233343536373839305a = refused",
"dt 4 x313937302d30312d30315430303a30303a30302e313233343536373839315a = refused",
"dt 4 x313937302d30312d30315430303a30305a5b215554435d5b5554435d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b21666f6f3d6261725d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b21752d63613d69736f383630315d5b752d63613d69736f383630315d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b464f4f3d6261725d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b552d43413d69736f383630315d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b5554435d5b215554435d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b5554435d5b21666f6f3d6261725d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b5554435d5b21666f6f3d6261725d5b752d63613d69736f383630315d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b5554435d5b5554435d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b5554435d5b666f6f3d6261725d5b5554435d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b5554435d5b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b5554435d5b752d63613d69736f383630315d5b5554435d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b666f6f3d6261725d5b215f666f6f2d626172303d446f6e742d49676e6f72652d546869732d39393939393939393939395d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b752d43413d69736f383630315d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b752d63613d69736f383630315d5b21666f6f3d6261725d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 4 x313937302d30312d30315430303a30305a5b752d63613d69736f383630315d5b666f6f3d6261725d5b21752d63613d69736f383630315d = refused",
"dt 4 x313937302d30312d30315430303a30305b5554435d = refused",
"dt 4 x313937362d31312d31385431353a32333a33302e3132e2889230323a3030 = refused",
"dt 4 x323032302d30302d30315430303a30305a = refused",
"dt 4 x323032302d3030312d30315430303a30305a = refused",
"dt 4 x323032302d3030315430303a30305a = refused",
"dt 4 x323032302d3031 = refused",
"dt 4 x323032302d30312d3030315430303a30305a = refused",
"dt 4 x323032302d30312d30305430303a30305a = refused",
"dt 4 x323032302d30312d3031 = refused",
"dt 4 x323032302d30312d3031543030 = refused",
"dt 4 x323032302d30312d3031543030315a = refused",
"dt 4 x323032302d30312d30315430303a3030 = refused",
"dt 4 x323032302d30312d30315430303a30302b32343a3030 = refused",
"dt 4 x323032302d30312d30315430303a30302d32343a3030 = refused",
"dt 4 x323032302d30312d30315430303a30303a3030 = refused",
"dt 4 x323032302d30312d30315430303a30303a30302b30303a30305b5554435d5b752d63613d69736f383630315d6a756e6b = refused",
"dt 4 x323032302d30312d30315430303a30303a30302b30303a30305b5554435d6a756e6b = refused",
"dt 4 x323032302d30312d30315430303a30303a30302b30303a30306a756e6b = refused",
"dt 4 x323032302d30312d30315430303a30303a30302e303030303030303030 = refused",
"dt 4 x323032302d30312d30315430303a30303a30302e3030303030303030305a6a756e6b = refused",
"dt 4 x323032302d30312d30315430303a30303a30305a6a756e6b = refused",
"dt 4 x323032302d30312d30315430303a30305a6a756e6b = refused",
"dt 4 x323032302d30312d30315430313a3030315a = refused",
"dt 4 x323032302d30312d30315430313a30313a3030315a = refused",
"dt 4 x323032302d30312d30315430313a36303a30305a = refused",
"dt 4 x323032302d30312d30315430313a36303a36315a = refused",
"dt 4 x323032302d30312d30315432353a30303a30305a = refused",
"dt 4 x323032302d30312d3031545a = refused",
"dt 4 x323032302d30312d33325430303a30305a = refused",
"dt 4 x323032302d30322d33305430303a30305a = refused",
"dt 4 x323032302d31332d30315430303a30305a = refused",
"dt 4 x323032302d573031 = refused",
"dt 4 x323032302d5730312d315430303a30305a = refused",
"dt 4 x323032312d30322d32395430303a30305a = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030303030303030305b2d3037303030302e3030303030303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030303030303030305b2d30373a30303a30302e3030303030303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030303030303030315b2d3037303030302e3030303030303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030303030303030315b2d30373a30303a30302e3030303030303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30303030303030305b2d3037303030302e30303030303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30303030303030305b2d30373a30303a30302e30303030303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30303030303030315b2d3037303030302e30303030303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30303030303030315b2d30373a30303a30302e30303030303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e303030303030305b2d3037303030302e303030303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e303030303030305b2d30373a30303a30302e303030303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e303030303030315b2d3037303030302e303030303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e303030303030315b2d30373a30303a30302e303030303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030303030305b2d3037303030302e3030303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030303030305b2d30373a30303a30302e3030303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030303030315b2d3037303030302e3030303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030303030315b2d30373a30303a30302e3030303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30303030305b2d3037303030302e30303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30303030305b2d30373a30303a30302e30303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30303030315b2d3037303030302e30303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30303030315b2d30373a30303a30302e30303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e303030305b2d3037303030302e303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e303030305b2d30373a30303a30302e303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e303030315b2d3037303030302e303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e303030315b2d30373a30303a30302e303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030305b2d3037303030302e3030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030305b2d30373a30303a30302e3030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030315b2d3037303030302e3030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e3030315b2d30373a30303a30302e3030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30305b2d3037303030302e30305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30305b2d30373a30303a30302e30305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30315b2d3037303030302e30315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e30315b2d30373a30303a30302e30315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e305b2d3037303030302e305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e305b2d30373a30303a30302e305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e315b2d3037303030302e315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30302e315b2d30373a30303a30302e315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30305b2d3037303030305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30305b2d30373a30303a30305d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30315b2d3037303030315d = refused",
"dt 4 x323032312d30382d31395431373a33302d30373a30303a30315b2d30373a30303a30315d = refused",
"dt 4 x323032322d30392d31352b30303a3030 = refused",
"dt 4 x323032322d30392d31352b30303a30305b5554435d = refused",
"dt 4 x323032322d30392d31352d30323a3330 = refused",
"dt 4 x323032322d30392d31352d30323a33305b416d65726963612f53745f4a6f686e735d = refused",
"dt 4 x323032322d30392d31355a = refused",
"dt 4 x323032322d30392d31355a5b4575726f70652f5669656e6e615d = refused",
"dt 4 x323032322d30392d31355a5b5554435d = refused",
"dt 4 x323032352d30342d30335430353a30372e3132335b5554435d = refused",
"dt 4 x323032352d30342d30335431322e355b5554435d = refused",
"dt 4 x503159 = refused",
"dt 4 x696e76616c69642069736f38363031 = refused",
"dt 4 xe288923030393939392d31312d31385431353a32333a33302e3132 = refused",
"dt 8 x = refused",
"dt 8 x2d3030303030302d31322d30375430333a32343a3330 = refused",
"dt 8 x2d3030303030302d31322d30375430333a32343a33302b30303a30305b5554435d = refused",
"dt 8 x2d3030303030302d31322d30375430333a32343a33302b30313a3030 = refused",
"dt 8 x30302b30303a30303a30302e31323334353637383930 = refused",
"dt 8 x30302b30303a30303a30302e31323334353637383931 = refused",
"dt 8 x303030303a3030 = refused",
"dt 8 x3030315a = refused",
"dt 8 x30303a30302b32343a3030 = refused",
"dt 8 x30303a30302d32343a3030 = refused",
"dt 8 x30303a30303030 = refused",
"dt 8 x30303a30303a30302b303030303a3030 = refused",
"dt 8 x30303a30303a30302b30303a30303030 = refused",
"dt 8 x30303a30303a30302b30303a30305b5554435d5b752d63613d69736f383630315d6a756e6b = refused",
"dt 8 x30303a30303a30302b30303a30305b5554435d6a756e6b = refused",
"dt 8 x30303a30303a30302b30303a30306a756e6b = refused",
"dt 8 x30303a30303a30302e3030303030303030305a6a756e6b = refused",
"dt 8 x30303a30303a30302e31323334353637383930 = refused",
"dt 8 x30303a30303a30302e31323334353637383931 = refused",
"dt 8 x30303a30303a30305a6a756e6b = refused",
"dt 8 x30303a30305a = refused",
"dt 8 x30303a30305a6a756e6b = refused",
"dt 8 x30303a30305b21666f6f3d6261725d = refused",
"dt 8 x30303a30305b21752d63613d69736f383630315d5b752d63613d69736f383630315d = refused",
"dt 8 x30303a30305b464f4f3d6261725d = refused",
"dt 8 x30303a30305b552d43413d69736f383630315d = refused",
"dt 8 x30303a30305b5554435d5b5554435d = refused",
"dt 8 x30303a30305b5554435d5b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 8 x30303a30305b752d43413d69736f383630315d = refused",
"dt 8 x30303a30305b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 8 x30303a30305b752d63613d69736f383630315d5b666f6f3d6261725d5b21752d63613d69736f383630315d = refused",
"dt 8 x30313a3030315a = refused",
"dt 8 x30313a30313a3030315a = refused",
"dt 8 x30313a36303a30305a = refused",
"dt 8 x30313a36303a36315a = refused",
"dt 8 x30353a30372e313233 = refused",
"dt 8 x30393a30303a30305a = refused",
"dt 8 x30393a30303a30305a5b5554435d = refused",
"dt 8 x31322e35 = refused",
"dt 8 x31353a32333a33302e3130306a756e6b = refused",
"dt 8 x313937302d30312d30315430302b30303a30303a30302e31323334353637383930 = refused",
"dt 8 x313937302d30312d30315430302b30303a30303a30302e31323334353637383931 = refused",
"dt 8 x313937302d30312d30315430303a30303a30302e31323334353637383930 = refused",
"dt 8 x313937302d30312d30315430303a30303a30302e31323334353637383931 = refused",
"dt 8 x313937302d30312d30315430303a30305b215554435d5b5554435d = refused",
"dt 8 x313937302d30312d30315430303a30305b21666f6f3d6261725d = refused",
"dt 8 x313937302d30312d30315430303a30305b21752d63613d69736f383630315d5b752d63613d69736f383630315d = refused",
"dt 8 x313937302d30312d30315430303a30305b464f4f3d6261725d = refused",
"dt 8 x313937302d30312d30315430303a30305b552d43413d69736f383630315d = refused",
"dt 8 x313937302d30312d30315430303a30305b5554435d5b215554435d = refused",
"dt 8 x313937302d30312d30315430303a30305b5554435d5b21666f6f3d6261725d = refused",
"dt 8 x313937302d30312d30315430303a30305b5554435d5b21666f6f3d6261725d5b752d63613d69736f383630315d = refused",
"dt 8 x313937302d30312d30315430303a30305b5554435d5b5554435d = refused",
"dt 8 x313937302d30312d30315430303a30305b5554435d5b666f6f3d6261725d5b5554435d = refused",
"dt 8 x313937302d30312d30315430303a30305b5554435d5b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 8 x313937302d30312d30315430303a30305b5554435d5b752d63613d69736f383630315d5b5554435d = refused",
"dt 8 x313937302d30312d30315430303a30305b666f6f3d6261725d5b215f666f6f2d626172303d446f6e742d49676e6f72652d546869732d39393939393939393939395d = refused",
"dt 8 x313937302d30312d30315430303a30305b752d43413d69736f383630315d = refused",
"dt 8 x313937302d30312d30315430303a30305b752d63613d69736f383630315d5b21666f6f3d6261725d = refused",
"dt 8 x313937302d30312d30315430303a30305b752d63613d69736f383630315d5b21752d63613d69736f383630315d = refused",
"dt 8 x313937302d30312d30315430303a30305b752d63613d69736f383630315d5b666f6f3d6261725d5b21752d63613d69736f383630315d = refused",
"dt 8 x313937362d31312d31385431353a32333a33302e3132e2889230323a3030 = refused",
"dt 8 x323031392d31302d3031 = refused",
"dt 8 x323031392d31302d30315430393a30303a30305a = refused",
"dt 8 x323031392d31302d30315430393a30303a30305a5b5554435d = refused",
"dt 8 x323032322d30392d31352b30303a3030 = refused",
"dt 8 x323032322d30392d31352b30303a30305b5554435d = refused",
"dt 8 x323032322d30392d31352d30323a3330 = refused",
"dt 8 x323032322d30392d31352d30323a33305b416d65726963612f53745f4a6f686e735d = refused",
"dt 8 x323032322d30392d31355a = refused",
"dt 8 x323032322d30392d31355a5b4575726f70652f5669656e6e615d = refused",
"dt 8 x323032322d30392d31355a5b5554435d = refused",
"dt 8 x32353a30303a30305a = refused",
"dt 8 x5430303a30305b21666f6f3d6261725d = refused",
"dt 8 x5430303a30305b464f4f3d6261725d = refused",
"dt 8 x5430303a30305b552d43413d69736f383630315d = refused",
"dt 8 x5430303a30305b5554435d5b5554435d = refused",
"dt 8 x5430303a30305b752d43413d69736f383630315d = refused",
"dt 8 x5a = refused",
"dt 8 x696e76616c69642069736f38363031 = refused",
"dt 8 xe288923030393939392d31312d31385431353a32333a33302e3132 = refused",
"offset x2b30 = refused",
"offset x2b3030303a3030 = refused",
"offset x2b30303a30302e30 = refused",
"offset x2b30303a30303030 = refused",
"offset x2b30303a30303a30302e30303030303030303030 = refused",
"offset x2d3030303a3030 = refused",
"offset x2d30303a303030 = refused",
"offset x2d30303a30303a303030 = refused",
"offset x30303a3030 = refused",
"offset x67617262616765 = refused",
"zone x = refused",
"zone x2b30303a30312e31 = refused",
"zone x2d3030303030302d31302d33315431373a34352b30303a30305b5554435d = refused",
"zone x2d3030303030302d31302d33315431373a34355a = refused",
"zone x2d30312e31 = refused",
"zone x2d31323a31323a35392e39 = refused",
"zone x323032312d30382d31395431373a3330 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a3030 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e30 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e3030 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e303030 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e30303030 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e3030303030 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e303030303030 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e30303030303030 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e3030303030303030 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e303030303030303030 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e303030303030303031 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e3030303030303031 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e30303030303031 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e303030303031 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e3030303031 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e30303031 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e303031 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e3031 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a30302e31 = refused",
"zone x323032312d30382d31395431373a33302d30373a30303a3031 = refused",
"zone x323032312d30382d31395431373a33303a34352e3132333435363738392b32333a35395b2b32333a35393a36305d = refused",
"zone x323032312d30382d31395431373a33303a34352e3132333435363738392d31323a31323a35392e395b2d31323a31323a35392e395d = refused",
"calendar x = refused",
"calendar x2d3030303030302d31302d3331 = refused",
"calendar x2d3030303030302d31302d33315431373a3435 = refused",
"calendar x2d3030303030302d31302d33315431373a34352b30303a30305b5554435d = refused",
"calendar x2d3030303030302d31302d33315431373a34352b30313a3030 = refused",
"calendar x2d3030303030302d31302d33315431373a34355a = refused",
"calendar xc4b0534f38363031 = refused",
"duration x = refused",
"duration x2b50 = refused",
"duration x2b5054 = refused",
"duration x2d50 = refused",
"duration x2d503130343234393939313337344454323448 = refused",
"duration x2d5031303432343939393133373544 = refused",
"duration x2d50343239343936373239364d = refused",
"duration x2d503432393439363732393657 = refused",
"duration x2d503432393439363732393659 = refused",
"duration x2d5054 = refused",
"duration x2d50543135303131393938373537393031364d363053 = refused",
"duration x2d50543135303131393938373537393031374d = refused",
"duration x2d5054323530313939393739323938334836304d = refused",
"duration x2d50543235303139393937393239383448 = refused",
"duration x2d50543930303731393932353437343039393253 = refused",
"duration x324454354d = refused",
"duration x50 = refused",
"duration x502d3159314d = refused",
"duration x50302e3559 = refused",
"duration x503130343234393939313337344454323448 = refused",
"duration x5031303432343939393133373544 = refused",
"duration x5031592d314d = refused",
"duration x503159302c354d = refused",
"duration x503159314d302e3557 = refused",
"duration x503159314d3157302c3544 = refused",
"duration x503159314d3157314454302e35483553 = refused",
"duration x503159314d3157314454312e3548302c354d = refused",
"duration x503159314d31573144543148302e354d302e3553 = refused",
"duration x503159314d31573144543148314d312e3031536a756e6b = refused",
"duration x503159314d31573144543148314d312e31323334353637383931323353 = refused",
"duration x50322c354d = refused",
"duration x50322e354d = refused",
"duration x503248 = refused",
"duration x503253 = refused",
"duration x50343239343936373239364d = refused",
"duration x503432393439363732393657 = refused",
"duration x503432393439363732393659 = refused",
"duration x5054 = refused",
"duration x50542c3148 = refused",
"duration x50542c314d = refused",
"duration x50542c3153 = refused",
"duration x50542e3148 = refused",
"duration x50542e314d = refused",
"duration x50542e3153 = refused",
"duration x5054302e3148302e304d = refused",
"duration x5054302e3148302e3053 = refused",
"duration x5054302e3148304d = refused",
"duration x5054302e31483053 = refused",
"duration x5054302e314d302e3053 = refused",
"duration x5054302e314d3053 = refused",
"duration x50543135303131393938373537393031364d363053 = refused",
"duration x50543135303131393938373537393031374d = refused",
"duration x5054322c48302c354d = refused",
"duration x5054322c48302c3553 = refused",
"duration x5054322c48334d = refused",
"duration x5054322c483353 = refused",
"duration x5054322e48302e354d = refused",
"duration x5054322e48302e3553 = refused",
"duration x5054322e48334d = refused",
"duration x5054322e483353 = refused",
"duration x5054323530313939393739323938334836304d = refused",
"duration x50543235303139393937393239383448 = refused",
"duration x50543248332c324d302c3353 = refused",
"duration x50543248332c324d3353 = refused",
"duration x50543248332e324d302e3353 = refused",
"duration x50543248332e324d3353 = refused",
"duration x50543930303731393932353437343039393253 = refused",
