// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "number_output.h"

#include "invariant.h"

#include <string.h>

// Bytes a writing fills before the output: more than every coordinate
// and every day with its time in a probe of CLDR's locales wrote, and all
// but one in a thousand of the mixed units.
enum
{
    outputScratchSize = 128,
};

mlocResult mlocWriteOutput(NumberWriting writing, const void* context, mlocOutput* output)
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
        memcpy(output->buffer, scratch, w.length);
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
