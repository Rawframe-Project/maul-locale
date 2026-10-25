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
option(MAUL_LOCALE_TEST_PACKAGE "Test the installed package by building samples/minimal against it" OFF)

# Maul Unicode: the message part calls it at run time (UTF-8, NFC and bidi
# classes), the generator for the currency symbols' letters and the unit
# names' case. An installed package serves; else the pinned release is
# fetched, before the family modules, so that ours, included after its
# copies, serve this project. FETCHCONTENT_SOURCE_DIR_MAUL-UNICODE builds
# a local copy instead, offline.
set(MAUL_LOCALE_UNICODE_VERSION 0.3.0)
find_package(maul-unicode ${MAUL_LOCALE_UNICODE_VERSION} CONFIG QUIET)
if(NOT maul-unicode_FOUND)
    include(FetchContent)
    set(MAUL_UNICODE_NORMALIZATION ON)
    set(MAUL_UNICODE_CASE ${MAUL_LOCALE_BUILD_TOOLS})
    set(MAUL_UNICODE_SECURITY OFF)
    set(MAUL_UNICODE_INSTALL ${MAUL_LOCALE_INSTALL})
    if(MAUL_LOCALE_BUILD_SHARED)
        # Its static library goes into ours.
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    endif()
    FetchContent_Declare(maul-unicode
        GIT_REPOSITORY https://github.com/Rawframe-Project/maul-unicode.git
        GIT_TAG v${MAUL_LOCALE_UNICODE_VERSION}
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

# A suite maul_add_test cannot make: one with arguments, or linked to the
# tools' libraries. Its name is its label, as maul_add_test gives, since
# tools/check.sh changed picks the suites a change reaches by label.
function(mloc_add_test name)
    add_test(NAME ${name} ${ARGN})
    set_tests_properties(${name} PROPERTIES LABELS ${name})
endfunction()

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
    src/date_range.c
    src/date_write.c
    src/datetime_format.c
    src/time_format.c
    src/time_write.c
    src/decimal.c
    src/decimal_round.c
    src/decimal_shortest.c
    src/grammar.c
    src/grammar_az.c
    src/grammar_kk.c
    src/grammar_ky.c
    src/grammar_lexicon.c
    src/grammar_tk.c
    src/grammar_fi.c
    src/grammar_fi_word.c
    src/grammar_ko.c
    src/grammar_hu.c
    src/grammar_hu_sound.c
    src/grammar_hu_words.c
    src/grammar_tr.c
    src/grammar_turkic.c
    src/grammar_uz.c
    src/list.c
    src/list_gender.c
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
    src/locale_direction.c
    src/locale_match.c
    src/locale_parse.c
    src/locale_prefs.c
    src/message.c
    src/message_bind.c
    src/message_access.c
    src/message_call.c
    src/message_check.c
    src/message_date.c
    src/message_duration.c
    src/message_emit.c
    src/message_format.c
    src/message_list.c
    src/message_grammar.c
    src/message_number.c
    src/message_offset.c
    src/message_operand.c
    src/message_parse.c
    src/message_prepare.c
    src/message_read.c
    src/message_resolve.c
    src/message_table.c
    src/message_text.c
    src/nfd_stream.c
    src/collation_index.c
    src/collation_tables.c
    src/collation_iterator.c
    src/search.c
    src/search_boundary.c
    src/search_back.c
    src/search_elements.c
    src/search_find.c
    src/search_previous.c
    src/index.c
    src/index_build.c
    src/index_labels.c
    src/collation_compare.c
    src/collation_fast_compare.c
    src/collation_reorder.c
    src/collate.c
    src/collation_key.c
    src/message_value.c
    src/message_words.c
    src/message_write.c
    src/catalog.c
    src/catalog_chain.c
    src/catalog_tables.c
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
    src/relative_tables.c
    src/segment_tables.c
    src/segment.c
    src/segment_filter.c
    src/relative_format.c
    src/relative_datetime.c
    src/duration_options.c
    src/duration_format.c
    src/duration_write.c
    src/duration_fields.c
    src/zone_format.c
    src/zone_codes.c
    src/zone_handle.c
    src/zone_local.c
    src/zone_rules.c
    src/zone_tables.c
    src/generated/builtin_data.c
    src/generated/plural_rules.c
    src/generated/calendar_eras.c
    src/generated/calendar_years.c
    src/generated/script_directions.c
    src/generated/list_genders.c)
maul_configure_library(maul-locale)
target_link_libraries(maul-locale PRIVATE maul-unicode::maul-unicode)
set(MAUL_PACKAGE_DEPENDENCIES "find_dependency(maul-unicode ${MAUL_LOCALE_UNICODE_VERSION})")
set(MAUL_PKG_REQUIRES_PRIVATE "maul-unicode >= ${MAUL_LOCALE_UNICODE_VERSION}")

if(MAUL_LOCALE_BUILD_TESTS)
    enable_testing()
    maul_add_test(base)
    maul_add_exports_test()
    # The snippets of docs/user/, on the compiled-in data and the data
    # files (tools/check_guide.py keeps them the same).
    maul_add_test(guide)
    if(TARGET test_guide)
        set(MLOC_GUIDE_DATA ${PROJECT_SOURCE_DIR}/src/generated)
        if(EMSCRIPTEN)
            # Every data file, about 19 MB, and the test's copy of each.
            foreach(file dates zone-names names subdivisions currency-names units spellout
                    relative collation segments)
                target_link_options(test_guide PRIVATE
                                    "SHELL:--embed-file ${MLOC_GUIDE_DATA}/${file}.mloc@/data/${file}.mloc")
            endforeach()
            target_link_options(test_guide PRIVATE "-sSTACK_SIZE=262144" "-sALLOW_MEMORY_GROWTH=1")
            set(MLOC_GUIDE_DATA /data)
        endif()
        if(WIN32)
            target_compile_definitions(test_guide PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_guide PRIVATE MLOC_DATA_DIR="${MLOC_GUIDE_DATA}")
    endif()
    maul_add_test(data WHITEBOX)
    maul_add_test(locale)
    maul_add_test(message WHITEBOX)
    # The suite's date cases read the dates.
    if(TARGET test_message)
        set(MLOC_MESSAGE_DATES ${PROJECT_SOURCE_DIR}/src/generated/dates.mloc)
        if(EMSCRIPTEN)
            target_link_options(test_message PRIVATE "SHELL:--embed-file ${MLOC_MESSAGE_DATES}@/dates.mloc")
            set(MLOC_MESSAGE_DATES /dates.mloc)
        endif()
        if(WIN32)
            target_compile_definitions(test_message PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_message PRIVATE MLOC_DATES_FILE="${MLOC_MESSAGE_DATES}")
    endif()
    # The NFD stream against Maul Unicode's NFD, and against its
    # NormalizationTest.txt when the sources were fetched. A fetched tree
    # without the file stops the configuration, so a move of the file in
    # Maul Unicode cannot silently drop the comparison.
    maul_add_test(nfd_stream WHITEBOX MANUAL)
    if(TARGET test_nfd_stream)
        target_link_libraries(test_nfd_stream PRIVATE maul-unicode::maul-unicode)
        if(maul-unicode_SOURCE_DIR AND NOT EMSCRIPTEN)
            set(MLOC_NORMALIZATION_TEST "${maul-unicode_SOURCE_DIR}/tools/ucd/NormalizationTest.txt")
            if(NOT EXISTS "${MLOC_NORMALIZATION_TEST}")
                message(FATAL_ERROR "Maul Unicode's sources lack ${MLOC_NORMALIZATION_TEST}, "
                                    "which the nfd_stream test compares against")
            endif()
            mloc_add_test(nfd_stream COMMAND test_nfd_stream "${MLOC_NORMALIZATION_TEST}")
        else()
            mloc_add_test(nfd_stream COMMAND test_nfd_stream)
        endif()
    endif()
    maul_add_test(message_functions)
    maul_add_test(message_parts)
    maul_add_test(message_words WHITEBOX)
    # It reads the spell-out rules, relative times and unit names; a
    # white-box test, so not built with the shared library.
    if(TARGET test_message_words)
        set(MLOC_WORDS_SPELLOUT ${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc)
        set(MLOC_WORDS_RELATIVE ${PROJECT_SOURCE_DIR}/src/generated/relative.mloc)
        set(MLOC_WORDS_UNITS ${PROJECT_SOURCE_DIR}/src/generated/units.mloc)
        if(EMSCRIPTEN)
            target_link_options(test_message_words PRIVATE "SHELL:--embed-file ${MLOC_WORDS_SPELLOUT}@/spellout.mloc"
                                "SHELL:--embed-file ${MLOC_WORDS_RELATIVE}@/relative.mloc"
                                "SHELL:--embed-file ${MLOC_WORDS_UNITS}@/units.mloc")
            set(MLOC_WORDS_SPELLOUT /spellout.mloc)
            set(MLOC_WORDS_RELATIVE /relative.mloc)
            set(MLOC_WORDS_UNITS /units.mloc)
        endif()
        if(WIN32)
            target_compile_definitions(test_message_words PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_message_words PRIVATE MLOC_SPELLOUT_FILE="${MLOC_WORDS_SPELLOUT}"
                                   MLOC_RELATIVE_FILE="${MLOC_WORDS_RELATIVE}"
                                   MLOC_UNITS_FILE="${MLOC_WORDS_UNITS}")
    endif()
    # They read the spell-out rules, which numbers are spoken by.
    foreach(test grammar grammar_turkic grammar_hu grammar_fi grammar_ko message_grammar)
        maul_add_test(${test})
        if(TARGET test_${test})
            set(MLOC_GRAMMAR_SPELLOUT ${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc)
            if(EMSCRIPTEN)
                target_link_options(test_${test} PRIVATE "SHELL:--embed-file ${MLOC_GRAMMAR_SPELLOUT}@/spellout.mloc")
                set(MLOC_GRAMMAR_SPELLOUT /spellout.mloc)
            endif()
            if(WIN32)
                target_compile_definitions(test_${test} PRIVATE _CRT_SECURE_NO_WARNINGS)
            endif()
            target_compile_definitions(test_${test} PRIVATE MLOC_SPELLOUT_FILE="${MLOC_GRAMMAR_SPELLOUT}")
        endif()
    endforeach()
    # It reads the dates for a date's parts.
    if(TARGET test_message_parts)
        set(MLOC_PARTS_DATES ${PROJECT_SOURCE_DIR}/src/generated/dates.mloc)
        if(EMSCRIPTEN)
            target_link_options(test_message_parts PRIVATE "SHELL:--embed-file ${MLOC_PARTS_DATES}@/dates.mloc")
            set(MLOC_PARTS_DATES /dates.mloc)
        endif()
        if(WIN32)
            target_compile_definitions(test_message_parts PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_message_parts PRIVATE MLOC_DATES_FILE="${MLOC_PARTS_DATES}")
    endif()
    maul_add_test(message_numbers WHITEBOX)
    maul_add_test(message_dates WHITEBOX)
    # It reads the dates; a white-box test, so not built with the shared
    # library.
    if(TARGET test_message_dates)
        set(MLOC_MESSAGE_DATES ${PROJECT_SOURCE_DIR}/src/generated/dates.mloc)
        set(MLOC_MESSAGE_ZONE_NAMES ${PROJECT_SOURCE_DIR}/src/generated/zone-names.mloc)
        set(MLOC_MESSAGE_LOCALE_NAMES ${PROJECT_SOURCE_DIR}/src/generated/names.mloc)
        if(EMSCRIPTEN)
            target_link_options(test_message_dates PRIVATE "SHELL:--embed-file ${MLOC_MESSAGE_DATES}@/dates.mloc"
                                "SHELL:--embed-file ${MLOC_MESSAGE_ZONE_NAMES}@/zone-names.mloc"
                                "SHELL:--embed-file ${MLOC_MESSAGE_LOCALE_NAMES}@/locale-names.mloc")
            set(MLOC_MESSAGE_DATES /dates.mloc)
            set(MLOC_MESSAGE_ZONE_NAMES /zone-names.mloc)
            set(MLOC_MESSAGE_LOCALE_NAMES /locale-names.mloc)
        endif()
        if(WIN32)
            target_compile_definitions(test_message_dates PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_message_dates PRIVATE MLOC_DATES_FILE="${MLOC_MESSAGE_DATES}"
                                   MLOC_ZONE_NAMES_FILE="${MLOC_MESSAGE_ZONE_NAMES}"
                                   MLOC_LOCALE_NAMES_FILE="${MLOC_MESSAGE_LOCALE_NAMES}")
    endif()
    # :unit reads the unit names; a white-box test, so not built with the
    # shared library.
    if(TARGET test_message_numbers)
        set(MLOC_MESSAGE_UNITS ${PROJECT_SOURCE_DIR}/src/generated/units.mloc)
        if(EMSCRIPTEN)
            target_link_options(test_message_numbers PRIVATE "SHELL:--embed-file ${MLOC_MESSAGE_UNITS}@/units.mloc")
            set(MLOC_MESSAGE_UNITS /units.mloc)
        endif()
        if(WIN32)
            target_compile_definitions(test_message_numbers PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_message_numbers PRIVATE MLOC_UNITS_FILE="${MLOC_MESSAGE_UNITS}")
    endif()
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
    # The currency names are a data file of their own.
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
    mloc_add_test(currency COMMAND test_currency)
    maul_add_test(currency_parse MANUAL)
    if(EMSCRIPTEN)
        target_link_options(test_currency_parse PRIVATE "SHELL:--embed-file ${PROJECT_SOURCE_DIR}/src/generated/currency-names.mloc@/names.mloc")
    endif()
    if(WIN32)
        target_compile_definitions(test_currency_parse PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_compile_definitions(test_currency_parse PRIVATE MLOC_NAMES_FILE="${MLOC_NAMES_FILE}")
    mloc_add_test(currency_parse COMMAND test_currency_parse)
    maul_add_test(number)
    maul_add_test(number_parse)
    maul_add_test(parse_patterns WHITEBOX)
    maul_add_test(currency_tables WHITEBOX)
    maul_add_test(number_tables WHITEBOX)
    maul_add_test(unit)
    maul_add_test(unit_tables WHITEBOX)
    maul_add_test(unit_names WHITEBOX)
    # So are the unit names.
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
    mloc_add_test(unit_format COMMAND test_unit_format)
    # Durations read the unit names; the test reads the digital pattern and
    # spoils a formatter's state.
    maul_add_test(duration MANUAL WHITEBOX)
    if(TARGET test_duration)
        if(EMSCRIPTEN)
            target_link_options(test_duration PRIVATE
                                "SHELL:--embed-file ${PROJECT_SOURCE_DIR}/src/generated/units.mloc@/units.mloc")
        endif()
        if(WIN32)
            target_compile_definitions(test_duration PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_duration PRIVATE MLOC_UNITS_FILE="${MLOC_UNITS_FILE}")
        mloc_add_test(duration COMMAND test_duration)
    endif()
    if(TARGET test_unit_names)
        if(EMSCRIPTEN)
            target_link_options(test_unit_names PRIVATE "SHELL:--embed-file ${PROJECT_SOURCE_DIR}/src/generated/units.mloc@/units.mloc")
        endif()
        if(WIN32)
            target_compile_definitions(test_unit_names PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_unit_names PRIVATE MLOC_UNITS_FILE="${MLOC_UNITS_FILE}")
    endif()
    # So are the spell-out rules.
    maul_add_test(spellout MANUAL)
    set(MLOC_SPELLOUT_FILE ${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc)
    if(EMSCRIPTEN)
        target_link_options(test_spellout PRIVATE "SHELL:--embed-file ${MLOC_SPELLOUT_FILE}@/spellout.mloc")
        # Rules nested 64 levels deep need more than Emscripten's 64 KB of
        # stack.
        target_link_options(test_spellout PRIVATE "-sSTACK_SIZE=262144")
        set(MLOC_SPELLOUT_FILE /spellout.mloc)
    endif()
    if(WIN32)
        target_compile_definitions(test_spellout PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_compile_definitions(test_spellout PRIVATE MLOC_SPELLOUT_FILE="${MLOC_SPELLOUT_FILE}")
    mloc_add_test(spellout COMMAND test_spellout)
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
    maul_add_test(list_gender)
    maul_add_test(turkic_letters WHITEBOX)
    # So are the names of languages and the rest, and the subdivision names.
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
    mloc_add_test(names COMMAND test_names)
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
    # And the date patterns.
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
    # And the zone names.
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
    # The collation data file.
    set(MLOC_COLLATION_FILE ${PROJECT_SOURCE_DIR}/src/generated/collation.mloc)
    set(MLOC_COLLATION_PATH ${MLOC_COLLATION_FILE})
    if(EMSCRIPTEN)
        set(MLOC_COLLATION_PATH /collation.mloc)
    endif()
    maul_add_test(collation_data WHITEBOX)
    if(TARGET test_collation_data)
        target_link_libraries(test_collation_data PRIVATE maul-unicode::maul-unicode)
        if(EMSCRIPTEN)
            target_link_options(test_collation_data PRIVATE
                                "SHELL:--embed-file ${MLOC_COLLATION_FILE}@/collation.mloc")
        endif()
        target_compile_definitions(test_collation_data PRIVATE MLOC_COLLATION_FILE="${MLOC_COLLATION_PATH}")
    endif()
    maul_add_test(collation_fast_latin WHITEBOX)
    if(TARGET test_collation_fast_latin)
        if(EMSCRIPTEN)
            target_link_options(test_collation_fast_latin PRIVATE
                                "SHELL:--embed-file ${MLOC_COLLATION_FILE}@/collation.mloc")
        endif()
        target_compile_definitions(test_collation_fast_latin PRIVATE
                                   MLOC_COLLATION_FILE="${MLOC_COLLATION_PATH}")
    endif()
    maul_add_test(collation_fast_compare WHITEBOX)
    if(TARGET test_collation_fast_compare)
        if(EMSCRIPTEN)
            target_link_options(test_collation_fast_compare PRIVATE
                                "SHELL:--embed-file ${MLOC_COLLATION_FILE}@/collation.mloc")
        endif()
        target_compile_definitions(test_collation_fast_compare PRIVATE
                                   MLOC_COLLATION_FILE="${MLOC_COLLATION_PATH}")
    endif()
    maul_add_test(collation_compare WHITEBOX)
    if(TARGET test_collation_compare)
        if(EMSCRIPTEN)
            target_link_options(test_collation_compare PRIVATE
                                "SHELL:--embed-file ${MLOC_COLLATION_FILE}@/collation.mloc")
        endif()
        target_compile_definitions(test_collation_compare PRIVATE MLOC_COLLATION_FILE="${MLOC_COLLATION_PATH}")
    endif()
    maul_add_test(collation_key WHITEBOX)
    if(TARGET test_collation_key)
        if(EMSCRIPTEN)
            target_link_options(test_collation_key PRIVATE
                                "SHELL:--embed-file ${MLOC_COLLATION_FILE}@/collation.mloc")
        endif()
        target_compile_definitions(test_collation_key PRIVATE MLOC_COLLATION_FILE="${MLOC_COLLATION_PATH}")
    endif()
    # The public collator over the same file.
    maul_add_test(collate)
    if(TARGET test_collate)
        if(EMSCRIPTEN)
            target_link_options(test_collate PRIVATE
                                "SHELL:--embed-file ${MLOC_COLLATION_FILE}@/collation.mloc")
        endif()
        target_compile_definitions(test_collate PRIVATE MLOC_COLLATION_FILE="${MLOC_COLLATION_PATH}")
    endif()
    # Search over the same file.
    maul_add_test(search)
    if(TARGET test_search)
        if(EMSCRIPTEN)
            target_link_options(test_search PRIVATE
                                "SHELL:--embed-file ${MLOC_COLLATION_FILE}@/collation.mloc")
        endif()
        target_compile_definitions(test_search PRIVATE MLOC_COLLATION_FILE="${MLOC_COLLATION_PATH}")
    endif()
    # The alphabetic index over the same file.
    maul_add_test(index)
    if(TARGET test_index)
        if(EMSCRIPTEN)
            target_link_options(test_index PRIVATE
                                "SHELL:--embed-file ${MLOC_COLLATION_FILE}@/collation.mloc")
        endif()
        target_compile_definitions(test_index PRIVATE MLOC_COLLATION_FILE="${MLOC_COLLATION_PATH}")
    endif()
    # And relative time.
    set(MLOC_RELATIVE_FILE ${PROJECT_SOURCE_DIR}/src/generated/relative.mloc)
    set(MLOC_RELATIVE_PATH ${MLOC_RELATIVE_FILE})
    if(EMSCRIPTEN)
        set(MLOC_RELATIVE_PATH /relative.mloc)
    endif()
    maul_add_test(relative_tables WHITEBOX)
    if(TARGET test_relative_tables)
        if(EMSCRIPTEN)
            target_link_options(test_relative_tables PRIVATE
                                "SHELL:--embed-file ${MLOC_RELATIVE_FILE}@/relative.mloc")
        endif()
        if(WIN32)
            target_compile_definitions(test_relative_tables PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_relative_tables PRIVATE MLOC_RELATIVE_FILE="${MLOC_RELATIVE_PATH}")
    endif()
    # And the segmentation tailorings.
    set(MLOC_SEGMENTS_FILE ${PROJECT_SOURCE_DIR}/src/generated/segments.mloc)
    set(MLOC_SEGMENTS_PATH ${MLOC_SEGMENTS_FILE})
    if(EMSCRIPTEN)
        set(MLOC_SEGMENTS_PATH /segments.mloc)
    endif()
    maul_add_test(segment_tables WHITEBOX)
    if(TARGET test_segment_tables)
        if(EMSCRIPTEN)
            target_link_options(test_segment_tables PRIVATE
                                "SHELL:--embed-file ${MLOC_SEGMENTS_FILE}@/segments.mloc")
        endif()
        if(WIN32)
            target_compile_definitions(test_segment_tables PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_segment_tables PRIVATE MLOC_SEGMENTS_FILE="${MLOC_SEGMENTS_PATH}")
    endif()
    maul_add_test(segment)
    if(TARGET test_segment)
        if(EMSCRIPTEN)
            target_link_options(test_segment PRIVATE
                                "SHELL:--embed-file ${MLOC_SEGMENTS_FILE}@/segments.mloc")
        endif()
        if(WIN32)
            target_compile_definitions(test_segment PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_segment PRIVATE MLOC_SEGMENTS_FILE="${MLOC_SEGMENTS_PATH}")
        # ICU4C's answers (tools/oracle/segment/) and the listed departures.
        set(MLOC_SEGMENT_ICU ${PROJECT_SOURCE_DIR}/test/segment_icu.tsv)
        set(MLOC_SEGMENT_EXCEPTIONS ${PROJECT_SOURCE_DIR}/tools/segment-exceptions.txt)
        if(EMSCRIPTEN)
            target_link_options(test_segment PRIVATE
                                "SHELL:--embed-file ${MLOC_SEGMENT_ICU}@/segment_icu.tsv"
                                "SHELL:--embed-file ${MLOC_SEGMENT_EXCEPTIONS}@/segment-exceptions.txt")
            target_compile_definitions(test_segment PRIVATE
                                       MLOC_SEGMENT_ICU_FILE="/segment_icu.tsv"
                                       MLOC_SEGMENT_EXCEPTIONS_FILE="/segment-exceptions.txt")
        else()
            target_compile_definitions(test_segment PRIVATE
                                       MLOC_SEGMENT_ICU_FILE="${MLOC_SEGMENT_ICU}"
                                       MLOC_SEGMENT_EXCEPTIONS_FILE="${MLOC_SEGMENT_EXCEPTIONS}")
        endif()
    endif()
    maul_add_test(relative)
    if(TARGET test_relative)
        if(EMSCRIPTEN)
            target_link_options(test_relative PRIVATE
                                "SHELL:--embed-file ${MLOC_RELATIVE_FILE}@/relative.mloc"
                                "SHELL:--embed-file ${MLOC_DATES_FILE}@/dates.mloc")
        endif()
        if(WIN32)
            target_compile_definitions(test_relative PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_compile_definitions(test_relative PRIVATE MLOC_RELATIVE_FILE="${MLOC_RELATIVE_PATH}"
                                                    MLOC_DATES_FILE="${MLOC_DATES_PATH}")
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
    # The date ranges read the dates, the spell-out rules and, for a
    # formatter with a zone, the zone names.
    maul_add_test(date_range)
    if(TARGET test_date_range)
        set(MLOC_RANGE_SPELLOUT_PATH ${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc)
        set(MLOC_DATE_RANGES_PATH ${PROJECT_SOURCE_DIR}/test/date_ranges_icu.tsv)
        set(MLOC_TIME_RANGES_PATH ${PROJECT_SOURCE_DIR}/test/time_ranges_icu.tsv)
        set(MLOC_DATETIME_RANGES_PATH ${PROJECT_SOURCE_DIR}/test/datetime_ranges_icu.tsv)
        if(EMSCRIPTEN)
            target_link_options(test_date_range PRIVATE "SHELL:--embed-file ${MLOC_DATES_FILE}@/dates.mloc"
                                "SHELL:--embed-file ${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc@/spellout.mloc"
                                "SHELL:--embed-file ${MLOC_ZONE_NAMES_FILE}@/zone-names.mloc"
                                "SHELL:--embed-file ${PROJECT_SOURCE_DIR}/test/date_ranges_icu.tsv@/date_ranges_icu.tsv"
                                "SHELL:--embed-file ${PROJECT_SOURCE_DIR}/test/time_ranges_icu.tsv@/time_ranges_icu.tsv"
                                "SHELL:--embed-file ${PROJECT_SOURCE_DIR}/test/datetime_ranges_icu.tsv@/datetime_ranges_icu.tsv"
                                "-sSTACK_SIZE=262144")
            set(MLOC_RANGE_SPELLOUT_PATH /spellout.mloc)
            set(MLOC_DATE_RANGES_PATH /date_ranges_icu.tsv)
            set(MLOC_TIME_RANGES_PATH /time_ranges_icu.tsv)
            set(MLOC_DATETIME_RANGES_PATH /datetime_ranges_icu.tsv)
        endif()
        target_compile_definitions(test_date_range PRIVATE MLOC_DATES_FILE="${MLOC_DATES_PATH}"
                                   MLOC_SPELLOUT_FILE="${MLOC_RANGE_SPELLOUT_PATH}"
                                   MLOC_ZONE_NAMES_FILE="${MLOC_ZONE_NAMES_PATH}"
                                   MLOC_DATE_RANGES_FILE="${MLOC_DATE_RANGES_PATH}"
                                   MLOC_TIME_RANGES_FILE="${MLOC_TIME_RANGES_PATH}"
                                   MLOC_DATETIME_RANGES_FILE="${MLOC_DATETIME_RANGES_PATH}")
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
    foreach(name likely canonical parents direction match numbers plurals preferences currency units
                 unit_prefs
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
    # The zones against zic and zdump (tools/check_zones.py), on the library's
    # internal tables. The general compare against CLDR's CollationTest files,
    # on the library's internal compare.
    if(NOT MAUL_LOCALE_BUILD_SHARED)
        add_executable(conform_collation test/conform_collation.c)
        target_link_libraries(conform_collation PRIVATE maul-locale)
        target_include_directories(conform_collation PRIVATE src)
        maul_apply_flags(conform_collation)
        add_executable(conform_index test/conform_index.c)
        target_link_libraries(conform_index PRIVATE maul-locale maul-unicode)
        maul_apply_flags(conform_index)
        add_executable(conform_index_labels test/conform_index_labels.c)
        target_link_libraries(conform_index_labels PRIVATE maul-locale maul-unicode)
        target_include_directories(conform_index_labels PRIVATE src)
        maul_apply_flags(conform_index_labels)
        add_executable(conform_search test/conform_search.c)
        target_link_libraries(conform_search PRIVATE maul-locale)
        maul_apply_flags(conform_search)
        add_executable(conform_search_elements test/conform_search_elements.c)
        target_link_libraries(conform_search_elements PRIVATE maul-locale)
        target_include_directories(conform_search_elements PRIVATE src)
        maul_apply_flags(conform_search_elements)
        add_executable(conform_collation_options test/conform_collation_options.c)
        target_link_libraries(conform_collation_options PRIVATE maul-locale)
        target_include_directories(conform_collation_options PRIVATE src)
        maul_apply_flags(conform_collation_options)
        add_executable(conform_collation_fast test/conform_collation_fast.c)
        target_link_libraries(conform_collation_fast PRIVATE maul-locale)
        target_include_directories(conform_collation_fast PRIVATE src)
        maul_apply_flags(conform_collation_fast)
        add_executable(conform_collation_reorder test/conform_collation_reorder.c)
        target_link_libraries(conform_collation_reorder PRIVATE maul-locale)
        target_include_directories(conform_collation_reorder PRIVATE src)
        maul_apply_flags(conform_collation_reorder)
    endif()
    # Tailored locales' public sort keys against ICU4C, which the cldr CI job
    # runs on the builder's queries.
    add_executable(conform_collation_keys test/conform_collation_keys.c)
    target_link_libraries(conform_collation_keys PRIVATE maul-locale)
    maul_apply_flags(conform_collation_keys)
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
    # Installs the build into the binary tree and builds and runs
    # samples/minimal against it, as a consumer of the package would.
    if(MAUL_LOCALE_TEST_PACKAGE AND MAUL_LOCALE_INSTALL)
        add_test(NAME package
                 COMMAND ${CMAKE_COMMAND} -DBINARY_DIR=${PROJECT_BINARY_DIR}
                         -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -DGENERATOR=${CMAKE_GENERATOR}
                         -DC_COMPILER=${CMAKE_C_COMPILER} -P ${PROJECT_SOURCE_DIR}/cmake/PackageTest.cmake)
        set_tests_properties(package PROPERTIES LABELS package)
    endif()
endif()

# The catalog tool's parts but its compiler, which also needs the
# generator's data file writer: mloccat_core and the fuzz targets of its
# readers build them.
set(MLOCCAT_SOURCES tools/mloccat/accept.c tools/mloccat/catalog_set.c
                    tools/mloccat/exchange_po.c tools/mloccat/file.c tools/mloccat/json.c
                    tools/mloccat/message_text.c tools/mloccat/mf2_parts.c tools/mloccat/pgs.c
                    tools/mloccat/po.c tools/mloccat/pseudo.c tools/mloccat/report.c
                    tools/mloccat/report_write.c tools/mloccat/resource.c
                    tools/mloccat/resource_check.c tools/mloccat/resource_edit.c
                    tools/mloccat/resource_write.c tools/mloccat/stale.c
                    tools/mloccat/xliff_codes.c tools/mloccat/xliff_pgs.c
                    tools/mloccat/xliff_read.c tools/mloccat/xliff_write.c tools/mloccat/table.c)

# The fuzz targets: test/fuzz_<area>.c, run as fuzz_<area> <corpus dir>.
# They reach internal functions, so the library is static.
if(MAUL_LOCALE_FUZZ)
    # The library is instrumented too: its coverage guides libFuzzer, and
    # the sanitizers watch its code, not only the targets'. Undefined
    # behavior aborts the run, so libFuzzer keeps the input as a crash
    # instead of printing a warning and going on.
    target_compile_options(maul-locale PRIVATE -fsanitize=fuzzer-no-link,address,undefined
                                               -fno-sanitize-recover=all)
    target_link_options(maul-locale PUBLIC -fsanitize=address,undefined)
    foreach(area catalog collate_fast collation collation_data data date_data decimal duration grammar ixdtf locale message parse
                 relative
                 segment segments
                 spellout
                 zone_codes zone_names zones)
        add_executable(fuzz_${area} test/fuzz_${area}.c)
        target_link_libraries(fuzz_${area} PRIVATE maul-locale)
        target_include_directories(fuzz_${area} PRIVATE src)
        maul_apply_flags(fuzz_${area})
        target_compile_options(fuzz_${area} PRIVATE -fsanitize=fuzzer,address,undefined
                                               -fno-sanitize-recover=all)
        target_link_options(fuzz_${area} PRIVATE -fsanitize=fuzzer,address,undefined)
    endforeach()
    target_compile_definitions(fuzz_collation PRIVATE
        MLOC_COLLATION_FILE="${PROJECT_SOURCE_DIR}/src/generated/collation.mloc")
    target_compile_definitions(fuzz_collate_fast PRIVATE
        MLOC_COLLATION_FILE="${PROJECT_SOURCE_DIR}/src/generated/collation.mloc")
    target_compile_definitions(fuzz_collation_data PRIVATE
        MLOC_COLLATION_FILE="${PROJECT_SOURCE_DIR}/src/generated/collation.mloc")
    target_compile_definitions(fuzz_segment PRIVATE
        MLOC_SEGMENTS_FILE="${PROJECT_SOURCE_DIR}/src/generated/segments.mloc")
    target_compile_definitions(fuzz_parse PRIVATE
        MLOC_NAMES_FILE="${PROJECT_SOURCE_DIR}/src/generated/currency-names.mloc")
    target_compile_definitions(fuzz_spellout PRIVATE
        MLOC_SPELLOUT_FILE="${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc")
    target_compile_definitions(fuzz_grammar PRIVATE
        MLOC_SPELLOUT_FILE="${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc")
    target_compile_definitions(fuzz_duration PRIVATE
        MLOC_UNITS_FILE="${PROJECT_SOURCE_DIR}/src/generated/units.mloc")
    # The catalog tool's resource reader, built from its sources since the
    # fuzz build leaves the tools out.
    add_executable(fuzz_resource test/fuzz_resource.c tools/mloccat/resource.c
                                 tools/mloccat/resource_check.c tools/mloccat/resource_write.c)
    target_link_libraries(fuzz_resource PRIVATE maul-locale maul-unicode)
    target_include_directories(fuzz_resource PRIVATE tools/mloccat)
    maul_apply_flags(fuzz_resource)
    target_compile_options(fuzz_resource PRIVATE -fsanitize=fuzzer,address,undefined
                                               -fno-sanitize-recover=all)
    target_link_options(fuzz_resource PRIVATE -fsanitize=fuzzer,address,undefined)
    # The tool's exchange readers (PO, XLIFF with the generator's XML reader,
    # selection messages), from its parts instrumented as the library is.
    add_library(mloccat_fuzz STATIC ${MLOCCAT_SOURCES} tools/mlocgen/xml.c)
    target_include_directories(mloccat_fuzz PUBLIC tools/mloccat tools/mlocgen src)
    target_link_libraries(mloccat_fuzz PUBLIC maul-locale maul-unicode)
    maul_apply_flags(mloccat_fuzz)
    target_compile_options(mloccat_fuzz PRIVATE -fsanitize=fuzzer-no-link,address,undefined
                                                -fno-sanitize-recover=all)
    foreach(area po pgs xliff)
        add_executable(fuzz_${area} test/fuzz_${area}.c)
        target_link_libraries(fuzz_${area} PRIVATE mloccat_fuzz)
        maul_apply_flags(fuzz_${area})
        target_compile_options(fuzz_${area} PRIVATE -fsanitize=fuzzer,address,undefined
                                                -fno-sanitize-recover=all)
        target_link_options(fuzz_${area} PRIVATE -fsanitize=fuzzer,address,undefined)
    endforeach()
    # The generator's collation rule parser, from its source likewise.
    add_executable(fuzz_collation_rules test/fuzz_collation_rules.c
                                        tools/mlocgen/collation_rules.c)
    target_link_libraries(fuzz_collation_rules PRIVATE maul-unicode)
    target_include_directories(fuzz_collation_rules PRIVATE tools/mlocgen)
    maul_apply_flags(fuzz_collation_rules)
    target_compile_options(fuzz_collation_rules PRIVATE -fsanitize=fuzzer,address,undefined
                                               -fno-sanitize-recover=all)
    target_link_options(fuzz_collation_rules PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

# The generator, mlocgen, writes the library's data from CLDR's XML. Its
# parts are a static library so that their tests link them alone.
if(MAUL_LOCALE_BUILD_TOOLS)
    add_library(mlocgen_core STATIC tools/mlocgen/alias_gen.c tools/mlocgen/calendar_gen.c
                                    tools/mlocgen/collation_gen.c tools/mlocgen/collation_root.c
                                    tools/mlocgen/collation_builder.c
                                    tools/mlocgen/collation_catalog.c
                                    tools/mlocgen/collation_encode.c
                                    tools/mlocgen/collation_fast_table.c
                                    tools/mlocgen/collation_unsafe_table.c
                                    tools/mlocgen/collation_mappings.c
                                    tools/mlocgen/collation_root_elements.c
                                    tools/mlocgen/collation_rules.c
                                    tools/mlocgen/collation_tailoring.c
                                    tools/mlocgen/collation_unit.c
                                    tools/mlocgen/collation_weights.c
                                    tools/mlocgen/direction_gen.c
                                    tools/mlocgen/gender_gen.c
                                    tools/mlocgen/cldr.c
                                    tools/mlocgen/context_gen.c
                                    tools/mlocgen/currency_gen.c tools/mlocgen/datafile.c
                                    tools/mlocgen/dtd.c tools/mlocgen/extension_gen.c
                                    tools/mlocgen/generate.c tools/mlocgen/keys.c
                                    tools/mlocgen/ldml.c tools/mlocgen/list_gen.c tools/mlocgen/locale_gen.c
                                    tools/mlocgen/map.c tools/mlocgen/names_gen.c
                                    tools/mlocgen/match_gen.c tools/mlocgen/match_regions.c
                                    tools/mlocgen/number_gen.c tools/mlocgen/parse_gen.c
                                    tools/mlocgen/set_pattern.c tools/mlocgen/table_gen.c
                                    tools/mlocgen/index_gen.c
                                    tools/mlocgen/trie_gen.c
                                    tools/mlocgen/parents_gen.c tools/mlocgen/plural_gen.c
                                    tools/mlocgen/prefs_gen.c tools/mlocgen/spellout_compile.c
                                    tools/mlocgen/spellout_gen.c tools/mlocgen/spellout_lead.c tools/mlocgen/titlecase_gen.c
                                    tools/mlocgen/unit_gen.c
                                    tools/mlocgen/unit_names_gen.c tools/mlocgen/unit_names_model.c
                                    tools/mlocgen/unit_derivations_gen.c tools/mlocgen/unit_patterns_gen.c
                                    tools/mlocgen/tzdb.c tools/mlocgen/zone_compile.c tools/mlocgen/zone_gen.c
                                    tools/mlocgen/zone_codes_gen.c tools/mlocgen/zone_ids.c tools/mlocgen/zone_names_gen.c
                                    tools/mlocgen/relative_gen.c
                                    tools/mlocgen/segment_gen.c
                                    tools/mlocgen/date_gen.c tools/mlocgen/date_names_gen.c tools/mlocgen/date_source.c tools/mlocgen/day_periods_gen.c tools/mlocgen/interval_gen.c tools/mlocgen/interval_match.c tools/mlocgen/semantic.c tools/mlocgen/time_gen.c tools/mlocgen/skeleton.c tools/mlocgen/xml.c src/calendar_arith.c src/calendar_astro.c
                                    src/calendar_chinese.c src/wide.c
                                    # The reordering stored with each tailoring, made as a
                                    # collator makes it.
                                    src/collation_reorder.c)
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
    # The catalog tool, mloccat: its parts are a static library so that their
    # tests link them alone.
    add_library(mloccat_core STATIC ${MLOCCAT_SOURCES} tools/mloccat/compile.c)
    # The compiler writes the data layout and the catalog units the library
    # reads (src/data_format.h, src/catalog_tables.h), with the generator's
    # data file writer.
    target_include_directories(mloccat_core PUBLIC tools/mloccat src)
    target_link_libraries(mloccat_core PUBLIC mlocgen_core maul-locale maul-unicode)
    maul_apply_flags(mloccat_core)
    if(WIN32)
        target_compile_definitions(mloccat_core PUBLIC _CRT_SECURE_NO_WARNINGS)
    endif()
    add_executable(mloccat tools/mloccat/main.c)
    target_link_libraries(mloccat PRIVATE mloccat_core)
    maul_apply_flags(mloccat)
    if(MAUL_LOCALE_BUILD_TESTS)
        # Catalogs compiled by the tool and read by the library, in a
        # directory of the build.
        add_executable(test_catalog test/test_catalog.c)
        target_link_libraries(test_catalog PRIVATE mloccat_core)
        target_include_directories(test_catalog PRIVATE test)
        maul_apply_flags(test_catalog)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/catalog-test)
        mloc_add_test(catalog COMMAND test_catalog ${CMAKE_CURRENT_BINARY_DIR}/catalog-test)
        # The catalog snippet of docs/user/catalogs.md, with the tool's
        # compiler.
        add_executable(test_guide_catalog test/test_guide_catalog.c)
        target_link_libraries(test_guide_catalog PRIVATE mloccat_core)
        target_include_directories(test_guide_catalog PRIVATE test)
        maul_apply_flags(test_guide_catalog)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guide-catalog-test)
        mloc_add_test(guide_catalog COMMAND test_guide_catalog ${CMAKE_CURRENT_BINARY_DIR}/guide-catalog-test)
        add_executable(test_accept test/test_accept.c)
        target_link_libraries(test_accept PRIVATE mloccat_core)
        target_include_directories(test_accept PRIVATE test)
        maul_apply_flags(test_accept)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/accept-test)
        mloc_add_test(accept COMMAND test_accept ${CMAKE_CURRENT_BINARY_DIR}/accept-test)
        add_executable(test_report test/test_report.c)
        target_link_libraries(test_report PRIVATE mloccat_core)
        target_include_directories(test_report PRIVATE test)
        maul_apply_flags(test_report)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/report-test)
        mloc_add_test(report COMMAND test_report ${CMAKE_CURRENT_BINARY_DIR}/report-test)
        add_executable(test_pseudo test/test_pseudo.c)
        target_link_libraries(test_pseudo PRIVATE mloccat_core)
        target_include_directories(test_pseudo PRIVATE test)
        maul_apply_flags(test_pseudo)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pseudo-test)
        mloc_add_test(pseudo COMMAND test_pseudo ${CMAKE_CURRENT_BINARY_DIR}/pseudo-test)
        add_executable(test_po test/test_po.c)
        target_link_libraries(test_po PRIVATE mloccat_core)
        target_include_directories(test_po PRIVATE test)
        maul_apply_flags(test_po)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/po-test)
        mloc_add_test(po COMMAND test_po ${CMAKE_CURRENT_BINARY_DIR}/po-test)
        add_executable(test_xliff test/test_xliff.c)
        target_link_libraries(test_xliff PRIVATE mloccat_core)
        target_include_directories(test_xliff PRIVATE test)
        maul_apply_flags(test_xliff)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/xliff-test)
        mloc_add_test(xliff COMMAND test_xliff ${CMAKE_CURRENT_BINARY_DIR}/xliff-test)
        add_executable(test_catalog_chain test/test_catalog_chain.c)
        target_link_libraries(test_catalog_chain PRIVATE mloccat_core)
        target_include_directories(test_catalog_chain PRIVATE test)
        maul_apply_flags(test_catalog_chain)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/catalog-chain-test)
        mloc_add_test(catalog_chain COMMAND test_catalog_chain
                 ${CMAKE_CURRENT_BINARY_DIR}/catalog-chain-test)
        # The units' validators alone, which a shared build hides.
        if(NOT MAUL_LOCALE_BUILD_SHARED)
            add_executable(test_catalog_tables test/test_catalog_tables.c)
            target_link_libraries(test_catalog_tables PRIVATE mloccat_core)
            target_include_directories(test_catalog_tables PRIVATE test)
            maul_apply_flags(test_catalog_tables)
            mloc_add_test(catalog_tables COMMAND test_catalog_tables)
        endif()
        add_executable(test_resource test/test_resource.c)
        target_link_libraries(test_resource PRIVATE mloccat_core)
        target_include_directories(test_resource PRIVATE test)
        maul_apply_flags(test_resource)
        mloc_add_test(resource COMMAND test_resource)
        add_executable(test_genxml test/test_genxml.c)
        target_link_libraries(test_genxml PRIVATE mlocgen_core)
        target_include_directories(test_genxml PRIVATE test)
        maul_apply_flags(test_genxml)
        mloc_add_test(genxml COMMAND test_genxml)
        add_executable(test_collation_rules test/test_collation_rules.c)
        target_link_libraries(test_collation_rules PRIVATE mlocgen_core)
        target_include_directories(test_collation_rules PRIVATE test)
        maul_apply_flags(test_collation_rules)
        mloc_add_test(collation_rules COMMAND test_collation_rules)
        add_executable(test_collation_weights test/test_collation_weights.c)
        target_link_libraries(test_collation_weights PRIVATE mlocgen_core)
        target_include_directories(test_collation_weights PRIVATE test)
        maul_apply_flags(test_collation_weights)
        mloc_add_test(collation_weights COMMAND test_collation_weights)
        add_executable(test_collation_builder test/test_collation_builder.c)
        target_link_libraries(test_collation_builder PRIVATE mlocgen_core)
        target_include_directories(test_collation_builder PRIVATE test)
        maul_apply_flags(test_collation_builder)
        mloc_add_test(collation_builder COMMAND test_collation_builder
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        add_executable(test_collation_mappings test/test_collation_mappings.c)
        target_link_libraries(test_collation_mappings PRIVATE mlocgen_core)
        target_include_directories(test_collation_mappings PRIVATE test)
        maul_apply_flags(test_collation_mappings)
        mloc_add_test(collation_mappings COMMAND test_collation_mappings
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        add_executable(test_collation_root_elements test/test_collation_root_elements.c)
        target_link_libraries(test_collation_root_elements PRIVATE mlocgen_core)
        target_include_directories(test_collation_root_elements PRIVATE test)
        maul_apply_flags(test_collation_root_elements)
        mloc_add_test(collation_root_elements COMMAND test_collation_root_elements
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        add_executable(test_collation_fast_table test/test_collation_fast_table.c)
        target_link_libraries(test_collation_fast_table PRIVATE mlocgen_core)
        target_include_directories(test_collation_fast_table PRIVATE test)
        maul_apply_flags(test_collation_fast_table)
        mloc_add_test(collation_fast_table COMMAND test_collation_fast_table
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        add_executable(test_collation_unsafe_table test/test_collation_unsafe_table.c)
        target_link_libraries(test_collation_unsafe_table PRIVATE mlocgen_core)
        target_include_directories(test_collation_unsafe_table PRIVATE test)
        maul_apply_flags(test_collation_unsafe_table)
        mloc_add_test(collation_unsafe_table COMMAND test_collation_unsafe_table
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        add_executable(test_gencollation test/test_gencollation.c)
        target_link_libraries(test_gencollation PRIVATE mlocgen_core)
        target_include_directories(test_gencollation PRIVATE test)
        maul_apply_flags(test_gencollation)
        mloc_add_test(gencollation COMMAND test_gencollation
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        # The root against allkeys_CLDR.txt of the same release, and the
        # element iterator against the root, which the cldr CI job runs;
        # the iterator is internal, which a shared build hides.
        if(NOT MAUL_LOCALE_BUILD_SHARED)
            add_executable(conform_collation_root test/conform_collation_root.c)
            target_link_libraries(conform_collation_root PRIVATE mlocgen_core)
            maul_apply_flags(conform_collation_root)
            add_executable(conform_collation_builder test/conform_collation_builder.c)
            target_link_libraries(conform_collation_builder PRIVATE mlocgen_core)
            maul_apply_flags(conform_collation_builder)
            add_executable(conform_collation_fast_builder test/conform_collation_fast_builder.c)
            target_link_libraries(conform_collation_fast_builder PRIVATE mlocgen_core)
            maul_apply_flags(conform_collation_fast_builder)
            add_executable(conform_collation_mappings test/conform_collation_mappings.c)
            target_link_libraries(conform_collation_mappings PRIVATE mlocgen_core)
            maul_apply_flags(conform_collation_mappings)
            add_executable(conform_collation_root_elements test/conform_collation_root_elements.c)
            target_link_libraries(conform_collation_root_elements PRIVATE mlocgen_core)
            maul_apply_flags(conform_collation_root_elements)
            add_executable(conform_collation_rules test/conform_collation_rules.c)
            target_link_libraries(conform_collation_rules PRIVATE mlocgen_core)
            maul_apply_flags(conform_collation_rules)
            add_executable(test_collation_tailoring test/test_collation_tailoring.c)
            target_link_libraries(test_collation_tailoring PRIVATE mlocgen_core)
            target_include_directories(test_collation_tailoring PRIVATE test)
            maul_apply_flags(test_collation_tailoring)
            mloc_add_test(collation_tailoring COMMAND test_collation_tailoring
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
            add_executable(test_collation_index test/test_collation_index.c)
            target_link_libraries(test_collation_index PRIVATE mlocgen_core)
            target_include_directories(test_collation_index PRIVATE test)
            maul_apply_flags(test_collation_index)
            target_compile_definitions(test_collation_index PRIVATE
                                       MLOC_COLLATION_FILE="${MLOC_COLLATION_FILE}")
            mloc_add_test(collation_index COMMAND test_collation_index
                     ${PROJECT_SOURCE_DIR}/test/cldr/common)
            add_executable(test_collation_iterator test/test_collation_iterator.c)
            target_link_libraries(test_collation_iterator PRIVATE mlocgen_core)
            target_include_directories(test_collation_iterator PRIVATE test)
            maul_apply_flags(test_collation_iterator)
            mloc_add_test(collation_iterator COMMAND test_collation_iterator
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
            add_executable(test_search_elements test/test_search_elements.c)
            target_link_libraries(test_search_elements PRIVATE mlocgen_core)
            target_include_directories(test_search_elements PRIVATE test)
            maul_apply_flags(test_search_elements)
            mloc_add_test(search_elements COMMAND test_search_elements
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
            add_executable(test_search_boundary test/test_search_boundary.c)
            target_link_libraries(test_search_boundary PRIVATE mlocgen_core)
            target_include_directories(test_search_boundary PRIVATE test)
            maul_apply_flags(test_search_boundary)
            mloc_add_test(search_boundary COMMAND test_search_boundary
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        endif()
        add_executable(test_trie test/test_trie.c)
        target_link_libraries(test_trie PRIVATE mlocgen_core)
        target_include_directories(test_trie PRIVATE test)
        maul_apply_flags(test_trie)
        mloc_add_test(trie COMMAND test_trie)
        add_executable(test_genresolve test/test_genresolve.c)
        target_link_libraries(test_genresolve PRIVATE mlocgen_core)
        target_include_directories(test_genresolve PRIVATE test)
        maul_apply_flags(test_genresolve)
        mloc_add_test(genresolve COMMAND test_genresolve ${PROJECT_SOURCE_DIR}/test/cldr/common)
        # The writer against the library's reader, which a shared build
        # hides.
        if(NOT MAUL_LOCALE_BUILD_SHARED)
            add_executable(test_genspellout test/test_genspellout.c)
            target_link_libraries(test_genspellout PRIVATE mlocgen_core maul-locale)
            target_include_directories(test_genspellout PRIVATE test src)
            maul_apply_flags(test_genspellout)
            mloc_add_test(genspellout COMMAND test_genspellout)
            add_executable(test_gendata test/test_gendata.c)
            target_link_libraries(test_gendata PRIVATE mlocgen_core maul-locale)
            target_include_directories(test_gendata PRIVATE test src)
            maul_apply_flags(test_gendata)
            mloc_add_test(gendata COMMAND test_gendata)
            add_executable(test_gencalendar test/test_gencalendar.c)
            target_link_libraries(test_gencalendar PRIVATE mlocgen_core maul-locale)
            target_include_directories(test_gencalendar PRIVATE test src)
            maul_apply_flags(test_gencalendar)
            mloc_add_test(gencalendar COMMAND test_gencalendar ${PROJECT_SOURCE_DIR})
            add_executable(test_gendirection test/test_gendirection.c)
            target_link_libraries(test_gendirection PRIVATE mlocgen_core maul-locale)
            target_include_directories(test_gendirection PRIVATE test src)
            maul_apply_flags(test_gendirection)
            mloc_add_test(gendirection COMMAND test_gendirection ${PROJECT_SOURCE_DIR})
            add_executable(test_gensegments test/test_gensegments.c)
            target_link_libraries(test_gensegments PRIVATE mlocgen_core maul-locale)
            target_include_directories(test_gensegments PRIVATE test src)
            maul_apply_flags(test_gensegments)
            mloc_add_test(gensegments COMMAND test_gensegments)
        endif()
    endif()
endif()

# The generator and every conformance program, which tools/check_cldr.sh
# runs against the pinned CLDR and tzdb releases.
if(TARGET mlocgen)
    get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    list(FILTER targets INCLUDE REGEX "^conform_")
    add_custom_target(conformance)
    add_dependencies(conformance mlocgen ${targets})
endif()

# Every suite carries its name as a label: tools/check.sh changed runs the
# suites a change reaches by label, so one without it would never run there.
get_property(suites DIRECTORY PROPERTY TESTS)
foreach(suite IN LISTS suites)
    get_test_property(${suite} LABELS labels)
    if(NOT suite IN_LIST labels)
        message(FATAL_ERROR "the test ${suite} is not labeled ${suite}, so tools/check.sh changed "
                            "would never run it: register it with maul_add_test or mloc_add_test")
    endif()
endforeach()

if(MAUL_LOCALE_BUILD_BENCH AND EXISTS ${PROJECT_SOURCE_DIR}/bench/bench_main.c)
    add_subdirectory(bench)
endif()

if(MAUL_LOCALE_INSTALL)
    include(cmake/Install.cmake)
endif()
