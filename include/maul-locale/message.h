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
        // No fault: what a function's entries return on success.
        mloc_messageFaultNone = 0,
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

    // A function's resolved value: 64 bytes its resolve entry fills as it
    // likes, which its format and match entries read back.
    typedef struct mlocMessageResolved
    {
        uint64_t opaque[8];
    } mlocMessageResolved;

    // What a function receives as its operand or an option's value.
    typedef enum mlocMessageOperandKind
    {
        // The expression has no operand.
        mloc_messageOperandNone = 0,
        // Text: a literal's, a string argument's, or a :string value's.
        mloc_messageOperandText = 1,
        // An argument of another kind than a string.
        mloc_messageOperandArgument = 2,
        // A value a program function resolved; function says which.
        mloc_messageOperandResolved = 3,
        // A value that failed to resolve (an unresolved variable, a failed
        // expression): the function may refuse it with
        // mloc_messageFaultBadOperand, or resolve something of its own;
        // the expression's text is then its fallback.
        mloc_messageOperandFallback = 4,
    } mlocMessageOperandKind;

    typedef struct mlocMessageOperand
    {
        mlocMessageOperandKind kind;
        // A resolved value's function: its index in the options' table.
        uint32_t function;
        const char* text;
        size_t length;
        const mlocMessageValue* argument;
        const mlocMessageResolved* resolved;
    } mlocMessageOperand;

    // A function call's options and locale, read with the accessors
    // below while the call lasts. Opaque.
    typedef struct mlocMessageCall
    {
        uint64_t opaque[4];
    } mlocMessageCall;

    // A program's function. Its name has a namespace ("ns:name"), not u:
    // or mloc:, which are the library's. Each entry receives context.
    // resolve turns the operand and the call's options into a value, or
    // returns the fault that stops it (the expression then falls back);
    // format writes a value as mlocOutput says (the length needed when
    // it does not fit), or returns a fault; match tells whether a key (in
    // NFC) matches a value, a fault making the selector match only "*";
    // better tells whether key1 is a better match than key2, both
    // matching; direction gives the direction of a value's text, for
    // bidi isolation. A null format or match means the function cannot
    // format or select (an unsupported operation, a bad selector); a null
    // better means no key is better than another; a null direction, an
    // unknown direction. The call's u: options are the formatter's
    // (u:dir replaces the direction and isolates), not the function's.
    typedef struct mlocMessageFunction
    {
        const char* name;
        size_t nameLength;
        void* context;
        mlocMessageFaultKind (*resolve)(void* context, const mlocMessageCall* call,
                                        const mlocMessageOperand* operand,
                                        mlocMessageResolved* valueOut);
        mlocMessageFaultKind (*format)(void* context, const mlocMessageResolved* value,
                                       mlocOutput* output);
        mlocMessageFaultKind (*match)(void* context, const mlocMessageResolved* value,
                                      const char* key, size_t keyLength, bool* matchOut);
        bool (*better)(void* context, const mlocMessageResolved* value, const char* key1,
                       size_t length1, const char* key2, size_t length2);
        mlocDirection (*direction)(void* context, const mlocMessageResolved* value);
    } mlocMessageFunction;

    // How a formatted message isolates its placeholders (UTS #35 Part 9,
    // "Handling Bidirectional Text").
    typedef enum mlocMessageIsolation
    {
        // The Default Bidi Strategy: each placeholder in FSI, LRI or RLI
        // and PDI by its direction, a left-to-right one left bare in a
        // left-to-right message unless its u:dir asks.
        mloc_messageIsolationDefault = 0,
        // No isolation: the text as the placeholders format.
        mloc_messageIsolationNone = 1,
    } mlocMessageIsolation;

    // A message formatter's options; the zero value is the default.
    typedef struct mlocMessageOptions
    {
        // Strict: a format call that meets a fault writes nothing and
        // returns mloc_errorFormat; else it writes the message with
        // fallbacks.
        bool strict;
        // Isolation, by the Default Bidi Strategy unless turned off; the
        // message's own direction is its locale's
        // (mlocGetLocaleDirection), unknown when that has none.
        mlocMessageIsolation isolation;
        // The program's functions, which must outlive the formatter; may
        // be null when count is 0.
        const mlocMessageFunction* functions;
        size_t functionCount;
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
    /// @return mloc_success; mloc_errorInvalid for a null pointer, an
    ///         unknown isolation, or a function without a resolve entry, a
    ///         name that is not an MF2 identifier with a namespace, a
    ///         namespace of u: or mloc:, or two functions of one name.
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
    /// with a null buffer and a capacity of 0 to learn the capacity. A
    /// bind that fails leaves the buffer holding no bound message, even
    /// one bound there before.
    ///
    /// @param formatter  A formatter from mlocInitMessageFormatter.
    /// @param message    A compiled message from mlocCompileMessage.
    /// @param size       Its size in bytes.
    /// @param buffer     Receives the bound message; any alignment.
    /// @param capacity   The buffer's size in bytes.
    /// @param sizeOut    Receives the size written, or needed with
    ///                   mloc_errorCapacity.
    /// @return mloc_success; mloc_errorCapacity when the buffer is too
    ///         small; mloc_errorData and mloc_errorDataVersion as
    ///         mlocWriteMessageText; mloc_errorInvalid for a null pointer,
    ///         a formatter the library did not make, more than 32
    ///         selectors, a chain of more than 64 expressions each naming
    ///         the next through a declaration, or a message whose format
    ///         call would resolve more than 65536 expressions (a local
    ///         is resolved again at each reference, an option's twice;
    ///         these two found only when binding, not when measuring).
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

    /// The number of options a function's call has (its attributes and
    /// its u: options aside, which are the formatter's), for
    /// mlocGetMessageOption.
    ///
    /// @param call  The call a function's resolve entry received, read
    ///              only while that entry runs.
    /// @return The count; 0 for a null pointer.
    /// @par Thread safety
    /// Safe from any thread; the call is used by one thread at a time.
    MLOC_API size_t mlocGetMessageOptionCount(const mlocMessageCall* call);

    /// Reads one of a call's options: its name and its value, resolved
    /// now (a variable's value resolved through its declaration).
    ///
    /// @param call        The call a function's resolve entry received.
    /// @param index       Below mlocGetMessageOptionCount.
    /// @param nameOut     Receives the name (UTF-8, NFC, not terminated).
    /// @param lengthOut   Receives its length.
    /// @param valueOut    Receives the value; mloc_messageOperandFallback
    ///                    for one that failed (the formatter has reported
    ///                    a bad option), which the function ignores.
    /// @param storage     Holds a resolved value valueOut points to; it
    ///                    must live as long as valueOut is read.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or an
    ///         index past the count.
    /// @par Thread safety
    /// Safe from any thread; the call is used by one thread at a time.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetMessageOption(const mlocMessageCall* call,
                                                            size_t index, const char** nameOut,
                                                            size_t* lengthOut,
                                                            mlocMessageOperand* valueOut,
                                                            mlocMessageResolved* storage);

    /// Finds a call's option by name and reads its value as
    /// mlocGetMessageOption does.
    ///
    /// @param call      The call a function's resolve entry received.
    /// @param name      The option's name, UTF-8 in NFC.
    /// @param length    Its length.
    /// @param valueOut  Receives the value.
    /// @param storage   As for mlocGetMessageOption.
    /// @return mloc_success; mloc_errorNotFound when the call has no such
    ///         option; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread; the call is used by one thread at a time.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindMessageOption(const mlocMessageCall* call,
                                                             const char* name, size_t length,
                                                             mlocMessageOperand* valueOut,
                                                             mlocMessageResolved* storage);

    /// The locale a call formats for: the formatter's.
    ///
    /// @param call        The call a function's resolve entry received.
    /// @param localeOut   Receives the locale.
    /// @return mloc_success; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread; the call is used by one thread at a time.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetMessageLocale(const mlocMessageCall* call,
                                                            mlocLocale* localeOut);

#ifdef __cplusplus
}
#endif

#endif
