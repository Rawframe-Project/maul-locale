// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Messages: Unicode MessageFormat 2 (UTS #35 Part 9) compiled once into
// caller memory, bound once to a locale, and formatted many times. A
// compiled message is locale-free: it holds the message's declarations,
// selectors, variants and patterns with their names resolved, keys and
// names in NFC, and every syntax and data model check done. It is the form
// a catalog stores; it is valid only for this library's version.

#ifndef MAUL_LOCALE_MESSAGE_H
#define MAUL_LOCALE_MESSAGE_H

#include "maul-locale/base.h"
#include "maul-locale/data.h"
#include "maul-locale/decimal.h"
#include "maul-locale/list.h"
#include "maul-locale/locale.h"
#include "maul-locale/zone.h"

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
    ///         stream-safe form bounds and NFC here does not take, and for
    ///         a name whose NFC form is no name: U+037E and U+1FEF become
    ///         ";" and "`"); mloc_errorCapacity
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
    // program's, for its own functions, with a type id of its choice; a
    // currency amount is a decimal by pointer and an ISO 4217 code (three
    // ASCII letters, any case), which :currency formats and the number
    // functions read as its amount; a unit amount is a decimal by pointer
    // and a unit identifier ("meter", "kilometer-per-hour"), which :unit
    // formats and the number functions read as its amount, both to
    // outlive the call; a date, a time of day and a date-time are
    // floating values (no zone), an instant an exact time (its zone UTC),
    // and a zoned date-time an instant and a zone by pointer (to outlive
    // the call), which the date and time functions format; a duration is
    // its fields by pointer (to outlive the call), which mloc:duration
    // formats; a list is its items' texts by pointer (to outlive the
    // call), which mloc:list formats and selects by its count.
    typedef enum mlocMessageValueKind
    {
        mloc_messageValueString = 0,
        mloc_messageValueInteger = 1,
        mloc_messageValueDouble = 2,
        mloc_messageValueDecimal = 3,
        mloc_messageValueObject = 4,
        mloc_messageValueCurrency = 5,
        mloc_messageValueUnit = 6,
        mloc_messageValueDate = 7,
        mloc_messageValueTime = 8,
        mloc_messageValueDateTime = 9,
        mloc_messageValueInstant = 10,
        mloc_messageValueZonedDateTime = 11,
        mloc_messageValueDuration = 12,
        mloc_messageValueList = 13,
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
            struct
            {
                const mlocDecimal* amount;
                char code[3];
            } currency;
            struct
            {
                const mlocDecimal* amount;
                const char* unit;
                size_t unitLength;
            } unit;
            mlocDate date;
            mlocTime time;
            mlocDateTime dateTime;
            mlocInstant instant;
            struct
            {
                mlocInstant instant;
                const mlocTimeZone* zone;
            } zoned;
            const mlocDurationValues* duration;
            struct
            {
                const mlocListItem* items;
                size_t count;
            } list;
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

    // A function's resolved value: 96 bytes its resolve entry fills as it
    // likes, which its format and match entries read back. As an
    // accessor's storage it may hold a number value's decimal, a
    // currency value's amount and code, a date value's date-time and its
    // instant and zone, or a duration's fields, instead.
    typedef union mlocMessageResolved
    {
        uint64_t opaque[12];
        mlocDecimal decimal;
        struct
        {
            mlocDecimal amount;
            char code[4];
        } currency;
        mlocDateTime dateTime;
        struct
        {
            mlocDateTime dateTime;
            mlocInstant instant;
            mlocTimeZone zone;
        } zoned;
        mlocDurationValues duration;
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
        // A number function's value (:number, :integer, :offset,
        // :percent): its decimal, unrounded; its options stay the
        // formatter's.
        mloc_messageOperandNumber = 5,
        // A :currency value: its amount as number, unrounded, and its
        // currency's code as text, three upper-case ASCII letters.
        mloc_messageOperandCurrency = 6,
        // A :unit value: its amount as number, unrounded, and its unit
        // identifier as text.
        mloc_messageOperandUnit = 7,
        // A date and time function's value (:date, :time, :datetime) as
        // dateTime: of a date alone (its time midnight), a time alone (its
        // date the first day of 1970), or both. A value with an exact time
        // also has its instant and zone (the timeZone option's, else its
        // operand's), dateTime then the zone's local date-time; a
        // floating value has neither.
        mloc_messageOperandDate = 8,
        mloc_messageOperandTime = 9,
        mloc_messageOperandDateTime = 10,
        // A mloc:duration value as duration, its fields.
        mloc_messageOperandDuration = 11,
        // A mloc:list value as its items, itemCount of them.
        mloc_messageOperandList = 12,
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
        const mlocDecimal* number;
        const mlocDateTime* dateTime;
        const mlocInstant* instant;
        const mlocTimeZone* zone;
        const mlocDurationValues* duration;
        const mlocListItem* items;
        size_t itemCount;
    } mlocMessageOperand;

    // A function call's options and locale, read with the accessors
    // below while the call lasts. Opaque.
    typedef struct mlocMessageCall
    {
        uint64_t opaque[4];
    } mlocMessageCall;

    // A program's function. Its name has a namespace ("ns:name"), not u: or
    // mloc:, which are the library's. Each entry receives context. resolve
    // turns the operand and the call's options into a value, or returns the
    // fault that stops it (the expression then falls back); format writes a
    // value as mlocOutput says (the length needed when it does not fit), or
    // returns a fault; match tells whether a key (in NFC) matches a value, a
    // fault making the selector match only "*"; better tells whether key1 is a
    // better match than key2, both matching; direction gives the direction of
    // a value's text, for bidi isolation. A null format or match means the
    // function cannot format or select (an unsupported operation, a bad
    // selector); a null better means no key is better than another; a null
    // direction, an unknown direction. The call's u: options are the
    // formatter's (u:dir replaces the direction and isolates), not the
    // function's. An entry may call the library back, the format call it is
    // inside of included, but not make again an object that call uses or write
    // its output or faults.
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
        // The default time zone the date and time functions show exact
        // times in (UTC when null), copied; a floating value has none
        // unless its expression names one.
        const mlocTimeZone* timeZone;
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
    ///         unknown isolation, a time zone the library did not make,
    ///         or a function without a resolve entry, a name that is not
    ///         an MF2 identifier with a namespace, a namespace of u: or
    ///         mloc:, or two functions of one name.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitMessageFormatter(mlocMessageFormatter* formatterOut,
                                                                const mlocDataSet* data,
                                                                const mlocLocale* locale,
                                                                const mlocMessageOptions* options);

    /// Binds a compiled message to a formatter in caller memory: the
    /// message is read as mlocWriteMessageText reads it, each function
    /// found, each chain of declarations measured, and each number
    /// function's formatter and plural rules made once where its options
    /// do not come from arguments (a format call makes its own when its
    /// options differ). No program function is called. The bound message
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
    ///         selectors, 1024 declarations, or 2^24 expressions or
    ///         markups, a chain of more than 64 expressions each naming
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

    // A formatted message's parts, as the output's part function receives
    // them. A part whose kind's low byte is 0x80 or more is the message's own:
    // the byte is one of these, and the bits above it the expression's or
    // markup's index in the message (0 for text and an isolation mark). A part
    // whose low byte is under 0x80 belongs to the placeholder around it, which
    // reported its range first: its kind is the function's own (mlocNumberPart
    // for the number functions, mlocDatePart for the date functions), its
    // range inside the placeholder's. The message's own parts tile its text in
    // order.
    typedef enum mlocMessagePart
    {
        // A text of the pattern.
        mloc_messagePartText = 0x80,
        // A placeholder's value: its text, the isolation marks around it
        // being parts of their own.
        mloc_messagePartPlaceholder = 0x81,
        // A placeholder's fallback ("{$name}") in place of its value.
        mloc_messagePartFallback = 0x82,
        // Markup, at its place, of no length.
        mloc_messagePartMarkupOpen = 0x83,
        mloc_messagePartMarkupStandalone = 0x84,
        mloc_messagePartMarkupClose = 0x85,
        // An isolation mark: LRI, RLI or FSI before a placeholder, PDI
        // after it.
        mloc_messagePartIsolation = 0x86,
    } mlocMessagePart;

    /// Formats a bound message with its arguments: the pattern selected,
    /// each text written, each placeholder resolved where it stands and
    /// written, markup writing nothing. An expression that fails is
    /// written as its fallback ("{$name}", "{|literal|}", "{:function}")
    /// and reported as a fault. A numeric argument without a function is
    /// written plainly ("1234.5"), a currency amount as its amount and
    /// code ("42.5 EUR"), a unit amount as its amount and unit ("3.5
    /// meter"), a date, time or date-time in ISO 8601's extended form
    /// (year, month and day joined by hyphens, a time's fields by colons,
    /// a "T" between them, the fraction of a second as needed), an
    /// instant the same in UTC followed by "Z", a zoned date-time its
    /// local date-time followed by its offset and its zone in brackets, a
    /// duration as Temporal writes it ("P2DT3H"); a list only through
    /// mloc:list (bare, an Unsupported Operation).
    ///
    /// When output->part is set, the call reports the text's parts
    /// (mlocMessagePart) once it is written, on mloc_success or
    /// mloc_withFallbacks: it formats a second time to report them, so a
    /// program function's entries must give the same results for the
    /// same values.
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
    ///         string, decimal, amount or unit among them), a decimal that
    ///         is not one the library makes, a currency code that is not
    ///         three ASCII letters, a date, time, instant or duration the
    ///         library does not make, a zone it did not make, a list whose
    ///         items or an item's text are missing or of more than
    ///         UINT32_MAX items, an unknown kind, or a bound message the
    ///         library did not make.
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

    // What a placeholder's or markup's part names, as the message wrote
    // it: an expression's function (none for an expression without one)
    // or a markup's name, and how many options and attributes it has.
    typedef struct mlocMessagePartInfo
    {
        const char* name;
        size_t nameLength;
        uint32_t parameterCount;
    } mlocMessagePartInfo;

    // An option or attribute as the message wrote it: its name, and its
    // value, a literal's text or a variable's name (variable then set),
    // none (null, 0) for an attribute without one. A value from a
    // variable is the call's; the bound message keeps only the name.
    typedef struct mlocMessageParameter
    {
        bool attribute;
        bool variable;
        const char* name;
        size_t nameLength;
        const char* value;
        size_t valueLength;
    } mlocMessageParameter;

    /// Reads what a part of a formatted message names: a placeholder's or
    /// fallback's expression, or a markup, by the index its kind holds
    /// (mlocMessagePart).
    ///
    /// @param bound    The bound message the part came from.
    /// @param part     The part's kind, as reported.
    /// @param infoOut  Receives what it names; its texts point into the
    ///                 compiled message.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a bound
    ///         message the library did not make, a part of another kind,
    ///         or an index the message does not have.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetMessagePartInfo(const void* bound, uint32_t part,
                                                              mlocMessagePartInfo* infoOut);

    /// Reads a placeholder's or markup's option or attribute by index, the
    /// options first, in the message's order.
    ///
    /// @param bound         The bound message the part came from.
    /// @param part          The part's kind, as reported.
    /// @param index         The parameter's index, under its count.
    /// @param parameterOut  Receives it; its texts point into the
    ///                      compiled message.
    /// @return mloc_success; mloc_errorInvalid as mlocGetMessagePartInfo,
    ///         or for an index past the count.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetMessagePartParameter(
        const void* bound, uint32_t part, uint32_t index, mlocMessageParameter* parameterOut);

    /// Finds a placeholder's or markup's option by name ("u:id" for its
    /// identifier, which formatting to parts serves), as
    /// mlocGetMessagePartParameter reads it.
    ///
    /// @param bound         The bound message the part came from.
    /// @param part          The part's kind, as reported.
    /// @param name          The option's name, UTF-8 in NFC.
    /// @param length        Its length.
    /// @param parameterOut  Receives it.
    /// @return mloc_success; mloc_errorNotFound when it has no such
    ///         option; mloc_errorInvalid as mlocGetMessagePartInfo.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocFindMessagePartOption(const void* bound, uint32_t part, const char* name, size_t length,
                              mlocMessageParameter* parameterOut);

#ifdef __cplusplus
}
#endif

#endif
