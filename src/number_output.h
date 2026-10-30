// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Writing a formatted text into a caller's output so that a failure
// leaves it as it was: the formats whose text can fail partway through.

#ifndef MLOC_NUMBER_OUTPUT_H
#define MLOC_NUMBER_OUTPUT_H

#include "invariant.h"
#include "number_core.h"

#include <string.h>

// Writes text into a writer: mloc_success, or the error that stops it.
typedef mlocResult (*NumberWriting)(const void* context, NumberWriter* writer);

// Copies a short text without a call: two overlapping words cover any
// length from 4 to 16 bytes.
static inline void mlocCopyText(char* to, const char* from, size_t length)
{
    if (length > 16)
    {
        memcpy(to, from, length);
    }
    else if (length >= 8)
    {
        memcpy(to, from, 8);
        memcpy(to + length - 8, from + length - 8, 8);
    }
    else if (length >= 4)
    {
        memcpy(to, from, 4);
        memcpy(to + length - 4, from + length - 4, 4);
    }
    else
    {
        for (size_t i = 0; i < length; i++)
        {
            to[i] = from[i];
        }
    }
}

// Bytes a writing fills before the output: more than every coordinate
// and every day with its time in a probe of CLDR's locales wrote, and all
// but one in a thousand of the mixed units.
enum
{
    outputScratchSize = 128,
};

// Writes the text a writing gives into an output, which a failure leaves
// as it was (base.h): the text goes to a scratch on the stack first and
// is copied only once all of it is written; a text longer than the
// scratch is written again, into the output. Gives the length on
// mloc_errorCapacity, and the parts after the text when the output asks.
static inline mlocResult mlocWriteOutput(NumberWriting writing, const void* context,
                                         mlocOutput* output)
{
    char scratch[outputScratchSize];
    NumberWriter w = {.buffer = scratch, .capacity = sizeof(scratch)};
    mlocResult r = writing(context, &w);
    if (r != mloc_success)
    {
        return r;
    }
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        output->length = w.length;
        return mloc_errorCapacity;
    }
    if (w.length <= sizeof(scratch))
    {
        mlocCopyText(output->buffer, scratch, w.length);
    }
    else
    {
        // Longer than the scratch: written again, as it succeeded.
        NumberWriter direct = {.buffer = output->buffer, .capacity = output->capacity};
        mlocResult again = writing(context, &direct);
        MLOC_ASSERT(again == mloc_success && direct.length == w.length);
        (void)again;
    }
    output->length = w.length;
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        mlocResult again = writing(context, &parts);
        MLOC_ASSERT(again == mloc_success);
        (void)again;
        mlocEndParts(&parts);
    }
    return mloc_success;
}

#endif
