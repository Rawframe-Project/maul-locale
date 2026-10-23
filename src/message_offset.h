// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// :offset's sum: a number value plus or minus a small integer, exactly, in
// a work area of digits.

#ifndef MLOC_SRC_MESSAGE_OFFSET_H
#define MLOC_SRC_MESSAGE_OFFSET_H

#include "message_number.h"

#include <stdbool.h>
#include <stdint.h>

// Adds a small integer to a value (:offset), keeping the fraction digits
// it shows; false when the sum needs more digits than a decimal holds.
bool mlocOffsetNumber(NumberValue* value, int32_t delta);

#endif
