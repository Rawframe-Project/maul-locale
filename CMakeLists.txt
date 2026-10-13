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
    src/decimal.c
    src/decimal_round.c
    src/decimal_shortest.c
    src/locale_canonical.c
    src/locale_distance.c
    src/locale_extensions.c
    src/locale_fallback.c
    src/locale_keyword.c
    src/locale_likely.c
    src/locale_lsr.c
    src/locale_match.c
    src/locale_parse.c
    src/locale_prefs.c
    src/currency.c
    src/currency_region.c
    src/number.c
    src/number_compact.c
    src/number_core.c
    src/number_data.c
    src/number_format.c
    src/number_precision.c
    src/plural.c
    src/unit.c
    src/unit_convert.c
    src/unit_data.c
    src/unit_compound.c
    src/unit_format.c
    src/unit_id.c
    src/unit_names.c
    src/unit_prefs.c
    src/wide.c
    src/generated/builtin_data.c
    src/generated/plural_rules.c)
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
    maul_add_test(number)
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
    maul_add_test(wide WHITEBOX)
    # CLDR's conformance files and ICU's matcher test, which the cldr CI
    # job runs on the pinned releases; built here so that every build
    # compiles them.
    foreach(name likely canonical parents match numbers plurals preferences currency units unit_prefs)
        add_executable(conform_${name} test/conform_${name}.c)
        target_link_libraries(conform_${name} PRIVATE maul-locale)
        maul_apply_flags(conform_${name})
        # It reads its file with standard fopen, which the Windows C
        # runtime reports as deprecated.
        if(WIN32)
            target_compile_definitions(conform_${name} PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
    endforeach()
    maul_add_header_tests()
endif()

# The fuzz targets: test/fuzz_<area>.c, run as fuzz_<area> <corpus dir>.
# They reach internal functions, so the library is static.
if(MAUL_LOCALE_FUZZ)
    # The library is instrumented too: its coverage guides libFuzzer, and
    # the sanitizers watch its code, not only the targets'.
    target_compile_options(maul-locale PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
    target_link_options(maul-locale PUBLIC -fsanitize=address,undefined)
    foreach(area data decimal locale)
        add_executable(fuzz_${area} test/fuzz_${area}.c)
        target_link_libraries(fuzz_${area} PRIVATE maul-locale)
        target_include_directories(fuzz_${area} PRIVATE src)
        maul_apply_flags(fuzz_${area})
        target_compile_options(fuzz_${area} PRIVATE -fsanitize=fuzzer,address,undefined)
        target_link_options(fuzz_${area} PRIVATE -fsanitize=fuzzer,address,undefined)
    endforeach()
endif()

# The generator, mlocgen, writes the library's data from CLDR's XML. Its
# parts are a static library so that their tests link them alone.
if(MAUL_LOCALE_BUILD_TOOLS)
    add_library(mlocgen_core STATIC tools/mlocgen/alias_gen.c tools/mlocgen/cldr.c
                                    tools/mlocgen/currency_gen.c tools/mlocgen/datafile.c
                                    tools/mlocgen/dtd.c tools/mlocgen/extension_gen.c
                                    tools/mlocgen/generate.c tools/mlocgen/keys.c
                                    tools/mlocgen/ldml.c tools/mlocgen/locale_gen.c tools/mlocgen/map.c
                                    tools/mlocgen/match_gen.c tools/mlocgen/match_regions.c
                                    tools/mlocgen/number_gen.c tools/mlocgen/table_gen.c
                                    tools/mlocgen/parents_gen.c tools/mlocgen/plural_gen.c
                                    tools/mlocgen/prefs_gen.c tools/mlocgen/unit_gen.c
                                    tools/mlocgen/unit_names_gen.c
                                    tools/mlocgen/xml.c src/wide.c)
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
            add_executable(test_gendata test/test_gendata.c)
            target_link_libraries(test_gendata PRIVATE mlocgen_core maul-locale)
            target_include_directories(test_gendata PRIVATE test src)
            maul_apply_flags(test_gendata)
            add_test(NAME gendata COMMAND test_gendata)
        endif()
    endif()
endif()

if(MAUL_LOCALE_BUILD_BENCH AND EXISTS ${PROJECT_SOURCE_DIR}/bench/bench_main.c)
    add_subdirectory(bench)
endif()

if(MAUL_LOCALE_INSTALL)
    include(cmake/Install.cmake)
endif()
