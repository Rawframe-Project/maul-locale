// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Writes a compiled message as canonical MF2 text: a simple message
// where the pattern can stand alone, else each declaration, the
// selectors and each variant on a line of its own; one space between an
// expression's parts; literals quoted only where they must be; text and
// quoted literals escaped where they must be.

#ifndef MLOC_SRC_MESSAGE_WRITE_H
#define MLOC_SRC_MESSAGE_WRITE_H

#include "message_read.h"

#include "maul-locale/base.h"

// Writes a message read by mlocReadMessage.
mlocResult mlocWriteMessage(const MessageView* view, mlocOutput* output);

#endif
