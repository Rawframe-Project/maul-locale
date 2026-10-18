// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Binding with preparation (record mloc-0045): each number function's
// expression is resolved once at bind with probe arguments, and the
// value it makes has its formatter and plural rules prepared in the
// bound message; a format call uses them when its value's options are
// the prepared ones, byte for byte, and makes its own otherwise.

#ifndef MLOC_SRC_MESSAGE_PREPARE_H
#define MLOC_SRC_MESSAGE_PREPARE_H

#include "message_date.h"
#include "message_duration.h"
#include "message_number.h"
#include "message_resolve.h"
#include "message_words.h"

// The value prepared for an expression, in the bound message, or null:
// none prepared, or the bound message moved to an address that leaves it
// unaligned.
// A PreparedNumber for a number function's value, a PreparedDate for a
// date function's.
const void* mlocPreparedFor(const Resolution* r, uint32_t expression);

#endif
