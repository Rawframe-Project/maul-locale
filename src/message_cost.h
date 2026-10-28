// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A message's bounds (the MLOC_MAX_MESSAGE_* constants of
// maul-locale/message.h), checked when a message is compiled and again
// when stored bytes are bound: its counts, the chains of declarations, and
// the resolutions a format call makes. The costs go in the words binding
// lays out: a word per variable (the declaration binding it, or none), a
// word per expression (in its top byte the length of its chain), then a
// word per expression for the resolutions it makes.

#ifndef MLOC_SRC_MESSAGE_COST_H
#define MLOC_SRC_MESSAGE_COST_H

#include "message_read.h"

#include "maul-locale/message.h"

#include <stdbool.h>
#include <stdint.h>

// A variable's word when no declaration binds it.
static const uint32_t messageUnbound = UINT32_MAX;

// Whether a message's counts (one per record kind) are within the bounds.
bool mlocMessageCountsFit(const uint32_t* count);

// The words a message's costs take.
uint64_t mlocMessageCostWords(const MessageView* view);

// Writes a message's costs into words: mloc_success; mloc_errorLimit for
// a chain past MLOC_MAX_MESSAGE_CHAIN or a format call that would resolve
// more than MLOC_MAX_MESSAGE_RESOLUTIONS expressions; mloc_errorFormat for
// a declaration naming a later one, which only crafted bytes have.
mlocResult mlocWriteMessageCosts(const MessageView* view, uint8_t* words);

#endif
