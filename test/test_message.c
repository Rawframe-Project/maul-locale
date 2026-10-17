// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Compiling messages (maul-locale/message.h): every case of the MF2 test
// suite (test/mf2_suite.inc) compiles or is refused as the suite says,
// whatever the buffer's size; the compiled form's records counted for
// known messages; names one variable however they are composed; and
// every prefix of every case compiles or is refused without a fault.

#include "bytes.h"
#include "message_form.h"
#include "test_harness.h"

#include "maul-locale/message.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SuiteCase
{
    const char* file;
    const char* locale;
    bool bidi;
    const char* source;
    size_t length;
    const char* expected;
    const char* errors;
    const char* arguments;
} SuiteCase;

static const SuiteCase s_suite[] = {
#include "mf2_suite.inc"
};

static uint8_t s_buffer[1 << 16];

// The compile error a case's errors name, if it names one.
static mlocMessageError ExpectedError(const char* errors)
{
    static const struct
    {
        const char* name;
        mlocMessageError error;
    } names[] = {
        {"syntax-error", mloc_messageErrorSyntax},
        {"variant-key-mismatch", mloc_messageErrorVariantKeyMismatch},
        {"missing-fallback-variant", mloc_messageErrorMissingFallbackVariant},
        {"missing-selector-annotation", mloc_messageErrorMissingSelectorAnnotation},
        {"duplicate-declaration", mloc_messageErrorDuplicateDeclaration},
        {"duplicate-option-name", mloc_messageErrorDuplicateOptionName},
        {"duplicate-variant", mloc_messageErrorDuplicateVariant},
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
    {
        if (strstr(errors, names[i].name) != NULL)
        {
            return names[i].error;
        }
    }
    return mloc_messageErrorNone;
}

static void CheckCase(const SuiteCase* c)
{
    mlocMessageError expected = ExpectedError(c->errors);
    size_t needed = 0;
    mlocMessageError error = mloc_messageErrorNone;
    mlocTextResult measured = mlocCompileMessage(c->source, c->length, NULL, 0, &needed, &error);
    size_t size = 0;
    mlocTextResult compiled =
        mlocCompileMessage(c->source, c->length, s_buffer, sizeof s_buffer, &size, &error);
    bool good;
    if (expected == mloc_messageErrorNone)
    {
        good = measured.status == mloc_errorCapacity && compiled.status == mloc_success &&
               compiled.offset == c->length && size <= needed && error == mloc_messageErrorNone &&
               memcmp(s_buffer, MLOC_MESSAGE_MAGIC, 4) == 0 &&
               mlocLoad32(s_buffer + messageSize) == size;
    }
    else
    {
        // The checks that compare names or keys run on the compiled form,
        // so measuring passes them.
        bool late = expected >= mloc_messageErrorMissingSelectorAnnotation;
        good = measured.status == (late ? mloc_errorCapacity : mloc_errorSyntax) &&
               compiled.status == mloc_errorSyntax && error == expected &&
               compiled.offset <= c->length;
    }
    if (!good)
    {
        printf("  %s: [%s] status %d/%d error %d\n", c->file, c->source, (int)measured.status,
               (int)compiled.status, (int)error);
    }
    CHECK(good, "a suite case compiles or is refused as the suite says");
}

static void TestSuite(void)
{
    size_t count = sizeof s_suite / sizeof s_suite[0];
    CHECK(count == 461, "the suite has 461 cases");
    for (size_t i = 0; i < count; i++)
    {
        CheckCase(&s_suite[i]);
    }
}

// A message's size when compiled, the capacity it needs, and each record
// count, for messages whose records are known.
static uint32_t Count(uint32_t kind)
{
    return mlocLoad32(s_buffer + messageCounts + kind * 4);
}

static bool Compile(const char* text, size_t* sizeOut)
{
    mlocMessageError error;
    mlocTextResult result =
        mlocCompileMessage(text, strlen(text), s_buffer, sizeof s_buffer, sizeOut, &error);
    return result.status == mloc_success;
}

static void TestRecords(void)
{
    size_t size;
    CHECK(Compile(".input {$n :number} .local $m = {$n :integer @a=|b|}\n"
                  ".match $n $m\n1 * {{one {$n}}}\n* * {{{#b}{$m}{/b} more}}",
                  &size),
          "a complex message compiles");
    CHECK(Count(messageDeclarations) == 2 && Count(messageExpressions) == 4 &&
              Count(messageMarkups) == 2 && Count(messageParameters) == 1 &&
              Count(messageSelectors) == 2 && Count(messageVariants) == 2 &&
              Count(messageVariables) == 2,
          "its records are counted");
    CHECK(Count(messagePieces) == 6, "its patterns' pieces are counted");
    CHECK((s_buffer[messageFlags] & messageComplex) != 0, "it is a complex message");

    CHECK(Compile("  Hello, {$name}! \\{x\\}", &size), "a simple message compiles");
    CHECK((s_buffer[messageFlags] & messageComplex) == 0, "it is a simple message");
    CHECK(Count(messagePieces) == 3 && Count(messageVariables) == 1 && Count(messagePool) == 18,
          "its text keeps its spaces and drops its escapes");

    // é composed and decomposed: one variable, its name stored in NFC.
    CHECK(Compile("{$\303\251}{$e\314\201}{$\342\200\216\303\251\342\200\216}", &size),
          "names in either form compile");
    CHECK(Count(messageVariables) == 1 && Count(messagePool) == 2, "they are one variable");

    // Names of three and four bytes (Devanagari ka, U+0915, after an E0
    // lead; U+10FFFD, the last name character); noncharacters refused.
    CHECK(Compile("{$\340\244\225}{$\364\217\277\275}", &size) && Count(messagePool) == 7,
          "names past two bytes compile");
    CHECK(!Compile("{$\357\277\276}", &size) && !Compile("{$\357\267\220}", &size),
          "a noncharacter starts no name");
}

// A record's 32-bit field.
static uint32_t Field(uint32_t kind, uint32_t index, uint32_t field)
{
    uint32_t at = messageHeaderSize;
    for (uint32_t k = 0; k < kind; k++)
    {
        if (k + 1 == messageVariants)
        {
            at += Count(messageSelectors) * Count(messageVariants) * messageKeySize;
        }
        at += Count(k) * messageRecordSizes[k];
    }
    return mlocLoad32(s_buffer + at + index * messageRecordSizes[kind] + field * 4);
}

// A pool string's bytes compared with text.
static bool PoolIs(uint32_t offset, uint32_t length, const char* text)
{
    uint32_t pool = (uint32_t)(mlocLoad32(s_buffer + messageSize) - Count(messagePool));
    return length == strlen(text) && memcmp(s_buffer + pool + offset, text, length) == 0;
}

static void TestLayout(void)
{
    size_t needed = 0;
    mlocMessageError error;
    const char* text = "{$x}{$y}";
    mlocTextResult result = mlocCompileMessage(text, 8, NULL, 0, &needed, &error);
    // The header, two expressions, two pieces, two variables and their
    // names, rounded to 8, then four slots of the table and two words a
    // variable.
    CHECK(result.status == mloc_errorCapacity && needed == 176 + 32 + 16,
          "the capacity is the message's records and the table");
    size_t size;
    CHECK(Compile(text, &size) && size == 170, "the message is its records alone");
    CHECK(mlocLoad16(s_buffer + messageFormat) == messageFormatVersion &&
              mlocLoad16(s_buffer + messageFlags) == 0 &&
              mlocLoad32(s_buffer + messageLibrary) ==
                  ((uint32_t)MLOC_VERSION_MAJOR << 16 | (uint32_t)MLOC_VERSION_MINOR << 8 |
                   MLOC_VERSION_PATCH) &&
              mlocLoad32(s_buffer + messageUnicode) >> 16 >= 18,
          "the header names the format and the versions");
    CHECK(Field(messageExpressions, 0, 0) == operandVariable &&
              Field(messageExpressions, 0, 1) == 0 && Field(messageExpressions, 1, 1) == 1 &&
              Field(messageExpressions, 0, 4) == 0 && Field(messageExpressions, 1, 7) == 4,
          "each expression names its variable, no function, its offset");
    CHECK(Field(messagePieces, 0, 0) == pieceExpression && Field(messagePieces, 1, 1) == 1,
          "each piece names its expression");
    CHECK(PoolIs(Field(messageVariables, 0, 0), Field(messageVariables, 0, 1), "x") &&
              PoolIs(Field(messageVariables, 1, 0), Field(messageVariables, 1, 1), "y") &&
              Field(messageVariables, 1, 2) == variableExternal,
          "each variable has its name");

    CHECK(Compile("ab{$x :f k=|v|}", &size), "an expression with an option compiles");
    CHECK(Field(messageExpressions, 0, 7) == 2 && Field(messageExpressions, 0, 6) == 1 &&
              PoolIs(Field(messageExpressions, 0, 3), Field(messageExpressions, 0, 4), "f") &&
              PoolIs(Field(messageParameters, 0, 1), Field(messageParameters, 0, 2), "k") &&
              PoolIs(Field(messageParameters, 0, 3), Field(messageParameters, 0, 4), "v"),
          "its function, option and offset are kept");
    CHECK(Compile("{:ns:fn}", &size) &&
              PoolIs(Field(messageExpressions, 0, 3), Field(messageExpressions, 0, 4), "ns:fn") &&
              Field(messageExpressions, 0, 0) == operandNone,
          "a function's namespace is kept with its name");
}

// Texts cut short of bytes that would continue them: nothing past the
// length is read.
static void TestCut(void)
{
    size_t size = 0;
    mlocMessageError error;
    mlocTextResult result = mlocCompileMessage("{$ab", 3, s_buffer, sizeof s_buffer, &size, &error);
    CHECK(result.status == mloc_errorSyntax && result.offset == 3, "a name ends at the length");
    result = mlocCompileMessage("a\\{", 2, s_buffer, sizeof s_buffer, &size, &error);
    CHECK(result.status == mloc_errorSyntax && result.offset == 2, "an escape ends at the length");
    result = mlocCompileMessage("{|a\\|}", 4, s_buffer, sizeof s_buffer, &size, &error);
    CHECK(result.status == mloc_errorSyntax && result.offset == 4,
          "a quoted escape ends at the length");
    result = mlocCompileMessage("ab", 1, s_buffer, sizeof s_buffer, &size, &error);
    CHECK(result.status == mloc_success && Count(messagePool) == 1, "text ends at the length");
}

// A run of more than 30 combining marks, in a name or a key.
static void TestMarks(void)
{
    char text[200];
    for (int marks = 30; marks <= 32; marks += 2)
    {
        size_t at = (size_t)snprintf(text, sizeof text, "{$a");
        for (int i = 0; i < marks; i++)
        {
            at += (size_t)snprintf(text + at, sizeof text - at, "\314\201");
        }
        snprintf(text + at, sizeof text - at, "}");
        size_t size = 0;
        mlocMessageError error;
        mlocTextResult result =
            mlocCompileMessage(text, strlen(text), s_buffer, sizeof s_buffer, &size, &error);
        CHECK((result.status == mloc_success) == (marks == 30), "a name's marks up to 30");
        at = (size_t)snprintf(text, sizeof text, ".input {$x :f}.match $x |a");
        for (int i = 0; i < marks; i++)
        {
            at += (size_t)snprintf(text + at, sizeof text - at, "\314\201");
        }
        snprintf(text + at, sizeof text - at, "| {{}} * {{}}");
        result = mlocCompileMessage(text, strlen(text), s_buffer, sizeof s_buffer, &size, &error);
        CHECK((result.status == mloc_success) == (marks == 30), "a key's marks up to 30");
    }
}

// A compile's error and offset.
static bool Refused(const char* text, mlocMessageError expected, size_t at)
{
    size_t size = 0;
    mlocMessageError error = mloc_messageErrorNone;
    mlocTextResult result =
        mlocCompileMessage(text, strlen(text), s_buffer, sizeof s_buffer, &size, &error);
    return result.status == mloc_errorSyntax && error == expected && result.offset == at;
}

static void TestDataModel(void)
{
    CHECK(Refused(".input {$x :f} .input {$x :g} {{}}", mloc_messageErrorDuplicateDeclaration, 22),
          "an input declared twice, at the second");
    CHECK(Refused(".input {$x :f o=$y} .local $y = {1} {{}}", mloc_messageErrorDuplicateDeclaration,
                  32),
          "a variable an earlier declaration used");
    CHECK(Refused(".local $x = {$x :f} {{}}", mloc_messageErrorDuplicateDeclaration, 12),
          "a local naming itself");
    CHECK(Refused(".input {$x :f o=$x} {{}}", mloc_messageErrorDuplicateDeclaration, 7),
          "an input naming itself in an option");
    size_t size;
    CHECK(Compile(".input {$x :f} .local $y = {$x :g o=$z} {{{$y}}}", &size),
          "an input used by a later local is fine");
    CHECK(Field(messageVariables, 0, 2) == variableInput &&
              Field(messageVariables, 1, 2) == variableLocal &&
              Field(messageVariables, 2, 2) == variableExternal,
          "each variable is marked by its declaration");

    CHECK(Refused("{$x :f a=1 b=2 a=3}", mloc_messageErrorDuplicateOptionName, 0),
          "an option named twice");
    CHECK(Refused("{#b a=1 a=2 @a}", mloc_messageErrorDuplicateOptionName, 0),
          "a markup's option named twice");
    CHECK(Compile("{$x :f a=1 @a @a=2}{$y :f a=2}", &size),
          "an attribute and other expressions do not count");

    CHECK(Refused(".input {$x} .match $x * {{}}", mloc_messageErrorMissingSelectorAnnotation, 19),
          "an input without a function");
    CHECK(Refused(".local $y = {$x} .match $y * {{}}", mloc_messageErrorMissingSelectorAnnotation,
                  24),
          "a local of an external variable");
    CHECK(Refused(".local $y = {|a|} .match $y * {{}}", mloc_messageErrorMissingSelectorAnnotation,
                  25),
          "a local of a literal");
    CHECK(Compile(".input {$x :f} .local $y = {$x} .local $z = {$y} .match $z * {{}}", &size),
          "a chain of locals to a function");

    CHECK(Refused(".input {$x :f} .input {$y :f} .match $x $y a * {{}} |a| |*| {{}} a |*| {{}} * * "
                  "{{}}",
                  mloc_messageErrorDuplicateVariant, 65),
          "keys equal as literals, quoted or not");
    CHECK(Compile(".input {$x :f} .match $x |*| {{}} * {{}}", &size),
          "a literal star differs from the catch-all");
    CHECK(Refused(".input {$x :f} .match $x \303\251 {{}} e\314\201 {{}} * {{}}",
                  mloc_messageErrorDuplicateVariant, 33),
          "keys equal in NFC");
}

// Many names, options and variants: each check is linear, so this takes
// a moment (a quadratic one would take minutes).
static void TestLinear(void)
{
    enum
    {
        many = 40000
    };
    static char text[many * 24];
    static uint8_t big[many * 160];
    size_t at = 0;
    for (int i = 0; i < many; i++)
    {
        at += (size_t)snprintf(text + at, sizeof text - at, "{$v%d :f o%d=1}", i, i);
    }
    size_t size = 0;
    mlocMessageError error;
    mlocTextResult result = mlocCompileMessage(text, at, big, sizeof big, &size, &error);
    CHECK(result.status == mloc_success, "forty thousand names and options compile");
    at = (size_t)snprintf(text, sizeof text, ".input {$x :f} .match $x\n");
    for (int i = 0; i < many; i++)
    {
        at += (size_t)snprintf(text + at, sizeof text - at, "k%d {{}}", i);
    }
    at += (size_t)snprintf(text + at, sizeof text - at, "* {{}}");
    result = mlocCompileMessage(text, at, big, sizeof big, &size, &error);
    CHECK(result.status == mloc_success, "forty thousand variants compile");
}

static void TestRefused(void)
{
    size_t size = 0;
    mlocMessageError error = mloc_messageErrorNone;
    mlocTextResult result =
        mlocCompileMessage("ab\xC3", 3, s_buffer, sizeof s_buffer, &size, &error);
    CHECK(result.status == mloc_errorSyntax && result.offset == 2 &&
              error == mloc_messageErrorSyntax,
          "ill-formed UTF-8 is a syntax error at its offset");
    result = mlocCompileMessage("a\0b", 3, s_buffer, sizeof s_buffer, &size, &error);
    CHECK(result.status == mloc_errorSyntax && result.offset == 1, "NUL is a syntax error");
    result = mlocCompileMessage("{$x", 3, s_buffer, sizeof s_buffer, &size, &error);
    CHECK(result.status == mloc_errorSyntax && result.offset == 3,
          "an unclosed placeholder is refused at the end");

    const char* text = ".input {$x :number}{{{$x}}}";
    result = mlocCompileMessage(text, strlen(text), NULL, 0, &size, &error);
    CHECK(result.status == mloc_errorCapacity && size > 0, "measuring gives the capacity");
    size_t needed = size;
    result = mlocCompileMessage(text, strlen(text), s_buffer, needed - 1, &size, &error);
    CHECK(result.status == mloc_errorCapacity && size == needed, "one byte short is refused");
    result = mlocCompileMessage(text, strlen(text), s_buffer, needed, &size, &error);
    CHECK(result.status == mloc_success && size <= needed, "the capacity measured suffices");

    result = mlocCompileMessage(NULL, 1, s_buffer, sizeof s_buffer, &size, &error);
    CHECK(result.status == mloc_errorInvalid, "a null text is refused");
    result = mlocCompileMessage("a", 1, NULL, 4, &size, &error);
    CHECK(result.status == mloc_errorInvalid, "a null buffer with a capacity is refused");
    result = mlocCompileMessage("a", 1, s_buffer, sizeof s_buffer, NULL, &error);
    CHECK(result.status == mloc_errorInvalid, "a null size is refused");
    // Refused before a byte is read: the text here is one byte long.
    result = mlocCompileMessage("a", (size_t)1 << 30, s_buffer, sizeof s_buffer, &size, &error);
    CHECK(result.status == mloc_errorInvalid, "a text of 1 GiB is refused");
    result = mlocCompileMessage("", 0, s_buffer, sizeof s_buffer, &size, NULL);
    CHECK(result.status == mloc_success, "the empty message compiles, without an error out");
}

// Every prefix of every case, in a buffer exactly as large as measured,
// so that a write past it is caught by the sanitizers.
static void TestPrefixes(void)
{
    size_t count = sizeof s_suite / sizeof s_suite[0];
    for (size_t i = 0; i < count; i++)
    {
        for (size_t length = 0; length <= s_suite[i].length; length++)
        {
            size_t needed = 0;
            mlocMessageError error;
            mlocTextResult result =
                mlocCompileMessage(s_suite[i].source, length, NULL, 0, &needed, &error);
            if (result.status != mloc_errorCapacity)
            {
                CHECK(result.status == mloc_errorSyntax || length == 0 || needed == 0,
                      "a prefix is measured or refused");
                continue;
            }
            uint8_t* buffer = malloc(needed);
            size_t size = 0;
            result = mlocCompileMessage(s_suite[i].source, length, buffer, needed, &size, &error);
            CHECK(result.status == mloc_success || result.status == mloc_errorSyntax,
                  "a measured prefix compiles or is refused for its data model");
            free(buffer);
        }
    }
}

int main(void)
{
    TestSuite();
    TestRecords();
    TestLayout();
    TestCut();
    TestMarks();
    TestDataModel();
    TestLinear();
    TestRefused();
    TestPrefixes();
    return s_failures == 0 ? 0 : 1;
}
