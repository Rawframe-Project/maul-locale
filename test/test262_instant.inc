// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) Ecma International; the test262 authors
//
// Generated from test262 at d53ebc8b1931d9f1541e43d59650776e4b766248 (github.com/tc39/test262); do not edit.
// The license is in LICENSES/BSD-3-Clause-test262.txt.
//
// Every assertion test262's Temporal.Instant tests make about an instant or a
// difference of two, one row each; an instant is its seconds since 1970 and
// the nanoseconds past them (floored): 'add', an instant and a time duration
// (seconds, nanoseconds); 'round', an instant, the unit (1 hour to 6
// nanosecond), the increment and the mode; 'until' or 'since', two instants,
// the largest and smallest units, the increment and the mode as test262
// gives it (since negates it). Then '=' and 'refused', or what is checked:
// ns, the result's nanoseconds since 1970; dh, dmi, ds, dms, dus, dns of a
// duration (since's as until gives them; each as the float64 Temporal keeps).
//
// 400 rows, 1636 checks, from 92 files:
// test/built-ins/Temporal/Instant/prototype/add/add-large-subseconds.js
// test/built-ins/Temporal/Instant/prototype/add/argument-duration-max.js
// test/built-ins/Temporal/Instant/prototype/add/argument-string-fractional-units-rounding-mode.js
// test/built-ins/Temporal/Instant/prototype/add/argument-string-negative-fractional-units.js
// test/built-ins/Temporal/Instant/prototype/add/argument-string.js
// test/built-ins/Temporal/Instant/prototype/add/blank-duration.js
// test/built-ins/Temporal/Instant/prototype/add/minimum-maximum-instant.js
// test/built-ins/Temporal/Instant/prototype/add/result-out-of-range.js
// test/built-ins/Temporal/Instant/prototype/round/accepts-plural-units.js
// test/built-ins/Temporal/Instant/prototype/round/accepts-string-parameter-for-smallestunit.js
// test/built-ins/Temporal/Instant/prototype/round/negative-instant.js
// test/built-ins/Temporal/Instant/prototype/round/rounding-direction.js
// test/built-ins/Temporal/Instant/prototype/round/roundingincrement-non-integer.js
// test/built-ins/Temporal/Instant/prototype/round/roundingincrement-undefined.js
// test/built-ins/Temporal/Instant/prototype/round/roundingmode-ceil.js
// test/built-ins/Temporal/Instant/prototype/round/roundingmode-expand.js
// test/built-ins/Temporal/Instant/prototype/round/roundingmode-floor.js
// test/built-ins/Temporal/Instant/prototype/round/roundingmode-halfCeil.js
// test/built-ins/Temporal/Instant/prototype/round/roundingmode-halfEven.js
// test/built-ins/Temporal/Instant/prototype/round/roundingmode-halfExpand.js
// test/built-ins/Temporal/Instant/prototype/round/roundingmode-halfFloor.js
// test/built-ins/Temporal/Instant/prototype/round/roundingmode-halfTrunc.js
// test/built-ins/Temporal/Instant/prototype/round/roundingmode-trunc.js
// test/built-ins/Temporal/Instant/prototype/round/roundingmode-undefined.js
// test/built-ins/Temporal/Instant/prototype/round/smallestunit-string-shorthand.js
// test/built-ins/Temporal/Instant/prototype/round/throws-on-increments-that-do-not-divide-evenly.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-calendar-annotation.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-time-separators.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-time-zone-annotation.js
// test/built-ins/Temporal/Instant/prototype/since/argument-string-unknown-annotation.js
// test/built-ins/Temporal/Instant/prototype/since/blank-result.js
// test/built-ins/Temporal/Instant/prototype/since/float64-representable-integer.js
// test/built-ins/Temporal/Instant/prototype/since/instant-string-multiple-offsets.js
// test/built-ins/Temporal/Instant/prototype/since/instant-string-sub-minute-offset.js
// test/built-ins/Temporal/Instant/prototype/since/instant-string.js
// test/built-ins/Temporal/Instant/prototype/since/invalid-increments.js
// test/built-ins/Temporal/Instant/prototype/since/largestunit-undefined.js
// test/built-ins/Temporal/Instant/prototype/since/largestunit.js
// test/built-ins/Temporal/Instant/prototype/since/leap-second.js
// test/built-ins/Temporal/Instant/prototype/since/options-object.js
// test/built-ins/Temporal/Instant/prototype/since/options-undefined.js
// test/built-ins/Temporal/Instant/prototype/since/round-cross-unit-boundary.js
// test/built-ins/Temporal/Instant/prototype/since/roundingincrement-non-integer.js
// test/built-ins/Temporal/Instant/prototype/since/roundingincrement-undefined.js
// test/built-ins/Temporal/Instant/prototype/since/roundingmode-ceil.js
// test/built-ins/Temporal/Instant/prototype/since/roundingmode-expand.js
// test/built-ins/Temporal/Instant/prototype/since/roundingmode-floor.js
// test/built-ins/Temporal/Instant/prototype/since/roundingmode-halfCeil.js
// test/built-ins/Temporal/Instant/prototype/since/roundingmode-halfEven.js
// test/built-ins/Temporal/Instant/prototype/since/roundingmode-halfExpand.js
// test/built-ins/Temporal/Instant/prototype/since/roundingmode-halfFloor.js
// test/built-ins/Temporal/Instant/prototype/since/roundingmode-halfTrunc.js
// test/built-ins/Temporal/Instant/prototype/since/roundingmode-trunc.js
// test/built-ins/Temporal/Instant/prototype/since/roundingmode-undefined.js
// test/built-ins/Temporal/Instant/prototype/since/smallestunit-undefined.js
// test/built-ins/Temporal/Instant/prototype/subtract/argument-duration-max.js
// test/built-ins/Temporal/Instant/prototype/subtract/argument-string-fractional-units-rounding-mode.js
// test/built-ins/Temporal/Instant/prototype/subtract/argument-string-negative-fractional-units.js
// test/built-ins/Temporal/Instant/prototype/subtract/argument-string.js
// test/built-ins/Temporal/Instant/prototype/subtract/blank-duration.js
// test/built-ins/Temporal/Instant/prototype/subtract/minimum-maximum-instant.js
// test/built-ins/Temporal/Instant/prototype/subtract/result-out-of-range.js
// test/built-ins/Temporal/Instant/prototype/subtract/subtract-large-subseconds.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-calendar-annotation.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-date-with-utc-offset.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-time-separators.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-time-zone-annotation.js
// test/built-ins/Temporal/Instant/prototype/until/argument-string-unknown-annotation.js
// test/built-ins/Temporal/Instant/prototype/until/blank-result.js
// test/built-ins/Temporal/Instant/prototype/until/float64-representable-integer.js
// test/built-ins/Temporal/Instant/prototype/until/instant-string-multiple-offsets.js
// test/built-ins/Temporal/Instant/prototype/until/instant-string-sub-minute-offset.js
// test/built-ins/Temporal/Instant/prototype/until/instant-string.js
// test/built-ins/Temporal/Instant/prototype/until/largestunit-undefined.js
// test/built-ins/Temporal/Instant/prototype/until/leap-second.js
// test/built-ins/Temporal/Instant/prototype/until/options-object.js
// test/built-ins/Temporal/Instant/prototype/until/options-undefined.js
// test/built-ins/Temporal/Instant/prototype/until/round-cross-unit-boundary.js
// test/built-ins/Temporal/Instant/prototype/until/roundingincrement-non-integer.js
// test/built-ins/Temporal/Instant/prototype/until/roundingincrement-undefined.js
// test/built-ins/Temporal/Instant/prototype/until/roundingmode-ceil.js
// test/built-ins/Temporal/Instant/prototype/until/roundingmode-expand.js
// test/built-ins/Temporal/Instant/prototype/until/roundingmode-floor.js
// test/built-ins/Temporal/Instant/prototype/until/roundingmode-halfCeil.js
// test/built-ins/Temporal/Instant/prototype/until/roundingmode-halfEven.js
// test/built-ins/Temporal/Instant/prototype/until/roundingmode-halfExpand.js
// test/built-ins/Temporal/Instant/prototype/until/roundingmode-halfFloor.js
// test/built-ins/Temporal/Instant/prototype/until/roundingmode-halfTrunc.js
// test/built-ins/Temporal/Instant/prototype/until/roundingmode-trunc.js
// test/built-ins/Temporal/Instant/prototype/until/roundingmode-undefined.js
// test/built-ins/Temporal/Instant/prototype/until/smallestunit-undefined.js

"add -8640000000000 0 -1 0 = refused",
"add -8640000000000 0 -3600 0 = refused",
"add -8640000000000 0 -60 0 = refused",
"add -8640000000000 0 0 -1000000 = refused",
"add -8640000000000 0 0 -1000 = refused",
"add -8640000000000 0 0 -1 = refused",
"add -8640000000000 0 0 0 = ns=-8640000000000000000000",
"add -8640000000000 0 0 1 = ns=-8639999999999999999999",
"add -8640000000000 0 17280000000000 0 = ns=8640000000000000000000",
"add 0 0 -3712 -500000000 = ns=-3712500000000",
"add 0 0 -8640000000000 0 = ns=-8640000000000000000000",
"add 0 0 -9007199254 -740990976 = ns=-9007199254740990976",
"add 0 0 -9007199254740 -990976000 = refused",
"add 0 0 -9007199254740990 -976000000 = refused",
"add 0 0 3712 500000000 = ns=3712500000000",
"add 0 0 8640000000000 0 = ns=8640000000000000000000",
"add 0 0 9007199254 740990976 = ns=9007199254740990976",
"add 0 0 9007199254740 990976000 = refused",
"add 0 0 9007199254740990 976000000 = refused",
"add 0 1 0 0 = ns=1",
"add 10 0 -10800 0 = ns=-10790000000000",
"add 10 0 10800 0 = ns=10810000000000",
"add 1000000000 0 -86434 -73407380 = ns=999913565926592620",
"add 1000000000 0 -88444 -404442800 = ns=999911555595557200",
"add 1000000000 0 86434 73407380 = ns=1000086434073407380",
"add 1000000000 0 88444 404442800 = ns=1000088444404442800",
"add 1582966647 747612578 -9007199 -254740991 = ns=1573959448492871587",
"add 1582966647 747612578 -9007199254 -740990976 = ns=-7424232606993378398",
"add 1582966647 747612578 -9007199254 -740991000 = ns=-7424232606993378422",
"add 1582966647 747612578 -9007199254740 -990976000 = refused",
"add 1582966647 747612578 -9007199254740 -991000000 = refused",
"add 1582966647 747612578 -9007199254740990 -976000000 = refused",
"add 1582966647 747612578 -9007199254740991 0 = refused",
"add 1582966647 747612578 9007199 254740991 = ns=1591973847002353569",
"add 1582966647 747612578 9007199254 740990976 = ns=10590165902488603554",
"add 1582966647 747612578 9007199254 740991000 = ns=10590165902488603578",
"add 1582966647 747612578 9007199254740 990976000 = refused",
"add 1582966647 747612578 9007199254740 991000000 = refused",
"add 1582966647 747612578 9007199254740990 976000000 = refused",
"add 1582966647 747612578 9007199254740991 0 = refused",
"add 8640000000000 0 -17280000000000 0 = ns=-8640000000000000000000",
"add 8640000000000 0 0 -1 = ns=8639999999999999999999",
"add 8640000000000 0 0 0 = ns=8640000000000000000000",
"add 8640000000000 0 0 1000000 = refused",
"add 8640000000000 0 0 1000 = refused",
"add 8640000000000 0 0 1 = refused",
"add 8640000000000 0 1 0 = refused",
"add 8640000000000 0 3600 0 = refused",
"add 8640000000000 0 60 0 = refused",
"round -1000000000 0 1 1 ceil = ns=-999997200000000000",
"round -1000000000 0 1 1 expand = ns=-999997200000000000",
"round -1000000000 0 1 1 floor = ns=-1000000800000000000",
"round -1000000000 0 1 1 halfCeil = ns=-1000000800000000000",
"round -1000000000 0 1 1 halfEven = ns=-1000000800000000000",
"round -1000000000 0 1 1 halfExpand = ns=-1000000800000000000",
"round -1000000000 0 1 1 halfFloor = ns=-1000000800000000000",
"round -1000000000 0 1 1 halfTrunc = ns=-1000000800000000000",
"round -1000000000 0 1 1 trunc = ns=-1000000800000000000",
"round -65261246400 500000000 3 1 ceil = ns=-65261246399000000000",
"round -65261246400 500000000 3 1 floor = ns=-65261246400000000000",
"round -65261246400 500000000 3 1 halfExpand = ns=-65261246399000000000",
"round -65261246400 500000000 3 1 trunc = ns=-65261246400000000000",
"round 0 0 1 7 halfExpand = refused",
"round 0 0 2 29 halfExpand = refused",
"round 0 0 3 29 halfExpand = refused",
"round 0 0 4 29 halfExpand = refused",
"round 0 0 5 29 halfExpand = refused",
"round 0 0 6 29 halfExpand = refused",
"round 1000000000 123456789 1 1 halfExpand = ns=1000000800000000000",
"round 1000000000 123456789 2 1 halfExpand = ns=1000000020000000000",
"round 1000000000 123456789 3 1 halfExpand = ns=1000000000000000000",
"round 1000000000 123456789 4 1 halfExpand = ns=1000000000123000000",
"round 1000000000 123456789 5 1 halfExpand = ns=1000000000123457000",
"round 1000000000 123456789 6 1 halfExpand = ns=1000000000123456789",
"round 1000000000 123987500 3 1 halfExpand = ns=1000000000000000000",
"round 1000000000 123987500 4 1 halfExpand = ns=1000000000124000000",
"round 1000000000 123987500 5 1 halfExpand = ns=1000000000123988000",
"round 1000000000 5 6 2 expand = ns=1000000000000000006",
"round 1000000000 987654321 1 1 halfExpand = ns=1000000800000000000",
"round 1000000000 987654321 2 1 halfExpand = ns=1000000020000000000",
"round 1000000000 987654321 3 1 halfExpand = ns=1000000001000000000",
"round 1000000000 987654321 4 1 halfExpand = ns=1000000000988000000",
"round 1000000000 987654321 5 1 halfExpand = ns=1000000000987654000",
"round 1000000000 987654321 6 1 halfExpand = ns=1000000000987654321",
"round 217175010 123987500 1 1 ceil = ns=217177200000000000",
"round 217175010 123987500 1 1 expand = ns=217177200000000000",
"round 217175010 123987500 1 1 floor = ns=217173600000000000",
"round 217175010 123987500 1 1 halfCeil = ns=217173600000000000",
"round 217175010 123987500 1 1 halfEven = ns=217173600000000000",
"round 217175010 123987500 1 1 halfExpand = ns=217173600000000000",
"round 217175010 123987500 1 1 halfFloor = ns=217173600000000000",
"round 217175010 123987500 1 1 halfTrunc = ns=217173600000000000",
"round 217175010 123987500 1 1 trunc = ns=217173600000000000",
"round 217175010 123987500 2 1 ceil = ns=217175040000000000",
"round 217175010 123987500 2 1 expand = ns=217175040000000000",
"round 217175010 123987500 2 1 floor = ns=217174980000000000",
"round 217175010 123987500 2 1 halfCeil = ns=217175040000000000",
"round 217175010 123987500 2 1 halfEven = ns=217175040000000000",
"round 217175010 123987500 2 1 halfExpand = ns=217175040000000000",
"round 217175010 123987500 2 1 halfFloor = ns=217175040000000000",
"round 217175010 123987500 2 1 halfTrunc = ns=217175040000000000",
"round 217175010 123987500 2 1 trunc = ns=217174980000000000",
"round 217175010 123987500 3 1 ceil = ns=217175011000000000",
"round 217175010 123987500 3 1 expand = ns=217175011000000000",
"round 217175010 123987500 3 1 floor = ns=217175010000000000",
"round 217175010 123987500 3 1 halfCeil = ns=217175010000000000",
"round 217175010 123987500 3 1 halfEven = ns=217175010000000000",
"round 217175010 123987500 3 1 halfExpand = ns=217175010000000000",
"round 217175010 123987500 3 1 halfFloor = ns=217175010000000000",
"round 217175010 123987500 3 1 halfTrunc = ns=217175010000000000",
"round 217175010 123987500 3 1 trunc = ns=217175010000000000",
"round 217175010 123987500 4 1 ceil = ns=217175010124000000",
"round 217175010 123987500 4 1 expand = ns=217175010124000000",
"round 217175010 123987500 4 1 floor = ns=217175010123000000",
"round 217175010 123987500 4 1 halfCeil = ns=217175010124000000",
"round 217175010 123987500 4 1 halfEven = ns=217175010124000000",
"round 217175010 123987500 4 1 halfExpand = ns=217175010124000000",
"round 217175010 123987500 4 1 halfFloor = ns=217175010124000000",
"round 217175010 123987500 4 1 halfTrunc = ns=217175010124000000",
"round 217175010 123987500 4 1 trunc = ns=217175010123000000",
"round 217175010 123987500 5 1 ceil = ns=217175010123988000",
"round 217175010 123987500 5 1 expand = ns=217175010123988000",
"round 217175010 123987500 5 1 floor = ns=217175010123987000",
"round 217175010 123987500 5 1 halfCeil = ns=217175010123988000",
"round 217175010 123987500 5 1 halfEven = ns=217175010123988000",
"round 217175010 123987500 5 1 halfExpand = ns=217175010123988000",
"round 217175010 123987500 5 1 halfFloor = ns=217175010123987000",
"round 217175010 123987500 5 1 halfTrunc = ns=217175010123987000",
"round 217175010 123987500 5 1 trunc = ns=217175010123987000",
"round 217175010 123987500 6 1 ceil = ns=217175010123987500",
"round 217175010 123987500 6 1 expand = ns=217175010123987500",
"round 217175010 123987500 6 1 floor = ns=217175010123987500",
"round 217175010 123987500 6 1 halfCeil = ns=217175010123987500",
"round 217175010 123987500 6 1 halfEven = ns=217175010123987500",
"round 217175010 123987500 6 1 halfExpand = ns=217175010123987500",
"round 217175010 123987500 6 1 halfFloor = ns=217175010123987500",
"round 217175010 123987500 6 1 halfTrunc = ns=217175010123987500",
"round 217175010 123987500 6 1 trunc = ns=217175010123987500",
"until 0 0 7199 0 1 2 1 expand = dh=2 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 0 0 -3600 0 3 6 1 trunc = dh=0 dmi=0 ds=-3600 dms=0 dus=0 dns=0",
"until 0 0 0 0 3 6 1 trunc = dh=0 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 0 0 3600 0 3 6 1 trunc = dh=0 dmi=0 ds=3600 dms=0 dus=0 dns=0",
"until 0 0 18446744073 709551616 5 6 1 trunc = dus=18446744073709552",
"until 0 1 0 1 3 6 1 trunc = dh=0 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1000000000 0 1000090061 123987500 3 3 1 trunc = dh=0 dmi=0 ds=90061 dms=0 dus=0 dns=0",
"until 1000000000 0 1000090061 123987500 3 4 1 trunc = dh=0 dmi=0 ds=90061 dms=123 dus=0 dns=0",
"until 1000000000 0 1000090061 123987500 3 5 1 trunc = dh=0 dmi=0 ds=90061 dms=123 dus=987 dns=0",
"until 1000000000 0 1000090061 987654321 3 6 1 trunc = dh=0 dmi=0 ds=90061 dms=987 dus=654 dns=321",
"until 1000000000 0 1000000000 5 3 6 2 trunc = dh=0 dmi=0 ds=0 dms=0 dus=0 dns=4",
"until 1000000000 987654321 1000090061 988655322 3 6 1 trunc = dh=0 dmi=0 ds=90061 dms=1 dus=1 dns=1",
"until 1483228799 0 1483228799 0 3 6 1 trunc = dh=0 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 1 1 ceil = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 1 1 expand = dh=-376436 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 1 1 floor = dh=-376436 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 1 1 halfCeil = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 1 1 halfEven = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 1 1 halfExpand = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 1 1 halfFloor = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 1 1 halfTrunc = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 1 1 trunc = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 2 1 ceil = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 2 1 expand = dh=-376435 dmi=-24 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 2 1 floor = dh=-376435 dmi=-24 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 2 1 halfCeil = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 2 1 halfEven = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 2 1 halfExpand = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 2 1 halfFloor = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 2 1 halfTrunc = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 2 1 trunc = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 3 1 ceil = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 3 1 expand = dh=-376435 dmi=-23 ds=-9 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 3 1 floor = dh=-376435 dmi=-23 ds=-9 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 3 1 halfCeil = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 3 1 halfEven = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 3 1 halfExpand = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 3 1 halfFloor = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 3 1 halfTrunc = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 3 1 trunc = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 4 1 ceil = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 4 1 expand = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 4 1 floor = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 4 1 halfCeil = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 4 1 halfEven = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 4 1 halfExpand = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 4 1 halfFloor = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 4 1 halfTrunc = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 4 1 trunc = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=0 dns=0",
"until 1572345998 271986289 217178610 123456789 1 5 1 ceil = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=0",
"until 1572345998 271986289 217178610 123456789 1 5 1 expand = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-530 dns=0",
"until 1572345998 271986289 217178610 123456789 1 5 1 floor = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-530 dns=0",
"until 1572345998 271986289 217178610 123456789 1 5 1 halfCeil = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=0",
"until 1572345998 271986289 217178610 123456789 1 5 1 halfEven = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-530 dns=0",
"until 1572345998 271986289 217178610 123456789 1 5 1 halfExpand = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-530 dns=0",
"until 1572345998 271986289 217178610 123456789 1 5 1 halfFloor = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-530 dns=0",
"until 1572345998 271986289 217178610 123456789 1 5 1 halfTrunc = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=0",
"until 1572345998 271986289 217178610 123456789 1 5 1 trunc = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=0",
"until 1572345998 271986289 217178610 123456789 1 6 1 ceil = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"until 1572345998 271986289 217178610 123456789 1 6 1 expand = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"until 1572345998 271986289 217178610 123456789 1 6 1 floor = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"until 1572345998 271986289 217178610 123456789 1 6 1 halfCeil = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"until 1572345998 271986289 217178610 123456789 1 6 1 halfEven = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"until 1572345998 271986289 217178610 123456789 1 6 1 halfExpand = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"until 1572345998 271986289 217178610 123456789 1 6 1 halfFloor = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"until 1572345998 271986289 217178610 123456789 1 6 1 halfTrunc = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"until 1572345998 271986289 217178610 123456789 1 6 1 trunc = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"until 217178610 123456789 1572345998 271986289 1 1 1 ceil = dh=376436 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 1 1 expand = dh=376436 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 1 1 floor = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 1 1 halfCeil = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 1 1 halfEven = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 1 1 halfExpand = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 1 1 halfFloor = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 1 1 halfTrunc = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 1 1 trunc = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 2 1 ceil = dh=376435 dmi=24 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 2 1 expand = dh=376435 dmi=24 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 2 1 floor = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 2 1 halfCeil = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 2 1 halfEven = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 2 1 halfExpand = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 2 1 halfFloor = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 2 1 halfTrunc = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 2 1 trunc = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 3 1 ceil = dh=376435 dmi=23 ds=9 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 3 1 expand = dh=376435 dmi=23 ds=9 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 3 1 floor = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 3 1 halfCeil = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 3 1 halfEven = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 3 1 halfExpand = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 3 1 halfFloor = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 3 1 halfTrunc = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 3 1 trunc = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 4 1 ceil = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 4 1 expand = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 4 1 floor = dh=376435 dmi=23 ds=8 dms=148 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 4 1 halfCeil = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 4 1 halfEven = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 4 1 halfExpand = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 4 1 halfFloor = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 4 1 halfTrunc = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 4 1 trunc = dh=376435 dmi=23 ds=8 dms=148 dus=0 dns=0",
"until 217178610 123456789 1572345998 271986289 1 5 1 ceil = dh=376435 dmi=23 ds=8 dms=148 dus=530 dns=0",
"until 217178610 123456789 1572345998 271986289 1 5 1 expand = dh=376435 dmi=23 ds=8 dms=148 dus=530 dns=0",
"until 217178610 123456789 1572345998 271986289 1 5 1 floor = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=0",
"until 217178610 123456789 1572345998 271986289 1 5 1 halfCeil = dh=376435 dmi=23 ds=8 dms=148 dus=530 dns=0",
"until 217178610 123456789 1572345998 271986289 1 5 1 halfEven = dh=376435 dmi=23 ds=8 dms=148 dus=530 dns=0",
"until 217178610 123456789 1572345998 271986289 1 5 1 halfExpand = dh=376435 dmi=23 ds=8 dms=148 dus=530 dns=0",
"until 217178610 123456789 1572345998 271986289 1 5 1 halfFloor = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=0",
"until 217178610 123456789 1572345998 271986289 1 5 1 halfTrunc = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=0",
"until 217178610 123456789 1572345998 271986289 1 5 1 trunc = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=0",
"until 217178610 123456789 1572345998 271986289 1 6 1 ceil = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"until 217178610 123456789 1572345998 271986289 1 6 1 expand = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"until 217178610 123456789 1572345998 271986289 1 6 1 floor = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"until 217178610 123456789 1572345998 271986289 1 6 1 halfCeil = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"until 217178610 123456789 1572345998 271986289 1 6 1 halfEven = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"until 217178610 123456789 1572345998 271986289 1 6 1 halfExpand = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"until 217178610 123456789 1572345998 271986289 1 6 1 halfFloor = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"until 217178610 123456789 1572345998 271986289 1 6 1 halfTrunc = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"until 217178610 123456789 1572345998 271986289 1 6 1 trunc = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"until 957270896 987654321 959949296 987654322 3 6 1 trunc = dh=0 dmi=0 ds=2678400 dns=1",
"since 0 0 7199 0 1 2 1 expand = dh=2 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 0 0 -3600 0 3 6 1 trunc = dh=0 dmi=0 ds=-3600 dms=0 dus=0 dns=0",
"since 0 0 0 0 3 6 1 trunc = dh=0 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 0 0 3600 0 3 6 1 trunc = dh=0 dmi=0 ds=3600 dms=0 dus=0 dns=0",
"since 0 0 18446744073 709551616 5 6 1 trunc = dus=18446744073709552",
"since 0 1 0 1 3 6 1 trunc = dh=0 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1000000000 0 0 0 1 1 11 trunc = refused",
"since 1000000000 0 0 0 1 1 24 trunc = refused",
"since 1000000000 0 0 0 1 2 29 trunc = refused",
"since 1000000000 0 0 0 1 2 60 trunc = refused",
"since 1000000000 0 0 0 1 3 29 trunc = refused",
"since 1000000000 0 0 0 1 3 60 trunc = refused",
"since 1000000000 0 0 0 1 4 1000 trunc = refused",
"since 1000000000 0 0 0 1 4 29 trunc = refused",
"since 1000000000 0 0 0 1 5 1000 trunc = refused",
"since 1000000000 0 0 0 1 5 29 trunc = refused",
"since 1000000000 0 0 0 1 6 1000 trunc = refused",
"since 1000000000 0 0 0 1 6 29 trunc = refused",
"since 1000000000 5 1000000000 0 3 6 2 trunc = dh=0 dmi=0 ds=0 dms=0 dus=0 dns=-4",
"since 1000090061 123987500 1000000000 0 3 3 1 trunc = dh=0 dmi=0 ds=-90061 dms=0 dus=0 dns=0",
"since 1000090061 123987500 1000000000 0 3 4 1 trunc = dh=0 dmi=0 ds=-90061 dms=-123 dus=0 dns=0",
"since 1000090061 123987500 1000000000 0 3 5 1 trunc = dh=0 dmi=0 ds=-90061 dms=-123 dus=-987 dns=0",
"since 1000090061 987654321 1000000000 0 3 6 1 trunc = dh=0 dmi=0 ds=-90061 dms=-987 dus=-654 dns=-321",
"since 1000090061 988655322 1000000000 987654321 3 6 1 trunc = dh=0 dmi=0 ds=-90061 dms=-1 dus=-1 dns=-1",
"since 1483228799 0 1483228799 0 3 6 1 trunc = dh=0 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 1 1 ceil = dh=-376436 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 1 1 expand = dh=-376436 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 1 1 floor = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 1 1 halfCeil = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 1 1 halfEven = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 1 1 halfExpand = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 1 1 halfFloor = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 1 1 halfTrunc = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 1 1 trunc = dh=-376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 2 1 ceil = dh=-376435 dmi=-24 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 2 1 expand = dh=-376435 dmi=-24 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 2 1 floor = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 2 1 halfCeil = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 2 1 halfEven = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 2 1 halfExpand = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 2 1 halfFloor = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 2 1 halfTrunc = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 2 1 trunc = dh=-376435 dmi=-23 ds=0 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 3 1 ceil = dh=-376435 dmi=-23 ds=-9 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 3 1 expand = dh=-376435 dmi=-23 ds=-9 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 3 1 floor = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 3 1 halfCeil = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 3 1 halfEven = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 3 1 halfExpand = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 3 1 halfFloor = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 3 1 halfTrunc = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 3 1 trunc = dh=-376435 dmi=-23 ds=-8 dms=0 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 4 1 ceil = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 4 1 expand = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 4 1 floor = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 4 1 halfCeil = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 4 1 halfEven = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 4 1 halfExpand = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 4 1 halfFloor = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 4 1 halfTrunc = dh=-376435 dmi=-23 ds=-8 dms=-149 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 4 1 trunc = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=0 dns=0",
"since 1572345998 271986289 217178610 123456789 1 5 1 ceil = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-530 dns=0",
"since 1572345998 271986289 217178610 123456789 1 5 1 expand = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-530 dns=0",
"since 1572345998 271986289 217178610 123456789 1 5 1 floor = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=0",
"since 1572345998 271986289 217178610 123456789 1 5 1 halfCeil = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-530 dns=0",
"since 1572345998 271986289 217178610 123456789 1 5 1 halfEven = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-530 dns=0",
"since 1572345998 271986289 217178610 123456789 1 5 1 halfExpand = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-530 dns=0",
"since 1572345998 271986289 217178610 123456789 1 5 1 halfFloor = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=0",
"since 1572345998 271986289 217178610 123456789 1 5 1 halfTrunc = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=0",
"since 1572345998 271986289 217178610 123456789 1 5 1 trunc = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=0",
"since 1572345998 271986289 217178610 123456789 1 6 1 ceil = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"since 1572345998 271986289 217178610 123456789 1 6 1 expand = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"since 1572345998 271986289 217178610 123456789 1 6 1 floor = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"since 1572345998 271986289 217178610 123456789 1 6 1 halfCeil = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"since 1572345998 271986289 217178610 123456789 1 6 1 halfEven = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"since 1572345998 271986289 217178610 123456789 1 6 1 halfExpand = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"since 1572345998 271986289 217178610 123456789 1 6 1 halfFloor = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"since 1572345998 271986289 217178610 123456789 1 6 1 halfTrunc = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"since 1572345998 271986289 217178610 123456789 1 6 1 trunc = dh=-376435 dmi=-23 ds=-8 dms=-148 dus=-529 dns=-500",
"since 217178610 123456789 1572345998 271986289 1 1 1 ceil = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 1 1 expand = dh=376436 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 1 1 floor = dh=376436 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 1 1 halfCeil = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 1 1 halfEven = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 1 1 halfExpand = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 1 1 halfFloor = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 1 1 halfTrunc = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 1 1 trunc = dh=376435 dmi=0 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 2 1 ceil = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 2 1 expand = dh=376435 dmi=24 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 2 1 floor = dh=376435 dmi=24 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 2 1 halfCeil = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 2 1 halfEven = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 2 1 halfExpand = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 2 1 halfFloor = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 2 1 halfTrunc = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 2 1 trunc = dh=376435 dmi=23 ds=0 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 3 1 ceil = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 3 1 expand = dh=376435 dmi=23 ds=9 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 3 1 floor = dh=376435 dmi=23 ds=9 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 3 1 halfCeil = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 3 1 halfEven = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 3 1 halfExpand = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 3 1 halfFloor = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 3 1 halfTrunc = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 3 1 trunc = dh=376435 dmi=23 ds=8 dms=0 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 4 1 ceil = dh=376435 dmi=23 ds=8 dms=148 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 4 1 expand = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 4 1 floor = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 4 1 halfCeil = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 4 1 halfEven = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 4 1 halfExpand = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 4 1 halfFloor = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 4 1 halfTrunc = dh=376435 dmi=23 ds=8 dms=149 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 4 1 trunc = dh=376435 dmi=23 ds=8 dms=148 dus=0 dns=0",
"since 217178610 123456789 1572345998 271986289 1 5 1 ceil = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=0",
"since 217178610 123456789 1572345998 271986289 1 5 1 expand = dh=376435 dmi=23 ds=8 dms=148 dus=530 dns=0",
"since 217178610 123456789 1572345998 271986289 1 5 1 floor = dh=376435 dmi=23 ds=8 dms=148 dus=530 dns=0",
"since 217178610 123456789 1572345998 271986289 1 5 1 halfCeil = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=0",
"since 217178610 123456789 1572345998 271986289 1 5 1 halfEven = dh=376435 dmi=23 ds=8 dms=148 dus=530 dns=0",
"since 217178610 123456789 1572345998 271986289 1 5 1 halfExpand = dh=376435 dmi=23 ds=8 dms=148 dus=530 dns=0",
"since 217178610 123456789 1572345998 271986289 1 5 1 halfFloor = dh=376435 dmi=23 ds=8 dms=148 dus=530 dns=0",
"since 217178610 123456789 1572345998 271986289 1 5 1 halfTrunc = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=0",
"since 217178610 123456789 1572345998 271986289 1 5 1 trunc = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=0",
"since 217178610 123456789 1572345998 271986289 1 6 1 ceil = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"since 217178610 123456789 1572345998 271986289 1 6 1 expand = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"since 217178610 123456789 1572345998 271986289 1 6 1 floor = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"since 217178610 123456789 1572345998 271986289 1 6 1 halfCeil = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"since 217178610 123456789 1572345998 271986289 1 6 1 halfEven = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"since 217178610 123456789 1572345998 271986289 1 6 1 halfExpand = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"since 217178610 123456789 1572345998 271986289 1 6 1 halfFloor = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"since 217178610 123456789 1572345998 271986289 1 6 1 halfTrunc = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"since 217178610 123456789 1572345998 271986289 1 6 1 trunc = dh=376435 dmi=23 ds=8 dms=148 dus=529 dns=500",
"since 2345678901 234567890 1234567890 123456789 1 6 1 trunc = dh=-308641 dmi=-56 ds=-51 dms=-111 dus=-111 dns=-101",
"since 2345678901 234567890 1234567890 123456789 2 6 1 trunc = dh=0 dmi=-18518516 ds=-51 dms=-111 dus=-111 dns=-101",
"since 2345678901 234567890 1234567890 123456789 3 6 1 trunc = dh=0 dmi=0 ds=-1111111011 dms=-111 dus=-111 dns=-101",
"since 2345678901 234567890 1234567890 123456789 4 6 1 trunc = dh=0 dmi=0 ds=0 dms=-1111111011111 dus=-111 dns=-101",
"since 2345678901 234567890 1234567890 123456789 5 6 1 trunc = dh=0 dmi=0 ds=0 dms=0 dus=-1111111011111111 dns=-101",
"since 2345678901 234567890 1234567890 123456789 6 6 1 trunc = dh=0 dmi=0 ds=0 dms=0 dus=0 dns=-1111111011111111040",
"since 959949296 987654322 957270896 987654321 3 6 1 trunc = dh=0 dmi=0 ds=-2678400 dns=-1",
