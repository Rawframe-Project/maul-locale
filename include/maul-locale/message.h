// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Messages (record mloc-0045): Unicode MessageFormat 2 (UTS #35 Part 9)
// compiled once into caller memory, bound once to a locale, and
// formatted many times. A compiled message is locale-free: it holds the
// message's declarations, selectors, variants and patterns with their
// names resolved, keys and names in NFC, and every syntax and data
// model check done. It is the form a catalog stores; it is valid only
// for this library's version.

#ifndef MAUL_LOCALE_MESSAGE_H
#define MAUL_LOCALE_MESSAGE_H

#include "maul-locale/base.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Why a message did not compile: a syntax error (the text is not
    // well-formed MF2, or not well-formed UTF-8), or one of the data
    // model errors of UTS #35 Part 9.
    typedef enum mlocMessageError
    {
        mloc_messageErrorNone = 0,
        mloc_messageErrorSyntax = 1,
        // A variant has another number of keys than there are selectors.
        mloc_messageErrorVariantKeyMismatch = 2,
        // No variant has only catch-all keys.
        mloc_messageErrorMissingFallbackVariant = 3,
        // A selector does not lead, directly or through local
        // declarations, to a declaration with a function.
        mloc_messageErrorMissingSelectorAnnotation = 4,
        // A variable is declared twice, after it was used in an earlier
        // declaration, or by a declaration that uses it.
        mloc_messageErrorDuplicateDeclaration = 5,
        // An expression or markup names an option twice.
        mloc_messageErrorDuplicateOptionName = 6,
        // Two variants have the same keys.
        mloc_messageErrorDuplicateVariant = 7,
    } mlocMessageError;

    /// Compiles a message from its MF2 text into caller memory. Call it
    /// with a null buffer and a capacity of 0 to learn the capacity
    /// needed. Syntax errors, a variant's key count and a missing
    /// fallback variant are found while measuring; the data model errors
    /// that compare names or keys need the compiled form, so they are
    /// found only once the buffer is large enough.
    ///
    /// @param text      The message, UTF-8. May be null when length is 0.
    /// @param length    The number of bytes.
    /// @param buffer    Receives the compiled message; any alignment.
    /// @param capacity  The buffer's size in bytes.
    /// @param sizeOut   Receives the compiled message's size on success,
    ///                  or the capacity needed with mloc_errorCapacity,
    ///                  which may be a little more than the size.
    /// @param errorOut  Receives what failed with mloc_errorSyntax, else
    ///                  mloc_messageErrorNone; may be null.
    /// @return mloc_success and the length read; mloc_errorSyntax and the
    ///         byte offset where the error is found (also for a name or key
    ///         with a run of more than 30 combining marks, which UAX #15's
    ///         stream-safe form bounds and NFC here does not take); mloc_errorCapacity
    ///         when the buffer is too small, nothing then being written
    ///         that the caller can use; mloc_errorInvalid for a null
    ///         pointer, or a text of 1 GiB or more.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocTextResult mlocCompileMessage(const char* text, size_t length,
                                                              void* buffer, size_t capacity,
                                                              size_t* sizeOut,
                                                              mlocMessageError* errorOut);

#ifdef __cplusplus
}
#endif

#endif
