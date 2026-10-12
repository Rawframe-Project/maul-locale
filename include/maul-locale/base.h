// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The base of the Maul Locale API: the library and CLDR versions, the
// export and attribute macros, and the result codes every fallible
// function returns.

#ifndef MAUL_LOCALE_BASE_H
#define MAUL_LOCALE_BASE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// The library version. CMake reads it from here.
#define MLOC_VERSION_MAJOR 0
#define MLOC_VERSION_MINOR 0
#define MLOC_VERSION_PATCH 1

// The one CLDR release every table in the library comes from.
#define MLOC_CLDR_VERSION_MAJOR 49
#define MLOC_CLDR_VERSION_MINOR 0
#define MLOC_CLDR_VERSION_PATCH 0

// MLOC_API marks the public functions: dllexport or dllimport in a
// shared Windows build (maul_locale_EXPORTS is defined while building
// the library), default visibility in a shared build elsewhere.
#if defined(MAUL_LOCALE_SHARED) && defined(_WIN32)
#if defined(maul_locale_EXPORTS)
#define MLOC_API __declspec(dllexport) extern
#else
#define MLOC_API __declspec(dllimport) extern
#endif
#elif defined(MAUL_LOCALE_SHARED) && (defined(__GNUC__) || defined(__clang__))
#define MLOC_API __attribute__((visibility("default"))) extern
#else
#define MLOC_API extern
#endif

// MLOC_NODISCARD marks a function whose result must be read: every
// function that returns a status. The attribute is standard in C23 and
// C++17 and left out for older dialects. MSVC keeps __cplusplus at
// 199711L unless /Zc:__cplusplus is given, so _MSVC_LANG is read too.
#if (defined(__cplusplus) && __cplusplus >= 201703L) ||                                            \
    (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define MLOC_NODISCARD [[nodiscard]]
#elif !defined(__cplusplus) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MLOC_NODISCARD [[nodiscard]]
#else
#define MLOC_NODISCARD
#endif

    // The status a fallible function returns. Zero is success, positive
    // values are outcomes that are not errors, negative values are errors.
    // The type has a fixed width so that result structs have one layout in
    // C and C++.
    typedef int32_t mlocResult;
    enum
    {
        // The call did what was asked.
        mloc_success = 0,
        // An argument is invalid: a null pointer where one is required, a
        // value out of range.
        mloc_errorInvalid = -1,
        // A caller buffer or a named limit is too small for the result.
        mloc_errorCapacity = -2,
        // Bytes given as data are not a valid data file: a wrong magic or
        // format, or a table, offset or length that does not hold.
        mloc_errorData = -3,
        // A valid data file of another data version: another CLDR,
        // Unicode, time zone or library release, or other code tables.
        mloc_errorDataVersion = -4,
    };

    // A version: major, minor and patch.
    typedef struct mlocVersion
    {
        uint16_t major;
        uint16_t minor;
        uint16_t patch;
    } mlocVersion;

    /// Returns the version of the library that was linked, which may differ
    /// from the MLOC_VERSION macros a program was compiled with.
    ///
    /// @return The library version.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_API mlocVersion mlocGetVersion(void);

    /// Returns the CLDR release the library's code and data come from.
    ///
    /// @return The CLDR version.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_API mlocVersion mlocGetCldrVersion(void);

    /// Returns the name of a result code, for diagnostics.
    ///
    /// @param result  Any value; an unknown one is named as such.
    /// @return A static, NUL-terminated string such as "mloc_errorCapacity".
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_API const char* mlocResultName(mlocResult result);

#ifdef __cplusplus
}
#endif

#endif
