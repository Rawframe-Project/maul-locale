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

// The library version. CMake reads it from here, and release-please
// moves it between the markers.
// x-release-please-start-major
#define MLOC_VERSION_MAJOR 0
// x-release-please-end
// x-release-please-start-minor
#define MLOC_VERSION_MINOR 4
// x-release-please-end
// x-release-please-start-patch
#define MLOC_VERSION_PATCH 0
// x-release-please-end

// The data format version: the layouts of the data files' tables, which a
// library reads. It changes only when one of them does, not with the
// library version, so data files stay valid across releases that keep the
// format. Bit 31 is set: the
// field once held a packed library version (major << 16 | minor << 8 |
// patch), which never sets it, so no older file can match a format.
#define MLOC_DATA_FORMAT_VERSION 0x80000006u

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
    // The errors from -1 to -31 mean the same, at the same values, in every
    // Maul library; the library's own errors count down from -32. The type
    // has a fixed width so that result structs have one layout in C and
    // C++.
    //
    // A call that returns an error leaves every output as it was, buffers
    // and single values alike, since no owner object of the library could
    // clear them. It writes only what the failure itself defines, which
    // each output says: an mlocOutput's length with mloc_errorCapacity,
    // the offset where a reading call stopped, a message's faults. An
    // outcome that is not an error (mloc_fellBack, mloc_withFallbacks,
    // mloc_done, mloc_needMoreText) writes the outputs it describes.
    typedef int32_t mlocResult;
    enum
    {
        // The call did what was asked.
        mloc_success = 0,
        // The call did what was asked with the data of a locale the asked one
        // falls back to.
        mloc_fellBack = 1,
        // The call wrote its result with fallbacks where parts of it
        // failed, which it reports apart (a message's expressions).
        mloc_withFallbacks = 2,
        // An iterator has reported its last result.
        mloc_done = 3,
        // An iterator needs the next piece of its text before it can
        // answer.
        mloc_needMoreText = 4,
        // An argument is invalid: a null pointer where one is required, an
        // object the library did not make, a value outside the range its
        // parameter's documentation gives.
        mloc_errorInvalid = -1,
        // The room is too small for the result: a caller's buffer or cache.
        // More room would succeed.
        mloc_errorCapacity = -2,
        // A valid data file of another data version: another CLDR,
        // Unicode, time zone or library release, or other code tables; or
        // a compiled message or catalog of another format version.
        mloc_errorVersion = -7,
        // Stored bytes are malformed: a data file, a compiled message or a
        // catalog with a wrong magic, or a table, offset, length or count
        // that does not hold.
        mloc_errorFormat = -8,
        // An argument, or its content, passes a fixed bound of the
        // library, published as an MLOC_MAX_* constant: more room would
        // not help.
        mloc_errorLimit = -9,
        // Text is not well-formed for what it was given as, such as a
        // locale id.
        mloc_errorSyntax = -32,
        // The data has no answer: no likely subtags for a language, or a
        // data set without the table asked for.
        mloc_errorNotFound = -33,
        // A part of the result failed and the call was asked to write
        // nothing then (a message formatter in strict mode).
        mloc_errorFallback = -34,
    };

    // The result of a call that reads text: the status and, on failure,
    // the byte offset where the fault starts; on success the offset is
    // the length read.
    typedef struct mlocTextResult
    {
        mlocResult status;
        size_t offset;
    } mlocTextResult;

    // Receives one part of a formatted text, such as a number's integer
    // digits: the part's kind (each formatting part names its own), and its
    // byte range in the output. It may call the library back, the call it is
    // inside of included, but not make again an object that call uses or write
    // its output.
    typedef void (*mlocPartFunction)(void* context, uint32_t part, size_t start, size_t length);

    // Where every formatting call writes its UTF-8 text. Set buffer and
    // capacity (a null buffer with capacity 0 measures); the call sets length
    // to the bytes written, or with mloc_errorCapacity to the bytes needed
    // (SIZE_MAX when they pass it), the buffer's contents then being
    // unspecified; any other error leaves length and buffer as they were.
    // No NUL is written. When
    // part is set, it is called for each part in order of start, after the
    // whole text is written and only on success.
    typedef struct mlocOutput
    {
        char* buffer;
        size_t capacity;
        size_t length;
        mlocPartFunction part;
        void* context;
    } mlocOutput;

    // A text's direction: a locale's (CLDR's character order), or a
    // message part's, which may be unknown.
    typedef enum mlocDirection
    {
        mloc_directionUnknown = 0,
        mloc_directionLeftToRight = 1,
        mloc_directionRightToLeft = 2,
    } mlocDirection;

    // Where a formatted text stands, which decides whether its first word
    // is capitalized (CLDR's context transforms): mid-sentence as the data
    // writes it; at the beginning of a sentence, titlecased; an item of a
    // UI list or menu, or alone (a label), titlecased when the locale's
    // data asks for it there.
    typedef enum mlocCapitalization
    {
        mloc_capitalizationNone = 0,
        mloc_capitalizationSentence = 1,
        mloc_capitalizationListOrMenu = 2,
        mloc_capitalizationStandAlone = 3,
    } mlocCapitalization;

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
    MLOC_API const char* mlocGetResultName(mlocResult result);

#ifdef __cplusplus
}
#endif

#endif
