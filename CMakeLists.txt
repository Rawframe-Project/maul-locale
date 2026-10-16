cmake_minimum_required(VERSION 3.25)

# The library's identity. The family's CMake modules and tools read these.
set(MAUL_LIBRARY maul-locale)
set(MAUL_API_PREFIX mloc)
set(MAUL_MACRO_PREFIX MLOC)
set(MAUL_TITLE "Maul Locale")
set(MAUL_PREFIX MAUL_LOCALE)

include(cmake/ReadVersion.cmake)
maul_read_version(${CMAKE_CURRENT_SOURCE_DIR}/include/maul-locale/base.h MLOC MAUL_VERSION)
project(maul-locale
    VERSION ${MAUL_VERSION}
    DESCRIPTION "Locale data and formatting from CLDR"
    LANGUAGES C)

option(MAUL_LOCALE_BUILD_TESTS "Build the test suites" ${PROJECT_IS_TOP_LEVEL})
option(MAUL_LOCALE_BUILD_BENCH "Build the benchmarks" ${PROJECT_IS_TOP_LEVEL})
# The generator runs on the build machine, so a web build leaves it out.
if(PROJECT_IS_TOP_LEVEL AND NOT EMSCRIPTEN)
    set(MAUL_LOCALE_TOOLS_DEFAULT ON)
else()
    set(MAUL_LOCALE_TOOLS_DEFAULT OFF)
endif()
option(MAUL_LOCALE_BUILD_TOOLS "Build the data generator and its tests" ${MAUL_LOCALE_TOOLS_DEFAULT})
option(MAUL_LOCALE_BUILD_SHARED "Build a shared library instead of a static one" OFF)
option(MAUL_LOCALE_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
option(MAUL_LOCALE_WERROR "Treat compiler warnings as errors" OFF)
option(MAUL_LOCALE_SANITIZE "Instrument with AddressSanitizer and UBSan" OFF)
option(MAUL_LOCALE_COVERAGE "Instrument for gcov coverage" OFF)
option(MAUL_LOCALE_FUZZ "Build the libFuzzer targets (Clang only)" OFF)

# The generator asks Maul Unicode which currency symbols start or end in
# letters (record mloc-0022); the library itself does not link it.
# Fetched before the family modules, so that ours, included after its
# copies, serve this project. FETCHCONTENT_SOURCE_DIR_MAUL-UNICODE builds
# a local copy instead, offline.
if(MAUL_LOCALE_BUILD_TOOLS)
    include(FetchContent)
    set(MAUL_UNICODE_NORMALIZATION OFF)
    # The generator lowercases the unit names' patterns.
    set(MAUL_UNICODE_CASE ON)
    set(MAUL_UNICODE_SECURITY OFF)
    set(MAUL_UNICODE_INSTALL OFF)
    FetchContent_Declare(maul-unicode
        GIT_REPOSITORY https://github.com/Rawframe-Project/maul-unicode.git
        GIT_TAG v0.3.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(maul-unicode)
endif()

# Every executable (tests, benchmarks, tools) lands in the top of the
# build tree, so scripts and CI find them in one place.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

include(GNUInstallDirs)
include(cmake/CompilerFlags.cmake)
include(cmake/Library.cmake)
include(cmake/Testing.cmake)

if(MAUL_LOCALE_BUILD_SHARED)
    set(MAUL_LIBRARY_KIND SHARED)
else()
    set(MAUL_LIBRARY_KIND STATIC)
endif()
add_library(maul-locale ${MAUL_LIBRARY_KIND}
    src/base.c
    src/data.c
    src/data_kinds.c
    src/date_data.c
    src/date_format.c
    src/date_write.c
    src/datetime_format.c
    src/time_format.c
    src/time_write.c
    src/decimal.c
    src/decimal_round.c
    src/decimal_shortest.c
    src/list.c
    src/list_data.c
    src/list_join.c
    src/locale_canonical.c
    src/locale_context.c
    src/locale_distance.c
    src/locale_extensions.c
    src/locale_fallback.c
    src/locale_keyword.c
    src/locale_likely.c
    src/locale_lsr.c
    src/locale_match.c
    src/locale_parse.c
    src/locale_prefs.c
    src/calendar.c
    src/calendar_add.c
    src/calendar_arith.c
    src/calendar_fields.c
    src/calendar_astro.c
    src/calendar_chinese.c
    src/calendar_lunar.c
    src/calendar_hebrew.c
    src/calendar_round.c
    src/calendar_umalqura.c
    src/calendar_until.c
    src/calendar_year.c
    src/names.c
    src/names_data.c
    src/names_locale.c
    src/names_text.c
    src/currency.c
    src/currency_region.c
    src/number.c
    src/number_compact.c
    src/number_core.c
    src/number_data.c
    src/number_format.c
    src/number_parse.c
    src/number_precision.c
    src/parse_currency.c
    src/parse_match.c
    src/plural.c
    src/spellout.c
    src/spellout_data.c
    src/spellout_format.c
    src/spellout_match.c
    src/spellout_parse.c
    src/spellout_value.c
    src/unit.c
    src/unit_convert.c
    src/unit_data.c
    src/unit_compound.c
    src/unit_coordinate.c
    src/unit_format.c
    src/unit_gender.c
    src/unit_id.c
    src/unit_mixed.c
    src/unit_mixed_format.c
    src/unit_names.c
    src/unit_normal.c
    src/unit_patterns.c
    src/unit_prefs.c
    src/unit_usage.c
    src/wide.c
    src/date_time.c
    src/datetime_arith.c
    src/instant_arith.c
    src/zoned_arith.c
    src/binary64.c
    src/duration_values.c
    src/ixdtf_parse.c
    src/ixdtf_write.c
    src/duration_round.c
    src/rounding.c
    src/time_arith.c
    src/zone.c
    src/zone_code_tables.c
    src/zone_name_tables.c
    src/zone_format.c
    src/zone_codes.c
    src/zone_handle.c
    src/zone_local.c
    src/zone_rules.c
    src/zone_tables.c
    src/generated/builtin_data.c
    src/generated/plural_rules.c
    src/generated/calendar_eras.c
    src/generated/calendar_years.c)
maul_configure_library(maul-locale)

if(MAUL_LOCALE_BUILD_TESTS)
    enable_testing()
    maul_add_test(base)
    maul_add_test(data WHITEBOX)
    maul_add_test(locale)
    maul_add_test(likely)
    maul_add_test(likely_tables WHITEBOX)
    maul_add_test(canonical)
    maul_add_test(alias_tables WHITEBOX)
    maul_add_test(parents)
    maul_add_test(fallback_units WHITEBOX)
    maul_add_test(match)
    maul_add_test(match_tables WHITEBOX)
    maul_add_test(preferences)
    maul_add_test(preference_tables WHITEBOX)
    maul_add_test(decimal)
    maul_add_test(plural)
    maul_add_test(plural_tables WHITEBOX)
    # The currency names are a data file of their own (record mloc-0022).
    maul_add_test(currency MANUAL)
    set(MLOC_NAMES_FILE ${PROJECT_SOURCE_DIR}/src/generated/currency-names.mloc)
    if(EMSCRIPTEN)
        # Node sees no host file: the file goes into the program's own.
        target_link_options(test_currency PRIVATE "SHELL:--embed-file ${MLOC_NAMES_FILE}@/names.mloc")
        set(MLOC_NAMES_FILE /names.mloc)
    endif()
    if(WIN32)
        target_compile_definitions(test_currency PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_compile_definitions(test_currency PRIVATE MLOC_NAMES_FILE="${MLOC_NAMES_FILE}")
    add_test(NAME currency COMMAND test_currency)
    maul_add_test(currency_parse MANUAL)
    if(EMSCRIPTEN)
        target_link_options(test_currency_parse PRIVATE "SHELL:--embed-file ${PROJECT_SOURCE_DIR}/src/generated/currency-names.mloc@/names.mloc")
    endif()
    if(WIN32)
        target_compile_definitions(test_currency_parse PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_compile_definitions(test_currency_parse PRIVATE MLOC_NAMES_FILE="${MLOC_NAMES_FILE}")
    add_test(NAME currency_parse COMMAND test_currency_parse)
    maul_add_test(number)
    maul_add_test(number_parse)
    maul_add_test(parse_patterns WHITEBOX)
    maul_add_test(currency_tables WHITEBOX)
    maul_add_test(number_tables WHITEBOX)
    maul_add_test(unit)
    maul_add_test(unit_tables WHITEBOX)
    maul_add_test(unit_names WHITEBOX)
    # So are the unit names (record mloc-0024).
    maul_add_test(unit_format MANUAL)
    # It builds names of its own with the data layout's constants.
    target_include_directories(test_unit_format PRIVATE ${PROJECT_SOURCE_DIR}/src)
    set(MLOC_UNITS_FILE ${PROJECT_SOURCE_DIR}/src/generated/units.mloc)
    if(EMSCRIPTEN)
        target_link_options(test_unit_format PRIVATE "SHELL:--embed-file ${MLOC_UNITS_FILE}@/units.mloc")
        set(MLOC_UNITS_FILE /units.mloc)
    endif()
    if(WIN32)
        target_compile_definitions(test_unit_format PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_compile_definitions(test_unit_format PRIVATE MLOC_UNITS_FILE="${MLOC_UNITS_FILE}")
    add_test(NAME unit_format COMMAND test_unit_format)
    if(TARGET test_unit_names)
        if(EMSCRIPTEN)
            target_link_options(test_unit_names PRIVATE "SHELL:--embed-file ${PROJECT_SOURCE_DIR}/src/generated/units.mloc@/units.mloc")
        endif()
        if(WIN32)
            target_compile_definitions(test_unit_names PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_unit_names PRIVATE MLOC_UNITS_FILE="${MLOC_UNITS_FILE}")
    endif()
    # So are the spell-out rules (record mloc-0034).
    maul_add_test(spellout MANUAL)
    set(MLOC_SPELLOUT_FILE ${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc)
    if(EMSCRIPTEN)
        target_link_options(test_spellout PRIVATE "SHELL:--embed-file ${MLOC_SPELLOUT_FILE}@/spellout.mloc")
        # Rules nested 64 levels deep need more than Emscripten's 64 KB
        # of stack (record mloc-0034).
        target_link_options(test_spellout PRIVATE "-sSTACK_SIZE=262144")
        set(MLOC_SPELLOUT_FILE /spellout.mloc)
    endif()
    if(WIN32)
        target_compile_definitions(test_spellout PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_compile_definitions(test_spellout PRIVATE MLOC_SPELLOUT_FILE="${MLOC_SPELLOUT_FILE}")
    add_test(NAME spellout COMMAND test_spellout)
    maul_add_test(wide WHITEBOX)
    maul_add_test(list WHITEBOX)
    maul_add_test(calendar)
    maul_add_test(calendar_astro WHITEBOX)
    maul_add_test(calendar_lunar WHITEBOX)
    maul_add_test(zone_rules WHITEBOX)
    maul_add_test(zone)
    maul_add_test(zone_files WHITEBOX)
    maul_add_test(zone_codes WHITEBOX)
    maul_add_test(zone_test262)
    maul_add_test(zone_local)
    maul_add_test(time_arith)
    maul_add_test(time_test262)
    maul_add_test(datetime_test262)
    maul_add_test(datetime_arith)
    maul_add_test(instant_test262)
    maul_add_test(instant_arith)
    maul_add_test(zoned_test262)
    maul_add_test(zoned_arith)
    maul_add_test(duration_test262)
    maul_add_test(duration_arith)
    maul_add_test(ixdtf)
    maul_add_test(ixdtf_test262)
    maul_add_test(calendar_year WHITEBOX)
    maul_add_test(calendar_arithmetic)
    maul_add_test(calendar_fields)
    maul_add_test(calendar_oracle)
    maul_add_test(calendar_test262)
    maul_add_test(list_format)
    # So are the names of languages and the rest (record mloc-0036), and
    # the subdivision names (record mloc-0039).
    set(MLOC_LOCALE_NAMES_FILE ${PROJECT_SOURCE_DIR}/src/generated/names.mloc)
    set(MLOC_LOCALE_NAMES_PATH ${MLOC_LOCALE_NAMES_FILE})
    set(MLOC_SUBDIVISIONS_FILE ${PROJECT_SOURCE_DIR}/src/generated/subdivisions.mloc)
    set(MLOC_SUBDIVISIONS_PATH ${MLOC_SUBDIVISIONS_FILE})
    if(EMSCRIPTEN)
        set(MLOC_LOCALE_NAMES_PATH /locale-names.mloc)
        set(MLOC_SUBDIVISIONS_PATH /subdivisions.mloc)
    endif()
    maul_add_test(names_data WHITEBOX)
    maul_add_test(names MANUAL)
    add_test(NAME names COMMAND test_names)
    foreach(target test_names_data test_names)
        if(TARGET ${target})
            if(EMSCRIPTEN)
                target_link_options(${target} PRIVATE "SHELL:--embed-file ${MLOC_LOCALE_NAMES_FILE}@/locale-names.mloc"
                                    "SHELL:--embed-file ${MLOC_SUBDIVISIONS_FILE}@/subdivisions.mloc")
            endif()
            if(WIN32)
                target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
            endif()
            target_compile_definitions(${target} PRIVATE MLOC_LOCALE_NAMES_FILE="${MLOC_LOCALE_NAMES_PATH}"
                                       MLOC_SUBDIVISIONS_FILE="${MLOC_SUBDIVISIONS_PATH}")
        endif()
    endforeach()
    # And the date patterns (record mloc-0042).
    set(MLOC_DATES_FILE ${PROJECT_SOURCE_DIR}/src/generated/dates.mloc)
    set(MLOC_DATES_PATH ${MLOC_DATES_FILE})
    if(EMSCRIPTEN)
        set(MLOC_DATES_PATH /dates.mloc)
    endif()
    maul_add_test(date_data WHITEBOX)
    if(TARGET test_date_data)
        if(EMSCRIPTEN)
            target_link_options(test_date_data PRIVATE "SHELL:--embed-file ${MLOC_DATES_FILE}@/dates.mloc")
        endif()
        if(WIN32)
            target_compile_definitions(test_date_data PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_date_data PRIVATE MLOC_DATES_FILE="${MLOC_DATES_PATH}")
    endif()
    # And the zone names (record mloc-0042).
    set(MLOC_ZONE_NAMES_FILE ${PROJECT_SOURCE_DIR}/src/generated/zone-names.mloc)
    set(MLOC_ZONE_NAMES_PATH ${MLOC_ZONE_NAMES_FILE})
    if(EMSCRIPTEN)
        set(MLOC_ZONE_NAMES_PATH /zone-names.mloc)
    endif()
    maul_add_test(zone_names WHITEBOX)
    if(TARGET test_zone_names)
        if(EMSCRIPTEN)
            target_link_options(test_zone_names PRIVATE
                                "SHELL:--embed-file ${MLOC_ZONE_NAMES_FILE}@/zone-names.mloc")
        endif()
        if(WIN32)
            target_compile_definitions(test_zone_names PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_zone_names PRIVATE MLOC_ZONE_NAMES_FILE="${MLOC_ZONE_NAMES_PATH}")
    endif()
    # The zone formatter reads the zone names and the region names, and
    # the dates for a date or time in a zone.
    maul_add_test(zone_format)
    if(TARGET test_zone_format)
        if(EMSCRIPTEN)
            target_link_options(test_zone_format PRIVATE
                                "SHELL:--embed-file ${MLOC_ZONE_NAMES_FILE}@/zone-names.mloc"
                                "SHELL:--embed-file ${MLOC_LOCALE_NAMES_FILE}@/locale-names.mloc"
                                "SHELL:--embed-file ${MLOC_DATES_FILE}@/dates.mloc")
        endif()
        target_compile_definitions(test_zone_format PRIVATE MLOC_ZONE_NAMES_FILE="${MLOC_ZONE_NAMES_PATH}"
                                   MLOC_LOCALE_NAMES_FILE="${MLOC_LOCALE_NAMES_PATH}"
                                   MLOC_DATES_FILE="${MLOC_DATES_PATH}")
    endif()
    # The date formatter reads the dates and the spell-out rules.
    maul_add_test(date_format)
    if(TARGET test_date_format)
        set(MLOC_DATE_SPELLOUT_PATH ${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc)
        if(EMSCRIPTEN)
            target_link_options(test_date_format PRIVATE "SHELL:--embed-file ${MLOC_DATES_FILE}@/dates.mloc"
                                "SHELL:--embed-file ${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc@/spellout.mloc"
                                "-sSTACK_SIZE=262144")
            set(MLOC_DATE_SPELLOUT_PATH /spellout.mloc)
        endif()
        target_compile_definitions(test_date_format PRIVATE MLOC_DATES_FILE="${MLOC_DATES_PATH}"
                                   MLOC_SPELLOUT_FILE="${MLOC_DATE_SPELLOUT_PATH}")
    endif()
    # The time formatter reads the dates.
    maul_add_test(time_format)
    if(TARGET test_time_format)
        if(EMSCRIPTEN)
            target_link_options(test_time_format PRIVATE "SHELL:--embed-file ${MLOC_DATES_FILE}@/dates.mloc")
        endif()
        target_compile_definitions(test_time_format PRIVATE MLOC_DATES_FILE="${MLOC_DATES_PATH}")
    endif()
    # CLDR's conformance files and ICU's matcher test, which the cldr CI
    # job runs on the pinned releases; built here so that every build
    # compiles them.
    foreach(name likely canonical parents match numbers plurals preferences currency units unit_prefs
                 spellout names locale_names)
        add_executable(conform_${name} test/conform_${name}.c)
        target_link_libraries(conform_${name} PRIVATE maul-locale)
        maul_apply_flags(conform_${name})
        # It reads its file with standard fopen, which the Windows C
        # runtime reports as deprecated.
        if(WIN32)
            target_compile_definitions(conform_${name} PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
    endforeach()
    # The zones against zic and zdump (tools/check_zones.py), on the
    # library's internal tables.
    if(NOT MAUL_LOCALE_BUILD_SHARED)
        add_executable(conform_zones test/conform_zones.c)
        target_link_libraries(conform_zones PRIVATE maul-locale)
        target_include_directories(conform_zones PRIVATE src)
        maul_apply_flags(conform_zones)
    endif()
    # Dates formatted against CLDR's datetime conformance data
    # (tools/check_datetime.py).
    add_executable(conform_datetime test/conform_datetime.c)
    target_link_libraries(conform_datetime PRIVATE maul-locale)
    maul_apply_flags(conform_datetime)
    # BCP 47 ids and Windows names against CLDR's files
    # (tools/check_zone_codes.py).
    add_executable(conform_zone_codes test/conform_zone_codes.c)
    target_link_libraries(conform_zone_codes PRIVATE maul-locale)
    maul_apply_flags(conform_zone_codes)
    # Local times against zoneinfo (tools/check_local.py), through the
    # public functions.
    add_executable(conform_local test/conform_local.c)
    target_link_libraries(conform_local PRIVATE maul-locale)
    maul_apply_flags(conform_local)
    # It reads its questions with standard sscanf, which the Windows C
    # runtime reports as deprecated.
    if(WIN32)
        target_compile_definitions(conform_local PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    # Zoned arithmetic against a reading of ECMA-262 over zoneinfo
    # (tools/check_zoned.py), through the public functions.
    # Temporal's strings against a reading of ECMA-262's grammar
    # (tools/check_ixdtf.py), through the public parsers.
    add_executable(conform_ixdtf test/conform_ixdtf.c)
    target_link_libraries(conform_ixdtf PRIVATE maul-locale)
    maul_apply_flags(conform_ixdtf)
    if(WIN32)
        target_compile_definitions(conform_ixdtf PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    add_executable(conform_zoned test/conform_zoned.c)
    target_link_libraries(conform_zoned PRIVATE maul-locale)
    maul_apply_flags(conform_zoned)
    if(WIN32)
        target_compile_definitions(conform_zoned PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    maul_add_header_tests()
endif()

# The fuzz targets: test/fuzz_<area>.c, run as fuzz_<area> <corpus dir>.
# They reach internal functions, so the library is static.
if(MAUL_LOCALE_FUZZ)
    # The library is instrumented too: its coverage guides libFuzzer, and
    # the sanitizers watch its code, not only the targets'.
    target_compile_options(maul-locale PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
    target_link_options(maul-locale PUBLIC -fsanitize=address,undefined)
    foreach(area data date_data decimal ixdtf locale parse spellout zone_codes zone_names zones)
        add_executable(fuzz_${area} test/fuzz_${area}.c)
        target_link_libraries(fuzz_${area} PRIVATE maul-locale)
        target_include_directories(fuzz_${area} PRIVATE src)
        maul_apply_flags(fuzz_${area})
        target_compile_options(fuzz_${area} PRIVATE -fsanitize=fuzzer,address,undefined)
        target_link_options(fuzz_${area} PRIVATE -fsanitize=fuzzer,address,undefined)
    endforeach()
    target_compile_definitions(fuzz_parse PRIVATE
        MLOC_NAMES_FILE="${PROJECT_SOURCE_DIR}/src/generated/currency-names.mloc")
    target_compile_definitions(fuzz_spellout PRIVATE
        MLOC_SPELLOUT_FILE="${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc")
endif()

# The generator, mlocgen, writes the library's data from CLDR's XML. Its
# parts are a static library so that their tests link them alone.
if(MAUL_LOCALE_BUILD_TOOLS)
    add_library(mlocgen_core STATIC tools/mlocgen/alias_gen.c tools/mlocgen/calendar_gen.c
                                    tools/mlocgen/cldr.c
                                    tools/mlocgen/context_gen.c
                                    tools/mlocgen/currency_gen.c tools/mlocgen/datafile.c
                                    tools/mlocgen/dtd.c tools/mlocgen/extension_gen.c
                                    tools/mlocgen/generate.c tools/mlocgen/keys.c
                                    tools/mlocgen/ldml.c tools/mlocgen/list_gen.c tools/mlocgen/locale_gen.c
                                    tools/mlocgen/map.c tools/mlocgen/names_gen.c
                                    tools/mlocgen/match_gen.c tools/mlocgen/match_regions.c
                                    tools/mlocgen/number_gen.c tools/mlocgen/parse_gen.c tools/mlocgen/table_gen.c
                                    tools/mlocgen/parents_gen.c tools/mlocgen/plural_gen.c
                                    tools/mlocgen/prefs_gen.c tools/mlocgen/spellout_compile.c
                                    tools/mlocgen/spellout_gen.c tools/mlocgen/spellout_lead.c tools/mlocgen/titlecase_gen.c
                                    tools/mlocgen/unit_gen.c
                                    tools/mlocgen/unit_names_gen.c tools/mlocgen/unit_names_model.c
                                    tools/mlocgen/unit_derivations_gen.c tools/mlocgen/unit_patterns_gen.c
                                    tools/mlocgen/tzdb.c tools/mlocgen/zone_compile.c tools/mlocgen/zone_gen.c
                                    tools/mlocgen/zone_codes_gen.c tools/mlocgen/zone_ids.c tools/mlocgen/zone_names_gen.c
                                    tools/mlocgen/date_gen.c tools/mlocgen/date_names_gen.c tools/mlocgen/date_source.c tools/mlocgen/day_periods_gen.c tools/mlocgen/interval_gen.c tools/mlocgen/interval_match.c tools/mlocgen/semantic.c tools/mlocgen/time_gen.c tools/mlocgen/skeleton.c tools/mlocgen/xml.c src/calendar_arith.c src/calendar_astro.c
                                    src/calendar_chinese.c src/wide.c)
    # The generator writes the layout src/data_format.h defines and reads
    # the versions of the public base header.
    target_include_directories(mlocgen_core PUBLIC tools/mlocgen src include)
    # The generator reads CLDR's ids with the library's own parser.
    target_link_libraries(mlocgen_core PUBLIC maul-locale maul-unicode)
    maul_apply_flags(mlocgen_core)
    # The generator uses standard fopen, which the Windows C runtime
    # reports as deprecated in favor of its own fopen_s.
    if(WIN32)
        target_compile_definitions(mlocgen_core PUBLIC _CRT_SECURE_NO_WARNINGS)
    endif()
    # The rule compiler checks a rule's exponent with ICU's logarithms.
    if(UNIX)
        target_link_libraries(mlocgen_core PUBLIC m)
    endif()
    add_executable(mlocgen tools/mlocgen/main.c)
    target_link_libraries(mlocgen PRIVATE mlocgen_core)
    maul_apply_flags(mlocgen)
    if(MAUL_LOCALE_BUILD_TESTS)
        add_executable(test_genxml test/test_genxml.c)
        target_link_libraries(test_genxml PRIVATE mlocgen_core)
        target_include_directories(test_genxml PRIVATE test)
        maul_apply_flags(test_genxml)
        add_test(NAME genxml COMMAND test_genxml)
        add_executable(test_genresolve test/test_genresolve.c)
        target_link_libraries(test_genresolve PRIVATE mlocgen_core)
        target_include_directories(test_genresolve PRIVATE test)
        maul_apply_flags(test_genresolve)
        add_test(NAME genresolve COMMAND test_genresolve ${PROJECT_SOURCE_DIR}/test/cldr/common)
        # The writer against the library's reader, which a shared build
        # hides.
        if(NOT MAUL_LOCALE_BUILD_SHARED)
            add_executable(test_genspellout test/test_genspellout.c)
            target_link_libraries(test_genspellout PRIVATE mlocgen_core maul-locale)
            target_include_directories(test_genspellout PRIVATE test src)
            maul_apply_flags(test_genspellout)
            add_test(NAME genspellout COMMAND test_genspellout)
            add_executable(test_gendata test/test_gendata.c)
            target_link_libraries(test_gendata PRIVATE mlocgen_core maul-locale)
            target_include_directories(test_gendata PRIVATE test src)
            maul_apply_flags(test_gendata)
            add_test(NAME gendata COMMAND test_gendata)
            add_executable(test_gencalendar test/test_gencalendar.c)
            target_link_libraries(test_gencalendar PRIVATE mlocgen_core maul-locale)
            target_include_directories(test_gencalendar PRIVATE test src)
            maul_apply_flags(test_gencalendar)
            add_test(NAME gencalendar COMMAND test_gencalendar ${PROJECT_SOURCE_DIR})
        endif()
    endif()
endif()

if(MAUL_LOCALE_BUILD_BENCH AND EXISTS ${PROJECT_SOURCE_DIR}/bench/bench_main.c)
    add_subdirectory(bench)
endif()

if(MAUL_LOCALE_INSTALL)
    include(cmake/Install.cmake)
endif()
