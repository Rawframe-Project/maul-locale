// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A resource read as a catalog source: a locale in its frontmatter, keys
// made of its section and entry identifiers and given once, the properties
// the tool understands on what they may describe, and every message
// compiling.

#ifndef MLOCCAT_RESOURCE_CHECK_H
#define MLOCCAT_RESOURCE_CHECK_H

#include "resource.h"

#include <stdio.h>

// Writes a key, its parts joined by dots, into out; 0 when a part holds
// a dot (an escaped one), which would make two keys one, or when it does
// not fit.
int ResourceKey(const ResId* section, const ResId* entry, char* out, size_t capacity);

// Checks a read resource; writes each problem to report as
// "path:line: reason" and returns 0 if there was one.
int ResourceCheck(const Resource* resource, const char* path, FILE* report);

#endif
