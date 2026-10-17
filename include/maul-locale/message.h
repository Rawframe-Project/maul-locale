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
#include "maul-locale/data.h"
#include "maul-locale/decimal.h"
#include "maul-locale/locale.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

    /// Writes a compiled message back as MF2 text, in a canonical form: a
    /// simple message where its pattern can stand alone; else each
    /// declaration, the selectors and each variant on a line of their
    /// own; one space between an expression's parts; literals quoted
    /// only where they must be. Compiling the text gives the same
    /// records, and writing them again the same text. The message is
    /// checked first, so bytes from a file are safe to give: every
    /// count, index and string must hold, the strings well-formed UTF-8
    /// and the names MF2 names.
    ///
    /// @param message  A compiled message from mlocCompileMessage.
    /// @param size     Its size in bytes.
    /// @param output   Receives the text.
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed); mloc_errorData
    ///         when the bytes are not a compiled message, or one whose text
    ///         would be 1 GiB or more, which could not compile again;
    ///         mloc_errorDataVersion for one of another library or Unicode
    ///         version; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocWriteMessageText(const void* message, size_t size,
                                                            mlocOutput* output);

    // An argument's kind of value. A string is UTF-8; a decimal is
    // given by pointer and must outlive the call; an object is the
    // program's, for its own functions, with a type id of its choice.
    typedef enum mlocMessageValueKind
    {
        mloc_messageValueString = 0,
        mloc_messageValueInteger = 1,
        mloc_messageValueDouble = 2,
        mloc_messageValueDecimal = 3,
        mloc_messageValueObject = 4,
    } mlocMessageValueKind;

    // A value given to a message: one of the kinds, its fields by kind.
    typedef struct mlocMessageValue
    {
        mlocMessageValueKind kind;
        // An object's type id; 0 for the other kinds.
        uint32_t type;
        union
        {
            struct
            {
                const char* text;
                size_t length;
            } string;
            int64_t integer;
            double number;
            const mlocDecimal* decimal;
            const void* object;
        } as;
    } mlocMessageValue;

    // A named argument. Its name matches a variable's after NFC, as MF2
    // compares names; one already in NFC is compared at once, else it is
    // normalized for each lookup (a name of more than 256 bytes after
    // NFC must be given in NFC).
    typedef struct mlocMessageArgument
    {
        const char* name;
        size_t nameLength;
        mlocMessageValue value;
    } mlocMessageArgument;

    // What went wrong in one expression while formatting (UTS #35 Part 9's
    // resolution and function errors); the message is still written,
    // with a fallback where the expression would be.
    typedef enum mlocMessageFaultKind
    {
        mloc_messageFaultUnresolvedVariable = 1,
        mloc_messageFaultUnknownFunction = 2,
        mloc_messageFaultBadOperand = 3,
        mloc_messageFaultBadOption = 4,
        mloc_messageFaultBadSelector = 5,
        mloc_messageFaultBadVariantKey = 6,
        mloc_messageFaultUnsupportedOperation = 7,
        // A program function's own failure.
        mloc_messageFaultFunction = 8,
    } mlocMessageFaultKind;

    // A fault and the byte offset in the message's source of the
    // expression or selector where it arose.
    typedef struct mlocMessageFault
    {
        mlocMessageFaultKind kind;
        uint32_t at;
    } mlocMessageFault;

    // Where a format call reports its faults: the first capacity of them
    // in order, and count, the number found.
    typedef struct mlocMessageFaults
    {
        mlocMessageFault* faults;
        size_t capacity;
        size_t count;
    } mlocMessageFaults;

    // A message formatter's options; the zero value is the default.
    typedef struct mlocMessageOptions
    {
        // Strict: a format call that meets a fault writes nothing and
        // returns mloc_errorFormat; else it writes the message with
        // fallbacks.
        bool strict;
    } mlocMessageOptions;

    // A locale's message formatter: a value that points into the data it
    // was made from, which must outlive it. Opaque; make it with
    // mlocInitMessageFormatter.
    typedef struct mlocMessageFormatter
    {
        uint64_t opaque[32];
    } mlocMessageFormatter;

    /// Makes a locale's message formatter.
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data its functions read.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitMessageFormatter(mlocMessageFormatter* formatterOut,
                                                                const mlocDataSet* data,
                                                                const mlocLocale* locale,
                                                                const mlocMessageOptions* options);

    /// Binds a compiled message to a formatter in caller memory: the
    /// message is read as mlocWriteMessageText reads it, each function
    /// found, each chain of declarations measured. The bound message
    /// points into the compiled one and the formatter, which must outlive
    /// it; it is read-only, so threads may format with it at once. Call it
    /// with a null buffer and a capacity of 0 to learn the capacity.
    ///
    /// @param formatter  A formatter from mlocInitMessageFormatter.
    /// @param message    A compiled message from mlocCompileMessage.
    /// @param size       Its size in bytes.
    /// @param buffer     Receives the bound message; any alignment.
    /// @param capacity   The buffer's size in bytes.
    /// @param sizeOut    Receives the size written, or needed with
    ///                   mloc_errorCapacity.
    /// @return mloc_success; mloc_errorCapacity when the buffer is too
    ///         small, or for a chain of more than 64 expressions each
    ///         naming the next through a declaration; mloc_errorData and
    ///         mloc_errorDataVersion as mlocWriteMessageText;
    ///         mloc_errorInvalid for a null pointer or a formatter the
    ///         library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocBindMessage(const mlocMessageFormatter* formatter,
                                                       const void* message, size_t size,
                                                       void* buffer, size_t capacity,
                                                       size_t* sizeOut);

    /// Formats a bound message with its arguments: the pattern selected,
    /// each text written, each placeholder resolved where it stands and
    /// written, markup writing nothing. An expression that fails is
    /// written as its fallback ("{$name}", "{|literal|}", "{:function}")
    /// and reported as a fault.
    ///
    /// @param bound          A bound message from mlocBindMessage.
    /// @param arguments      The arguments; may be null when count is 0.
    /// @param argumentCount  Their number.
    /// @param output         Receives the text.
    /// @param faults         Receives the faults; may be null.
    /// @return mloc_success; mloc_withFallbacks when the text holds
    ///         fallbacks; in strict mode mloc_errorFormat at the first
    ///         fault, nothing written; mloc_errorCapacity when the text does
    ///         not fit (output->length is then the length needed);
    ///         mloc_errorInvalid for a null pointer (an argument's name,
    ///         string or decimal among them), an unknown kind, or a bound
    ///         message the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatMessage(const void* bound,
                                                         const mlocMessageArgument* arguments,
                                                         size_t argumentCount, mlocOutput* output,
                                                         mlocMessageFaults* faults);

#ifdef __cplusplus
}
#endif

#endif
