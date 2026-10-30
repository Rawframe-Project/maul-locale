// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Writing a formatted text into a caller's output so that a failure
// leaves it as it was: the formats whose text can fail partway through.

#ifndef MLOC_NUMBER_OUTPUT_H
#define MLOC_NUMBER_OUTPUT_H

#include "number_core.h"

// Writes text into a writer: mloc_success, or the error that stops it.
typedef mlocResult (*NumberWriting)(const void* context, NumberWriter* writer);

// Writes the text a writing gives into an output, which a failure leaves
// as it was (base.h): the text goes to a scratch on the stack first and
// is copied only once all of it is written; a text longer than the
// scratch is written again, into the output. Gives the length on
// mloc_errorCapacity, and the parts after the text when the output asks.
mlocResult mlocWriteOutput(NumberWriting writing, const void* context, mlocOutput* output);

#endif
