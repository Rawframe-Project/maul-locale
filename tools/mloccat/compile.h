// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Catalogs compiled from their sources: the first resource is the set's
// source, the others its translations. Every resource is read and checked;
// the source's keys (but the obsolete) get their table and order; each
// resource becomes a data file of its locale, LOCALE.mloc in the directory
// given, holding the keys and its messages by index. A translation's key
// the source lacks is reported and left out; a key marked @translate no
// keeps the source's message.

#ifndef MLOCCAT_COMPILE_H
#define MLOCCAT_COMPILE_H

#include <stddef.h>
#include <stdio.h>

// Compiles; writes each problem and each catalog's counts to report.
// Returns 0 when a resource did not read or check, or a file could not
// be written.
int CatalogCompile(const char* directory, char* const* paths, size_t count, FILE* report);

#endif
