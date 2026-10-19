// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// JSON strings for the catalog tool's output: quotes, backslashes and
// control characters escaped, other bytes (UTF-8) written as they are.

#ifndef MLOCCAT_JSON_H
#define MLOCCAT_JSON_H

#include <stddef.h>
#include <stdio.h>

void JsonString(FILE* out, const char* bytes, size_t length);

#endif
