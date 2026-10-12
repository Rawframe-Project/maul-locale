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
    src/base.c)
maul_configure_library(maul-locale)

if(MAUL_LOCALE_BUILD_TESTS)
    enable_testing()
    maul_add_test(base)
    maul_add_header_tests()
endif()

# The generator, mlocgen, writes the library's data from CLDR's XML. Its
# parts are a static library so that their tests link them alone.
if(MAUL_LOCALE_BUILD_TOOLS)
    add_library(mlocgen_core STATIC tools/mlocgen/cldr.c tools/mlocgen/dtd.c
                                    tools/mlocgen/ldml.c tools/mlocgen/map.c tools/mlocgen/xml.c)
    target_include_directories(mlocgen_core PUBLIC tools/mlocgen)
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
    endif()
endif()

if(MAUL_LOCALE_BUILD_BENCH AND EXISTS ${PROJECT_SOURCE_DIR}/bench/bench_main.c)
    add_subdirectory(bench)
endif()

if(MAUL_LOCALE_INSTALL)
    include(cmake/Install.cmake)
endif()
