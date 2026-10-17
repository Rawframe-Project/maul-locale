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

#include "message_number.h"
#include "message_resolve.h"

// The value prepared for an expression, or null: in the bound message
// when its address is 8-aligned there, else copied into storage.
const PreparedNumber* mlocPreparedFor(const Resolution* r, uint32_t expression,
                                      PreparedNumber* storage);

#endif
