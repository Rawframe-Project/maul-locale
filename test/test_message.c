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

// The errors this slice finds; the others come with the data model's
// checks on the compiled form.
static bool Found(mlocMessageError error)
{
    return error == mloc_messageErrorNone || error == mloc_messageErrorSyntax ||
           error == mloc_messageErrorVariantKeyMismatch ||
           error == mloc_messageErrorMissingFallbackVariant;
}

static void CheckCase(const SuiteCase* c)
{
    mlocMessageError expected = ExpectedError(c->errors);
    if (!Found(expected))
    {
        return;
    }
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
        good = measured.status == mloc_errorSyntax && compiled.status == mloc_errorSyntax &&
               error == expected && compiled.offset <= c->length;
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
    TestRefused();
    TestPrefixes();
    return s_failures == 0 ? 0 : 1;
}
