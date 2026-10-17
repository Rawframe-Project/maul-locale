// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for messages (record mloc-0045). The input's first
// byte chooses: even, the rest is MF2 text, which, when it compiles, is
// bound and formatted without arguments, written back, compiles again, and writes back the same
// text (the canonical form is a fixed point) with the same counts; odd, the rest is taken as a
// compiled message (its magic and versions made this library's), which mlocWriteMessageText reads
// or refuses without reading past it, and whose text, when it reads it, writes the same twice and
// measures its own length.

#include "bytes.h"
#include "message_form.h"

#include "maul-locale/message.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static uint8_t* Compile(const char* text, size_t length, size_t* sizeOut)
{
    size_t needed = 0;
    mlocMessageError error;
    mlocTextResult measured = mlocCompileMessage(text, length, NULL, 0, &needed, &error);
    if (measured.status != mloc_errorCapacity)
    {
        return NULL;
    }
    uint8_t* message = malloc(needed);
    mlocTextResult result = mlocCompileMessage(text, length, message, needed, sizeOut, &error);
    if (result.status != mloc_success)
    {
        free(message);
        return NULL;
    }
    Require(*sizeOut <= needed);
    return message;
}

// The message's text, measured first then written; null if refused.
static char* Write(const uint8_t* message, size_t size, size_t* lengthOut)
{
    mlocOutput measure = {NULL, 0, 0, NULL, NULL};
    mlocResult result = mlocWriteMessageText(message, size, &measure);
    if (result != mloc_errorCapacity && result != mloc_success)
    {
        return NULL;
    }
    char* text = malloc(measure.length + 1);
    mlocOutput output = {text, measure.length, 0, NULL, NULL};
    Require(mlocWriteMessageText(message, size, &output) == mloc_success &&
            output.length == measure.length);
    *lengthOut = output.length;
    return text;
}

// Binds a message and formats it with no arguments: every variable
// unresolved, every function unknown, the text measured then written.
static void Format(const uint8_t* message, size_t size)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    mlocLocale locale;
    mlocMessageFormatter formatter;
    Require(mlocParseLocale("en", 2, &locale).status == mloc_success &&
            mlocInitMessageFormatter(&formatter, &data, &locale, NULL) == mloc_success);
    size_t needed = 0;
    if (mlocBindMessage(&formatter, message, size, NULL, 0, &needed) != mloc_errorCapacity)
    {
        return;
    }
    uint8_t* bound = malloc(needed);
    size_t boundSize;
    Require(mlocBindMessage(&formatter, message, size, bound, needed, &boundSize) == mloc_success);
    mlocOutput measure = {NULL, 0, 0, NULL, NULL};
    mlocResult measured = mlocFormatMessage(bound, NULL, 0, &measure, NULL);
    char* text = malloc(measure.length + 1);
    mlocOutput output = {text, measure.length, 0, NULL, NULL};
    mlocResult result = mlocFormatMessage(bound, NULL, 0, &output, NULL);
    Require((measured == mloc_errorCapacity || measure.length == 0) &&
            (result == mloc_success || result == mloc_withFallbacks) &&
            output.length == measure.length);
    free(text);
    free(bound);
}

static void RoundTrip(const char* text, size_t length)
{
    size_t size;
    uint8_t* first = Compile(text, length, &size);
    if (first == NULL)
    {
        return;
    }
    Format(first, size);
    size_t written;
    char* canonical = Write(first, size, &written);
    Require(canonical != NULL);
    size_t again;
    uint8_t* second = Compile(canonical, written, &again);
    Require(second != NULL);
    // The same records, the source offsets aside.
    Require(memcmp(first + messageCounts, second + messageCounts, messageCount * 4) == 0);
    size_t rewritten;
    char* fixed = Write(second, again, &rewritten);
    Require(fixed != NULL && rewritten == written && memcmp(fixed, canonical, written) == 0);
    free(fixed);
    free(second);
    free(canonical);
    free(first);
}

static void ReadCompiled(const uint8_t* bytes, size_t size)
{
    // A copy of exactly the size, so that a read past it is caught.
    uint8_t* message = malloc(size > 0 ? size : 1);
    memcpy(message, bytes, size);
    if (size >= messageHeaderSize)
    {
        // This library's magic and versions, so that seeds compiled by
        // another version still reach the records.
        uint8_t current[128];
        size_t currentSize;
        mlocMessageError error;
        mlocTextResult result =
            mlocCompileMessage("", 0, current, sizeof current, &currentSize, &error);
        Require(result.status == mloc_success);
        memcpy(message, current, 4);
        memcpy(message + messageLibrary, current + messageLibrary, 8);
    }
    size_t length;
    char* text = Write(message, size, &length);
    if (text != NULL)
    {
        size_t again;
        char* twice = Write(message, size, &again);
        Require(twice != NULL && again == length && memcmp(text, twice, length) == 0);
        free(twice);
    }
    free(text);
    free(message);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
    {
        return 0;
    }
    if ((data[0] & 1) == 0)
    {
        RoundTrip((const char*)data + 1, size - 1);
    }
    else
    {
        ReadCompiled(data + 1, size - 1);
    }
    return 0;
}
