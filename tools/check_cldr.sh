#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# The checks against the pinned CLDR and tzdb releases, after the
# generator and the conformance programs are built:
#
#   tools/check_cldr.sh BUILD CLDR_CACHE TZDB TZDB_OLDER
#
# BUILD holds mlocgen and the conform_* programs; CLDR_CACHE is the
# directory tools/fetch_cldr.sh filled; TZDB and TZDB_OLDER are the
# release directories tools/fetch_tzdb.sh unpacked. Each check fails on a
# new difference and on a listed one that went away.
# docs/internals/conformance.md says what each proves.
set -euo pipefail

build=${1:?usage: check_cldr.sh BUILD CLDR_CACHE TZDB TZDB_OLDER}
cldr=${2:?usage: check_cldr.sh BUILD CLDR_CACHE TZDB TZDB_OLDER}
tzdb=${3:?usage: check_cldr.sh BUILD CLDR_CACHE TZDB TZDB_OLDER}
older=${4:?usage: check_cldr.sh BUILD CLDR_CACHE TZDB TZDB_OLDER}
common=$cldr/cldr/common
json=$cldr/json
uca=$common/uca
gen=src/generated

step() { printf '\n== %s\n' "$*"; }

step "root collation against allkeys"
"$build/conform_collation_root" "$uca/FractionalUCA.txt" "$uca/allkeys_CLDR.txt"

step "root elements against ICU4C 79's, Han's section aside"
"$build/conform_collation_root_elements" "$uca/FractionalUCA.txt" --expect 6110 ab8be4117eb364f2

step "the tailoring builder's lookup against the iterator"
"$build/conform_collation_mappings" "$uca/FractionalUCA.txt"

step "every collation type built, its answers ICU4C 79's"
"$build/conform_collation_builder" "$uca/FractionalUCA.txt" --expect afcad9d236469b29 \
    "$common"/collation/*.xml test/collation/crafted.xml

step "every tailoring's unit read through the runtime iterator"
"$build/conform_collation_builder" "$uca/FractionalUCA.txt" --units "$common"/collation/*.xml \
    test/collation/crafted.xml

step "every type's fast Latin table built, written and read back"
"$build/conform_collation_fast_builder" "$uca/FractionalUCA.txt" --expect edb9afb552346149 \
    "$common"/collation/*.xml test/collation/crafted.xml

step "tailored locales' sort keys, as ICU4C 79's where its data has them"
"$build/conform_collation_builder" "$uca/FractionalUCA.txt" --queries "$common"/collation/*.xml |
    "$build/conform_collation_keys" $gen/collation.mloc --expect 5191759b1ca1b666

step "every collation rule string parsed"
"$build/conform_collation_rules" "$common"/collation/*.xml

step "the general compare against CollationTest"
short=$uca/CollationTest_CLDR_NON_IGNORABLE_SHORT.txt
long=$uca/CollationTest_CLDR_NON_IGNORABLE.txt
"$build/conform_collation" $gen/collation.mloc "$short" "$long"
"$build/conform_collation" $gen/collation.mloc --identical "$short" "$long"
short=$uca/CollationTest_CLDR_SHIFTED_SHORT.txt
long=$uca/CollationTest_CLDR_SHIFTED.txt
"$build/conform_collation" $gen/collation.mloc --shifted "$short" "$long"
"$build/conform_collation" $gen/collation.mloc --shifted --identical "$short" "$long"

step "the generator's resolution, preferences, currencies and units against the JSON"
python3 tools/check_resolution.py "$build/mlocgen" "$common" "$json"
python3 tools/check_preferences.py "$build/conform_preferences" "$json"
python3 tools/check_currency.py "$build/conform_currency" "$json"
python3 tools/check_units.py "$build/conform_units" "$common/testData/units/unitsTest.txt"
python3 tools/check_unit_prefs.py "$build/conform_unit_prefs" "$common/testData/units"

step "the zones against zic and zdump, from 1800 to 2500"
python3 tools/check_zones.py "$build/conform_zones" "$tzdb"

step "a zones file of an older release against its zic"
"$build/mlocgen" zonesfile "$older" "$common" "$build/zones-older.mloc"
python3 tools/check_zones.py "$build/conform_zones" "$older" "$build/zones-older.mloc"

step "skeleton matching and interval patterns against ICU4C 79's tables"
python3 tools/check_skeletons.py "$build/mlocgen" "$common" test/skeletons_icu.tsv \
    tools/skeleton-exceptions.txt
python3 tools/check_intervals.py "$build/mlocgen" "$common" test/intervals_icu.tsv \
    tools/interval-exceptions.txt

step "semantic skeletons and dates against CLDR's test data"
python3 tools/check_datetime.py "$build/mlocgen" "$common" "$build/conform_datetime" \
    $gen/dates.mloc $gen/spellout.mloc $gen/zone-names.mloc $gen/names.mloc

step "BCP 47 ids and Windows names against CLDR"
python3 tools/check_zone_codes.py "$build/conform_zone_codes" "$common" "$tzdb"

step "local times against zoneinfo"
python3 tools/check_local.py "$build/conform_local" "$tzdb"

step "Temporal strings against ECMA-262's grammar"
python3 tools/check_ixdtf.py "$build/conform_ixdtf"

step "zoned arithmetic against ECMA-262 over zoneinfo"
python3 tools/check_zoned.py "$build/conform_zoned" "$tzdb"

step "CLDR's conformance files"
ids=$common/testData/localeIdentifiers
"$build/conform_likely" "$ids/likelySubtags.txt"
"$build/conform_canonical" "$ids/localeCanonicalization.txt"
ls "$common/main" | sed -n 's/\.xml$//p' |
    awk '{ print "main\t" $0; print "collation\t" $0 }' |
    "$build/mlocgen" chains "$common" | "$build/conform_parents"
python3 tools/cldr_directions.py "$common" | "$build/conform_direction"
"$build/mlocgen" plural-samples "$common" | "$build/conform_plurals"
cat "$common"/testData/decimal/*.tsv | "$build/conform_numbers" tools/number-exceptions.txt
"$build/conform_spellout" $gen/spellout.mloc "$common"/testData/rbnf/*.ssv
"$build/conform_locale_names" $gen/names.mloc $gen/subdivisions.mloc "$ids/localeDisplayName.txt" \
    tools/locale-name-exceptions.txt

step "matching against ICU's matcher test"
"$build/conform_match" tools/external/icu/localeMatcherTest.txt tools/match-exceptions.txt

step "the calendar suites' full sweeps, which the suites sample"
for suite in calendar calendar_lunar calendar_arithmetic calendar_oracle; do
    "$build/conform_$suite"
done

step "every CLDR check passed"
