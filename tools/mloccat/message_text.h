// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Messages as the library compiles them. A message compiles when
// mlocCompileMessage succeeds into a buffer of the size it asked for: the
// size query alone passes messages whose data model is wrong (a variable
// declared twice, a selector without a function), which only the full
// compile refuses.

#ifndef MLOCCAT_MESSAGE_TEXT_H
#define MLOCCAT_MESSAGE_TEXT_H

#include <stddef.h>

int MessageCompiles(const char* text, size_t length);

// The message compiled and written back canonically (mlocWriteMessageText),
// NUL-terminated; the caller frees it. Null when it does not compile.
char* CanonicalMessage(const char* text, size_t length, size_t* lengthOut);

#endif
