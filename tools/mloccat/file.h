// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Whole files read and written by the catalog tool.

#ifndef MLOCCAT_FILE_H
#define MLOCCAT_FILE_H

#include <stddef.h>

// A file's bytes, which the caller frees, NUL after them; null when it
// cannot be read.
char* FileRead(const char* path, size_t* lengthOut);

// Writes bytes to a file; 0 when it cannot.
int FileWrite(const char* path, const void* bytes, size_t length);

#endif
