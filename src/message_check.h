// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The data model's rules (UTS #35 Part 9, "Data Model Errors") that
// compare names or keys, checked on a message just compiled: duplicate
// declarations, duplicate option names, selectors without a function,
// and duplicate variants. Each check is linear, through a hash table and
// two words a variable in the compile's scratch. It also marks each
// variable as external, bound by .input or bound by .local.

#ifndef MLOC_SRC_MESSAGE_CHECK_H
#define MLOC_SRC_MESSAGE_CHECK_H

#include "message_table.h"

#include "maul-locale/message.h"

#include <stdint.h>

typedef struct MessageCheck
{
    mlocMessageError error;
    uint32_t at;
} MessageCheck;

// Checks a message message_emit wrote; table is its scratch table and
// words its two words a variable.
MessageCheck mlocCheckMessage(uint8_t* message, MessageTable* table, uint8_t* words);

#endif
